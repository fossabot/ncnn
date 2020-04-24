// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "padding_vulkan.h"
#include <algorithm>
#include "layer_shader_type.h"

namespace ncnn {

DEFINE_LAYER_CREATOR(Padding_vulkan)

Padding_vulkan::Padding_vulkan()
{
    support_vulkan = true;
    support_image_storage = true;

    pipeline_padding = 0;
    pipeline_padding_pack4 = 0;
    pipeline_padding_pack8 = 0;
}

int Padding_vulkan::create_pipeline(const Option& opt)
{
    const Mat& shape = bottom_shapes.empty() ? Mat() : bottom_shapes[0];
    const Mat& out_shape = top_shapes.empty() ? Mat() : top_shapes[0];

    int elempack = 1;
    if (shape.dims == 1) elempack = opt.use_shader_pack8 && shape.w % 8 == 0 ? 8 : shape.w % 4 == 0 ? 4 : 1;
    if (shape.dims == 2) elempack = opt.use_shader_pack8 && shape.h % 8 == 0 ? 8 : shape.h % 4 == 0 ? 4 : 1;
    if (shape.dims == 3) elempack = opt.use_shader_pack8 && shape.c % 8 == 0 ? 8 : shape.c % 4 == 0 ? 4 : 1;

    int out_elempack = 1;
    if (out_shape.dims == 1) out_elempack = opt.use_shader_pack8 && out_shape.w % 8 == 0 ? 8 : out_shape.w % 4 == 0 ? 4 : 1;
    if (out_shape.dims == 2) out_elempack = opt.use_shader_pack8 && out_shape.h % 8 == 0 ? 8 : out_shape.h % 4 == 0 ? 4 : 1;
    if (out_shape.dims == 3) out_elempack = opt.use_shader_pack8 && out_shape.c % 8 == 0 ? 8 : out_shape.c % 4 == 0 ? 4 : 1;

    size_t elemsize;
    size_t out_elemsize;
    if (opt.use_image_storage && opt.use_image_fp16_storage)
    {
        elemsize = elempack * 2u;
        out_elemsize = out_elempack * 2u;
    }
    else if (opt.use_image_storage)
    {
        elemsize = elempack * 4u;
        out_elemsize = out_elempack * 4u;
    }
    else if (opt.use_fp16_storage)
    {
        elemsize = elempack * 2u;
        out_elemsize = out_elempack * 2u;
    }
    else if (opt.use_fp16_packed)
    {
        elemsize = elempack == 1 ? 4u : elempack * 2u;
        out_elemsize = out_elempack == 1 ? 4u : out_elempack * 2u;
    }
    else
    {
        elemsize = elempack * 4u;
        out_elemsize = out_elempack * 4u;
    }

    Mat shape_packed;
    if (shape.dims == 1) shape_packed = Mat(shape.w / elempack, (void*)0, elemsize, elempack);
    if (shape.dims == 2) shape_packed = Mat(shape.w, shape.h / elempack, (void*)0, elemsize, elempack);
    if (shape.dims == 3) shape_packed = Mat(shape.w, shape.h, shape.c / elempack, (void*)0, elemsize, elempack);

    Mat out_shape_packed;
    if (out_shape.dims == 1) out_shape_packed = Mat(out_shape.w / out_elempack, (void*)0, out_elemsize, out_elempack);
    if (out_shape.dims == 2) out_shape_packed = Mat(out_shape.w, out_shape.h / out_elempack, (void*)0, out_elemsize, out_elempack);
    if (out_shape.dims == 3) out_shape_packed = Mat(out_shape.w, out_shape.h, out_shape.c / out_elempack, (void*)0, out_elemsize, out_elempack);

    std::vector<vk_specialization_type> specializations(3 + 10);
    specializations[0].i = type;
    specializations[1].f = value;
    specializations[2].i = per_channel_pad_data_size ? 1 : 0;
    specializations[3 + 0].i = shape_packed.dims;
    specializations[3 + 1].i = shape_packed.w;
    specializations[3 + 2].i = shape_packed.h;
    specializations[3 + 3].i = shape_packed.c;
    specializations[3 + 4].i = shape_packed.cstep;
    specializations[3 + 5].i = out_shape_packed.dims;
    specializations[3 + 6].i = out_shape_packed.w;
    specializations[3 + 7].i = out_shape_packed.h;
    specializations[3 + 8].i = out_shape_packed.c;
    specializations[3 + 9].i = out_shape_packed.cstep;

    Mat local_size_xyz;
    if (out_shape_packed.dims != 0)
    {
        local_size_xyz.w = std::min(4, out_shape_packed.w);
        local_size_xyz.h = std::min(4, out_shape_packed.h);
        local_size_xyz.c = std::min(4, out_shape_packed.c);
    }

    // pack1
    if (shape.dims == 0 || elempack == 1)
    {
        pipeline_padding = new Pipeline(vkdev);
        pipeline_padding->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_padding->create(LayerShaderType::padding, opt, specializations);
    }

    // pack4
    if (shape.dims == 0 || elempack == 4)
    {
        pipeline_padding_pack4 = new Pipeline(vkdev);
        pipeline_padding_pack4->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_padding_pack4->create(LayerShaderType::padding_pack4, opt, specializations);
    }

    // pack8
    if ((opt.use_shader_pack8 && shape.dims == 0) || elempack == 8)
    {
        pipeline_padding_pack8 = new Pipeline(vkdev);
        pipeline_padding_pack8->set_optimal_local_size_xyz(local_size_xyz);
        pipeline_padding_pack8->create(LayerShaderType::padding_pack8, opt, specializations);
    }

    return 0;
}

int Padding_vulkan::destroy_pipeline(const Option& /*opt*/)
{
    delete pipeline_padding;
    pipeline_padding = 0;

    delete pipeline_padding_pack4;
    pipeline_padding_pack4 = 0;

    delete pipeline_padding_pack8;
    pipeline_padding_pack8 = 0;

    return 0;
}

int Padding_vulkan::upload_model(VkTransfer& cmd, const Option& opt)
{
    if (per_channel_pad_data_size == 0)
    {
        cmd.record_upload(Mat(1), per_channel_pad_data_gpu_image, opt);
        return 0;
    }

    int elempack = opt.use_shader_pack8 && per_channel_pad_data_size % 8 == 0 ? 8 : per_channel_pad_data_size % 4 == 0 ? 4 : 1;

    Mat per_channel_pad_data_packed;
    convert_packing(per_channel_pad_data, per_channel_pad_data_packed, elempack);

    cmd.record_upload(per_channel_pad_data_packed, per_channel_pad_data_gpu, opt);

    cmd.record_upload(per_channel_pad_data_packed, per_channel_pad_data_gpu_image, opt);

    return 0;
}

int Padding_vulkan::forward(const VkMat& bottom_blob, VkMat& top_blob, VkCompute& cmd, const Option& opt) const
{
    if (top == 0 && bottom == 0 && left == 0 && right == 0)
    {
        top_blob = bottom_blob;
        return 0;
    }

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;
    size_t elemsize = bottom_blob.elemsize;
    int elempack = bottom_blob.elempack;

    // TODO vec and image padding

    int outw = w + left + right;
    int outh = h + top + bottom;

    top_blob.create(outw, outh, channels, elemsize, elempack, opt.blob_vkallocator);
    if (top_blob.empty())
        return -100;

    std::vector<VkMat> bindings(3);
    bindings[0] = bottom_blob;
    bindings[1] = top_blob;
    bindings[2] = per_channel_pad_data_size ? per_channel_pad_data_gpu : top_blob;// TODO use dummy buffer

    std::vector<vk_constant_type> constants(12);
    constants[0].i = bottom_blob.dims;
    constants[1].i = bottom_blob.w;
    constants[2].i = bottom_blob.h;
    constants[3].i = bottom_blob.c;
    constants[4].i = bottom_blob.cstep;
    constants[5].i = top_blob.dims;
    constants[6].i = top_blob.w;
    constants[7].i = top_blob.h;
    constants[8].i = top_blob.c;
    constants[9].i = top_blob.cstep;
    constants[10].i = left;
    constants[11].i = top;

    const Pipeline* pipeline = elempack == 8 ? pipeline_padding_pack8
                             : elempack == 4 ? pipeline_padding_pack4
                             : pipeline_padding;

    cmd.record_pipeline(pipeline, bindings, constants, top_blob);

    return 0;
}

int Padding_vulkan::forward(const std::vector<VkMat>& bottom_blobs, std::vector<VkMat>& top_blobs, VkCompute& cmd, const Option& opt) const
{
    const VkMat& bottom_blob = bottom_blobs[0];
    const VkMat& reference_blob = bottom_blobs[1];

    VkMat& top_blob = top_blobs[0];

    int _top;
    int _bottom;
    int _left;
    int _right;
    {
        const int* param_data = reference_blob.mapped();

        _top = param_data[0];
        _bottom = param_data[1];
        _left = param_data[2];
        _right = param_data[3];
    }

    if (_top == 0 && _bottom == 0 && _left == 0 && _right == 0)
    {
        top_blob = bottom_blob;
        return 0;
    }

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;
    size_t elemsize = bottom_blob.elemsize;
    int elempack = bottom_blob.elempack;

    // TODO vec and image padding

    int outw = w + _left + _right;
    int outh = h + _top + _bottom;

    top_blob.create(outw, outh, channels, elemsize, elempack, opt.blob_vkallocator);
    if (top_blob.empty())
        return -100;

    std::vector<VkMat> bindings(3);
    bindings[0] = bottom_blob;
    bindings[1] = top_blob;
    bindings[2] = per_channel_pad_data_size ? per_channel_pad_data_gpu : top_blob;// TODO use dummy buffer

    std::vector<vk_constant_type> constants(12);
    constants[0].i = bottom_blob.dims;
    constants[1].i = bottom_blob.w;
    constants[2].i = bottom_blob.h;
    constants[3].i = bottom_blob.c;
    constants[4].i = bottom_blob.cstep;
    constants[5].i = top_blob.dims;
    constants[6].i = top_blob.w;
    constants[7].i = top_blob.h;
    constants[8].i = top_blob.c;
    constants[9].i = top_blob.cstep;
    constants[10].i = _left;
    constants[11].i = _top;

    const Pipeline* pipeline = elempack == 8 ? pipeline_padding_pack8
                             : elempack == 4 ? pipeline_padding_pack4
                             : pipeline_padding;

    cmd.record_pipeline(pipeline, bindings, constants, top_blob);

    return 0;
}

int Padding_vulkan::forward(const VkImageMat& bottom_blob, VkImageMat& top_blob, VkCompute& cmd, const Option& opt) const
{
    if (top == 0 && bottom == 0 && left == 0 && right == 0)
    {
        top_blob = bottom_blob;
        return 0;
    }

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;
    size_t elemsize = bottom_blob.elemsize;
    int elempack = bottom_blob.elempack;

    // TODO vec and image padding

    int outw = w + left + right;
    int outh = h + top + bottom;

    top_blob.create(outw, outh, channels, elemsize, elempack, opt.blob_vkallocator);
    if (top_blob.empty())
        return -100;

    std::vector<VkImageMat> bindings(3);
    bindings[0] = bottom_blob;
    bindings[1] = top_blob;
    bindings[2] = per_channel_pad_data_gpu_image;// TODO use dummy buffer

    std::vector<vk_constant_type> constants(12);
    constants[0].i = bottom_blob.dims;
    constants[1].i = bottom_blob.w;
    constants[2].i = bottom_blob.h;
    constants[3].i = bottom_blob.c;
    constants[4].i = 0;//bottom_blob.cstep;
    constants[5].i = top_blob.dims;
    constants[6].i = top_blob.w;
    constants[7].i = top_blob.h;
    constants[8].i = top_blob.c;
    constants[9].i = 0;//top_blob.cstep;
    constants[10].i = left;
    constants[11].i = top;

    const Pipeline* pipeline = elempack == 8 ? pipeline_padding_pack8
                             : elempack == 4 ? pipeline_padding_pack4
                             : pipeline_padding;

    cmd.record_pipeline(pipeline, bindings, constants, top_blob);

    return 0;
}

int Padding_vulkan::forward(const std::vector<VkImageMat>& bottom_blobs, std::vector<VkImageMat>& top_blobs, VkCompute& cmd, const Option& opt) const
{
    const VkImageMat& bottom_blob = bottom_blobs[0];
    const VkImageMat& reference_blob = bottom_blobs[1];

    VkImageMat& top_blob = top_blobs[0];

    int _top;
    int _bottom;
    int _left;
    int _right;
    {
        const int* param_data = reference_blob.mapped();

        _top = param_data[0];
        _bottom = param_data[1];
        _left = param_data[2];
        _right = param_data[3];
    }

    if (_top == 0 && _bottom == 0 && _left == 0 && _right == 0)
    {
        top_blob = bottom_blob;
        return 0;
    }

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;
    size_t elemsize = bottom_blob.elemsize;
    int elempack = bottom_blob.elempack;

    // TODO vec and image padding

    int outw = w + _left + _right;
    int outh = h + _top + _bottom;

    top_blob.create(outw, outh, channels, elemsize, elempack, opt.blob_vkallocator);
    if (top_blob.empty())
        return -100;

    std::vector<VkImageMat> bindings(3);
    bindings[0] = bottom_blob;
    bindings[1] = top_blob;
    bindings[2] = per_channel_pad_data_gpu_image;// TODO use dummy buffer

    std::vector<vk_constant_type> constants(12);
    constants[0].i = bottom_blob.dims;
    constants[1].i = bottom_blob.w;
    constants[2].i = bottom_blob.h;
    constants[3].i = bottom_blob.c;
    constants[4].i = 0;//bottom_blob.cstep;
    constants[5].i = top_blob.dims;
    constants[6].i = top_blob.w;
    constants[7].i = top_blob.h;
    constants[8].i = top_blob.c;
    constants[9].i = 0;//top_blob.cstep;
    constants[10].i = _left;
    constants[11].i = _top;

    const Pipeline* pipeline = elempack == 8 ? pipeline_padding_pack8
                             : elempack == 4 ? pipeline_padding_pack4
                             : pipeline_padding;

    cmd.record_pipeline(pipeline, bindings, constants, top_blob);

    return 0;
}

} // namespace ncnn
