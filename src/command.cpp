// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "command.h"

#if NCNN_VULKAN

#include <stdio.h>

namespace ncnn {

Command::Command(VulkanDevice* _vkdev, uint32_t queue_index) : vkdev(_vkdev)
{
    // get queue
    vkGetDeviceQueue(vkdev->vkdevice(), queue_index, 0, &queue);

    create_command_pool();

    create_command_buffer();

    // create fence
    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = 0;
    fenceCreateInfo.flags = 0;

    VkResult ret = vkCreateFence(vkdev->vkdevice(), &fenceCreateInfo, 0, &fence);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateFence failed %d\n", ret);
    }
}

Command::~Command()
{
    vkDestroyFence(vkdev->vkdevice(), fence, 0);

    vkFreeCommandBuffers(vkdev->vkdevice(), command_pool, 1, &command_buffer);

    vkDestroyCommandPool(vkdev->vkdevice(), command_pool, 0);
}

int Command::create_command_pool()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = 0;
    commandPoolCreateInfo.flags = 0;
    commandPoolCreateInfo.queueFamilyIndex = vkdev->info.compute_queue_index;

    VkResult ret = vkCreateCommandPool(vkdev->vkdevice(), &commandPoolCreateInfo, 0, &command_pool);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateCommandPool failed %d\n", ret);
        return -1;
    }

    return 0;
}

int Command::create_command_buffer()
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = 0;
    commandBufferAllocateInfo.commandPool = command_pool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkResult ret = vkAllocateCommandBuffers(vkdev->vkdevice(), &commandBufferAllocateInfo, &command_buffer);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkAllocateCommandBuffers failed %d\n", ret);
        return -1;
    }

    return 0;
}

int Command::begin_command_buffer()
{
    fprintf(stderr, "==================== begin\n");

    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = 0;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    commandBufferBeginInfo.pInheritanceInfo = 0;

    VkResult ret = vkBeginCommandBuffer(command_buffer, &commandBufferBeginInfo);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkBeginCommandBuffer failed %d\n", ret);
        return -1;
    }

    return 0;
}

int Command::end_command_buffer()
{
    fprintf(stderr, "==================== end\n");

    VkResult ret = vkEndCommandBuffer(command_buffer);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkEndCommandBuffer failed %d\n", ret);
        return -1;
    }

    return 0;
}

int Command::queue_submit()
{
    fprintf(stderr, "==================== submit\n");

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = 0;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = 0;
    submitInfo.pWaitDstStageMask = 0;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = 0;

    VkResult ret = vkQueueSubmit(queue, 1, &submitInfo, fence);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkQueueSubmit failed %d\n", ret);
        return -1;
    }

    return 0;
}

int Command::wait_fence()
{
    fprintf(stderr, "==================== wait\n");

    VkResult ret = vkWaitForFences(vkdev->vkdevice(), 1, &fence, VK_TRUE, UINT64_MAX);
    if (ret != VK_SUCCESS)
    {
        fprintf(stderr, "vkWaitForFences failed %d\n", ret);
        return -1;
    }

    return 0;
}

VkCompute::VkCompute(VulkanDevice* _vkdev) : Command(_vkdev, _vkdev->info.compute_queue_index)
{
}

VkCompute::~VkCompute()
{
    if (!vkdev->info.support_VK_KHR_push_descriptor)
    {
        for (size_t i=0; i<descriptorsets.size(); i++)
        {
            vkFreeDescriptorSets(vkdev->vkdevice(), descriptor_pools[i], 1, &descriptorsets[i]);
            vkDestroyDescriptorPool(vkdev->vkdevice(), descriptor_pools[i], 0);
        }
    }
}

int VkCompute::begin()
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return begin_command_buffer();

    record_type r;
    r.type = 0;
    delayed_records.push_back(r);

    return 0;
}

void VkCompute::record_upload(const VkMat& m)
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return copy_buffer(m.staging_buffer, 0, m.buffer, m.offset, m.total() * m.elemsize);

    record_type r;
    r.type = 1;
    r.copy.src = m.staging_buffer;
    r.copy.src_offset = 0;
    r.copy.dst = m.buffer;
    r.copy.dst_offset = m.offset;
    r.copy.size = m.total() * m.elemsize;
    delayed_records.push_back(r);
}

void VkCompute::record_download(const VkMat& m)
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return copy_buffer(m.buffer, m.offset, m.staging_buffer, 0, m.total() * m.elemsize);

    record_type r;
    r.type = 1;
    r.copy.src = m.buffer;
    r.copy.src_offset = m.offset;
    r.copy.dst = m.staging_buffer;
    r.copy.dst_offset = 0;
    r.copy.size = m.total() * m.elemsize;
    delayed_records.push_back(r);
}

void VkCompute::record_clone(const VkMat& src, const VkMat& dst)
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return copy_buffer(src.buffer, src.offset, dst.buffer, dst.offset, src.total() * src.elemsize);

    record_type r;
    r.type = 1;
    r.copy.src = src.buffer;
    r.copy.src_offset = src.offset;
    r.copy.dst = dst.buffer;
    r.copy.dst_offset = dst.offset;
    r.copy.size = src.total() * src.elemsize;
    delayed_records.push_back(r);
}

void VkCompute::record_copy_region(const VkMat& src, const VkMat& dst, const VkBufferCopy& region)
{
    std::vector<VkBufferCopy> regions(1);
    regions[0] = region;

    record_copy_regions(src, dst, regions);
}

void VkCompute::record_copy_regions(const VkMat& src, const VkMat& dst, const std::vector<VkBufferCopy>& regions)
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return copy_buffer_regions(src.buffer, dst.buffer, regions);

    record_type r;
    r.type = 2;
    r.copy_regions.src = src.buffer;
    r.copy_regions.dst = dst.buffer;
    r.regions = regions;
    delayed_records.push_back(r);
}

void VkCompute::record_bind_pipeline(VkPipeline pipeline)
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return bind_pipeline(pipeline);

    record_type r;
    r.type = 3;
    r.bind_pipeline.pipeline = pipeline;
    delayed_records.push_back(r);
}

void VkCompute::record_update_bindings(VkPipelineLayout pipeline_layout, VkDescriptorSetLayout descriptorset_layout, VkDescriptorUpdateTemplateKHR descriptor_update_template, const std::vector<VkMat>& bindings)
{
    const int binding_count = bindings.size();

    std::vector<VkDescriptorBufferInfo> descriptorBufferInfos(binding_count);
    for (int i=0; i<binding_count; i++)
    {
        descriptorBufferInfos[i].buffer = bindings[i].buffer;
        descriptorBufferInfos[i].offset = bindings[i].offset;
        descriptorBufferInfos[i].range = bindings[i].total() * bindings[i].elemsize;
    }

    if (vkdev->info.support_VK_KHR_push_descriptor)
        return update_bindings(pipeline_layout, descriptor_update_template, descriptorBufferInfos);

    // create new descriptor_pool and descriptorset
    VkDescriptorPool descriptor_pool;
    {
        VkDescriptorPoolSize poolSize;
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = binding_count;

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.pNext = 0;
        descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descriptorPoolCreateInfo.maxSets = 1;
        descriptorPoolCreateInfo.poolSizeCount = 1;
        descriptorPoolCreateInfo.pPoolSizes = &poolSize;

        VkResult ret = vkCreateDescriptorPool(vkdev->vkdevice(), &descriptorPoolCreateInfo, 0, &descriptor_pool);
        if (ret != VK_SUCCESS)
        {
            fprintf(stderr, "vkCreateDescriptorPool failed %d\n", ret);
            return;
        }
    }
    descriptor_pools.push_back(descriptor_pool);

    VkDescriptorSet descriptorset;
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.pNext = 0;
        descriptorSetAllocateInfo.descriptorPool = descriptor_pool;
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.pSetLayouts = &descriptorset_layout;

        VkResult ret = vkAllocateDescriptorSets(vkdev->vkdevice(), &descriptorSetAllocateInfo, &descriptorset);
        if (ret != VK_SUCCESS)
        {
            fprintf(stderr, "vkAllocateDescriptorSets failed %d\n", ret);
            return;
        }
    }
    descriptorsets.push_back(descriptorset);

    fprintf(stderr, "update descriptorset %p\n", descriptorset);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets(binding_count);
    for (int i=0; i<binding_count; i++)
    {
        writeDescriptorSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[i].pNext = 0;
        writeDescriptorSets[i].dstSet = descriptorset;
        writeDescriptorSets[i].dstBinding = i;
        writeDescriptorSets[i].dstArrayElement = 0;
        writeDescriptorSets[i].descriptorCount = 1;
        writeDescriptorSets[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeDescriptorSets[i].pImageInfo = 0;
        writeDescriptorSets[i].pBufferInfo = &descriptorBufferInfos[i];
        writeDescriptorSets[i].pTexelBufferView = 0;
    }

    vkUpdateDescriptorSets(vkdev->vkdevice(), binding_count, writeDescriptorSets.data(), 0, 0);

    record_type r;
    r.type = 4;
    r.bind_descriptorset.pipeline_layout = pipeline_layout;
    r.bind_descriptorset.descriptorset = descriptorset;
    delayed_records.push_back(r);
}

void VkCompute::record_push_constants(VkPipelineLayout pipeline_layout, const std::vector<vk_constant_type>& constants)
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return push_constants(pipeline_layout, constants);

    record_type r;
    r.type = 5;
    r.push_constants.pipeline_layout = pipeline_layout;
    r.constants = constants;
    delayed_records.push_back(r);
}

void VkCompute::record_dispatch(const uint32_t* group_count_xyz)
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return dispatch(group_count_xyz);

    record_type r;
    r.type = 6;
    r.dispatch.group_count_xyz[0] = group_count_xyz[0];
    r.dispatch.group_count_xyz[1] = group_count_xyz[1];
    r.dispatch.group_count_xyz[2] = group_count_xyz[2];
    delayed_records.push_back(r);
}

void VkCompute::record_upload_compute_barrier(const VkMat& m)
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return upload_compute_barrier(m.buffer, m.offset, m.total() * m.elemsize);

    record_type r;
    r.type = 7;
    r.upload_compute_barrier.buffer = m.buffer;
    r.upload_compute_barrier.offset = m.offset;
    r.upload_compute_barrier.size = m.total() * m.elemsize;
    delayed_records.push_back(r);
}

void VkCompute::record_compute_download_barrier(const VkMat& m)
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return compute_download_barrier(m.buffer, m.offset, m.total() * m.elemsize);

    record_type r;
    r.type = 8;
    r.compute_download_barrier.buffer = m.buffer;
    r.compute_download_barrier.offset = m.offset;
    r.compute_download_barrier.size = m.total() * m.elemsize;
    delayed_records.push_back(r);
}

void VkCompute::record_compute_compute_barrier(const VkMat& m)
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return compute_compute_barrier(m.buffer, m.offset, m.total() * m.elemsize);

    record_type r;
    r.type = 9;
    r.compute_compute_barrier.buffer = m.buffer;
    r.compute_compute_barrier.offset = m.offset;
    r.compute_compute_barrier.size = m.total() * m.elemsize;
    delayed_records.push_back(r);
}

int VkCompute::end()
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return end_command_buffer();

    record_type r;
    r.type = 10;
    delayed_records.push_back(r);

    return 0;
}

int VkCompute::submit()
{
    if (vkdev->info.support_VK_KHR_push_descriptor)
        return queue_submit();

    // handle delayed records
    for (size_t i=0; i<delayed_records.size(); i++)
    {
        const record_type& r = delayed_records[i];

        switch (r.type)
        {
        case 0:
            begin_command_buffer();
            break;
        case 1:
            copy_buffer(r.copy.src, r.copy.src_offset, r.copy.dst, r.copy.dst_offset, r.copy.size);
            break;
        case 2:
            copy_buffer_regions(r.copy_regions.src, r.copy_regions.dst, r.regions);
            break;
        case 3:
            bind_pipeline(r.bind_pipeline.pipeline);
            break;
        case 4:
            bind_descriptorset(r.bind_descriptorset.pipeline_layout, r.bind_descriptorset.descriptorset);
            break;
        case 5:
            push_constants(r.push_constants.pipeline_layout, r.constants);
            break;
        case 6:
            dispatch(r.dispatch.group_count_xyz);
            break;
        case 7:
            upload_compute_barrier(r.upload_compute_barrier.buffer, r.upload_compute_barrier.offset, r.upload_compute_barrier.size);
            break;
        case 8:
            compute_download_barrier(r.compute_download_barrier.buffer, r.compute_download_barrier.offset, r.compute_download_barrier.size);
            break;
        case 9:
            compute_compute_barrier(r.compute_compute_barrier.buffer, r.compute_compute_barrier.offset, r.compute_compute_barrier.size);
            break;
        case 10:
            end_command_buffer();
            break;
        }
    }

    return queue_submit();
}

int VkCompute::wait()
{
    return wait_fence();
}

void VkCompute::copy_buffer(VkBuffer src, size_t src_offset, VkBuffer dst, size_t dst_offset, size_t size)
{
//     fprintf(stderr, "cmd copy %p to %p\n", src, dst);

    VkBufferCopy region;
    region.srcOffset = src_offset;
    region.dstOffset = dst_offset;
    region.size = size;

    vkCmdCopyBuffer(command_buffer, src, dst, 1, &region);
}

void VkCompute::copy_buffer_regions(VkBuffer src, VkBuffer dst, const std::vector<VkBufferCopy>& regions)
{
//     fprintf(stderr, "cmd copy regions %p to %p\n", src, dst);

    vkCmdCopyBuffer(command_buffer, src, dst, regions.size(), regions.data());
}

void VkCompute::bind_pipeline(VkPipeline pipeline)
{
//     fprintf(stderr, "cmd bind_pipeline %p\n", pipeline);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

void VkCompute::bind_descriptorset(VkPipelineLayout pipeline_layout, VkDescriptorSet descriptorset)
{
//     fprintf(stderr, "cmd bind_descriptorset %p %p\n", pipeline_layout, descriptorset);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptorset, 0, 0);
}

void VkCompute::update_bindings(VkPipelineLayout pipeline_layout, VkDescriptorUpdateTemplateKHR descriptor_update_template, const std::vector<VkDescriptorBufferInfo>& descriptorBufferInfos)
{
//     fprintf(stderr, "cmd update_bindings %p %p\n", pipeline_layout, descriptor_update_template);

    vkdev->vkCmdPushDescriptorSetWithTemplateKHR(command_buffer, descriptor_update_template, pipeline_layout, 0, descriptorBufferInfos.data());
}

void VkCompute::push_constants(VkPipelineLayout pipeline_layout, const std::vector<vk_constant_type>& constants)
{
//     fprintf(stderr, "cmd push_constants %p\n", pipeline_layout);

    vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, constants.size() * sizeof(vk_constant_type), constants.data());
}

void VkCompute::dispatch(const uint32_t* group_count_xyz)
{
//     fprintf(stderr, "cmd dispatch %d %d %d\n", group_count_xyz[0], group_count_xyz[1], group_count_xyz[2]);

    vkCmdDispatch(command_buffer, group_count_xyz[0], group_count_xyz[1], group_count_xyz[2]);
}

void VkCompute::upload_compute_barrier(VkBuffer buffer, size_t offset, size_t size)
{
//     fprintf(stderr, "cmd upload_compute_barrier %p\n", buffer);

    VkBufferMemoryBarrier bufferBarrier;
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.pNext = 0;
    bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = buffer;
    bufferBarrier.offset = offset;
    bufferBarrier.size = size;

    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    vkCmdPipelineBarrier(command_buffer, srcStageMask, dstStageMask, 0, 0, 0, 1, &bufferBarrier, 0, 0);
}

void VkCompute::compute_download_barrier(VkBuffer buffer, size_t offset, size_t size)
{
//     fprintf(stderr, "cmd compute_download_barrier %p\n", buffer);

    VkBufferMemoryBarrier bufferBarrier;
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.pNext = 0;
    bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = buffer;
    bufferBarrier.offset = offset;
    bufferBarrier.size = size;

    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdPipelineBarrier(command_buffer, srcStageMask, dstStageMask, 0, 0, 0, 1, &bufferBarrier, 0, 0);
}

void VkCompute::compute_compute_barrier(VkBuffer buffer, size_t offset, size_t size)
{
//     fprintf(stderr, "cmd compute_compute_barrier %p\n", buffer);

    VkBufferMemoryBarrier bufferBarrier;
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.pNext = 0;
    bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = buffer;
    bufferBarrier.offset = offset;
    bufferBarrier.size = size;

    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    vkCmdPipelineBarrier(command_buffer, srcStageMask, dstStageMask, 0, 0, 0, 1, &bufferBarrier, 0, 0);
}

// VkTransfer::VkTransfer(VulkanDevice* _vkdev) : Command(_vkdev, _vkdev->info.transfer_queue_index) // TODO use transfer queue
VkTransfer::VkTransfer(VulkanDevice* _vkdev) : Command(_vkdev, _vkdev->info.compute_queue_index)
{
    staging_buffer = 0;
    staging_memory = 0;
    mapped_ptr = 0;
}

VkTransfer::~VkTransfer()
{
}

void VkTransfer::record_upload(const Mat& src, VkMat& dst)
{
    dst.create_like(src, weight_vkallocator, staging_vkallocator);

    record_type r;
    r.type = 0;
    r.size = src.total() * src.elemsize;
    r.upload.src = src.data;
    r.upload.dst = dst.buffer;
    r.upload.dst_offset = dst.offset;
    delayed_records.push_back(r);
}

void VkTransfer::record_download(const VkMat& src, Mat& dst)
{
    dst.create_like(src);// TODO respect blob allocator

    record_type r;
    r.type = 1;
    r.size = src.total() * src.elemsize;
    r.download.src = src.buffer;
    r.download.src_offset = src.offset;
    r.download.dst = dst.data;
    delayed_records.push_back(r);
}

int VkTransfer::submit()
{
    int transfer_count = delayed_records.size();

    // solve staging buffer size
    size_t staging_buffer_size = 0;
    for (int i=0; i<transfer_count; i++)
    {
        const record_type& r = delayed_records[i];
        staging_buffer_size += r.size;
    }

    // allocate staging buffer
    staging_buffer = staging_vkallocator->create_staging_buffer(staging_buffer_size);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(vkdev->vkdevice(), staging_buffer, &memoryRequirements);

    staging_memory = staging_vkallocator->fastMalloc(memoryRequirements.size);

    vkBindBufferMemory(vkdev->vkdevice(), staging_buffer, staging_memory, 0);

    // map
    vkMapMemory(vkdev->vkdevice(), staging_memory, 0, staging_buffer_size, 0, &mapped_ptr);

    // copy upload data
    size_t mapped_ptr_offset = 0;
    for (int i=0; i<transfer_count; i++)
    {
        const record_type& r = delayed_records[i];
        if (r.type == 0)
        {
            memcpy((unsigned char*)mapped_ptr + mapped_ptr_offset, r.upload.src, r.size);
        }

        mapped_ptr_offset += r.size;
    }

    begin_command_buffer();

    fprintf(stderr, "cmd transfer %p %lu\n", staging_buffer, staging_buffer_size);

    // handle delayed records
    size_t staging_buffer_offset = 0;
    for (int i=0; i<transfer_count; i++)
    {
        const record_type& r = delayed_records[i];

        switch (r.type)
        {
        case 0:
            copy_buffer(staging_buffer, staging_buffer_offset, r.upload.dst, r.upload.dst_offset, r.size);
            break;
        case 1:
            copy_buffer(r.download.src, r.download.src_offset, staging_buffer, staging_buffer_offset, r.size);
            break;
        }

        staging_buffer_offset += r.size;
    }

    end_command_buffer();

    return queue_submit();
}

int VkTransfer::wait()
{
    int ret = wait_fence();

    int transfer_count = delayed_records.size();

    // copy download data
    size_t mapped_ptr_offset = 0;
    for (int i=0; i<transfer_count; i++)
    {
        const record_type& r = delayed_records[i];
        if (r.type == 1)
        {
            memcpy(r.download.dst, (unsigned char*)mapped_ptr + mapped_ptr_offset, r.size);
        }

        mapped_ptr_offset += r.size;
    }

    // unmap
    vkUnmapMemory(vkdev->vkdevice(), staging_memory);
    mapped_ptr = 0;

    // deallocate staging buffer
    staging_vkallocator->destroy_buffer(staging_buffer);
    staging_vkallocator->fastFree(staging_memory);

    staging_buffer = 0;
    staging_memory = 0;

    return ret;
}

void VkTransfer::copy_buffer(VkBuffer src, size_t src_offset, VkBuffer dst, size_t dst_offset, size_t size)
{
//     fprintf(stderr, "cmd copy %p to %p\n", src, dst);

    VkBufferCopy region;
    region.srcOffset = src_offset;
    region.dstOffset = dst_offset;
    region.size = size;

    vkCmdCopyBuffer(command_buffer, src, dst, 1, &region);
}

void VkTransfer::copy_buffer_regions(VkBuffer src, VkBuffer dst, const std::vector<VkBufferCopy>& regions)
{
//     fprintf(stderr, "cmd copy regions %p to %p\n", src, dst);

    vkCmdCopyBuffer(command_buffer, src, dst, regions.size(), regions.data());
}

} // namespace ncnn

#endif // NCNN_VULKAN
