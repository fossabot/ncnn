// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2017 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "binaryop.h"

#include <math.h>

namespace ncnn {

BinaryOp::BinaryOp()
{
    one_blob_only = false;
    support_inplace = false;
}

int BinaryOp::load_param(const ParamDict& pd)
{
    op_type = pd.get(0, 0);
    with_scalar = pd.get(1, 0);
    b = pd.get(2, 0.f);

    if (with_scalar != 0)
    {
        one_blob_only = true;
        support_inplace = true;
    }

    return 0;
}

// broadcasting rule
// https://github.com/Tencent/ncnn/wiki/binaryop-broadcasting

template<typename Op>
static int binary_op_scalar(const Mat& a, float b, Mat& c, const Option& opt)
{
    Op op;

    int w = a.w;
    int h = a.h;
    int d = a.d;
    int channels = a.c;
    int size = w * h * d;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int q = 0; q < channels; q++)
    {
        const float* ptr = a.channel(q);
        float* outptr = c.channel(q);

        for (int i = 0; i < size; i++)
        {
            outptr[i] = op(ptr[i], b);
        }
    }

    return 0;
}

template<typename Op>
static int binary_op_no_broadcast(const Mat& a, const Mat& b, Mat& c, const Option& opt)
{
    Op op;

    int channels = a.c;
    int size = a.w * a.h * a.d;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int q = 0; q < channels; q++)
    {
        const float* ptr = a.channel(q);
        const float* ptr1 = b.channel(q);
        float* outptr = c.channel(q);

        for (int i = 0; i < size; i++)
        {
            outptr[i] = op(ptr[i], ptr1[i]);
        }
    }

    return 0;
}

template<typename Op>
static int binary_op_broadcast_inner(const Mat& a, const Mat& b, Mat& c, const Option& opt)
{
    Op op;

    int w = a.w;
    int h = a.h;
    int d = a.d;
    int channels = a.c;
    int size = w * h * d;
    size_t elemsize = a.elemsize;

    int w1 = b.w;
    int h1 = b.h;
    int d1 = b.d;
    int channels1 = b.c;
    int size1 = w1 * h1 * d1;

    if (a.dims == 2 && b.dims == 1)
    {
        // type 12
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int y = 0; y < h; y++)
        {
            const float* ptr = a.row(y);
            const float b0 = b[y];
            float* outptr = c.row(y);

            for (int x = 0; x < w; x++)
            {
                outptr[x] = op(ptr[x], b0);
            }
        }
    }

    if ((a.dims == 3 || a.dims == 4) && b.dims == 1)
    {
        // type 9 11
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            const float* ptr = a.channel(q);
            const float b0 = b[q];
            float* outptr = c.channel(q);

            for (int i = 0; i < size; i++)
            {
                outptr[i] = op(ptr[i], b0);
            }
        }
    }

    if (a.dims == 3 && b.dims == 2)
    {
        // type 10
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            const float* ptr = a.channel(q);
            const float* ptr1 = b.row(q);
            float* outptr = c.channel(q);

            for (int y = 0; y < h; y++)
            {
                const float b0 = ptr1[y];
                for (int x = 0; x < w; x++)
                {
                    outptr[x] = op(ptr[x], b0);
                }

                ptr += w;
                outptr += w;
            }
        }
    }

    if (a.dims == 4 && b.dims == 2)
    {
        // type 12
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            const float* ptr = a.channel(q);
            const float* ptr1 = b.row(q);
            float* outptr = c.channel(q);

            for (int z = 0; z < d; z++)
            {
                const float b0 = ptr1[z];
                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        outptr[x] = op(ptr[x], b0);
                    }

                    ptr += w;
                    outptr += w;
                }
            }
        }
    }

    if (a.dims == 4 && b.dims == 3)
    {
        // type 13
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            const float* ptr = a.channel(q);
            const float* ptr1 = b.channel(q);
            float* outptr = c.channel(q);

            for (int z = 0; z < d; z++)
            {
                for (int y = 0; y < h; y++)
                {
                    const float b0 = ptr1[y];
                    for (int x = 0; x < w; x++)
                    {
                        outptr[x] = op(ptr[x], b0);
                    }

                    ptr += w;
                    outptr += w;
                }

                ptr1 += h;
            }
        }
    }

    return 0;
}

template<typename Op>
static int binary_op_broadcast_outer(const Mat& a, const Mat& b, Mat& c, const Option& opt)
{
    Op op;

    int w = a.w;
    int h = a.h;
    int d = a.d;
    int channels = a.c;

    if (a.dims == 2)
    {
        // type 14
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int y = 0; y < h; y++)
        {
            const float* ptr = a.row(y);
            const float* ptr1 = b;
            float* outptr = c.row(y);

            for (int x = 0; x < w; x++)
            {
                outptr[x] = op(ptr[x], ptr1[x]);
            }
        }
    }

    if (a.dims == 3 || a.dims == 4)
    {
        // type 15 16 17 18 19
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            const float* ptr = a.channel(q);
            float* outptr = c.channel(q);

            for (int z = 0; z < d; z++)
            {
                int z1 = std::min(z, b.d - 1);
                for (int y = 0; y < h; y++)
                {
                    int y1 = std::min(y, b.h - 1);

                    const float* ptr1 = b.depth(z1).row(y1);
                    for (int x = 0; x < w; x++)
                    {
                        outptr[x] = op(ptr[x], ptr1[x]);
                    }

                    ptr += w;
                    outptr += w;
                }
            }
        }
    }

    return 0;
}

template<typename Op>
static int binary_op_broadcast_20(const Mat& a, const Mat& b, Mat& c, const Option& opt)
{
    Op op;

    int w = a.w;
    int h = a.h;
    int d = a.d;
    int channels = a.c;

    int w1 = b.w;
    int h1 = b.h;
    int d1 = b.d;
    int channels1 = b.c;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int q = 0; q < channels1; q++)
    {
        const float* ptr = a.channel(q);
        const float* ptr1 = b.channel(q);
        float* outptr = c.channel(q);

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                outptr[x] = op(ptr[x], ptr1[x]);
            }

            ptr += w;
            outptr += w;
        }
    }

    return 0;
}

template<typename Op>
static int binary_op_scalar_inplace(Mat& a, float b, const Option& opt)
{
    Op op;

    int w = a.w;
    int h = a.h;
    int d = a.d;
    int channels = a.c;
    int size = w * h * d;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int q = 0; q < channels; q++)
    {
        float* ptr = a.channel(q);

        for (int i = 0; i < size; i++)
        {
            ptr[i] = op(ptr[i], b);
        }
    }

    return 0;
}

struct binary_op_add
{
    float operator()(const float& x, const float& y) const
    {
        return x + y;
    }
};

struct binary_op_sub
{
    float operator()(const float& x, const float& y) const
    {
        return x - y;
    }
};

struct binary_op_mul
{
    float operator()(const float& x, const float& y) const
    {
        return x * y;
    }
};

struct binary_op_div
{
    float operator()(const float& x, const float& y) const
    {
        return x / y;
    }
};

struct binary_op_max
{
    float operator()(const float& x, const float& y) const
    {
        return std::max(x, y);
    }
};

struct binary_op_min
{
    float operator()(const float& x, const float& y) const
    {
        return std::min(x, y);
    }
};

struct binary_op_pow
{
    float operator()(const float& x, const float& y) const
    {
        return (float)pow(x, y);
    }
};

struct binary_op_rsub
{
    float operator()(const float& x, const float& y) const
    {
        return y - x;
    }
};

struct binary_op_rdiv
{
    float operator()(const float& x, const float& y) const
    {
        return y / x;
    }
};

struct binary_op_rpow
{
    float operator()(const float& x, const float& y) const
    {
        return (float)pow(y, x);
    }
};

static int binary_op_scalar(const Mat& a, float b, Mat& c, int op_type, const Option& opt)
{
    if (op_type == BinaryOp::Operation_ADD) return binary_op_scalar<binary_op_add>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_SUB) return binary_op_scalar<binary_op_sub>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MUL) return binary_op_scalar<binary_op_mul>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_DIV) return binary_op_scalar<binary_op_div>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MAX) return binary_op_scalar<binary_op_max>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MIN) return binary_op_scalar<binary_op_min>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_POW) return binary_op_scalar<binary_op_pow>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RSUB) return binary_op_scalar<binary_op_rsub>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RDIV) return binary_op_scalar<binary_op_rdiv>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RPOW) return binary_op_scalar<binary_op_rpow>(a, b, c, opt);

    // should never reach here
    return 0;
}

static int binary_op_no_broadcast(const Mat& a, const Mat& b, Mat& c, int op_type, const Option& opt)
{
    if (op_type == BinaryOp::Operation_ADD) return binary_op_no_broadcast<binary_op_add>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_SUB) return binary_op_no_broadcast<binary_op_sub>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MUL) return binary_op_no_broadcast<binary_op_mul>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_DIV) return binary_op_no_broadcast<binary_op_div>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MAX) return binary_op_no_broadcast<binary_op_max>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MIN) return binary_op_no_broadcast<binary_op_min>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_POW) return binary_op_no_broadcast<binary_op_pow>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RSUB) return binary_op_no_broadcast<binary_op_rsub>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RDIV) return binary_op_no_broadcast<binary_op_rdiv>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RPOW) return binary_op_no_broadcast<binary_op_rpow>(a, b, c, opt);

    // should never reach here
    return 0;
}

static int binary_op_broadcast_inner(const Mat& a, const Mat& b, Mat& c, int op_type, const Option& opt)
{
    // squeeze inner axes
    Mat b2 = b;
    if (b.dims == 2) b2 = b.reshape(b.h);
    if (b.dims == 3 && b.h == 1) b2 = b.reshape(b.c);
    if (b.dims == 3 && b.w == 1) b2 = b.reshape(b.h, b.c);
    if (b.dims == 4 && b.d == 1) b2 = b.reshape(b.c);
    if (b.dims == 4 && b.h == 1) b2 = b.reshape(b.d, b.c);
    if (b.dims == 4 && b.w == 1) b2 = b.reshape(b.h, b.d, b.c);

    if (op_type == BinaryOp::Operation_ADD) return binary_op_broadcast_inner<binary_op_add>(a, b2, c, opt);
    if (op_type == BinaryOp::Operation_SUB) return binary_op_broadcast_inner<binary_op_sub>(a, b2, c, opt);
    if (op_type == BinaryOp::Operation_MUL) return binary_op_broadcast_inner<binary_op_mul>(a, b2, c, opt);
    if (op_type == BinaryOp::Operation_DIV) return binary_op_broadcast_inner<binary_op_div>(a, b2, c, opt);
    if (op_type == BinaryOp::Operation_MAX) return binary_op_broadcast_inner<binary_op_max>(a, b2, c, opt);
    if (op_type == BinaryOp::Operation_MIN) return binary_op_broadcast_inner<binary_op_min>(a, b2, c, opt);
    if (op_type == BinaryOp::Operation_POW) return binary_op_broadcast_inner<binary_op_pow>(a, b2, c, opt);
    if (op_type == BinaryOp::Operation_RSUB) return binary_op_broadcast_inner<binary_op_rsub>(a, b2, c, opt);
    if (op_type == BinaryOp::Operation_RDIV) return binary_op_broadcast_inner<binary_op_rdiv>(a, b2, c, opt);
    if (op_type == BinaryOp::Operation_RPOW) return binary_op_broadcast_inner<binary_op_rpow>(a, b2, c, opt);

    // should never reach here
    return 0;
}

static int binary_op_broadcast_outer(const Mat& a, const Mat& b, Mat& c, int op_type, const Option& opt)
{
    if (op_type == BinaryOp::Operation_ADD) return binary_op_broadcast_outer<binary_op_add>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_SUB) return binary_op_broadcast_outer<binary_op_sub>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MUL) return binary_op_broadcast_outer<binary_op_mul>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_DIV) return binary_op_broadcast_outer<binary_op_div>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MAX) return binary_op_broadcast_outer<binary_op_max>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MIN) return binary_op_broadcast_outer<binary_op_min>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_POW) return binary_op_broadcast_outer<binary_op_pow>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RSUB) return binary_op_broadcast_outer<binary_op_rsub>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RDIV) return binary_op_broadcast_outer<binary_op_rdiv>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RPOW) return binary_op_broadcast_outer<binary_op_rpow>(a, b, c, opt);

    // should never reach here
    return 0;
}

static int binary_op_broadcast_20(const Mat& a, const Mat& b, Mat& c, int op_type, const Option& opt)
{
    if (op_type == BinaryOp::Operation_ADD) return binary_op_broadcast_20<binary_op_add>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_SUB) return binary_op_broadcast_20<binary_op_sub>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MUL) return binary_op_broadcast_20<binary_op_mul>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_DIV) return binary_op_broadcast_20<binary_op_div>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MAX) return binary_op_broadcast_20<binary_op_max>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_MIN) return binary_op_broadcast_20<binary_op_min>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_POW) return binary_op_broadcast_20<binary_op_pow>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RSUB) return binary_op_broadcast_20<binary_op_rsub>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RDIV) return binary_op_broadcast_20<binary_op_rdiv>(a, b, c, opt);
    if (op_type == BinaryOp::Operation_RPOW) return binary_op_broadcast_20<binary_op_rpow>(a, b, c, opt);

    // should never reach here
    return 0;
}

static int get_reverse_op_type(int op_type)
{
    if (op_type == BinaryOp::Operation_SUB) return BinaryOp::Operation_RSUB;
    if (op_type == BinaryOp::Operation_DIV) return BinaryOp::Operation_RDIV;
    if (op_type == BinaryOp::Operation_POW) return BinaryOp::Operation_RPOW;
    if (op_type == BinaryOp::Operation_RSUB) return BinaryOp::Operation_SUB;
    if (op_type == BinaryOp::Operation_RDIV) return BinaryOp::Operation_DIV;
    if (op_type == BinaryOp::Operation_RPOW) return BinaryOp::Operation_POW;
    return op_type;
}

int BinaryOp::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    const bool b_rank_is_lower = bottom_blobs[1].dims < bottom_blobs[0].dims;
    const bool b_size_is_lower = bottom_blobs[1].w * bottom_blobs[1].h * bottom_blobs[1].d * bottom_blobs[1].c < bottom_blobs[0].w * bottom_blobs[0].h * bottom_blobs[0].d * bottom_blobs[0].c;
    const bool b_is_lower = b_rank_is_lower || b_size_is_lower;
    const Mat& a = b_is_lower ? bottom_blobs[0] : bottom_blobs[1];
    const Mat& b = b_is_lower ? bottom_blobs[1] : bottom_blobs[0];
    const int op_type_r = b_is_lower ? op_type : get_reverse_op_type(op_type);

    Mat& top_blob = top_blobs[0];
    top_blob.create_like(a, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    NCNN_LOGE("BinaryOp %d %d     %d %d %d %d      %d %d %d %d", a.dims, b.dims, a.w, a.h, a.d, a.c, b.w, b.h, b.d, b.c);

    // b is a scalar
    if (b.w * b.h * b.d * b.c == 1)
    {
        return binary_op_scalar(a, b[0], top_blob, op_type_r, opt);
    }

    // no broadcast
    if (a.dims == b.dims && a.w == b.w && a.h == b.h && a.d == b.d && a.c == b.c)
    {
        return binary_op_no_broadcast(a, b, top_blob, op_type_r, opt);
    }

    // broadcast b for inner axis
    if ((b.dims < a.dims)
        || (a.dims == 2 && b.w == 1 && b.h == a.h)
        || (a.dims == 3 && b.w == 1 && b.h == 1 && b.c == a.c)
        || (a.dims == 3 && b.w == 1 && b.h == a.h && b.c == a.c)
        || (a.dims == 4 && b.w == 1 && b.h == 1 && b.d == 1 && b.c == a.c)
        || (a.dims == 4 && b.w == 1 && b.h == 1 && b.d == a.d && b.c == a.c)
        || (a.dims == 4 && b.w == 1 && b.h == a.h && b.d == a.d && b.c == a.c))
    {
        return binary_op_broadcast_inner(a, b, top_blob, op_type_r, opt);
    }

    // broadcast b for outer axis
    if ((a.dims == 2 && b.w == a.w && b.h == 1)
        || (a.dims == 3 && b.w == a.w && b.h == 1 && b.c == 1)
        || (a.dims == 3 && b.w == a.w && b.h == a.h && b.c == 1)
        || (a.dims == 4 && b.w == a.w && b.h == 1 && b.d == 1 && b.c == 1)
        || (a.dims == 4 && b.w == a.w && b.h == a.h && b.d == 1 && b.c == 1)
        || (a.dims == 4 && b.w == a.w && b.h == a.h && b.d == a.d && b.c == 1))
    {
        return binary_op_broadcast_outer(a, b, top_blob, op_type_r, opt);
    }

    // some special broadcast rule here
    if (a.dims == 3 && b.dims == 3 && a.w == b.w && b.h == 1 && a.c == b.c)
    {
        return binary_op_broadcast_20(a, b, top_blob, op_type_r, opt);
    }

    return 0;
}

int BinaryOp::forward_inplace(Mat& bottom_top_blob, const Option& opt) const
{
    if (op_type == Operation_ADD) return binary_op_scalar_inplace<binary_op_add>(bottom_top_blob, b, opt);
    if (op_type == Operation_SUB) return binary_op_scalar_inplace<binary_op_sub>(bottom_top_blob, b, opt);
    if (op_type == Operation_MUL) return binary_op_scalar_inplace<binary_op_mul>(bottom_top_blob, b, opt);
    if (op_type == Operation_DIV) return binary_op_scalar_inplace<binary_op_div>(bottom_top_blob, b, opt);
    if (op_type == Operation_MAX) return binary_op_scalar_inplace<binary_op_max>(bottom_top_blob, b, opt);
    if (op_type == Operation_MIN) return binary_op_scalar_inplace<binary_op_min>(bottom_top_blob, b, opt);
    if (op_type == Operation_POW) return binary_op_scalar_inplace<binary_op_pow>(bottom_top_blob, b, opt);
    if (op_type == Operation_RSUB) return binary_op_scalar_inplace<binary_op_rsub>(bottom_top_blob, b, opt);
    if (op_type == Operation_RDIV) return binary_op_scalar_inplace<binary_op_rdiv>(bottom_top_blob, b, opt);
    if (op_type == Operation_RPOW) return binary_op_scalar_inplace<binary_op_rpow>(bottom_top_blob, b, opt);

    // should nerver reach here
    return 0;
}

} // namespace ncnn
