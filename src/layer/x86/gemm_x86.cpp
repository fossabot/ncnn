// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "gemm_x86.h"

#if __SSE2__
#include <emmintrin.h>
#if __AVX__
#include <immintrin.h>
#endif // __AVX__
#endif // __SSE2__
#include "x86_usability.h"

namespace ncnn {

Gemm_x86::Gemm_x86()
{
#if __SSE2__
    support_packing = true;
#endif // __SSE2__
}

static int gemm_x86(const Mat& A, const Mat& B, const Mat& C, Mat& top_blob, int transA, int transB, float alpha, float beta, const Option& opt)
{
    const int M = transA ? A.w : A.h * A.elempack;
    const int K = transA ? A.h * A.elempack : A.w;
    const int N = transB ? B.h * B.elempack : B.w;

    // NCNN_LOGE("M/N/K = %d %d %d", M, N, K);

    // TODO do not hardcode
    int TILE_M = 16 * 16; // 256
    int TILE_N = 12 * 20; // 240
    int TILE_K = 256;

    TILE_M = std::min(TILE_M, (M / ((M + TILE_M - 1) / TILE_M) + 15) / 16 * 16);
    TILE_N = std::min(TILE_N, (N / ((N + TILE_N - 1) / TILE_N) + 11) / 12 * 12);
    TILE_K = std::min(TILE_K, K / ((K + TILE_K - 1) / TILE_K));

    // NCNN_LOGE("TILE M/N/K = %d %d %d", TILE_M, TILE_N, TILE_K);

    int out_elempack = 1;
#if __SSE2__
    if (opt.use_packing_layout)
    {
#if __AVX512F__
        out_elempack = M % 16 == 0 ? 16 : M % 8 == 0 ? 8 : M % 4 == 0 ? 4 : 1;
#elif __AVX__
        out_elempack = M % 8 == 0 ? 8 : M % 4 == 0 ? 4 : 1;
#else
        out_elempack = M % 4 == 0 ? 4 : 1;
#endif
    }
#endif // __SSE2__
    size_t out_elemsize = 4u * out_elempack;

    top_blob.create(N, M / out_elempack, out_elemsize, out_elempack, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    const float* pC = C;

    int broadcast_type_C = 0;
    if (pC)
    {
        if (C.dims == 1 && C.w == 1)
        {
            // scalar
            broadcast_type_C = 0;
        }
        if (C.dims == 1 && C.w * C.elempack == M)
        {
            // M
            // auto broadcast from h to w is the ncnn-style convention
            broadcast_type_C = 1;
        }
        if (C.dims == 1 && C.w * C.elempack == N)
        {
            // N
            broadcast_type_C = 4;
        }
        if (C.dims == 2 && C.w == 1 && C.h * C.elempack == M)
        {
            // Mx1
            broadcast_type_C = 2;
        }
        if (C.dims == 2 && C.w == N && C.h * C.elempack == M)
        {
            // MxN
            broadcast_type_C = 3;
        }
        if (C.dims == 2 && C.w == N && C.h * C.elempack == 1)
        {
            // 1xN
            broadcast_type_C = 4;
        }
    }

    Mat AT(TILE_K * TILE_M, (K + TILE_K - 1) / TILE_K, 4u, opt.blob_allocator);
    Mat BT(TILE_K * TILE_N, (K + TILE_K - 1) / TILE_K, (N + TILE_N - 1) / TILE_N, 4u, opt.blob_allocator);

    Mat tmp;
    if (K > TILE_K)
        tmp.create(TILE_N, TILE_M, 4u, opt.blob_allocator);

    int i = 0;
    for (; i < M; i += TILE_M)
    {
        int j = 0;
        for (; j < N; j += TILE_N)
        {
            int k = 0;
            for (; k < K; k += TILE_K)
            {
                const int max_ii = std::min((M - i), TILE_M);
                const int max_jj = std::min((N - j), TILE_N);
                const int max_kk = std::min((K - k), TILE_K);

                // NCNN_LOGE("max_ii/jj/kk = %d %d %d", max_ii, max_jj, max_kk);

                float* ptmp = tmp;

                if (j == 0)
                {
                    // pack A
                    if (transA)
                    {
                        // transpose_pack_a(A, AT, TILE_M, TILE_K, opt);
                        const int elempack = A.elempack;

                        float* pp = AT.row(k / TILE_K);

                        int ii = 0;
#if __SSE2__
#if __AVX__
#if __AVX512F__
                        for (; ii + 15 < max_ii; ii += 16)
                        {
                            for (int kk = 0; kk < max_kk; kk++)
                            {
                                const float* p0 = A.row((k + kk) / elempack) + (i + ii) * elempack + (k + kk) % elempack;

                                pp[0] = p0[0];
                                pp[1] = p0[1 * elempack];
                                pp[2] = p0[2 * elempack];
                                pp[3] = p0[3 * elempack];
                                pp[4] = p0[4 * elempack];
                                pp[5] = p0[5 * elempack];
                                pp[6] = p0[6 * elempack];
                                pp[7] = p0[7 * elempack];
                                pp[8] = p0[8 * elempack];
                                pp[9] = p0[9 * elempack];
                                pp[10] = p0[10 * elempack];
                                pp[11] = p0[11 * elempack];
                                pp[12] = p0[12 * elempack];
                                pp[13] = p0[13 * elempack];
                                pp[14] = p0[14 * elempack];
                                pp[15] = p0[15 * elempack];
                                pp += 16;
                            }
                        }
#endif // __AVX512F__
                        for (; ii + 7 < max_ii; ii += 8)
                        {
                            for (int kk = 0; kk < max_kk; kk++)
                            {
                                const float* p0 = A.row((k + kk) / elempack) + (i + ii) * elempack + (k + kk) % elempack;

                                pp[0] = p0[0];
                                pp[1] = p0[1 * elempack];
                                pp[2] = p0[2 * elempack];
                                pp[3] = p0[3 * elempack];
                                pp[4] = p0[4 * elempack];
                                pp[5] = p0[5 * elempack];
                                pp[6] = p0[6 * elempack];
                                pp[7] = p0[7 * elempack];
                                pp += 8;
                            }
                        }
#endif // __AVX__
                        for (; ii + 3 < max_ii; ii += 4)
                        {
                            for (int kk = 0; kk < max_kk; kk++)
                            {
                                const float* p0 = A.row((k + kk) / elempack) + (i + ii) * elempack + (k + kk) % elempack;

                                pp[0] = p0[0];
                                pp[1] = p0[1 * elempack];
                                pp[2] = p0[2 * elempack];
                                pp[3] = p0[3 * elempack];
                                pp += 4;
                            }
                        }
#endif // __SSE2__
                        for (; ii + 1 < max_ii; ii += 2)
                        {
                            for (int kk = 0; kk < max_kk; kk++)
                            {
                                const float* p0 = A.row((k + kk) / elempack) + (i + ii) * elempack + (k + kk) % elempack;

                                pp[0] = p0[0];
                                pp[1] = p0[1 * elempack];
                                pp += 2;
                            }
                        }
                        for (; ii < max_ii; ii += 1)
                        {
                            for (int kk = 0; kk < max_kk; kk++)
                            {
                                const float* p0 = A.row((k + kk) / elempack) + (i + ii) * elempack + (k + kk) % elempack;

                                pp[0] = p0[0];
                                pp += 1;
                            }
                        }
                    }
                    else
                    {
                        // pack_a(A, AT, TILE_M, TILE_K, opt);
                        const int elempack = A.elempack;

                        float* pp = AT.row(k / TILE_K);

                        int ii = 0;
#if __SSE2__
#if __AVX__
#if __AVX512F__
                        for (; ii + 15 < max_ii; ii += 16)
                        {
                            if (elempack == 16)
                            {
                                const float* p0 = A.row((i + ii) / 16 + 0) + k * 16;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p0[1];
                                    pp[2] = p0[2];
                                    pp[3] = p0[3];
                                    pp[4] = p0[4];
                                    pp[5] = p0[5];
                                    pp[6] = p0[6];
                                    pp[7] = p0[7];
                                    pp[8] = p0[8];
                                    pp[9] = p0[9];
                                    pp[10] = p0[10];
                                    pp[11] = p0[11];
                                    pp[12] = p0[12];
                                    pp[13] = p0[13];
                                    pp[14] = p0[14];
                                    pp[15] = p0[15];
                                    pp += 16;
                                    p0 += 16;
                                }
                            }
                            if (elempack == 8)
                            {
                                const float* p0 = A.row((i + ii) / 8 + 0) + k * 8;
                                const float* p1 = A.row((i + ii) / 8 + 1) + k * 8;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p0[1];
                                    pp[2] = p0[2];
                                    pp[3] = p0[3];
                                    pp[4] = p0[4];
                                    pp[5] = p0[5];
                                    pp[6] = p0[6];
                                    pp[7] = p0[7];
                                    pp[8] = p1[0];
                                    pp[9] = p1[1];
                                    pp[10] = p1[2];
                                    pp[11] = p1[3];
                                    pp[12] = p1[4];
                                    pp[13] = p1[5];
                                    pp[14] = p1[6];
                                    pp[15] = p1[7];
                                    pp += 16;
                                    p0 += 8;
                                    p1 += 8;
                                }
                            }
                            if (elempack == 4)
                            {
                                const float* p0 = A.row((i + ii) / 4 + 0) + k * 4;
                                const float* p1 = A.row((i + ii) / 4 + 1) + k * 4;
                                const float* p2 = A.row((i + ii) / 4 + 2) + k * 4;
                                const float* p3 = A.row((i + ii) / 4 + 3) + k * 4;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p0[1];
                                    pp[2] = p0[2];
                                    pp[3] = p0[3];
                                    pp[4] = p1[0];
                                    pp[5] = p1[1];
                                    pp[6] = p1[2];
                                    pp[7] = p1[3];
                                    pp[8] = p2[0];
                                    pp[9] = p2[1];
                                    pp[10] = p2[2];
                                    pp[11] = p2[3];
                                    pp[12] = p3[0];
                                    pp[13] = p3[1];
                                    pp[14] = p3[2];
                                    pp[15] = p3[3];
                                    pp += 16;
                                    p0 += 4;
                                    p1 += 4;
                                    p2 += 4;
                                    p3 += 4;
                                }
                            }
                            if (elempack == 1)
                            {
                                const float* p0 = A.row(i + ii + 0) + k;
                                const float* p1 = A.row(i + ii + 1) + k;
                                const float* p2 = A.row(i + ii + 2) + k;
                                const float* p3 = A.row(i + ii + 3) + k;
                                const float* p4 = A.row(i + ii + 4) + k;
                                const float* p5 = A.row(i + ii + 5) + k;
                                const float* p6 = A.row(i + ii + 6) + k;
                                const float* p7 = A.row(i + ii + 7) + k;
                                const float* p8 = A.row(i + ii + 8) + k;
                                const float* p9 = A.row(i + ii + 9) + k;
                                const float* pa = A.row(i + ii + 10) + k;
                                const float* pb = A.row(i + ii + 11) + k;
                                const float* pc = A.row(i + ii + 12) + k;
                                const float* pd = A.row(i + ii + 13) + k;
                                const float* pe = A.row(i + ii + 14) + k;
                                const float* pf = A.row(i + ii + 15) + k;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p1[0];
                                    pp[2] = p2[0];
                                    pp[3] = p3[0];
                                    pp[4] = p4[0];
                                    pp[5] = p5[0];
                                    pp[6] = p6[0];
                                    pp[7] = p7[0];
                                    pp[8] = p8[0];
                                    pp[9] = p9[0];
                                    pp[10] = pa[0];
                                    pp[11] = pb[0];
                                    pp[12] = pc[0];
                                    pp[13] = pd[0];
                                    pp[14] = pe[0];
                                    pp[15] = pf[0];
                                    pp += 16;
                                    p0++;
                                    p1++;
                                    p2++;
                                    p3++;
                                    p4++;
                                    p5++;
                                    p6++;
                                    p7++;
                                    p8++;
                                    p9++;
                                    pa++;
                                    pb++;
                                    pc++;
                                    pd++;
                                    pe++;
                                    pf++;
                                }
                            }
                        }
#endif // __AVX512F__
                        for (; ii + 7 < max_ii; ii += 8)
                        {
                            if (elempack == 8)
                            {
                                const float* p0 = A.row((i + ii) / 8 + 0) + k * 8;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    _mm256_store_ps(pp, _mm256_load_ps(p0));
                                    // pp[0] = p0[0];
                                    // pp[1] = p0[1];
                                    // pp[2] = p0[2];
                                    // pp[3] = p0[3];
                                    // pp[4] = p0[4];
                                    // pp[5] = p0[5];
                                    // pp[6] = p0[6];
                                    // pp[7] = p0[7];
                                    pp += 8;
                                    p0 += 8;
                                }
                            }
                            if (elempack == 4)
                            {
                                const float* p0 = A.row((i + ii) / 4 + 0) + k * 4;
                                const float* p1 = A.row((i + ii) / 4 + 1) + k * 4;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p0[1];
                                    pp[2] = p0[2];
                                    pp[3] = p0[3];
                                    pp[4] = p1[0];
                                    pp[5] = p1[1];
                                    pp[6] = p1[2];
                                    pp[7] = p1[3];
                                    pp += 8;
                                    p0 += 4;
                                    p1 += 4;
                                }
                            }
                            if (elempack == 1)
                            {
                                const float* p0 = A.row(i + ii + 0) + k;
                                const float* p1 = A.row(i + ii + 1) + k;
                                const float* p2 = A.row(i + ii + 2) + k;
                                const float* p3 = A.row(i + ii + 3) + k;
                                const float* p4 = A.row(i + ii + 4) + k;
                                const float* p5 = A.row(i + ii + 5) + k;
                                const float* p6 = A.row(i + ii + 6) + k;
                                const float* p7 = A.row(i + ii + 7) + k;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p1[0];
                                    pp[2] = p2[0];
                                    pp[3] = p3[0];
                                    pp[4] = p4[0];
                                    pp[5] = p5[0];
                                    pp[6] = p6[0];
                                    pp[7] = p7[0];
                                    pp += 8;
                                    p0++;
                                    p1++;
                                    p2++;
                                    p3++;
                                    p4++;
                                    p5++;
                                    p6++;
                                    p7++;
                                }
                            }
                        }
#endif // __AVX__
                        for (; ii + 3 < max_ii; ii += 4)
                        {
                            if (elempack == 4)
                            {
                                const float* p0 = A.row((i + ii) / 4 + 0) + k * 4;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p0[1];
                                    pp[2] = p0[2];
                                    pp[3] = p0[3];
                                    pp += 4;
                                    p0 += 4;
                                }
                            }
                            if (elempack == 1)
                            {
                                const float* p0 = A.row(i + ii + 0) + k;
                                const float* p1 = A.row(i + ii + 1) + k;
                                const float* p2 = A.row(i + ii + 2) + k;
                                const float* p3 = A.row(i + ii + 3) + k;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p1[0];
                                    pp[2] = p2[0];
                                    pp[3] = p3[0];
                                    pp += 4;
                                    p0++;
                                    p1++;
                                    p2++;
                                    p3++;
                                }
                            }
                        }
#endif // __SSE2__
                        for (; ii + 1 < max_ii; ii += 2)
                        {
                            // if (elempack == 1)
                            {
                                const float* p0 = A.row(i + ii + 0) + k;
                                const float* p1 = A.row(i + ii + 1) + k;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p1[0];
                                    pp += 2;
                                    p0++;
                                    p1++;
                                }
                            }
                        }
                        for (; ii < max_ii; ii += 1)
                        {
                            // if (elempack == 1)
                            {
                                const float* p0 = A.row(i + ii + 0) + k;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp += 1;
                                    p0++;
                                }
                            }
                        }
                    }
                }

                if (i == 0)
                {
                    // pack B
                    if (transB)
                    {
                        // pack_b(B, BT, TILE_N, TILE_K, opt);
                        const int elempack = B.elempack;

                        float* pp = BT.channel(j / TILE_N).row(k / TILE_K);

                        int jj = 0;
#if __SSE2__
                        for (; jj + 11 < max_jj; jj += 12)
                        {
#if __AVX__
#if __AVX512F__
                            if (elempack == 16)
                            {
                                const float* p0 = B.row((j + jj) / 16 + 0) + k * 16;
                                const float* p1 = B.row((j + jj) / 16 + 1) + k * 16;

                                if ((j + jj) % 16 == 0)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[0];
                                        pp[1] = p0[1];
                                        pp[2] = p0[2];
                                        pp[3] = p0[3];
                                        pp[4] = p0[4];
                                        pp[5] = p0[5];
                                        pp[6] = p0[6];
                                        pp[7] = p0[7];
                                        pp[8] = p0[8];
                                        pp[9] = p0[9];
                                        pp[10] = p0[10];
                                        pp[11] = p0[11];
                                        pp += 12;
                                        p0 += 16;
                                    }
                                }
                                if ((j + jj) % 16 == 4)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[4];
                                        pp[1] = p0[5];
                                        pp[2] = p0[6];
                                        pp[3] = p0[7];
                                        pp[4] = p0[8];
                                        pp[5] = p0[9];
                                        pp[6] = p0[10];
                                        pp[7] = p0[11];
                                        pp[8] = p0[12];
                                        pp[9] = p0[13];
                                        pp[10] = p0[14];
                                        pp[11] = p0[15];
                                        pp += 12;
                                        p0 += 16;
                                    }
                                }
                                if ((j + jj) % 16 == 8)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[8];
                                        pp[1] = p0[9];
                                        pp[2] = p0[10];
                                        pp[3] = p0[11];
                                        pp[4] = p0[12];
                                        pp[5] = p0[13];
                                        pp[6] = p0[14];
                                        pp[7] = p0[15];
                                        pp[8] = p1[0];
                                        pp[9] = p1[1];
                                        pp[10] = p1[2];
                                        pp[11] = p1[3];
                                        pp += 12;
                                        p0 += 16;
                                        p1 += 16;
                                    }
                                }
                                if ((j + jj) % 16 == 12)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[12];
                                        pp[1] = p0[13];
                                        pp[2] = p0[14];
                                        pp[3] = p0[15];
                                        pp[4] = p1[0];
                                        pp[5] = p1[1];
                                        pp[6] = p1[2];
                                        pp[7] = p1[3];
                                        pp[8] = p1[4];
                                        pp[9] = p1[5];
                                        pp[10] = p1[6];
                                        pp[11] = p1[7];
                                        pp += 12;
                                        p0 += 16;
                                        p1 += 16;
                                    }
                                }
                            }
#endif // __AVX512F__
                            if (elempack == 8)
                            {
                                const float* p0 = B.row((j + jj) / 8 + 0) + k * 8;
                                const float* p1 = B.row((j + jj) / 8 + 1) + k * 8;

                                if ((j + jj) % 8 == 0)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        _mm256_storeu_ps(pp, _mm256_loadu_ps(p0));
                                        _mm_storeu_ps(pp + 8, _mm_loadu_ps(p1));
                                        // pp[0] = p0[0];
                                        // pp[1] = p0[1];
                                        // pp[2] = p0[2];
                                        // pp[3] = p0[3];
                                        // pp[4] = p0[4];
                                        // pp[5] = p0[5];
                                        // pp[6] = p0[6];
                                        // pp[7] = p0[7];
                                        // pp[8] = p1[0];
                                        // pp[9] = p1[1];
                                        // pp[10] = p1[2];
                                        // pp[11] = p1[3];
                                        pp += 12;
                                        p0 += 8;
                                        p1 += 8;
                                    }
                                }
                                if ((j + jj) % 8 == 4)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        _mm_storeu_ps(pp, _mm_loadu_ps(p0 + 4));
                                        _mm256_storeu_ps(pp + 4, _mm256_loadu_ps(p1));
                                        // pp[0] = p0[4];
                                        // pp[1] = p0[5];
                                        // pp[2] = p0[6];
                                        // pp[3] = p0[7];
                                        // pp[4] = p1[0];
                                        // pp[5] = p1[1];
                                        // pp[6] = p1[2];
                                        // pp[7] = p1[3];
                                        // pp[8] = p1[4];
                                        // pp[9] = p1[5];
                                        // pp[10] = p1[6];
                                        // pp[11] = p1[7];
                                        pp += 12;
                                        p0 += 8;
                                        p1 += 8;
                                    }
                                }
                            }
#endif // __AVX__
                            if (elempack == 4)
                            {
                                const float* p0 = B.row((j + jj) / 4 + 0) + k * 4;
                                const float* p1 = B.row((j + jj) / 4 + 1) + k * 4;
                                const float* p2 = B.row((j + jj) / 4 + 2) + k * 4;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p0[1];
                                    pp[2] = p0[2];
                                    pp[3] = p0[3];
                                    pp[4] = p1[0];
                                    pp[5] = p1[1];
                                    pp[6] = p1[2];
                                    pp[7] = p1[3];
                                    pp[8] = p2[0];
                                    pp[9] = p2[1];
                                    pp[10] = p2[2];
                                    pp[11] = p2[3];
                                    pp += 12;
                                    p0 += 4;
                                    p1 += 4;
                                    p2 += 4;
                                }
                            }
                            if (elempack == 1)
                            {
                                const float* p0 = B.row(j + jj + 0) + k;
                                const float* p1 = B.row(j + jj + 1) + k;
                                const float* p2 = B.row(j + jj + 2) + k;
                                const float* p3 = B.row(j + jj + 3) + k;
                                const float* p4 = B.row(j + jj + 4) + k;
                                const float* p5 = B.row(j + jj + 5) + k;
                                const float* p6 = B.row(j + jj + 6) + k;
                                const float* p7 = B.row(j + jj + 7) + k;
                                const float* p8 = B.row(j + jj + 8) + k;
                                const float* p9 = B.row(j + jj + 9) + k;
                                const float* pa = B.row(j + jj + 10) + k;
                                const float* pb = B.row(j + jj + 11) + k;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p1[0];
                                    pp[2] = p2[0];
                                    pp[3] = p3[0];
                                    pp[4] = p4[0];
                                    pp[5] = p5[0];
                                    pp[6] = p6[0];
                                    pp[7] = p7[0];
                                    pp[8] = p8[0];
                                    pp[9] = p9[0];
                                    pp[10] = pa[0];
                                    pp[11] = pb[0];
                                    pp += 12;
                                    p0++;
                                    p1++;
                                    p2++;
                                    p3++;
                                    p4++;
                                    p5++;
                                    p6++;
                                    p7++;
                                    p8++;
                                    p9++;
                                    pa++;
                                    pb++;
                                }
                            }
                        }
                        for (; jj + 7 < max_jj; jj += 8)
                        {
#if __AVX__
#if __AVX512F__
                            if (elempack == 16)
                            {
                                const float* p0 = B.row((j + jj) / 16 + 0) + k * 16;
                                const float* p1 = B.row((j + jj) / 16 + 1) + k * 16;

                                if ((j + jj) % 16 == 0)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[0];
                                        pp[1] = p0[1];
                                        pp[2] = p0[2];
                                        pp[3] = p0[3];
                                        pp[4] = p0[4];
                                        pp[5] = p0[5];
                                        pp[6] = p0[6];
                                        pp[7] = p0[7];
                                        pp += 8;
                                        p0 += 16;
                                    }
                                }
                                if ((j + jj) % 16 == 4)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[4];
                                        pp[1] = p0[5];
                                        pp[2] = p0[6];
                                        pp[3] = p0[7];
                                        pp[4] = p0[8];
                                        pp[5] = p0[9];
                                        pp[6] = p0[10];
                                        pp[7] = p0[11];
                                        pp += 8;
                                        p0 += 16;
                                    }
                                }
                                if ((j + jj) % 16 == 8)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[8];
                                        pp[1] = p0[9];
                                        pp[2] = p0[10];
                                        pp[3] = p0[11];
                                        pp[4] = p0[12];
                                        pp[5] = p0[13];
                                        pp[6] = p0[14];
                                        pp[7] = p0[15];
                                        pp += 8;
                                        p0 += 16;
                                    }
                                }
                                if ((j + jj) % 16 == 12)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[12];
                                        pp[1] = p0[13];
                                        pp[2] = p0[14];
                                        pp[3] = p0[15];
                                        pp[4] = p1[0];
                                        pp[5] = p1[1];
                                        pp[6] = p1[2];
                                        pp[7] = p1[3];
                                        pp += 8;
                                        p0 += 16;
                                        p1 += 16;
                                    }
                                }
                            }
#endif // __AVX512F__
                            if (elempack == 8)
                            {
                                const float* p0 = B.row((j + jj) / 8 + 0) + k * 8;
                                const float* p1 = B.row((j + jj) / 8 + 1) + k * 8;

                                if ((j + jj) % 8 == 0)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        _mm256_storeu_ps(pp, _mm256_loadu_ps(p0));
                                        // pp[0] = p0[0];
                                        // pp[1] = p0[1];
                                        // pp[2] = p0[2];
                                        // pp[3] = p0[3];
                                        // pp[4] = p0[4];
                                        // pp[5] = p0[5];
                                        // pp[6] = p0[6];
                                        // pp[7] = p0[7];
                                        pp += 8;
                                        p0 += 8;
                                    }
                                }
                                if ((j + jj) % 8 == 4)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        _mm_storeu_ps(pp, _mm_loadu_ps(p0 + 4));
                                        _mm_storeu_ps(pp + 4, _mm_loadu_ps(p1));
                                        // pp[0] = p0[4];
                                        // pp[1] = p0[5];
                                        // pp[2] = p0[6];
                                        // pp[3] = p0[7];
                                        // pp[4] = p1[0];
                                        // pp[5] = p1[1];
                                        // pp[6] = p1[2];
                                        // pp[7] = p1[3];
                                        pp += 8;
                                        p0 += 8;
                                        p1 += 8;
                                    }
                                }
                            }
#endif // __AVX__
                            if (elempack == 4)
                            {
                                const float* p0 = B.row((j + jj) / 4 + 0) + k * 4;
                                const float* p1 = B.row((j + jj) / 4 + 1) + k * 4;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p0[1];
                                    pp[2] = p0[2];
                                    pp[3] = p0[3];
                                    pp[4] = p1[0];
                                    pp[5] = p1[1];
                                    pp[6] = p1[2];
                                    pp[7] = p1[3];
                                    pp += 8;
                                    p0 += 4;
                                    p1 += 4;
                                }
                            }
                            if (elempack == 1)
                            {
                                const float* p0 = B.row(j + jj + 0) + k;
                                const float* p1 = B.row(j + jj + 1) + k;
                                const float* p2 = B.row(j + jj + 2) + k;
                                const float* p3 = B.row(j + jj + 3) + k;
                                const float* p4 = B.row(j + jj + 4) + k;
                                const float* p5 = B.row(j + jj + 5) + k;
                                const float* p6 = B.row(j + jj + 6) + k;
                                const float* p7 = B.row(j + jj + 7) + k;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p1[0];
                                    pp[2] = p2[0];
                                    pp[3] = p3[0];
                                    pp[4] = p4[0];
                                    pp[5] = p5[0];
                                    pp[6] = p6[0];
                                    pp[7] = p7[0];
                                    pp += 8;
                                    p0++;
                                    p1++;
                                    p2++;
                                    p3++;
                                    p4++;
                                    p5++;
                                    p6++;
                                    p7++;
                                }
                            }
                        }
                        for (; jj + 3 < max_jj; jj += 4)
                        {
#if __AVX__
#if __AVX512F__
                            if (elempack == 16)
                            {
                                const float* p0 = B.row((j + jj) / 16 + 0) + k * 16;

                                if ((j + jj) % 16 == 0)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[0];
                                        pp[1] = p0[1];
                                        pp[2] = p0[2];
                                        pp[3] = p0[3];
                                        pp += 4;
                                        p0 += 16;
                                    }
                                }
                                if ((j + jj) % 16 == 4)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[4];
                                        pp[1] = p0[5];
                                        pp[2] = p0[6];
                                        pp[3] = p0[7];
                                        pp += 4;
                                        p0 += 16;
                                    }
                                }
                                if ((j + jj) % 16 == 8)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[8];
                                        pp[1] = p0[9];
                                        pp[2] = p0[10];
                                        pp[3] = p0[11];
                                        pp += 4;
                                        p0 += 16;
                                    }
                                }
                                if ((j + jj) % 16 == 12)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        pp[0] = p0[12];
                                        pp[1] = p0[13];
                                        pp[2] = p0[14];
                                        pp[3] = p0[15];
                                        pp += 4;
                                        p0 += 16;
                                    }
                                }
                            }
#endif // __AVX512F__
                            if (elempack == 8)
                            {
                                const float* p0 = B.row((j + jj) / 8 + 0) + k * 8;

                                if ((j + jj) % 8 == 0)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        _mm_storeu_ps(pp, _mm_loadu_ps(p0));
                                        // pp[0] = p0[0];
                                        // pp[1] = p0[1];
                                        // pp[2] = p0[2];
                                        // pp[3] = p0[3];
                                        pp += 4;
                                        p0 += 8;
                                    }
                                }
                                if ((j + jj) % 8 == 4)
                                {
                                    for (int kk = 0; kk < max_kk; kk++)
                                    {
                                        _mm_storeu_ps(pp, _mm_loadu_ps(p0 + 4));
                                        // pp[0] = p0[4];
                                        // pp[1] = p0[5];
                                        // pp[2] = p0[6];
                                        // pp[3] = p0[7];
                                        pp += 4;
                                        p0 += 8;
                                    }
                                }
                            }
#endif // __AVX__
                            if (elempack == 4)
                            {
                                const float* p0 = B.row((j + jj) / 4 + 0) + k * 4;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p0[1];
                                    pp[2] = p0[2];
                                    pp[3] = p0[3];
                                    pp += 4;
                                    p0 += 4;
                                }
                            }
                            if (elempack == 1)
                            {
                                const float* p0 = B.row(j + jj + 0) + k;
                                const float* p1 = B.row(j + jj + 1) + k;
                                const float* p2 = B.row(j + jj + 2) + k;
                                const float* p3 = B.row(j + jj + 3) + k;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p1[0];
                                    pp[2] = p2[0];
                                    pp[3] = p3[0];
                                    pp += 4;
                                    p0++;
                                    p1++;
                                    p2++;
                                    p3++;
                                }
                            }
                        }
#endif // __SSE2__
                        for (; jj + 1 < max_jj; jj += 2)
                        {
                            // if (elempack == 1)
                            {
                                const float* p0 = B.row(j + jj + 0) + k;
                                const float* p1 = B.row(j + jj + 1) + k;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp[1] = p1[0];
                                    pp += 2;
                                    p0++;
                                    p1++;
                                }
                            }
                        }
                        for (; jj < max_jj; jj += 1)
                        {
                            // if (elempack == 1)
                            {
                                const float* p0 = B.row(j + jj + 0) + k;

                                for (int kk = 0; kk < max_kk; kk++)
                                {
                                    pp[0] = p0[0];
                                    pp += 1;
                                    p0++;
                                }
                            }
                        }
                    }
                    else
                    {
                        // transpose_pack_b(B, BT, TILE_N, TILE_K, opt);
                        const int elempack = B.elempack;

                        float* pp = BT.channel(j / TILE_N).row(k / TILE_K);

                        int jj = 0;
#if __SSE2__
                        for (; jj + 11 < max_jj; jj += 12)
                        {
                            for (int kk = 0; kk < max_kk; kk++)
                            {
                                const float* p0 = B.row((k + kk) / elempack) + (j + jj) * elempack + (k + kk) % elempack;

                                pp[0] = p0[0];
                                pp[1] = p0[1 * elempack];
                                pp[2] = p0[2 * elempack];
                                pp[3] = p0[3 * elempack];
                                pp[4] = p0[4 * elempack];
                                pp[5] = p0[5 * elempack];
                                pp[6] = p0[6 * elempack];
                                pp[7] = p0[7 * elempack];
                                pp[8] = p0[8 * elempack];
                                pp[9] = p0[9 * elempack];
                                pp[10] = p0[10 * elempack];
                                pp[11] = p0[11 * elempack];
                                pp += 12;
                            }
                        }
                        for (; jj + 7 < max_jj; jj += 8)
                        {
                            for (int kk = 0; kk < max_kk; kk++)
                            {
                                const float* p0 = B.row((k + kk) / elempack) + (j + jj) * elempack + (k + kk) % elempack;

                                pp[0] = p0[0];
                                pp[1] = p0[1 * elempack];
                                pp[2] = p0[2 * elempack];
                                pp[3] = p0[3 * elempack];
                                pp[4] = p0[4 * elempack];
                                pp[5] = p0[5 * elempack];
                                pp[6] = p0[6 * elempack];
                                pp[7] = p0[7 * elempack];
                                pp += 8;
                            }
                        }
                        for (; jj + 3 < max_jj; jj += 4)
                        {
                            for (int kk = 0; kk < max_kk; kk++)
                            {
                                const float* p0 = B.row((k + kk) / elempack) + (j + jj) * elempack + (k + kk) % elempack;

                                pp[0] = p0[0];
                                pp[1] = p0[1 * elempack];
                                pp[2] = p0[2 * elempack];
                                pp[3] = p0[3 * elempack];
                                pp += 4;
                            }
                        }
#endif // __SSE2__
                        for (; jj + 1 < max_jj; jj += 2)
                        {
                            for (int kk = 0; kk < max_kk; kk++)
                            {
                                const float* p0 = B.row((k + kk) / elempack) + (j + jj) * elempack + (k + kk) % elempack;

                                pp[0] = p0[0];
                                pp[1] = p0[1 * elempack];
                                pp += 2;
                            }
                        }
                        for (; jj < max_jj; jj += 1)
                        {
                            for (int kk = 0; kk < max_kk; kk++)
                            {
                                const float* p0 = B.row((k + kk) / elempack) + (j + jj) * elempack + (k + kk) % elempack;

                                pp[0] = p0[0];
                                pp += 1;
                            }
                        }
                    }
                }

                const float* pA0 = AT.row(k / TILE_K);
                const float* pB0 = BT.channel(j / TILE_N).row(k / TILE_K);

                int ii = 0;
#if __SSE2__
#if __AVX__
#if __AVX512F__
                for (; ii + 15 < max_ii; ii += 16)
                {
                    float* outptr0 = top_blob.row((i + ii) / out_elempack) + j * out_elempack;

                    const float* pB = pB0;

                    int jj = 0;
                    for (; jj + 11 < max_jj; jj += 12)
                    {
                        __m512 _sum0;
                        __m512 _sum1;
                        __m512 _sum2;
                        __m512 _sum3;
                        __m512 _sum4;
                        __m512 _sum5;
                        __m512 _sum6;
                        __m512 _sum7;
                        __m512 _sum8;
                        __m512 _sum9;
                        __m512 _suma;
                        __m512 _sumb;

                        if (k == 0)
                        {
                            _sum0 = _mm512_setzero_ps();
                            _sum1 = _mm512_setzero_ps();
                            _sum2 = _mm512_setzero_ps();
                            _sum3 = _mm512_setzero_ps();
                            _sum4 = _mm512_setzero_ps();
                            _sum5 = _mm512_setzero_ps();
                            _sum6 = _mm512_setzero_ps();
                            _sum7 = _mm512_setzero_ps();
                            _sum8 = _mm512_setzero_ps();
                            _sum9 = _mm512_setzero_ps();
                            _suma = _mm512_setzero_ps();
                            _sumb = _mm512_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm512_set1_ps(pC[0]);
                                    _sum1 = _mm512_set1_ps(pC[0]);
                                    _sum2 = _mm512_set1_ps(pC[0]);
                                    _sum3 = _mm512_set1_ps(pC[0]);
                                    _sum4 = _mm512_set1_ps(pC[0]);
                                    _sum5 = _mm512_set1_ps(pC[0]);
                                    _sum6 = _mm512_set1_ps(pC[0]);
                                    _sum7 = _mm512_set1_ps(pC[0]);
                                    _sum8 = _mm512_set1_ps(pC[0]);
                                    _sum9 = _mm512_set1_ps(pC[0]);
                                    _suma = _mm512_set1_ps(pC[0]);
                                    _sumb = _mm512_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm512_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                    _sum2 = _sum0;
                                    _sum3 = _sum0;
                                    _sum4 = _sum0;
                                    _sum5 = _sum0;
                                    _sum6 = _sum0;
                                    _sum7 = _sum0;
                                    _sum8 = _sum0;
                                    _sum9 = _sum0;
                                    _suma = _sum0;
                                    _sumb = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 16)
                                    {
                                        _sum0 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16);
                                        _sum2 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 2);
                                        _sum3 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 3);
                                        _sum4 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 4);
                                        _sum5 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 5);
                                        _sum6 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 6);
                                        _sum7 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 7);
                                        _sum8 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 8);
                                        _sum9 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 9);
                                        _suma = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 10);
                                        _sumb = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 11);
                                    }
                                    if (out_elempack == 8)
                                    {
                                        __m256 _sum0_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m256 _sum1_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8);
                                        __m256 _sum2_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 2);
                                        __m256 _sum3_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 3);
                                        __m256 _sum4_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 4);
                                        __m256 _sum5_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 5);
                                        __m256 _sum6_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 6);
                                        __m256 _sum7_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 7);
                                        __m256 _sum8_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 8);
                                        __m256 _sum9_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 9);
                                        __m256 _suma_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 10);
                                        __m256 _sumb_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 11);
                                        __m256 _sum0_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack);
                                        __m256 _sum1_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8);
                                        __m256 _sum2_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 2);
                                        __m256 _sum3_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 3);
                                        __m256 _sum4_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 4);
                                        __m256 _sum5_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 5);
                                        __m256 _sum6_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 6);
                                        __m256 _sum7_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 7);
                                        __m256 _sum8_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 8);
                                        __m256 _sum9_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 9);
                                        __m256 _suma_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 10);
                                        __m256 _sumb_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 11);
                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum0_0), _sum0_1, 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum1_0), _sum1_1, 1);
                                        _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum2_0), _sum2_1, 1);
                                        _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum3_0), _sum3_1, 1);
                                        _sum4 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum4_0), _sum4_1, 1);
                                        _sum5 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum5_0), _sum5_1, 1);
                                        _sum6 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum6_0), _sum6_1, 1);
                                        _sum7 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum7_0), _sum7_1, 1);
                                        _sum8 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum8_0), _sum8_1, 1);
                                        _sum9 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum9_0), _sum9_1, 1);
                                        _suma = _mm512_insertf32x8(_mm512_castps256_ps512(_suma_0), _suma_1, 1);
                                        _sumb = _mm512_insertf32x8(_mm512_castps256_ps512(_sumb_0), _sumb_1, 1);
                                    }
                                    if (out_elempack == 4)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 7);
                                        __m128 _sum8_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 8);
                                        __m128 _sum9_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 9);
                                        __m128 _suma_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 10);
                                        __m128 _sumb_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 11);
                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 7);
                                        __m128 _sum8_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 8);
                                        __m128 _sum9_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 9);
                                        __m128 _suma_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 10);
                                        __m128 _sumb_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 11);
                                        __m128 _sum0_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 7);
                                        __m128 _sum8_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 8);
                                        __m128 _sum9_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 9);
                                        __m128 _suma_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 10);
                                        __m128 _sumb_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 11);
                                        __m128 _sum0_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 7);
                                        __m128 _sum8_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 8);
                                        __m128 _sum9_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 9);
                                        __m128 _suma_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 10);
                                        __m128 _sumb_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 11);
                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_2), _sum0_3, 1), 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_2), _sum1_3, 1), 1);
                                        _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum2_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_2), _sum2_3, 1), 1);
                                        _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum3_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_2), _sum3_3, 1), 1);
                                        _sum4 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum4_0), _sum4_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum4_2), _sum4_3, 1), 1);
                                        _sum5 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum5_0), _sum5_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum5_2), _sum5_3, 1), 1);
                                        _sum6 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum6_0), _sum6_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum6_2), _sum6_3, 1), 1);
                                        _sum7 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum7_0), _sum7_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum7_2), _sum7_3, 1), 1);
                                        _sum8 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum8_0), _sum8_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum8_2), _sum8_3, 1), 1);
                                        _sum9 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum9_0), _sum9_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum9_2), _sum9_3, 1), 1);
                                        _suma = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_suma_0), _suma_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_suma_2), _suma_3, 1), 1);
                                        _sumb = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sumb_0), _sumb_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sumb_2), _sumb_3, 1), 1);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj);
                                        __m128 _sum2_0 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj);
                                        __m128 _sum3_0 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj);
                                        __m128 _sum4_0 = _mm_loadu_ps(pC + (i + ii + 4) * N + j + jj);
                                        __m128 _sum5_0 = _mm_loadu_ps(pC + (i + ii + 5) * N + j + jj);
                                        __m128 _sum6_0 = _mm_loadu_ps(pC + (i + ii + 6) * N + j + jj);
                                        __m128 _sum7_0 = _mm_loadu_ps(pC + (i + ii + 7) * N + j + jj);
                                        __m128 _sum8_0 = _mm_loadu_ps(pC + (i + ii + 8) * N + j + jj);
                                        __m128 _sum9_0 = _mm_loadu_ps(pC + (i + ii + 9) * N + j + jj);
                                        __m128 _suma_0 = _mm_loadu_ps(pC + (i + ii + 10) * N + j + jj);
                                        __m128 _sumb_0 = _mm_loadu_ps(pC + (i + ii + 11) * N + j + jj);
                                        __m128 _sumc_0 = _mm_loadu_ps(pC + (i + ii + 12) * N + j + jj);
                                        __m128 _sumd_0 = _mm_loadu_ps(pC + (i + ii + 13) * N + j + jj);
                                        __m128 _sume_0 = _mm_loadu_ps(pC + (i + ii + 14) * N + j + jj);
                                        __m128 _sumf_0 = _mm_loadu_ps(pC + (i + ii + 15) * N + j + jj);

                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj + 4);
                                        __m128 _sum1_1 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj + 4);
                                        __m128 _sum2_1 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj + 4);
                                        __m128 _sum3_1 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj + 4);
                                        __m128 _sum4_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + j + jj + 4);
                                        __m128 _sum5_1 = _mm_loadu_ps(pC + (i + ii + 5) * N + j + jj + 4);
                                        __m128 _sum6_1 = _mm_loadu_ps(pC + (i + ii + 6) * N + j + jj + 4);
                                        __m128 _sum7_1 = _mm_loadu_ps(pC + (i + ii + 7) * N + j + jj + 4);
                                        __m128 _sum8_1 = _mm_loadu_ps(pC + (i + ii + 8) * N + j + jj + 4);
                                        __m128 _sum9_1 = _mm_loadu_ps(pC + (i + ii + 9) * N + j + jj + 4);
                                        __m128 _suma_1 = _mm_loadu_ps(pC + (i + ii + 10) * N + j + jj + 4);
                                        __m128 _sumb_1 = _mm_loadu_ps(pC + (i + ii + 11) * N + j + jj + 4);
                                        __m128 _sumc_1 = _mm_loadu_ps(pC + (i + ii + 12) * N + j + jj + 4);
                                        __m128 _sumd_1 = _mm_loadu_ps(pC + (i + ii + 13) * N + j + jj + 4);
                                        __m128 _sume_1 = _mm_loadu_ps(pC + (i + ii + 14) * N + j + jj + 4);
                                        __m128 _sumf_1 = _mm_loadu_ps(pC + (i + ii + 15) * N + j + jj + 4);

                                        __m128 _sum0_2 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj + 8);
                                        __m128 _sum1_2 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj + 8);
                                        __m128 _sum2_2 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj + 8);
                                        __m128 _sum3_2 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj + 8);
                                        __m128 _sum4_2 = _mm_loadu_ps(pC + (i + ii + 4) * N + j + jj + 8);
                                        __m128 _sum5_2 = _mm_loadu_ps(pC + (i + ii + 5) * N + j + jj + 8);
                                        __m128 _sum6_2 = _mm_loadu_ps(pC + (i + ii + 6) * N + j + jj + 8);
                                        __m128 _sum7_2 = _mm_loadu_ps(pC + (i + ii + 7) * N + j + jj + 8);
                                        __m128 _sum8_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + j + jj + 8);
                                        __m128 _sum9_2 = _mm_loadu_ps(pC + (i + ii + 9) * N + j + jj + 8);
                                        __m128 _suma_2 = _mm_loadu_ps(pC + (i + ii + 10) * N + j + jj + 8);
                                        __m128 _sumb_2 = _mm_loadu_ps(pC + (i + ii + 11) * N + j + jj + 8);
                                        __m128 _sumc_2 = _mm_loadu_ps(pC + (i + ii + 12) * N + j + jj + 8);
                                        __m128 _sumd_2 = _mm_loadu_ps(pC + (i + ii + 13) * N + j + jj + 8);
                                        __m128 _sume_2 = _mm_loadu_ps(pC + (i + ii + 14) * N + j + jj + 8);
                                        __m128 _sumf_2 = _mm_loadu_ps(pC + (i + ii + 15) * N + j + jj + 8);

                                        _MM_TRANSPOSE4_PS(_sum0_0, _sum1_0, _sum2_0, _sum3_0);
                                        _MM_TRANSPOSE4_PS(_sum4_0, _sum5_0, _sum6_0, _sum7_0);
                                        _MM_TRANSPOSE4_PS(_sum8_0, _sum9_0, _suma_0, _sumb_0);
                                        _MM_TRANSPOSE4_PS(_sumc_0, _sumd_0, _sume_0, _sumf_0);

                                        _MM_TRANSPOSE4_PS(_sum0_1, _sum1_1, _sum2_1, _sum3_1);
                                        _MM_TRANSPOSE4_PS(_sum4_1, _sum5_1, _sum6_1, _sum7_1);
                                        _MM_TRANSPOSE4_PS(_sum8_1, _sum9_1, _suma_1, _sumb_1);
                                        _MM_TRANSPOSE4_PS(_sumc_1, _sumd_1, _sume_1, _sumf_1);

                                        _MM_TRANSPOSE4_PS(_sum0_2, _sum1_2, _sum2_2, _sum3_2);
                                        _MM_TRANSPOSE4_PS(_sum4_2, _sum5_2, _sum6_2, _sum7_2);
                                        _MM_TRANSPOSE4_PS(_sum8_2, _sum9_2, _suma_2, _sumb_2);
                                        _MM_TRANSPOSE4_PS(_sumc_2, _sumd_2, _sume_2, _sumf_2);

                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum4_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum8_0), _sumc_0, 1), 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum5_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum9_0), _sumd_0, 1), 1);
                                        _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum6_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_suma_0), _sume_0, 1), 1);
                                        _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum7_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sumb_0), _sumf_0, 1), 1);

                                        _sum4 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_1), _sum4_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum8_1), _sumc_1, 1), 1);
                                        _sum5 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_1), _sum5_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum9_1), _sumd_1, 1), 1);
                                        _sum6 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_1), _sum6_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_suma_1), _sume_1, 1), 1);
                                        _sum7 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_1), _sum7_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sumb_1), _sumf_1, 1), 1);

                                        _sum8 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_2), _sum4_2, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum8_2), _sumc_2, 1), 1);
                                        _sum9 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_2), _sum5_2, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum9_2), _sumd_2, 1), 1);
                                        _suma = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_2), _sum6_2, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_suma_2), _sume_2, 1), 1);
                                        _sumb = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_2), _sum7_2, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sumb_2), _sumf_2, 1), 1);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm512_set1_ps(pC[j + jj]);
                                    _sum1 = _mm512_set1_ps(pC[j + jj + 1]);
                                    _sum2 = _mm512_set1_ps(pC[j + jj + 2]);
                                    _sum3 = _mm512_set1_ps(pC[j + jj + 3]);
                                    _sum4 = _mm512_set1_ps(pC[j + jj + 4]);
                                    _sum5 = _mm512_set1_ps(pC[j + jj + 5]);
                                    _sum6 = _mm512_set1_ps(pC[j + jj + 6]);
                                    _sum7 = _mm512_set1_ps(pC[j + jj + 7]);
                                    _sum8 = _mm512_set1_ps(pC[j + jj + 8]);
                                    _sum9 = _mm512_set1_ps(pC[j + jj + 9]);
                                    _suma = _mm512_set1_ps(pC[j + jj + 10]);
                                    _sumb = _mm512_set1_ps(pC[j + jj + 11]);
                                }

                                __m512 _beta = _mm512_set1_ps(beta);
                                _sum0 = _mm512_mul_ps(_sum0, _beta);
                                _sum1 = _mm512_mul_ps(_sum1, _beta);
                                _sum2 = _mm512_mul_ps(_sum2, _beta);
                                _sum3 = _mm512_mul_ps(_sum3, _beta);
                                _sum4 = _mm512_mul_ps(_sum4, _beta);
                                _sum5 = _mm512_mul_ps(_sum5, _beta);
                                _sum6 = _mm512_mul_ps(_sum6, _beta);
                                _sum7 = _mm512_mul_ps(_sum7, _beta);
                                _sum8 = _mm512_mul_ps(_sum8, _beta);
                                _sum9 = _mm512_mul_ps(_sum9, _beta);
                                _suma = _mm512_mul_ps(_suma, _beta);
                                _sumb = _mm512_mul_ps(_sumb, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm512_load_ps(ptmp);
                            _sum1 = _mm512_load_ps(ptmp + 16 * 1);
                            _sum2 = _mm512_load_ps(ptmp + 16 * 2);
                            _sum3 = _mm512_load_ps(ptmp + 16 * 3);
                            _sum4 = _mm512_load_ps(ptmp + 16 * 4);
                            _sum5 = _mm512_load_ps(ptmp + 16 * 5);
                            _sum6 = _mm512_load_ps(ptmp + 16 * 6);
                            _sum7 = _mm512_load_ps(ptmp + 16 * 7);
                            _sum8 = _mm512_load_ps(ptmp + 16 * 8);
                            _sum9 = _mm512_load_ps(ptmp + 16 * 9);
                            _suma = _mm512_load_ps(ptmp + 16 * 10);
                            _sumb = _mm512_load_ps(ptmp + 16 * 11);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m512 _pA = _mm512_load_ps(pA);

                            _sum0 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[1]), _sum1);
                            _sum2 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[2]), _sum2);
                            _sum3 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[3]), _sum3);
                            _sum4 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[4]), _sum4);
                            _sum5 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[5]), _sum5);
                            _sum6 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[6]), _sum6);
                            _sum7 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[7]), _sum7);
                            _sum8 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[8]), _sum8);
                            _sum9 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[9]), _sum9);
                            _suma = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[10]), _suma);
                            _sumb = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[11]), _sumb);

                            pA += 16;
                            pB += 12;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m512 _alpha = _mm512_set1_ps(alpha);
                            _sum0 = _mm512_mul_ps(_sum0, _alpha);
                            _sum1 = _mm512_mul_ps(_sum1, _alpha);
                            _sum2 = _mm512_mul_ps(_sum2, _alpha);
                            _sum3 = _mm512_mul_ps(_sum3, _alpha);
                            _sum4 = _mm512_mul_ps(_sum4, _alpha);
                            _sum5 = _mm512_mul_ps(_sum5, _alpha);
                            _sum6 = _mm512_mul_ps(_sum6, _alpha);
                            _sum7 = _mm512_mul_ps(_sum7, _alpha);
                            _sum8 = _mm512_mul_ps(_sum8, _alpha);
                            _sum9 = _mm512_mul_ps(_sum9, _alpha);
                            _suma = _mm512_mul_ps(_suma, _alpha);
                            _sumb = _mm512_mul_ps(_sumb, _alpha);

                            if (out_elempack == 16)
                            {
                                _mm512_storeu_ps(outptr0, _sum0);
                                _mm512_storeu_ps(outptr0 + 16 * 1, _sum1);
                                _mm512_storeu_ps(outptr0 + 16 * 2, _sum2);
                                _mm512_storeu_ps(outptr0 + 16 * 3, _sum3);
                                _mm512_storeu_ps(outptr0 + 16 * 4, _sum4);
                                _mm512_storeu_ps(outptr0 + 16 * 5, _sum5);
                                _mm512_storeu_ps(outptr0 + 16 * 6, _sum6);
                                _mm512_storeu_ps(outptr0 + 16 * 7, _sum7);
                                _mm512_storeu_ps(outptr0 + 16 * 8, _sum8);
                                _mm512_storeu_ps(outptr0 + 16 * 9, _sum9);
                                _mm512_storeu_ps(outptr0 + 16 * 10, _suma);
                                _mm512_storeu_ps(outptr0 + 16 * 11, _sumb);
                                outptr0 += 192;
                            }
                            if (out_elempack == 8)
                            {
                                _mm256_storeu_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 4, _mm512_extractf32x8_ps(_sum4, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 5, _mm512_extractf32x8_ps(_sum5, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 6, _mm512_extractf32x8_ps(_sum6, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 7, _mm512_extractf32x8_ps(_sum7, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 8, _mm512_extractf32x8_ps(_sum8, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 9, _mm512_extractf32x8_ps(_sum9, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 10, _mm512_extractf32x8_ps(_suma, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 11, _mm512_extractf32x8_ps(_sumb, 0));

                                _mm256_storeu_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum0, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 4, _mm512_extractf32x8_ps(_sum4, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 5, _mm512_extractf32x8_ps(_sum5, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 6, _mm512_extractf32x8_ps(_sum6, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 7, _mm512_extractf32x8_ps(_sum7, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 8, _mm512_extractf32x8_ps(_sum8, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 9, _mm512_extractf32x8_ps(_sum9, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 10, _mm512_extractf32x8_ps(_suma, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 11, _mm512_extractf32x8_ps(_sumb, 1));

                                outptr0 += 96;
                            }
                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _mm512_extractf32x4_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 0));
                                _mm_storeu_ps(outptr0 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 0));
                                _mm_storeu_ps(outptr0 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 0));
                                _mm_storeu_ps(outptr0 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 0));
                                _mm_storeu_ps(outptr0 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 0));
                                _mm_storeu_ps(outptr0 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 0));
                                _mm_storeu_ps(outptr0 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 0));
                                _mm_storeu_ps(outptr0 + 4 * 8, _mm512_extractf32x4_ps(_sum8, 0));
                                _mm_storeu_ps(outptr0 + 4 * 9, _mm512_extractf32x4_ps(_sum9, 0));
                                _mm_storeu_ps(outptr0 + 4 * 10, _mm512_extractf32x4_ps(_suma, 0));
                                _mm_storeu_ps(outptr0 + 4 * 11, _mm512_extractf32x4_ps(_sumb, 0));

                                _mm_storeu_ps(outptr0 + N * 4, _mm512_extractf32x4_ps(_sum0, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 8, _mm512_extractf32x4_ps(_sum8, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 9, _mm512_extractf32x4_ps(_sum9, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 10, _mm512_extractf32x4_ps(_suma, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 11, _mm512_extractf32x4_ps(_sumb, 1));

                                _mm_storeu_ps(outptr0 + N * 8, _mm512_extractf32x4_ps(_sum0, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 8, _mm512_extractf32x4_ps(_sum8, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 9, _mm512_extractf32x4_ps(_sum9, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 10, _mm512_extractf32x4_ps(_suma, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 11, _mm512_extractf32x4_ps(_sumb, 2));

                                _mm_storeu_ps(outptr0 + N * 12, _mm512_extractf32x4_ps(_sum0, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 8, _mm512_extractf32x4_ps(_sum8, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 9, _mm512_extractf32x4_ps(_sum9, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 10, _mm512_extractf32x4_ps(_suma, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 11, _mm512_extractf32x4_ps(_sumb, 3));

                                outptr0 += 48;
                            }
                            if (out_elempack == 1)
                            {
                                transpose16x12_ps(_sum0, _sum1, _sum2, _sum3, _sum4, _sum5, _sum6, _sum7, _sum8, _sum9, _suma, _sumb);

                                _mm256_storeu_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + 8, _mm512_extractf32x4_ps(_sum0, 2));
                                _mm_storeu_ps(outptr0 + N * 1, _mm512_extractf32x4_ps(_sum0, 3));
                                _mm256_storeu_ps(outptr0 + N * 1 + 4, _mm512_extractf32x8_ps(_sum1, 0));
                                _mm256_storeu_ps(outptr0 + N * 2, _mm512_extractf32x8_ps(_sum1, 1));
                                _mm_storeu_ps(outptr0 + N * 2 + 8, _mm512_extractf32x4_ps(_sum2, 0));
                                _mm_storeu_ps(outptr0 + N * 3, _mm512_extractf32x4_ps(_sum2, 1));
                                _mm256_storeu_ps(outptr0 + N * 3 + 4, _mm512_extractf32x8_ps(_sum2, 1));

                                _mm256_storeu_ps(outptr0 + N * 4, _mm512_extractf32x8_ps(_sum3, 0));
                                _mm_storeu_ps(outptr0 + N * 4 + 8, _mm512_extractf32x4_ps(_sum3, 2));
                                _mm_storeu_ps(outptr0 + N * 5, _mm512_extractf32x4_ps(_sum3, 3));
                                _mm256_storeu_ps(outptr0 + N * 5 + 4, _mm512_extractf32x8_ps(_sum4, 0));
                                _mm256_storeu_ps(outptr0 + N * 6, _mm512_extractf32x8_ps(_sum4, 1));
                                _mm_storeu_ps(outptr0 + N * 6 + 8, _mm512_extractf32x4_ps(_sum5, 0));
                                _mm_storeu_ps(outptr0 + N * 7, _mm512_extractf32x4_ps(_sum5, 1));
                                _mm256_storeu_ps(outptr0 + N * 7 + 4, _mm512_extractf32x8_ps(_sum5, 1));

                                _mm256_storeu_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum6, 0));
                                _mm_storeu_ps(outptr0 + N * 8 + 8, _mm512_extractf32x4_ps(_sum6, 2));
                                _mm_storeu_ps(outptr0 + N * 9, _mm512_extractf32x4_ps(_sum6, 3));
                                _mm256_storeu_ps(outptr0 + N * 9 + 4, _mm512_extractf32x8_ps(_sum7, 0));
                                _mm256_storeu_ps(outptr0 + N * 10, _mm512_extractf32x8_ps(_sum7, 1));
                                _mm_storeu_ps(outptr0 + N * 10 + 8, _mm512_extractf32x4_ps(_sum8, 0));
                                _mm_storeu_ps(outptr0 + N * 11, _mm512_extractf32x4_ps(_sum8, 1));
                                _mm256_storeu_ps(outptr0 + N * 11 + 4, _mm512_extractf32x8_ps(_sum8, 1));

                                _mm256_storeu_ps(outptr0 + N * 12, _mm512_extractf32x8_ps(_sum9, 0));
                                _mm_storeu_ps(outptr0 + N * 12 + 8, _mm512_extractf32x4_ps(_sum9, 2));
                                _mm_storeu_ps(outptr0 + N * 13, _mm512_extractf32x4_ps(_sum9, 3));
                                _mm256_storeu_ps(outptr0 + N * 13 + 4, _mm512_extractf32x8_ps(_suma, 0));
                                _mm256_storeu_ps(outptr0 + N * 14, _mm512_extractf32x8_ps(_suma, 1));
                                _mm_storeu_ps(outptr0 + N * 14 + 8, _mm512_extractf32x4_ps(_sumb, 0));
                                _mm_storeu_ps(outptr0 + N * 15, _mm512_extractf32x4_ps(_sumb, 1));
                                _mm256_storeu_ps(outptr0 + N * 15 + 4, _mm512_extractf32x8_ps(_sumb, 1));

                                outptr0 += 12;
                            }
                        }
                        else
                        {
                            _mm512_store_ps(ptmp, _sum0);
                            _mm512_store_ps(ptmp + 16 * 1, _sum1);
                            _mm512_store_ps(ptmp + 16 * 2, _sum2);
                            _mm512_store_ps(ptmp + 16 * 3, _sum3);
                            _mm512_store_ps(ptmp + 16 * 4, _sum4);
                            _mm512_store_ps(ptmp + 16 * 5, _sum5);
                            _mm512_store_ps(ptmp + 16 * 6, _sum6);
                            _mm512_store_ps(ptmp + 16 * 7, _sum7);
                            _mm512_store_ps(ptmp + 16 * 8, _sum8);
                            _mm512_store_ps(ptmp + 16 * 9, _sum9);
                            _mm512_store_ps(ptmp + 16 * 10, _suma);
                            _mm512_store_ps(ptmp + 16 * 11, _sumb);
                        }

                        ptmp += 192;
                    }
                    for (; jj + 7 < max_jj; jj += 8)
                    {
                        __m512 _sum0;
                        __m512 _sum1;
                        __m512 _sum2;
                        __m512 _sum3;
                        __m512 _sum4;
                        __m512 _sum5;
                        __m512 _sum6;
                        __m512 _sum7;

                        if (k == 0)
                        {
                            _sum0 = _mm512_setzero_ps();
                            _sum1 = _mm512_setzero_ps();
                            _sum2 = _mm512_setzero_ps();
                            _sum3 = _mm512_setzero_ps();
                            _sum4 = _mm512_setzero_ps();
                            _sum5 = _mm512_setzero_ps();
                            _sum6 = _mm512_setzero_ps();
                            _sum7 = _mm512_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm512_set1_ps(pC[0]);
                                    _sum1 = _mm512_set1_ps(pC[0]);
                                    _sum2 = _mm512_set1_ps(pC[0]);
                                    _sum3 = _mm512_set1_ps(pC[0]);
                                    _sum4 = _mm512_set1_ps(pC[0]);
                                    _sum5 = _mm512_set1_ps(pC[0]);
                                    _sum6 = _mm512_set1_ps(pC[0]);
                                    _sum7 = _mm512_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm512_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                    _sum2 = _sum0;
                                    _sum3 = _sum0;
                                    _sum4 = _sum0;
                                    _sum5 = _sum0;
                                    _sum6 = _sum0;
                                    _sum7 = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 16)
                                    {
                                        _sum0 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16);
                                        _sum2 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 2);
                                        _sum3 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 3);
                                        _sum4 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 4);
                                        _sum5 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 5);
                                        _sum6 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 6);
                                        _sum7 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16 * 7);
                                    }
                                    if (out_elempack == 8)
                                    {
                                        __m256 _sum0_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m256 _sum1_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8);
                                        __m256 _sum2_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 2);
                                        __m256 _sum3_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 3);
                                        __m256 _sum4_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 4);
                                        __m256 _sum5_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 5);
                                        __m256 _sum6_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 6);
                                        __m256 _sum7_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8 * 7);
                                        __m256 _sum0_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack);
                                        __m256 _sum1_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8);
                                        __m256 _sum2_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 2);
                                        __m256 _sum3_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 3);
                                        __m256 _sum4_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 4);
                                        __m256 _sum5_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 5);
                                        __m256 _sum6_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 6);
                                        __m256 _sum7_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8 * 7);
                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum0_0), _sum0_1, 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum1_0), _sum1_1, 1);
                                        _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum2_0), _sum2_1, 1);
                                        _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum3_0), _sum3_1, 1);
                                        _sum4 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum4_0), _sum4_1, 1);
                                        _sum5 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum5_0), _sum5_1, 1);
                                        _sum6 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum6_0), _sum6_1, 1);
                                        _sum7 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum7_0), _sum7_1, 1);
                                    }
                                    if (out_elempack == 4)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 7);
                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 7);
                                        __m128 _sum0_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4 * 7);
                                        __m128 _sum0_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4 * 7);
                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_2), _sum0_3, 1), 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_2), _sum1_3, 1), 1);
                                        _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum2_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_2), _sum2_3, 1), 1);
                                        _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum3_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_2), _sum3_3, 1), 1);
                                        _sum4 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum4_0), _sum4_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum4_2), _sum4_3, 1), 1);
                                        _sum5 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum5_0), _sum5_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum5_2), _sum5_3, 1), 1);
                                        _sum6 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum6_0), _sum6_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum6_2), _sum6_3, 1), 1);
                                        _sum7 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum7_0), _sum7_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum7_2), _sum7_3, 1), 1);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        __m256 _r0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + j + jj);
                                        __m256 _r1 = _mm256_loadu_ps(pC + (i + ii + 1) * N + j + jj);
                                        __m256 _r2 = _mm256_loadu_ps(pC + (i + ii + 2) * N + j + jj);
                                        __m256 _r3 = _mm256_loadu_ps(pC + (i + ii + 3) * N + j + jj);
                                        __m256 _r4 = _mm256_loadu_ps(pC + (i + ii + 4) * N + j + jj);
                                        __m256 _r5 = _mm256_loadu_ps(pC + (i + ii + 5) * N + j + jj);
                                        __m256 _r6 = _mm256_loadu_ps(pC + (i + ii + 6) * N + j + jj);
                                        __m256 _r7 = _mm256_loadu_ps(pC + (i + ii + 7) * N + j + jj);
                                        __m256 _r8 = _mm256_loadu_ps(pC + (i + ii + 8) * N + j + jj);
                                        __m256 _r9 = _mm256_loadu_ps(pC + (i + ii + 9) * N + j + jj);
                                        __m256 _ra = _mm256_loadu_ps(pC + (i + ii + 10) * N + j + jj);
                                        __m256 _rb = _mm256_loadu_ps(pC + (i + ii + 11) * N + j + jj);
                                        __m256 _rc = _mm256_loadu_ps(pC + (i + ii + 12) * N + j + jj);
                                        __m256 _rd = _mm256_loadu_ps(pC + (i + ii + 13) * N + j + jj);
                                        __m256 _re = _mm256_loadu_ps(pC + (i + ii + 14) * N + j + jj);
                                        __m256 _rf = _mm256_loadu_ps(pC + (i + ii + 15) * N + j + jj);

                                        // transpose8x16
                                        __m256 _tmp0 = _mm256_unpacklo_ps(_r0, _r1);
                                        __m256 _tmp1 = _mm256_unpackhi_ps(_r0, _r1);
                                        __m256 _tmp2 = _mm256_unpacklo_ps(_r2, _r3);
                                        __m256 _tmp3 = _mm256_unpackhi_ps(_r2, _r3);
                                        __m256 _tmp4 = _mm256_unpacklo_ps(_r4, _r5);
                                        __m256 _tmp5 = _mm256_unpackhi_ps(_r4, _r5);
                                        __m256 _tmp6 = _mm256_unpacklo_ps(_r6, _r7);
                                        __m256 _tmp7 = _mm256_unpackhi_ps(_r6, _r7);
                                        __m256 _tmp8 = _mm256_unpacklo_ps(_r8, _r9);
                                        __m256 _tmp9 = _mm256_unpackhi_ps(_r8, _r9);
                                        __m256 _tmpa = _mm256_unpacklo_ps(_ra, _rb);
                                        __m256 _tmpb = _mm256_unpackhi_ps(_ra, _rb);
                                        __m256 _tmpc = _mm256_unpacklo_ps(_rc, _rd);
                                        __m256 _tmpd = _mm256_unpackhi_ps(_rc, _rd);
                                        __m256 _tmpe = _mm256_unpacklo_ps(_re, _rf);
                                        __m256 _tmpf = _mm256_unpackhi_ps(_re, _rf);

                                        __m256 _tmpg = _mm256_shuffle_ps(_tmp0, _tmp2, _MM_SHUFFLE(1, 0, 1, 0));
                                        __m256 _tmph = _mm256_shuffle_ps(_tmp0, _tmp2, _MM_SHUFFLE(3, 2, 3, 2));
                                        __m256 _tmpi = _mm256_shuffle_ps(_tmp1, _tmp3, _MM_SHUFFLE(1, 0, 1, 0));
                                        __m256 _tmpj = _mm256_shuffle_ps(_tmp1, _tmp3, _MM_SHUFFLE(3, 2, 3, 2));
                                        __m256 _tmpk = _mm256_shuffle_ps(_tmp4, _tmp6, _MM_SHUFFLE(1, 0, 1, 0));
                                        __m256 _tmpl = _mm256_shuffle_ps(_tmp4, _tmp6, _MM_SHUFFLE(3, 2, 3, 2));
                                        __m256 _tmpm = _mm256_shuffle_ps(_tmp5, _tmp7, _MM_SHUFFLE(1, 0, 1, 0));
                                        __m256 _tmpn = _mm256_shuffle_ps(_tmp5, _tmp7, _MM_SHUFFLE(3, 2, 3, 2));
                                        __m256 _tmpo = _mm256_shuffle_ps(_tmp8, _tmpa, _MM_SHUFFLE(1, 0, 1, 0));
                                        __m256 _tmpp = _mm256_shuffle_ps(_tmp8, _tmpa, _MM_SHUFFLE(3, 2, 3, 2));
                                        __m256 _tmpq = _mm256_shuffle_ps(_tmp9, _tmpb, _MM_SHUFFLE(1, 0, 1, 0));
                                        __m256 _tmpr = _mm256_shuffle_ps(_tmp9, _tmpb, _MM_SHUFFLE(3, 2, 3, 2));
                                        __m256 _tmps = _mm256_shuffle_ps(_tmpc, _tmpe, _MM_SHUFFLE(1, 0, 1, 0));
                                        __m256 _tmpt = _mm256_shuffle_ps(_tmpc, _tmpe, _MM_SHUFFLE(3, 2, 3, 2));
                                        __m256 _tmpu = _mm256_shuffle_ps(_tmpd, _tmpf, _MM_SHUFFLE(1, 0, 1, 0));
                                        __m256 _tmpv = _mm256_shuffle_ps(_tmpd, _tmpf, _MM_SHUFFLE(3, 2, 3, 2));

                                        _r0 = _mm256_permute2f128_ps(_tmpg, _tmpk, _MM_SHUFFLE(0, 2, 0, 0));
                                        _r1 = _mm256_permute2f128_ps(_tmpo, _tmps, _MM_SHUFFLE(0, 2, 0, 0));
                                        _r2 = _mm256_permute2f128_ps(_tmph, _tmpl, _MM_SHUFFLE(0, 2, 0, 0));
                                        _r3 = _mm256_permute2f128_ps(_tmpp, _tmpt, _MM_SHUFFLE(0, 2, 0, 0));
                                        _r4 = _mm256_permute2f128_ps(_tmpi, _tmpm, _MM_SHUFFLE(0, 2, 0, 0));
                                        _r5 = _mm256_permute2f128_ps(_tmpq, _tmpu, _MM_SHUFFLE(0, 2, 0, 0));
                                        _r6 = _mm256_permute2f128_ps(_tmpj, _tmpn, _MM_SHUFFLE(0, 2, 0, 0));
                                        _r7 = _mm256_permute2f128_ps(_tmpr, _tmpv, _MM_SHUFFLE(0, 2, 0, 0));
                                        _r8 = _mm256_permute2f128_ps(_tmpg, _tmpk, _MM_SHUFFLE(0, 3, 0, 1));
                                        _r9 = _mm256_permute2f128_ps(_tmpo, _tmps, _MM_SHUFFLE(0, 3, 0, 1));
                                        _ra = _mm256_permute2f128_ps(_tmph, _tmpl, _MM_SHUFFLE(0, 3, 0, 1));
                                        _rb = _mm256_permute2f128_ps(_tmpp, _tmpt, _MM_SHUFFLE(0, 3, 0, 1));
                                        _rc = _mm256_permute2f128_ps(_tmpi, _tmpm, _MM_SHUFFLE(0, 3, 0, 1));
                                        _rd = _mm256_permute2f128_ps(_tmpq, _tmpu, _MM_SHUFFLE(0, 3, 0, 1));
                                        _re = _mm256_permute2f128_ps(_tmpj, _tmpn, _MM_SHUFFLE(0, 3, 0, 1));
                                        _rf = _mm256_permute2f128_ps(_tmpr, _tmpv, _MM_SHUFFLE(0, 3, 0, 1));

                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_r0), _r1, 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_r2), _r3, 1);
                                        _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_r4), _r5, 1);
                                        _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_r6), _r7, 1);
                                        _sum4 = _mm512_insertf32x8(_mm512_castps256_ps512(_r8), _r9, 1);
                                        _sum5 = _mm512_insertf32x8(_mm512_castps256_ps512(_ra), _rb, 1);
                                        _sum6 = _mm512_insertf32x8(_mm512_castps256_ps512(_rc), _rd, 1);
                                        _sum7 = _mm512_insertf32x8(_mm512_castps256_ps512(_re), _rf, 1);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm512_set1_ps(pC[j + jj]);
                                    _sum1 = _mm512_set1_ps(pC[j + jj + 1]);
                                    _sum2 = _mm512_set1_ps(pC[j + jj + 2]);
                                    _sum3 = _mm512_set1_ps(pC[j + jj + 3]);
                                    _sum4 = _mm512_set1_ps(pC[j + jj + 4]);
                                    _sum5 = _mm512_set1_ps(pC[j + jj + 5]);
                                    _sum6 = _mm512_set1_ps(pC[j + jj + 6]);
                                    _sum7 = _mm512_set1_ps(pC[j + jj + 7]);
                                }

                                __m512 _beta = _mm512_set1_ps(beta);
                                _sum0 = _mm512_mul_ps(_sum0, _beta);
                                _sum1 = _mm512_mul_ps(_sum1, _beta);
                                _sum2 = _mm512_mul_ps(_sum2, _beta);
                                _sum3 = _mm512_mul_ps(_sum3, _beta);
                                _sum4 = _mm512_mul_ps(_sum4, _beta);
                                _sum5 = _mm512_mul_ps(_sum5, _beta);
                                _sum6 = _mm512_mul_ps(_sum6, _beta);
                                _sum7 = _mm512_mul_ps(_sum7, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm512_loadu_ps(ptmp);
                            _sum1 = _mm512_loadu_ps(ptmp + 16 * 1);
                            _sum2 = _mm512_loadu_ps(ptmp + 16 * 2);
                            _sum3 = _mm512_loadu_ps(ptmp + 16 * 3);
                            _sum4 = _mm512_loadu_ps(ptmp + 16 * 4);
                            _sum5 = _mm512_loadu_ps(ptmp + 16 * 5);
                            _sum6 = _mm512_loadu_ps(ptmp + 16 * 6);
                            _sum7 = _mm512_loadu_ps(ptmp + 16 * 7);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m512 _pA = _mm512_loadu_ps(pA);

                            _sum0 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[1]), _sum1);
                            _sum2 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[2]), _sum2);
                            _sum3 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[3]), _sum3);
                            _sum4 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[4]), _sum4);
                            _sum5 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[5]), _sum5);
                            _sum6 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[6]), _sum6);
                            _sum7 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[7]), _sum7);

                            pA += 16;
                            pB += 8;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m512 _alpha = _mm512_set1_ps(alpha);
                            _sum0 = _mm512_mul_ps(_sum0, _alpha);
                            _sum1 = _mm512_mul_ps(_sum1, _alpha);
                            _sum2 = _mm512_mul_ps(_sum2, _alpha);
                            _sum3 = _mm512_mul_ps(_sum3, _alpha);
                            _sum4 = _mm512_mul_ps(_sum4, _alpha);
                            _sum5 = _mm512_mul_ps(_sum5, _alpha);
                            _sum6 = _mm512_mul_ps(_sum6, _alpha);
                            _sum7 = _mm512_mul_ps(_sum7, _alpha);

                            if (out_elempack == 16)
                            {
                                _mm512_storeu_ps(outptr0, _sum0);
                                _mm512_storeu_ps(outptr0 + 16 * 1, _sum1);
                                _mm512_storeu_ps(outptr0 + 16 * 2, _sum2);
                                _mm512_storeu_ps(outptr0 + 16 * 3, _sum3);
                                _mm512_storeu_ps(outptr0 + 16 * 4, _sum4);
                                _mm512_storeu_ps(outptr0 + 16 * 5, _sum5);
                                _mm512_storeu_ps(outptr0 + 16 * 6, _sum6);
                                _mm512_storeu_ps(outptr0 + 16 * 7, _sum7);
                                outptr0 += 128;
                            }
                            if (out_elempack == 8)
                            {
                                _mm256_storeu_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 4, _mm512_extractf32x8_ps(_sum4, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 5, _mm512_extractf32x8_ps(_sum5, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 6, _mm512_extractf32x8_ps(_sum6, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 7, _mm512_extractf32x8_ps(_sum7, 0));

                                _mm256_storeu_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum0, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 4, _mm512_extractf32x8_ps(_sum4, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 5, _mm512_extractf32x8_ps(_sum5, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 6, _mm512_extractf32x8_ps(_sum6, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 7, _mm512_extractf32x8_ps(_sum7, 1));

                                outptr0 += 64;
                            }
                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _mm512_extractf32x4_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 0));
                                _mm_storeu_ps(outptr0 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 0));
                                _mm_storeu_ps(outptr0 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 0));
                                _mm_storeu_ps(outptr0 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 0));
                                _mm_storeu_ps(outptr0 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 0));
                                _mm_storeu_ps(outptr0 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 0));
                                _mm_storeu_ps(outptr0 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 0));

                                _mm_storeu_ps(outptr0 + N * 4, _mm512_extractf32x4_ps(_sum0, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 1));

                                _mm_storeu_ps(outptr0 + N * 8, _mm512_extractf32x4_ps(_sum0, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 2));

                                _mm_storeu_ps(outptr0 + N * 12, _mm512_extractf32x4_ps(_sum0, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 3));

                                outptr0 += 32;
                            }
                            if (out_elempack == 1)
                            {
                                transpose16x8_ps(_sum0, _sum1, _sum2, _sum3, _sum4, _sum5, _sum6, _sum7);

                                _mm256_storeu_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                                _mm256_storeu_ps(outptr0 + N * 1, _mm512_extractf32x8_ps(_sum0, 1));
                                _mm256_storeu_ps(outptr0 + N * 2, _mm512_extractf32x8_ps(_sum1, 0));
                                _mm256_storeu_ps(outptr0 + N * 3, _mm512_extractf32x8_ps(_sum1, 1));
                                _mm256_storeu_ps(outptr0 + N * 4, _mm512_extractf32x8_ps(_sum2, 0));
                                _mm256_storeu_ps(outptr0 + N * 5, _mm512_extractf32x8_ps(_sum2, 1));
                                _mm256_storeu_ps(outptr0 + N * 6, _mm512_extractf32x8_ps(_sum3, 0));
                                _mm256_storeu_ps(outptr0 + N * 7, _mm512_extractf32x8_ps(_sum3, 1));
                                _mm256_storeu_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum4, 0));
                                _mm256_storeu_ps(outptr0 + N * 9, _mm512_extractf32x8_ps(_sum4, 1));
                                _mm256_storeu_ps(outptr0 + N * 10, _mm512_extractf32x8_ps(_sum5, 0));
                                _mm256_storeu_ps(outptr0 + N * 11, _mm512_extractf32x8_ps(_sum5, 1));
                                _mm256_storeu_ps(outptr0 + N * 12, _mm512_extractf32x8_ps(_sum6, 0));
                                _mm256_storeu_ps(outptr0 + N * 13, _mm512_extractf32x8_ps(_sum6, 1));
                                _mm256_storeu_ps(outptr0 + N * 14, _mm512_extractf32x8_ps(_sum7, 0));
                                _mm256_storeu_ps(outptr0 + N * 15, _mm512_extractf32x8_ps(_sum7, 1));

                                outptr0 += 8;
                            }
                        }
                        else
                        {
                            _mm512_storeu_ps(ptmp, _sum0);
                            _mm512_storeu_ps(ptmp + 16 * 1, _sum1);
                            _mm512_storeu_ps(ptmp + 16 * 2, _sum2);
                            _mm512_storeu_ps(ptmp + 16 * 3, _sum3);
                            _mm512_storeu_ps(ptmp + 16 * 4, _sum4);
                            _mm512_storeu_ps(ptmp + 16 * 5, _sum5);
                            _mm512_storeu_ps(ptmp + 16 * 6, _sum6);
                            _mm512_storeu_ps(ptmp + 16 * 7, _sum7);
                        }

                        ptmp += 128;
                    }
                    for (; jj + 3 < max_jj; jj += 4)
                    {
                        __m512 _sum0;
                        __m512 _sum1;
                        __m512 _sum2;
                        __m512 _sum3;

                        if (k == 0)
                        {
                            _sum0 = _mm512_setzero_ps();
                            _sum1 = _mm512_setzero_ps();
                            _sum2 = _mm512_setzero_ps();
                            _sum3 = _mm512_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm512_set1_ps(pC[0]);
                                    _sum1 = _mm512_set1_ps(pC[0]);
                                    _sum2 = _mm512_set1_ps(pC[0]);
                                    _sum3 = _mm512_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm512_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                    _sum2 = _sum0;
                                    _sum3 = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 16)
                                    {
                                        _sum0 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16);
                                        _sum2 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 32);
                                        _sum3 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 48);
                                    }
                                    if (out_elempack == 8)
                                    {
                                        __m256 _sum0_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m256 _sum1_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8);
                                        __m256 _sum2_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 16);
                                        __m256 _sum3_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 24);
                                        __m256 _sum0_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack);
                                        __m256 _sum1_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8);
                                        __m256 _sum2_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 16);
                                        __m256 _sum3_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 24);
                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum0_0), _sum0_1, 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum1_0), _sum1_1, 1);
                                        _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum2_0), _sum2_1, 1);
                                        _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum3_0), _sum3_1, 1);
                                    }
                                    if (out_elempack == 4)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8);
                                        __m128 _sum3_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 12);
                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 8);
                                        __m128 _sum3_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 12);
                                        __m128 _sum0_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8);
                                        __m128 _sum3_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 12);
                                        __m128 _sum0_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 8);
                                        __m128 _sum3_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 12);
                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_2), _sum0_3, 1), 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_2), _sum1_3, 1), 1);
                                        _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum2_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_2), _sum2_3, 1), 1);
                                        _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum3_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_2), _sum3_3, 1), 1);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj);
                                        __m128 _sum2_0 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj);
                                        __m128 _sum3_0 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj);
                                        __m128 _sum4_0 = _mm_loadu_ps(pC + (i + ii + 4) * N + j + jj);
                                        __m128 _sum5_0 = _mm_loadu_ps(pC + (i + ii + 5) * N + j + jj);
                                        __m128 _sum6_0 = _mm_loadu_ps(pC + (i + ii + 6) * N + j + jj);
                                        __m128 _sum7_0 = _mm_loadu_ps(pC + (i + ii + 7) * N + j + jj);
                                        __m128 _sum8_0 = _mm_loadu_ps(pC + (i + ii + 8) * N + j + jj);
                                        __m128 _sum9_0 = _mm_loadu_ps(pC + (i + ii + 9) * N + j + jj);
                                        __m128 _suma_0 = _mm_loadu_ps(pC + (i + ii + 10) * N + j + jj);
                                        __m128 _sumb_0 = _mm_loadu_ps(pC + (i + ii + 11) * N + j + jj);
                                        __m128 _sumc_0 = _mm_loadu_ps(pC + (i + ii + 12) * N + j + jj);
                                        __m128 _sumd_0 = _mm_loadu_ps(pC + (i + ii + 13) * N + j + jj);
                                        __m128 _sume_0 = _mm_loadu_ps(pC + (i + ii + 14) * N + j + jj);
                                        __m128 _sumf_0 = _mm_loadu_ps(pC + (i + ii + 15) * N + j + jj);

                                        _MM_TRANSPOSE4_PS(_sum0_0, _sum1_0, _sum2_0, _sum3_0);
                                        _MM_TRANSPOSE4_PS(_sum4_0, _sum5_0, _sum6_0, _sum7_0);
                                        _MM_TRANSPOSE4_PS(_sum8_0, _sum9_0, _suma_0, _sumb_0);
                                        _MM_TRANSPOSE4_PS(_sumc_0, _sumd_0, _sume_0, _sumf_0);

                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum4_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum8_0), _sumc_0, 1), 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum5_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum9_0), _sumd_0, 1), 1);
                                        _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum6_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_suma_0), _sume_0, 1), 1);
                                        _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum7_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sumb_0), _sumf_0, 1), 1);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm512_set1_ps(pC[j + jj]);
                                    _sum1 = _mm512_set1_ps(pC[j + jj + 1]);
                                    _sum2 = _mm512_set1_ps(pC[j + jj + 2]);
                                    _sum3 = _mm512_set1_ps(pC[j + jj + 3]);
                                }

                                __m512 _beta = _mm512_set1_ps(beta);
                                _sum0 = _mm512_mul_ps(_sum0, _beta);
                                _sum1 = _mm512_mul_ps(_sum1, _beta);
                                _sum2 = _mm512_mul_ps(_sum2, _beta);
                                _sum3 = _mm512_mul_ps(_sum3, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm512_loadu_ps(ptmp);
                            _sum1 = _mm512_loadu_ps(ptmp + 16 * 1);
                            _sum2 = _mm512_loadu_ps(ptmp + 16 * 2);
                            _sum3 = _mm512_loadu_ps(ptmp + 16 * 3);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m512 _pA = _mm512_loadu_ps(pA);

                            _sum0 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[1]), _sum1);
                            _sum2 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[2]), _sum2);
                            _sum3 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[3]), _sum3);

                            pA += 16;
                            pB += 4;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m512 _alpha = _mm512_set1_ps(alpha);
                            _sum0 = _mm512_mul_ps(_sum0, _alpha);
                            _sum1 = _mm512_mul_ps(_sum1, _alpha);
                            _sum2 = _mm512_mul_ps(_sum2, _alpha);
                            _sum3 = _mm512_mul_ps(_sum3, _alpha);

                            if (out_elempack == 16)
                            {
                                _mm512_storeu_ps(outptr0, _sum0);
                                _mm512_storeu_ps(outptr0 + 16 * 1, _sum1);
                                _mm512_storeu_ps(outptr0 + 16 * 2, _sum2);
                                _mm512_storeu_ps(outptr0 + 16 * 3, _sum3);
                                outptr0 += 64;
                            }
                            if (out_elempack == 8)
                            {
                                _mm256_storeu_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 0));
                                _mm256_storeu_ps(outptr0 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 0));

                                _mm256_storeu_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum0, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 1));

                                outptr0 += 32;
                            }
                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _mm512_extractf32x4_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 0));
                                _mm_storeu_ps(outptr0 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 0));
                                _mm_storeu_ps(outptr0 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 0));

                                _mm_storeu_ps(outptr0 + N * 4, _mm512_extractf32x4_ps(_sum0, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 1));

                                _mm_storeu_ps(outptr0 + N * 8, _mm512_extractf32x4_ps(_sum0, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 2));

                                _mm_storeu_ps(outptr0 + N * 12, _mm512_extractf32x4_ps(_sum0, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 3));

                                outptr0 += 16;
                            }
                            if (out_elempack == 1)
                            {
                                __m128 _sum0_0 = _mm512_extractf32x4_ps(_sum0, 0);
                                __m128 _sum1_0 = _mm512_extractf32x4_ps(_sum1, 0);
                                __m128 _sum2_0 = _mm512_extractf32x4_ps(_sum2, 0);
                                __m128 _sum3_0 = _mm512_extractf32x4_ps(_sum3, 0);
                                __m128 _sum0_1 = _mm512_extractf32x4_ps(_sum0, 1);
                                __m128 _sum1_1 = _mm512_extractf32x4_ps(_sum1, 1);
                                __m128 _sum2_1 = _mm512_extractf32x4_ps(_sum2, 1);
                                __m128 _sum3_1 = _mm512_extractf32x4_ps(_sum3, 1);
                                __m128 _sum0_2 = _mm512_extractf32x4_ps(_sum0, 2);
                                __m128 _sum1_2 = _mm512_extractf32x4_ps(_sum1, 2);
                                __m128 _sum2_2 = _mm512_extractf32x4_ps(_sum2, 2);
                                __m128 _sum3_2 = _mm512_extractf32x4_ps(_sum3, 2);
                                __m128 _sum0_3 = _mm512_extractf32x4_ps(_sum0, 3);
                                __m128 _sum1_3 = _mm512_extractf32x4_ps(_sum1, 3);
                                __m128 _sum2_3 = _mm512_extractf32x4_ps(_sum2, 3);
                                __m128 _sum3_3 = _mm512_extractf32x4_ps(_sum3, 3);

                                _MM_TRANSPOSE4_PS(_sum0_0, _sum1_0, _sum2_0, _sum3_0);
                                _MM_TRANSPOSE4_PS(_sum0_1, _sum1_1, _sum2_1, _sum3_1);
                                _MM_TRANSPOSE4_PS(_sum0_2, _sum1_2, _sum2_2, _sum3_2);
                                _MM_TRANSPOSE4_PS(_sum0_3, _sum1_3, _sum2_3, _sum3_3);

                                _mm_storeu_ps(outptr0, _sum0_0);
                                _mm_storeu_ps(outptr0 + N * 1, _sum1_0);
                                _mm_storeu_ps(outptr0 + N * 2, _sum2_0);
                                _mm_storeu_ps(outptr0 + N * 3, _sum3_0);
                                _mm_storeu_ps(outptr0 + N * 4, _sum0_1);
                                _mm_storeu_ps(outptr0 + N * 5, _sum1_1);
                                _mm_storeu_ps(outptr0 + N * 6, _sum2_1);
                                _mm_storeu_ps(outptr0 + N * 7, _sum3_1);
                                _mm_storeu_ps(outptr0 + N * 8, _sum0_2);
                                _mm_storeu_ps(outptr0 + N * 9, _sum1_2);
                                _mm_storeu_ps(outptr0 + N * 10, _sum2_2);
                                _mm_storeu_ps(outptr0 + N * 11, _sum3_2);
                                _mm_storeu_ps(outptr0 + N * 12, _sum0_3);
                                _mm_storeu_ps(outptr0 + N * 13, _sum1_3);
                                _mm_storeu_ps(outptr0 + N * 14, _sum2_3);
                                _mm_storeu_ps(outptr0 + N * 15, _sum3_3);

                                outptr0 += 4;
                            }
                        }
                        else
                        {
                            _mm512_storeu_ps(ptmp, _sum0);
                            _mm512_storeu_ps(ptmp + 16 * 1, _sum1);
                            _mm512_storeu_ps(ptmp + 16 * 2, _sum2);
                            _mm512_storeu_ps(ptmp + 16 * 3, _sum3);
                        }

                        ptmp += 64;
                    }
                    for (; jj + 1 < max_jj; jj += 2)
                    {
                        __m512 _sum0;
                        __m512 _sum1;

                        if (k == 0)
                        {
                            _sum0 = _mm512_setzero_ps();
                            _sum1 = _mm512_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm512_set1_ps(pC[0]);
                                    _sum1 = _mm512_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm512_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 16)
                                    {
                                        _sum0 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16);
                                    }
                                    if (out_elempack == 8)
                                    {
                                        __m256 _sum0_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m256 _sum1_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8);
                                        __m256 _sum0_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack);
                                        __m256 _sum1_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 8);
                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum0_0), _sum0_1, 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum1_0), _sum1_1, 1);
                                    }
                                    if (out_elempack == 4)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum0_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum0_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack + 4);
                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_2), _sum0_3, 1), 1);
                                        _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_2), _sum1_3, 1), 1);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        float sum0[16];
                                        float sum1[16];
                                        sum0[0] = pC[(i + ii + 0) * N + j + jj];
                                        sum0[1] = pC[(i + ii + 1) * N + j + jj];
                                        sum0[2] = pC[(i + ii + 2) * N + j + jj];
                                        sum0[3] = pC[(i + ii + 3) * N + j + jj];
                                        sum0[4] = pC[(i + ii + 4) * N + j + jj];
                                        sum0[5] = pC[(i + ii + 5) * N + j + jj];
                                        sum0[6] = pC[(i + ii + 6) * N + j + jj];
                                        sum0[7] = pC[(i + ii + 7) * N + j + jj];
                                        sum0[8] = pC[(i + ii + 8) * N + j + jj];
                                        sum0[9] = pC[(i + ii + 9) * N + j + jj];
                                        sum0[10] = pC[(i + ii + 10) * N + j + jj];
                                        sum0[11] = pC[(i + ii + 11) * N + j + jj];
                                        sum0[12] = pC[(i + ii + 12) * N + j + jj];
                                        sum0[13] = pC[(i + ii + 13) * N + j + jj];
                                        sum0[14] = pC[(i + ii + 14) * N + j + jj];
                                        sum0[15] = pC[(i + ii + 15) * N + j + jj];
                                        sum1[0] = pC[(i + ii + 0) * N + j + jj + 1];
                                        sum1[1] = pC[(i + ii + 1) * N + j + jj + 1];
                                        sum1[2] = pC[(i + ii + 2) * N + j + jj + 1];
                                        sum1[3] = pC[(i + ii + 3) * N + j + jj + 1];
                                        sum1[4] = pC[(i + ii + 4) * N + j + jj + 1];
                                        sum1[5] = pC[(i + ii + 5) * N + j + jj + 1];
                                        sum1[6] = pC[(i + ii + 6) * N + j + jj + 1];
                                        sum1[7] = pC[(i + ii + 7) * N + j + jj + 1];
                                        sum1[8] = pC[(i + ii + 8) * N + j + jj + 1];
                                        sum1[9] = pC[(i + ii + 9) * N + j + jj + 1];
                                        sum1[10] = pC[(i + ii + 10) * N + j + jj + 1];
                                        sum1[11] = pC[(i + ii + 11) * N + j + jj + 1];
                                        sum1[12] = pC[(i + ii + 12) * N + j + jj + 1];
                                        sum1[13] = pC[(i + ii + 13) * N + j + jj + 1];
                                        sum1[14] = pC[(i + ii + 14) * N + j + jj + 1];
                                        sum1[15] = pC[(i + ii + 15) * N + j + jj + 1];

                                        _sum0 = _mm512_loadu_ps(sum0);
                                        _sum1 = _mm512_loadu_ps(sum1);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm512_set1_ps(pC[j + jj]);
                                    _sum1 = _mm512_set1_ps(pC[j + jj + 1]);
                                }

                                __m512 _beta = _mm512_set1_ps(beta);
                                _sum0 = _mm512_mul_ps(_sum0, _beta);
                                _sum1 = _mm512_mul_ps(_sum1, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm512_loadu_ps(ptmp);
                            _sum1 = _mm512_loadu_ps(ptmp + 16 * 1);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m512 _pA = _mm512_loadu_ps(pA);

                            _sum0 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[1]), _sum1);

                            pA += 16;
                            pB += 2;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m512 _alpha = _mm512_set1_ps(alpha);
                            _sum0 = _mm512_mul_ps(_sum0, _alpha);
                            _sum1 = _mm512_mul_ps(_sum1, _alpha);

                            if (out_elempack == 16)
                            {
                                _mm512_storeu_ps(outptr0, _sum0);
                                _mm512_storeu_ps(outptr0 + 16 * 1, _sum1);
                                outptr0 += 32;
                            }
                            if (out_elempack == 8)
                            {
                                _mm256_storeu_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                                _mm256_storeu_ps(outptr0 + 8, _mm512_extractf32x8_ps(_sum1, 0));

                                _mm256_storeu_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum0, 1));
                                _mm256_storeu_ps(outptr0 + N * 8 + 8, _mm512_extractf32x8_ps(_sum1, 1));
                                outptr0 += 16;
                            }
                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _mm512_extractf32x4_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + 4, _mm512_extractf32x4_ps(_sum1, 0));

                                _mm_storeu_ps(outptr0 + N * 4, _mm512_extractf32x4_ps(_sum0, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4, _mm512_extractf32x4_ps(_sum1, 1));

                                _mm_storeu_ps(outptr0 + N * 8, _mm512_extractf32x4_ps(_sum0, 2));
                                _mm_storeu_ps(outptr0 + N * 8 + 4, _mm512_extractf32x4_ps(_sum1, 2));

                                _mm_storeu_ps(outptr0 + N * 12, _mm512_extractf32x4_ps(_sum0, 3));
                                _mm_storeu_ps(outptr0 + N * 12 + 4, _mm512_extractf32x4_ps(_sum1, 3));
                                outptr0 += 8;
                            }
                            if (out_elempack == 1)
                            {
                                float sum0[16];
                                float sum1[16];
                                _mm512_storeu_ps(sum0, _sum0);
                                _mm512_storeu_ps(sum1, _sum1);

                                outptr0[0] = sum0[0];
                                outptr0[N] = sum0[1];
                                outptr0[N * 2] = sum0[2];
                                outptr0[N * 3] = sum0[3];
                                outptr0[N * 4] = sum0[4];
                                outptr0[N * 5] = sum0[5];
                                outptr0[N * 6] = sum0[6];
                                outptr0[N * 7] = sum0[7];
                                outptr0[N * 8] = sum0[8];
                                outptr0[N * 9] = sum0[9];
                                outptr0[N * 10] = sum0[10];
                                outptr0[N * 11] = sum0[11];
                                outptr0[N * 12] = sum0[12];
                                outptr0[N * 13] = sum0[13];
                                outptr0[N * 14] = sum0[14];
                                outptr0[N * 15] = sum0[15];

                                outptr0[1] = sum1[0];
                                outptr0[N + 1] = sum1[1];
                                outptr0[N * 2 + 1] = sum1[2];
                                outptr0[N * 3 + 1] = sum1[3];
                                outptr0[N * 4 + 1] = sum1[4];
                                outptr0[N * 5 + 1] = sum1[5];
                                outptr0[N * 6 + 1] = sum1[6];
                                outptr0[N * 7 + 1] = sum1[7];
                                outptr0[N * 8 + 1] = sum1[8];
                                outptr0[N * 9 + 1] = sum1[9];
                                outptr0[N * 10 + 1] = sum1[10];
                                outptr0[N * 11 + 1] = sum1[11];
                                outptr0[N * 12 + 1] = sum1[12];
                                outptr0[N * 13 + 1] = sum1[13];
                                outptr0[N * 14 + 1] = sum1[14];
                                outptr0[N * 15 + 1] = sum1[15];
                                outptr0 += 2;
                            }
                        }
                        else
                        {
                            _mm512_storeu_ps(ptmp, _sum0);
                            _mm512_storeu_ps(ptmp + 16 * 1, _sum1);
                        }

                        ptmp += 32;
                    }
                    for (; jj < max_jj; jj += 1)
                    {
                        __m512 _sum0;

                        if (k == 0)
                        {
                            _sum0 = _mm512_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm512_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm512_loadu_ps(pC + i + ii);
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 16)
                                    {
                                        _sum0 = _mm512_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                    }
                                    if (out_elempack == 8)
                                    {
                                        __m256 _sum0_0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m256 _sum0_1 = _mm256_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack);
                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum0_0), _sum0_1, 1);
                                    }
                                    if (out_elempack == 4)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack);
                                        __m128 _sum0_2 = _mm_loadu_ps(pC + (i + ii + 8) * N + (j + jj) * out_elempack);
                                        __m128 _sum0_3 = _mm_loadu_ps(pC + (i + ii + 12) * N + (j + jj) * out_elempack);
                                        _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_2), _sum0_3, 1), 1);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        float sum0[16];
                                        sum0[0] = pC[(i + ii + 0) * N + j + jj];
                                        sum0[1] = pC[(i + ii + 1) * N + j + jj];
                                        sum0[2] = pC[(i + ii + 2) * N + j + jj];
                                        sum0[3] = pC[(i + ii + 3) * N + j + jj];
                                        sum0[4] = pC[(i + ii + 4) * N + j + jj];
                                        sum0[5] = pC[(i + ii + 5) * N + j + jj];
                                        sum0[6] = pC[(i + ii + 6) * N + j + jj];
                                        sum0[7] = pC[(i + ii + 7) * N + j + jj];
                                        sum0[8] = pC[(i + ii + 8) * N + j + jj];
                                        sum0[9] = pC[(i + ii + 9) * N + j + jj];
                                        sum0[10] = pC[(i + ii + 10) * N + j + jj];
                                        sum0[11] = pC[(i + ii + 11) * N + j + jj];
                                        sum0[12] = pC[(i + ii + 12) * N + j + jj];
                                        sum0[13] = pC[(i + ii + 13) * N + j + jj];
                                        sum0[14] = pC[(i + ii + 14) * N + j + jj];
                                        sum0[15] = pC[(i + ii + 15) * N + j + jj];

                                        _sum0 = _mm512_loadu_ps(sum0);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm512_set1_ps(pC[j + jj]);
                                }

                                __m512 _beta = _mm512_set1_ps(beta);
                                _sum0 = _mm512_mul_ps(_sum0, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm512_loadu_ps(ptmp);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m512 _pA = _mm512_loadu_ps(pA);

                            _sum0 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[0]), _sum0);

                            pA += 16;
                            pB += 1;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m512 _alpha = _mm512_set1_ps(alpha);
                            _sum0 = _mm512_mul_ps(_sum0, _alpha);

                            if (out_elempack == 16)
                            {
                                _mm512_storeu_ps(outptr0, _sum0);
                                outptr0 += 16;
                            }
                            if (out_elempack == 8)
                            {
                                _mm256_storeu_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                                _mm256_storeu_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum0, 1));
                                outptr0 += 8;
                            }
                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _mm512_extractf32x4_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + N * 4, _mm512_extractf32x4_ps(_sum0, 1));
                                _mm_storeu_ps(outptr0 + N * 8, _mm512_extractf32x4_ps(_sum0, 2));
                                _mm_storeu_ps(outptr0 + N * 12, _mm512_extractf32x4_ps(_sum0, 3));
                                outptr0 += 4;
                            }
                            if (out_elempack == 1)
                            {
                                float sum0[16];
                                _mm512_storeu_ps(sum0, _sum0);

                                outptr0[0] = sum0[0];
                                outptr0[N * 1] = sum0[1];
                                outptr0[N * 2] = sum0[2];
                                outptr0[N * 3] = sum0[3];
                                outptr0[N * 4] = sum0[4];
                                outptr0[N * 5] = sum0[5];
                                outptr0[N * 6] = sum0[6];
                                outptr0[N * 7] = sum0[7];
                                outptr0[N * 8] = sum0[8];
                                outptr0[N * 9] = sum0[9];
                                outptr0[N * 10] = sum0[10];
                                outptr0[N * 11] = sum0[11];
                                outptr0[N * 12] = sum0[12];
                                outptr0[N * 13] = sum0[13];
                                outptr0[N * 14] = sum0[14];
                                outptr0[N * 15] = sum0[15];
                                outptr0++;
                            }
                        }
                        else
                        {
                            _mm512_storeu_ps(ptmp, _sum0);
                        }

                        ptmp += 16;
                    }

                    pA0 += max_kk * 16;
                }
#endif // __AVX512F__
                for (; ii + 7 < max_ii; ii += 8)
                {
                    float* outptr0 = top_blob.row((i + ii) / out_elempack) + j * out_elempack;

                    const float* pB = pB0;

                    int jj = 0;
                    for (; jj + 11 < max_jj; jj += 12)
                    {
                        __m256 _sum0;
                        __m256 _sum1;
                        __m256 _sum2;
                        __m256 _sum3;
                        __m256 _sum4;
                        __m256 _sum5;
                        __m256 _sum6;
                        __m256 _sum7;
                        __m256 _sum8;
                        __m256 _sum9;
                        __m256 _suma;
                        __m256 _sumb;

                        if (k == 0)
                        {
                            _sum0 = _mm256_setzero_ps();
                            _sum1 = _mm256_setzero_ps();
                            _sum2 = _mm256_setzero_ps();
                            _sum3 = _mm256_setzero_ps();
                            _sum4 = _mm256_setzero_ps();
                            _sum5 = _mm256_setzero_ps();
                            _sum6 = _mm256_setzero_ps();
                            _sum7 = _mm256_setzero_ps();
                            _sum8 = _mm256_setzero_ps();
                            _sum9 = _mm256_setzero_ps();
                            _suma = _mm256_setzero_ps();
                            _sumb = _mm256_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm256_set1_ps(pC[0]);
                                    _sum1 = _mm256_set1_ps(pC[0]);
                                    _sum2 = _mm256_set1_ps(pC[0]);
                                    _sum3 = _mm256_set1_ps(pC[0]);
                                    _sum4 = _mm256_set1_ps(pC[0]);
                                    _sum5 = _mm256_set1_ps(pC[0]);
                                    _sum6 = _mm256_set1_ps(pC[0]);
                                    _sum7 = _mm256_set1_ps(pC[0]);
                                    _sum8 = _mm256_set1_ps(pC[0]);
                                    _sum9 = _mm256_set1_ps(pC[0]);
                                    _suma = _mm256_set1_ps(pC[0]);
                                    _sumb = _mm256_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm256_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                    _sum2 = _sum0;
                                    _sum3 = _sum0;
                                    _sum4 = _sum0;
                                    _sum5 = _sum0;
                                    _sum6 = _sum0;
                                    _sum7 = _sum0;
                                    _sum8 = _sum0;
                                    _sum9 = _sum0;
                                    _suma = _sum0;
                                    _sumb = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 8)
                                    {
                                        _sum0 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8);
                                        _sum2 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 2);
                                        _sum3 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 3);
                                        _sum4 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 4);
                                        _sum5 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 5);
                                        _sum6 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 6);
                                        _sum7 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 7);
                                        _sum8 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 8);
                                        _sum9 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 9);
                                        _suma = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 10);
                                        _sumb = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 11);
                                    }
                                    if (out_elempack == 4)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 7);
                                        __m128 _sum8_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 8);
                                        __m128 _sum9_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 9);
                                        __m128 _suma_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 10);
                                        __m128 _sumb_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 11);
                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 7);
                                        __m128 _sum8_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 8);
                                        __m128 _sum9_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 9);
                                        __m128 _suma_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 10);
                                        __m128 _sumb_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 11);
                                        _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1);
                                        _sum1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1);
                                        _sum2 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum2_1, 1);
                                        _sum3 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum3_1, 1);
                                        _sum4 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum4_0), _sum4_1, 1);
                                        _sum5 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum5_0), _sum5_1, 1);
                                        _sum6 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum6_0), _sum6_1, 1);
                                        _sum7 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum7_0), _sum7_1, 1);
                                        _sum8 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum8_0), _sum8_1, 1);
                                        _sum9 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum9_0), _sum9_1, 1);
                                        _suma = _mm256_insertf128_ps(_mm256_castps128_ps256(_suma_0), _suma_1, 1);
                                        _sumb = _mm256_insertf128_ps(_mm256_castps128_ps256(_sumb_0), _sumb_1, 1);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        _sum0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + j + jj);
                                        _sum1 = _mm256_loadu_ps(pC + (i + ii + 1) * N + j + jj);
                                        _sum2 = _mm256_loadu_ps(pC + (i + ii + 2) * N + j + jj);
                                        _sum3 = _mm256_loadu_ps(pC + (i + ii + 3) * N + j + jj);
                                        _sum4 = _mm256_loadu_ps(pC + (i + ii + 4) * N + j + jj);
                                        _sum5 = _mm256_loadu_ps(pC + (i + ii + 5) * N + j + jj);
                                        _sum6 = _mm256_loadu_ps(pC + (i + ii + 6) * N + j + jj);
                                        _sum7 = _mm256_loadu_ps(pC + (i + ii + 7) * N + j + jj);

                                        transpose8x8_ps(_sum0, _sum1, _sum2, _sum3, _sum4, _sum5, _sum6, _sum7);

                                        __m128 _sum0_8 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj + 8);
                                        __m128 _sum1_8 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj + 8);
                                        __m128 _sum2_8 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj + 8);
                                        __m128 _sum3_8 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj + 8);
                                        __m128 _sum4_8 = _mm_loadu_ps(pC + (i + ii + 4) * N + j + jj + 8);
                                        __m128 _sum5_8 = _mm_loadu_ps(pC + (i + ii + 5) * N + j + jj + 8);
                                        __m128 _sum6_8 = _mm_loadu_ps(pC + (i + ii + 6) * N + j + jj + 8);
                                        __m128 _sum7_8 = _mm_loadu_ps(pC + (i + ii + 7) * N + j + jj + 8);

                                        _MM_TRANSPOSE4_PS(_sum0_8, _sum1_8, _sum2_8, _sum3_8);
                                        _MM_TRANSPOSE4_PS(_sum4_8, _sum5_8, _sum6_8, _sum7_8);

                                        _sum8 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_8), _sum4_8, 1);
                                        _sum9 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_8), _sum5_8, 1);
                                        _suma = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_8), _sum6_8, 1);
                                        _sumb = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_8), _sum7_8, 1);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm256_set1_ps(pC[j + jj]);
                                    _sum1 = _mm256_set1_ps(pC[j + jj + 1]);
                                    _sum2 = _mm256_set1_ps(pC[j + jj + 2]);
                                    _sum3 = _mm256_set1_ps(pC[j + jj + 3]);
                                    _sum4 = _mm256_set1_ps(pC[j + jj + 4]);
                                    _sum5 = _mm256_set1_ps(pC[j + jj + 5]);
                                    _sum6 = _mm256_set1_ps(pC[j + jj + 6]);
                                    _sum7 = _mm256_set1_ps(pC[j + jj + 7]);
                                    _sum8 = _mm256_set1_ps(pC[j + jj + 8]);
                                    _sum9 = _mm256_set1_ps(pC[j + jj + 9]);
                                    _suma = _mm256_set1_ps(pC[j + jj + 10]);
                                    _sumb = _mm256_set1_ps(pC[j + jj + 11]);
                                }

                                __m256 _beta = _mm256_set1_ps(beta);
                                _sum0 = _mm256_mul_ps(_sum0, _beta);
                                _sum1 = _mm256_mul_ps(_sum1, _beta);
                                _sum2 = _mm256_mul_ps(_sum2, _beta);
                                _sum3 = _mm256_mul_ps(_sum3, _beta);
                                _sum4 = _mm256_mul_ps(_sum4, _beta);
                                _sum5 = _mm256_mul_ps(_sum5, _beta);
                                _sum6 = _mm256_mul_ps(_sum6, _beta);
                                _sum7 = _mm256_mul_ps(_sum7, _beta);
                                _sum8 = _mm256_mul_ps(_sum8, _beta);
                                _sum9 = _mm256_mul_ps(_sum9, _beta);
                                _suma = _mm256_mul_ps(_suma, _beta);
                                _sumb = _mm256_mul_ps(_sumb, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm256_load_ps(ptmp);
                            _sum1 = _mm256_load_ps(ptmp + 8 * 1);
                            _sum2 = _mm256_load_ps(ptmp + 8 * 2);
                            _sum3 = _mm256_load_ps(ptmp + 8 * 3);
                            _sum4 = _mm256_load_ps(ptmp + 8 * 4);
                            _sum5 = _mm256_load_ps(ptmp + 8 * 5);
                            _sum6 = _mm256_load_ps(ptmp + 8 * 6);
                            _sum7 = _mm256_load_ps(ptmp + 8 * 7);
                            _sum8 = _mm256_load_ps(ptmp + 8 * 8);
                            _sum9 = _mm256_load_ps(ptmp + 8 * 9);
                            _suma = _mm256_load_ps(ptmp + 8 * 10);
                            _sumb = _mm256_load_ps(ptmp + 8 * 11);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m256 _pA = _mm256_load_ps(pA);

                            _sum0 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[1]), _sum1);
                            _sum2 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[2]), _sum2);
                            _sum3 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[3]), _sum3);
                            _sum4 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[4]), _sum4);
                            _sum5 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[5]), _sum5);
                            _sum6 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[6]), _sum6);
                            _sum7 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[7]), _sum7);
                            _sum8 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[8]), _sum8);
                            _sum9 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[9]), _sum9);
                            _suma = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[10]), _suma);
                            _sumb = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[11]), _sumb);

                            pA += 8;
                            pB += 12;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m256 _alpha = _mm256_set1_ps(alpha);
                            _sum0 = _mm256_mul_ps(_sum0, _alpha);
                            _sum1 = _mm256_mul_ps(_sum1, _alpha);
                            _sum2 = _mm256_mul_ps(_sum2, _alpha);
                            _sum3 = _mm256_mul_ps(_sum3, _alpha);
                            _sum4 = _mm256_mul_ps(_sum4, _alpha);
                            _sum5 = _mm256_mul_ps(_sum5, _alpha);
                            _sum6 = _mm256_mul_ps(_sum6, _alpha);
                            _sum7 = _mm256_mul_ps(_sum7, _alpha);
                            _sum8 = _mm256_mul_ps(_sum8, _alpha);
                            _sum9 = _mm256_mul_ps(_sum9, _alpha);
                            _suma = _mm256_mul_ps(_suma, _alpha);
                            _sumb = _mm256_mul_ps(_sumb, _alpha);

                            if (out_elempack == 8)
                            {
                                _mm256_storeu_ps(outptr0, _sum0);
                                _mm256_storeu_ps(outptr0 + 8 * 1, _sum1);
                                _mm256_storeu_ps(outptr0 + 8 * 2, _sum2);
                                _mm256_storeu_ps(outptr0 + 8 * 3, _sum3);
                                _mm256_storeu_ps(outptr0 + 8 * 4, _sum4);
                                _mm256_storeu_ps(outptr0 + 8 * 5, _sum5);
                                _mm256_storeu_ps(outptr0 + 8 * 6, _sum6);
                                _mm256_storeu_ps(outptr0 + 8 * 7, _sum7);
                                _mm256_storeu_ps(outptr0 + 8 * 8, _sum8);
                                _mm256_storeu_ps(outptr0 + 8 * 9, _sum9);
                                _mm256_storeu_ps(outptr0 + 8 * 10, _suma);
                                _mm256_storeu_ps(outptr0 + 8 * 11, _sumb);
                                outptr0 += 96;
                            }
                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _mm256_extractf128_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + 4 * 1, _mm256_extractf128_ps(_sum1, 0));
                                _mm_storeu_ps(outptr0 + 4 * 2, _mm256_extractf128_ps(_sum2, 0));
                                _mm_storeu_ps(outptr0 + 4 * 3, _mm256_extractf128_ps(_sum3, 0));
                                _mm_storeu_ps(outptr0 + 4 * 4, _mm256_extractf128_ps(_sum4, 0));
                                _mm_storeu_ps(outptr0 + 4 * 5, _mm256_extractf128_ps(_sum5, 0));
                                _mm_storeu_ps(outptr0 + 4 * 6, _mm256_extractf128_ps(_sum6, 0));
                                _mm_storeu_ps(outptr0 + 4 * 7, _mm256_extractf128_ps(_sum7, 0));
                                _mm_storeu_ps(outptr0 + 4 * 8, _mm256_extractf128_ps(_sum8, 0));
                                _mm_storeu_ps(outptr0 + 4 * 9, _mm256_extractf128_ps(_sum9, 0));
                                _mm_storeu_ps(outptr0 + 4 * 10, _mm256_extractf128_ps(_suma, 0));
                                _mm_storeu_ps(outptr0 + 4 * 11, _mm256_extractf128_ps(_sumb, 0));

                                _mm_storeu_ps(outptr0 + N * 4, _mm256_extractf128_ps(_sum0, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 1, _mm256_extractf128_ps(_sum1, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 2, _mm256_extractf128_ps(_sum2, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 3, _mm256_extractf128_ps(_sum3, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 4, _mm256_extractf128_ps(_sum4, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 5, _mm256_extractf128_ps(_sum5, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 6, _mm256_extractf128_ps(_sum6, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 7, _mm256_extractf128_ps(_sum7, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 8, _mm256_extractf128_ps(_sum8, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 9, _mm256_extractf128_ps(_sum9, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 10, _mm256_extractf128_ps(_suma, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 11, _mm256_extractf128_ps(_sumb, 1));

                                outptr0 += 48;
                            }
                            if (out_elempack == 1)
                            {
                                transpose8x8_ps(_sum0, _sum1, _sum2, _sum3, _sum4, _sum5, _sum6, _sum7);

                                _mm256_storeu_ps(outptr0, _sum0);
                                _mm256_storeu_ps(outptr0 + N * 1, _sum1);
                                _mm256_storeu_ps(outptr0 + N * 2, _sum2);
                                _mm256_storeu_ps(outptr0 + N * 3, _sum3);
                                _mm256_storeu_ps(outptr0 + N * 4, _sum4);
                                _mm256_storeu_ps(outptr0 + N * 5, _sum5);
                                _mm256_storeu_ps(outptr0 + N * 6, _sum6);
                                _mm256_storeu_ps(outptr0 + N * 7, _sum7);

                                __m128 _sum8_0 = _mm256_extractf128_ps(_sum8, 0);
                                __m128 _sum9_0 = _mm256_extractf128_ps(_sum9, 0);
                                __m128 _suma_0 = _mm256_extractf128_ps(_suma, 0);
                                __m128 _sumb_0 = _mm256_extractf128_ps(_sumb, 0);
                                __m128 _sum8_1 = _mm256_extractf128_ps(_sum8, 1);
                                __m128 _sum9_1 = _mm256_extractf128_ps(_sum9, 1);
                                __m128 _suma_1 = _mm256_extractf128_ps(_suma, 1);
                                __m128 _sumb_1 = _mm256_extractf128_ps(_sumb, 1);

                                _MM_TRANSPOSE4_PS(_sum8_0, _sum9_0, _suma_0, _sumb_0);
                                _MM_TRANSPOSE4_PS(_sum8_1, _sum9_1, _suma_1, _sumb_1);

                                _mm_storeu_ps(outptr0 + 8, _sum8_0);
                                _mm_storeu_ps(outptr0 + N * 1 + 8, _sum9_0);
                                _mm_storeu_ps(outptr0 + N * 2 + 8, _suma_0);
                                _mm_storeu_ps(outptr0 + N * 3 + 8, _sumb_0);
                                _mm_storeu_ps(outptr0 + N * 4 + 8, _sum8_1);
                                _mm_storeu_ps(outptr0 + N * 5 + 8, _sum9_1);
                                _mm_storeu_ps(outptr0 + N * 6 + 8, _suma_1);
                                _mm_storeu_ps(outptr0 + N * 7 + 8, _sumb_1);

                                outptr0 += 12;
                            }
                        }
                        else
                        {
                            _mm256_store_ps(ptmp, _sum0);
                            _mm256_store_ps(ptmp + 8 * 1, _sum1);
                            _mm256_store_ps(ptmp + 8 * 2, _sum2);
                            _mm256_store_ps(ptmp + 8 * 3, _sum3);
                            _mm256_store_ps(ptmp + 8 * 4, _sum4);
                            _mm256_store_ps(ptmp + 8 * 5, _sum5);
                            _mm256_store_ps(ptmp + 8 * 6, _sum6);
                            _mm256_store_ps(ptmp + 8 * 7, _sum7);
                            _mm256_store_ps(ptmp + 8 * 8, _sum8);
                            _mm256_store_ps(ptmp + 8 * 9, _sum9);
                            _mm256_store_ps(ptmp + 8 * 10, _suma);
                            _mm256_store_ps(ptmp + 8 * 11, _sumb);
                        }

                        ptmp += 96;
                    }
                    for (; jj + 7 < max_jj; jj += 8)
                    {
                        __m256 _sum0;
                        __m256 _sum1;
                        __m256 _sum2;
                        __m256 _sum3;
                        __m256 _sum4;
                        __m256 _sum5;
                        __m256 _sum6;
                        __m256 _sum7;

                        if (k == 0)
                        {
                            _sum0 = _mm256_setzero_ps();
                            _sum1 = _mm256_setzero_ps();
                            _sum2 = _mm256_setzero_ps();
                            _sum3 = _mm256_setzero_ps();
                            _sum4 = _mm256_setzero_ps();
                            _sum5 = _mm256_setzero_ps();
                            _sum6 = _mm256_setzero_ps();
                            _sum7 = _mm256_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm256_set1_ps(pC[0]);
                                    _sum1 = _mm256_set1_ps(pC[0]);
                                    _sum2 = _mm256_set1_ps(pC[0]);
                                    _sum3 = _mm256_set1_ps(pC[0]);
                                    _sum4 = _mm256_set1_ps(pC[0]);
                                    _sum5 = _mm256_set1_ps(pC[0]);
                                    _sum6 = _mm256_set1_ps(pC[0]);
                                    _sum7 = _mm256_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm256_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                    _sum2 = _sum0;
                                    _sum3 = _sum0;
                                    _sum4 = _sum0;
                                    _sum5 = _sum0;
                                    _sum6 = _sum0;
                                    _sum7 = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 8)
                                    {
                                        _sum0 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8);
                                        _sum2 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 2);
                                        _sum3 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 3);
                                        _sum4 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 4);
                                        _sum5 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 5);
                                        _sum6 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 6);
                                        _sum7 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8 * 7);
                                    }
                                    if (out_elempack == 4)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4 * 7);
                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 2);
                                        __m128 _sum3_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 3);
                                        __m128 _sum4_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 4);
                                        __m128 _sum5_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 5);
                                        __m128 _sum6_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 6);
                                        __m128 _sum7_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4 * 7);
                                        _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1);
                                        _sum1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1);
                                        _sum2 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum2_1, 1);
                                        _sum3 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum3_1, 1);
                                        _sum4 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum4_0), _sum4_1, 1);
                                        _sum5 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum5_0), _sum5_1, 1);
                                        _sum6 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum6_0), _sum6_1, 1);
                                        _sum7 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum7_0), _sum7_1, 1);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        _sum0 = _mm256_loadu_ps(pC + (i + ii + 0) * N + j + jj);
                                        _sum1 = _mm256_loadu_ps(pC + (i + ii + 1) * N + j + jj);
                                        _sum2 = _mm256_loadu_ps(pC + (i + ii + 2) * N + j + jj);
                                        _sum3 = _mm256_loadu_ps(pC + (i + ii + 3) * N + j + jj);
                                        _sum4 = _mm256_loadu_ps(pC + (i + ii + 4) * N + j + jj);
                                        _sum5 = _mm256_loadu_ps(pC + (i + ii + 5) * N + j + jj);
                                        _sum6 = _mm256_loadu_ps(pC + (i + ii + 6) * N + j + jj);
                                        _sum7 = _mm256_loadu_ps(pC + (i + ii + 7) * N + j + jj);

                                        transpose8x8_ps(_sum0, _sum1, _sum2, _sum3, _sum4, _sum5, _sum6, _sum7);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm256_set1_ps(pC[j + jj]);
                                    _sum1 = _mm256_set1_ps(pC[j + jj + 1]);
                                    _sum2 = _mm256_set1_ps(pC[j + jj + 2]);
                                    _sum3 = _mm256_set1_ps(pC[j + jj + 3]);
                                    _sum4 = _mm256_set1_ps(pC[j + jj + 4]);
                                    _sum5 = _mm256_set1_ps(pC[j + jj + 5]);
                                    _sum6 = _mm256_set1_ps(pC[j + jj + 6]);
                                    _sum7 = _mm256_set1_ps(pC[j + jj + 7]);
                                }

                                __m256 _beta = _mm256_set1_ps(beta);
                                _sum0 = _mm256_mul_ps(_sum0, _beta);
                                _sum1 = _mm256_mul_ps(_sum1, _beta);
                                _sum2 = _mm256_mul_ps(_sum2, _beta);
                                _sum3 = _mm256_mul_ps(_sum3, _beta);
                                _sum4 = _mm256_mul_ps(_sum4, _beta);
                                _sum5 = _mm256_mul_ps(_sum5, _beta);
                                _sum6 = _mm256_mul_ps(_sum6, _beta);
                                _sum7 = _mm256_mul_ps(_sum7, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm256_loadu_ps(ptmp);
                            _sum1 = _mm256_loadu_ps(ptmp + 8 * 1);
                            _sum2 = _mm256_loadu_ps(ptmp + 8 * 2);
                            _sum3 = _mm256_loadu_ps(ptmp + 8 * 3);
                            _sum4 = _mm256_loadu_ps(ptmp + 8 * 4);
                            _sum5 = _mm256_loadu_ps(ptmp + 8 * 5);
                            _sum6 = _mm256_loadu_ps(ptmp + 8 * 6);
                            _sum7 = _mm256_loadu_ps(ptmp + 8 * 7);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m256 _pA = _mm256_loadu_ps(pA);

                            _sum0 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[1]), _sum1);
                            _sum2 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[2]), _sum2);
                            _sum3 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[3]), _sum3);
                            _sum4 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[4]), _sum4);
                            _sum5 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[5]), _sum5);
                            _sum6 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[6]), _sum6);
                            _sum7 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[7]), _sum7);

                            pA += 8;
                            pB += 8;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m256 _alpha = _mm256_set1_ps(alpha);
                            _sum0 = _mm256_mul_ps(_sum0, _alpha);
                            _sum1 = _mm256_mul_ps(_sum1, _alpha);
                            _sum2 = _mm256_mul_ps(_sum2, _alpha);
                            _sum3 = _mm256_mul_ps(_sum3, _alpha);
                            _sum4 = _mm256_mul_ps(_sum4, _alpha);
                            _sum5 = _mm256_mul_ps(_sum5, _alpha);
                            _sum6 = _mm256_mul_ps(_sum6, _alpha);
                            _sum7 = _mm256_mul_ps(_sum7, _alpha);

                            if (out_elempack == 8)
                            {
                                _mm256_storeu_ps(outptr0, _sum0);
                                _mm256_storeu_ps(outptr0 + 8 * 1, _sum1);
                                _mm256_storeu_ps(outptr0 + 8 * 2, _sum2);
                                _mm256_storeu_ps(outptr0 + 8 * 3, _sum3);
                                _mm256_storeu_ps(outptr0 + 8 * 4, _sum4);
                                _mm256_storeu_ps(outptr0 + 8 * 5, _sum5);
                                _mm256_storeu_ps(outptr0 + 8 * 6, _sum6);
                                _mm256_storeu_ps(outptr0 + 8 * 7, _sum7);
                                outptr0 += 64;
                            }
                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _mm256_extractf128_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + 4 * 1, _mm256_extractf128_ps(_sum1, 0));
                                _mm_storeu_ps(outptr0 + 4 * 2, _mm256_extractf128_ps(_sum2, 0));
                                _mm_storeu_ps(outptr0 + 4 * 3, _mm256_extractf128_ps(_sum3, 0));
                                _mm_storeu_ps(outptr0 + 4 * 4, _mm256_extractf128_ps(_sum4, 0));
                                _mm_storeu_ps(outptr0 + 4 * 5, _mm256_extractf128_ps(_sum5, 0));
                                _mm_storeu_ps(outptr0 + 4 * 6, _mm256_extractf128_ps(_sum6, 0));
                                _mm_storeu_ps(outptr0 + 4 * 7, _mm256_extractf128_ps(_sum7, 0));

                                _mm_storeu_ps(outptr0 + N * 4, _mm256_extractf128_ps(_sum0, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 1, _mm256_extractf128_ps(_sum1, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 2, _mm256_extractf128_ps(_sum2, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 3, _mm256_extractf128_ps(_sum3, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 4, _mm256_extractf128_ps(_sum4, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 5, _mm256_extractf128_ps(_sum5, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 6, _mm256_extractf128_ps(_sum6, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 7, _mm256_extractf128_ps(_sum7, 1));

                                outptr0 += 32;
                            }
                            if (out_elempack == 1)
                            {
                                transpose8x8_ps(_sum0, _sum1, _sum2, _sum3, _sum4, _sum5, _sum6, _sum7);

                                _mm256_storeu_ps(outptr0, _sum0);
                                _mm256_storeu_ps(outptr0 + N * 1, _sum1);
                                _mm256_storeu_ps(outptr0 + N * 2, _sum2);
                                _mm256_storeu_ps(outptr0 + N * 3, _sum3);
                                _mm256_storeu_ps(outptr0 + N * 4, _sum4);
                                _mm256_storeu_ps(outptr0 + N * 5, _sum5);
                                _mm256_storeu_ps(outptr0 + N * 6, _sum6);
                                _mm256_storeu_ps(outptr0 + N * 7, _sum7);

                                outptr0 += 8;
                            }
                        }
                        else
                        {
                            _mm256_storeu_ps(ptmp, _sum0);
                            _mm256_storeu_ps(ptmp + 8 * 1, _sum1);
                            _mm256_storeu_ps(ptmp + 8 * 2, _sum2);
                            _mm256_storeu_ps(ptmp + 8 * 3, _sum3);
                            _mm256_storeu_ps(ptmp + 8 * 4, _sum4);
                            _mm256_storeu_ps(ptmp + 8 * 5, _sum5);
                            _mm256_storeu_ps(ptmp + 8 * 6, _sum6);
                            _mm256_storeu_ps(ptmp + 8 * 7, _sum7);
                        }

                        ptmp += 64;
                    }
                    for (; jj + 3 < max_jj; jj += 4)
                    {
                        __m256 _sum0;
                        __m256 _sum1;
                        __m256 _sum2;
                        __m256 _sum3;

                        if (k == 0)
                        {
                            _sum0 = _mm256_setzero_ps();
                            _sum1 = _mm256_setzero_ps();
                            _sum2 = _mm256_setzero_ps();
                            _sum3 = _mm256_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm256_set1_ps(pC[0]);
                                    _sum1 = _mm256_set1_ps(pC[0]);
                                    _sum2 = _mm256_set1_ps(pC[0]);
                                    _sum3 = _mm256_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm256_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                    _sum2 = _sum0;
                                    _sum3 = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 8)
                                    {
                                        _sum0 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8);
                                        _sum2 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16);
                                        _sum3 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 24);
                                    }
                                    if (out_elempack == 4)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 8);
                                        __m128 _sum3_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 12);
                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum2_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 8);
                                        __m128 _sum3_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 12);
                                        _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1);
                                        _sum1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1);
                                        _sum2 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum2_1, 1);
                                        _sum3 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum3_1, 1);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj);
                                        __m128 _sum2_0 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj);
                                        __m128 _sum3_0 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj);
                                        __m128 _sum4_0 = _mm_loadu_ps(pC + (i + ii + 4) * N + j + jj);
                                        __m128 _sum5_0 = _mm_loadu_ps(pC + (i + ii + 5) * N + j + jj);
                                        __m128 _sum6_0 = _mm_loadu_ps(pC + (i + ii + 6) * N + j + jj);
                                        __m128 _sum7_0 = _mm_loadu_ps(pC + (i + ii + 7) * N + j + jj);

                                        _MM_TRANSPOSE4_PS(_sum0_0, _sum1_0, _sum2_0, _sum3_0);
                                        _MM_TRANSPOSE4_PS(_sum4_0, _sum5_0, _sum6_0, _sum7_0);

                                        _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum4_0, 1);
                                        _sum1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum5_0, 1);
                                        _sum2 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum6_0, 1);
                                        _sum3 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum7_0, 1);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm256_set1_ps(pC[j + jj]);
                                    _sum1 = _mm256_set1_ps(pC[j + jj + 1]);
                                    _sum2 = _mm256_set1_ps(pC[j + jj + 2]);
                                    _sum3 = _mm256_set1_ps(pC[j + jj + 3]);
                                }

                                __m256 _beta = _mm256_set1_ps(beta);
                                _sum0 = _mm256_mul_ps(_sum0, _beta);
                                _sum1 = _mm256_mul_ps(_sum1, _beta);
                                _sum2 = _mm256_mul_ps(_sum2, _beta);
                                _sum3 = _mm256_mul_ps(_sum3, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm256_loadu_ps(ptmp);
                            _sum1 = _mm256_loadu_ps(ptmp + 8 * 1);
                            _sum2 = _mm256_loadu_ps(ptmp + 8 * 2);
                            _sum3 = _mm256_loadu_ps(ptmp + 8 * 3);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m256 _pA = _mm256_loadu_ps(pA);

                            _sum0 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[1]), _sum1);
                            _sum2 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[2]), _sum2);
                            _sum3 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[3]), _sum3);

                            pA += 8;
                            pB += 4;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m256 _alpha = _mm256_set1_ps(alpha);
                            _sum0 = _mm256_mul_ps(_sum0, _alpha);
                            _sum1 = _mm256_mul_ps(_sum1, _alpha);
                            _sum2 = _mm256_mul_ps(_sum2, _alpha);
                            _sum3 = _mm256_mul_ps(_sum3, _alpha);

                            if (out_elempack == 8)
                            {
                                _mm256_storeu_ps(outptr0, _sum0);
                                _mm256_storeu_ps(outptr0 + 8 * 1, _sum1);
                                _mm256_storeu_ps(outptr0 + 8 * 2, _sum2);
                                _mm256_storeu_ps(outptr0 + 8 * 3, _sum3);
                                outptr0 += 32;
                            }
                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _mm256_extractf128_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + 4 * 1, _mm256_extractf128_ps(_sum1, 0));
                                _mm_storeu_ps(outptr0 + 4 * 2, _mm256_extractf128_ps(_sum2, 0));
                                _mm_storeu_ps(outptr0 + 4 * 3, _mm256_extractf128_ps(_sum3, 0));

                                _mm_storeu_ps(outptr0 + N * 4, _mm256_extractf128_ps(_sum0, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 1, _mm256_extractf128_ps(_sum1, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 2, _mm256_extractf128_ps(_sum2, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4 * 3, _mm256_extractf128_ps(_sum3, 1));

                                outptr0 += 16;
                            }
                            if (out_elempack == 1)
                            {
                                __m128 _sum0_0 = _mm256_extractf128_ps(_sum0, 0);
                                __m128 _sum1_0 = _mm256_extractf128_ps(_sum1, 0);
                                __m128 _sum2_0 = _mm256_extractf128_ps(_sum2, 0);
                                __m128 _sum3_0 = _mm256_extractf128_ps(_sum3, 0);
                                __m128 _sum0_1 = _mm256_extractf128_ps(_sum0, 1);
                                __m128 _sum1_1 = _mm256_extractf128_ps(_sum1, 1);
                                __m128 _sum2_1 = _mm256_extractf128_ps(_sum2, 1);
                                __m128 _sum3_1 = _mm256_extractf128_ps(_sum3, 1);

                                _MM_TRANSPOSE4_PS(_sum0_0, _sum1_0, _sum2_0, _sum3_0);
                                _MM_TRANSPOSE4_PS(_sum0_1, _sum1_1, _sum2_1, _sum3_1);

                                _mm_storeu_ps(outptr0, _sum0_0);
                                _mm_storeu_ps(outptr0 + N * 1, _sum1_0);
                                _mm_storeu_ps(outptr0 + N * 2, _sum2_0);
                                _mm_storeu_ps(outptr0 + N * 3, _sum3_0);
                                _mm_storeu_ps(outptr0 + N * 4, _sum0_1);
                                _mm_storeu_ps(outptr0 + N * 5, _sum1_1);
                                _mm_storeu_ps(outptr0 + N * 6, _sum2_1);
                                _mm_storeu_ps(outptr0 + N * 7, _sum3_1);

                                outptr0 += 4;
                            }
                        }
                        else
                        {
                            _mm256_storeu_ps(ptmp, _sum0);
                            _mm256_storeu_ps(ptmp + 8 * 1, _sum1);
                            _mm256_storeu_ps(ptmp + 8 * 2, _sum2);
                            _mm256_storeu_ps(ptmp + 8 * 3, _sum3);
                        }

                        ptmp += 32;
                    }
                    for (; jj + 1 < max_jj; jj += 2)
                    {
                        __m256 _sum0;
                        __m256 _sum1;

                        if (k == 0)
                        {
                            _sum0 = _mm256_setzero_ps();
                            _sum1 = _mm256_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm256_set1_ps(pC[0]);
                                    _sum1 = _mm256_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm256_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 8)
                                    {
                                        _sum0 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8);
                                    }
                                    if (out_elempack == 4)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack + 4);
                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack);
                                        __m128 _sum1_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack + 4);
                                        _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1);
                                        _sum1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        float sum0[8];
                                        float sum1[8];
                                        sum0[0] = pC[(i + ii + 0) * N + j + jj];
                                        sum0[1] = pC[(i + ii + 1) * N + j + jj];
                                        sum0[2] = pC[(i + ii + 2) * N + j + jj];
                                        sum0[3] = pC[(i + ii + 3) * N + j + jj];
                                        sum0[4] = pC[(i + ii + 4) * N + j + jj];
                                        sum0[5] = pC[(i + ii + 5) * N + j + jj];
                                        sum0[6] = pC[(i + ii + 6) * N + j + jj];
                                        sum0[7] = pC[(i + ii + 7) * N + j + jj];
                                        sum1[0] = pC[(i + ii + 0) * N + j + jj + 1];
                                        sum1[1] = pC[(i + ii + 1) * N + j + jj + 1];
                                        sum1[2] = pC[(i + ii + 2) * N + j + jj + 1];
                                        sum1[3] = pC[(i + ii + 3) * N + j + jj + 1];
                                        sum1[4] = pC[(i + ii + 4) * N + j + jj + 1];
                                        sum1[5] = pC[(i + ii + 5) * N + j + jj + 1];
                                        sum1[6] = pC[(i + ii + 6) * N + j + jj + 1];
                                        sum1[7] = pC[(i + ii + 7) * N + j + jj + 1];

                                        _sum0 = _mm256_loadu_ps(sum0);
                                        _sum1 = _mm256_loadu_ps(sum1);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm256_set1_ps(pC[j + jj]);
                                    _sum1 = _mm256_set1_ps(pC[j + jj + 1]);
                                }

                                __m256 _beta = _mm256_set1_ps(beta);
                                _sum0 = _mm256_mul_ps(_sum0, _beta);
                                _sum1 = _mm256_mul_ps(_sum1, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm256_loadu_ps(ptmp);
                            _sum1 = _mm256_loadu_ps(ptmp + 8 * 1);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m256 _pA = _mm256_loadu_ps(pA);

                            _sum0 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[1]), _sum1);

                            pA += 8;
                            pB += 2;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m256 _alpha = _mm256_set1_ps(alpha);
                            _sum0 = _mm256_mul_ps(_sum0, _alpha);
                            _sum1 = _mm256_mul_ps(_sum1, _alpha);

                            if (out_elempack == 8)
                            {
                                _mm256_storeu_ps(outptr0, _sum0);
                                _mm256_storeu_ps(outptr0 + 8 * 1, _sum1);
                                outptr0 += 16;
                            }
                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _mm256_extractf128_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + 4, _mm256_extractf128_ps(_sum1, 0));

                                _mm_storeu_ps(outptr0 + N * 4, _mm256_extractf128_ps(_sum0, 1));
                                _mm_storeu_ps(outptr0 + N * 4 + 4, _mm256_extractf128_ps(_sum1, 1));
                                outptr0 += 8;
                            }
                            if (out_elempack == 1)
                            {
                                float sum0[8];
                                float sum1[8];
                                _mm256_storeu_ps(sum0, _sum0);
                                _mm256_storeu_ps(sum1, _sum1);

                                outptr0[0] = sum0[0];
                                outptr0[N] = sum0[1];
                                outptr0[N * 2] = sum0[2];
                                outptr0[N * 3] = sum0[3];
                                outptr0[N * 4] = sum0[4];
                                outptr0[N * 5] = sum0[5];
                                outptr0[N * 6] = sum0[6];
                                outptr0[N * 7] = sum0[7];

                                outptr0[1] = sum1[0];
                                outptr0[N + 1] = sum1[1];
                                outptr0[N * 2 + 1] = sum1[2];
                                outptr0[N * 3 + 1] = sum1[3];
                                outptr0[N * 4 + 1] = sum1[4];
                                outptr0[N * 5 + 1] = sum1[5];
                                outptr0[N * 6 + 1] = sum1[6];
                                outptr0[N * 7 + 1] = sum1[7];
                                outptr0 += 2;
                            }
                        }
                        else
                        {
                            _mm256_storeu_ps(ptmp, _sum0);
                            _mm256_storeu_ps(ptmp + 8 * 1, _sum1);
                        }

                        ptmp += 16;
                    }
                    for (; jj < max_jj; jj += 1)
                    {
                        __m256 _sum0;

                        if (k == 0)
                        {
                            _sum0 = _mm256_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm256_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm256_loadu_ps(pC + i + ii);
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 8)
                                    {
                                        _sum0 = _mm256_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                    }
                                    if (out_elempack == 4)
                                    {
                                        __m128 _sum0_0 = _mm_loadu_ps(pC + (i + ii + 0) * N + (j + jj) * out_elempack);
                                        __m128 _sum0_1 = _mm_loadu_ps(pC + (i + ii + 4) * N + (j + jj) * out_elempack);
                                        _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        float sum0[8];
                                        sum0[0] = pC[(i + ii) * N + j + jj];
                                        sum0[1] = pC[(i + ii + 1) * N + j + jj];
                                        sum0[2] = pC[(i + ii + 2) * N + j + jj];
                                        sum0[3] = pC[(i + ii + 3) * N + j + jj];
                                        sum0[4] = pC[(i + ii + 4) * N + j + jj];
                                        sum0[5] = pC[(i + ii + 5) * N + j + jj];
                                        sum0[6] = pC[(i + ii + 6) * N + j + jj];
                                        sum0[7] = pC[(i + ii + 7) * N + j + jj];

                                        _sum0 = _mm256_loadu_ps(sum0);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm256_set1_ps(pC[j + jj]);
                                }

                                __m256 _beta = _mm256_set1_ps(beta);
                                _sum0 = _mm256_mul_ps(_sum0, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm256_loadu_ps(ptmp);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m256 _pA = _mm256_loadu_ps(pA);

                            _sum0 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[0]), _sum0);

                            pA += 8;
                            pB += 1;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m256 _alpha = _mm256_set1_ps(alpha);
                            _sum0 = _mm256_mul_ps(_sum0, _alpha);

                            if (out_elempack == 8)
                            {
                                _mm256_storeu_ps(outptr0, _sum0);
                                outptr0 += 8;
                            }
                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _mm256_extractf128_ps(_sum0, 0));
                                _mm_storeu_ps(outptr0 + N * 4, _mm256_extractf128_ps(_sum0, 1));
                                outptr0 += 4;
                            }
                            if (out_elempack == 1)
                            {
                                float sum0[8];
                                _mm256_storeu_ps(sum0, _sum0);

                                outptr0[0] = sum0[0];
                                outptr0[N * 1] = sum0[1];
                                outptr0[N * 2] = sum0[2];
                                outptr0[N * 3] = sum0[3];
                                outptr0[N * 4] = sum0[4];
                                outptr0[N * 5] = sum0[5];
                                outptr0[N * 6] = sum0[6];
                                outptr0[N * 7] = sum0[7];
                                outptr0++;
                            }
                        }
                        else
                        {
                            _mm256_storeu_ps(ptmp, _sum0);
                        }

                        ptmp += 8;
                    }

                    pA0 += max_kk * 8;
                }
#endif // __AVX__
                for (; ii + 3 < max_ii; ii += 4)
                {
                    float* outptr0 = top_blob.row((i + ii) / out_elempack) + j * out_elempack;

                    const float* pB = pB0;

                    int jj = 0;
                    for (; jj + 11 < max_jj; jj += 12)
                    {
                        __m128 _sum0;
                        __m128 _sum1;
                        __m128 _sum2;
                        __m128 _sum3;
                        __m128 _sum4;
                        __m128 _sum5;
                        __m128 _sum6;
                        __m128 _sum7;
                        __m128 _sum8;
                        __m128 _sum9;
                        __m128 _suma;
                        __m128 _sumb;

                        if (k == 0)
                        {
                            _sum0 = _mm_setzero_ps();
                            _sum1 = _mm_setzero_ps();
                            _sum2 = _mm_setzero_ps();
                            _sum3 = _mm_setzero_ps();
                            _sum4 = _mm_setzero_ps();
                            _sum5 = _mm_setzero_ps();
                            _sum6 = _mm_setzero_ps();
                            _sum7 = _mm_setzero_ps();
                            _sum8 = _mm_setzero_ps();
                            _sum9 = _mm_setzero_ps();
                            _suma = _mm_setzero_ps();
                            _sumb = _mm_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm_set1_ps(pC[0]);
                                    _sum1 = _mm_set1_ps(pC[0]);
                                    _sum2 = _mm_set1_ps(pC[0]);
                                    _sum3 = _mm_set1_ps(pC[0]);
                                    _sum4 = _mm_set1_ps(pC[0]);
                                    _sum5 = _mm_set1_ps(pC[0]);
                                    _sum6 = _mm_set1_ps(pC[0]);
                                    _sum7 = _mm_set1_ps(pC[0]);
                                    _sum8 = _mm_set1_ps(pC[0]);
                                    _sum9 = _mm_set1_ps(pC[0]);
                                    _suma = _mm_set1_ps(pC[0]);
                                    _sumb = _mm_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                    _sum2 = _sum0;
                                    _sum3 = _sum0;
                                    _sum4 = _sum0;
                                    _sum5 = _sum0;
                                    _sum6 = _sum0;
                                    _sum7 = _sum0;
                                    _sum8 = _sum0;
                                    _sum9 = _sum0;
                                    _suma = _sum0;
                                    _sumb = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 4)
                                    {
                                        _sum0 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 4);
                                        _sum2 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8);
                                        _sum3 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 12);
                                        _sum4 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16);
                                        _sum5 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 20);
                                        _sum6 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 24);
                                        _sum7 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 28);
                                        _sum8 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 32);
                                        _sum9 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 36);
                                        _suma = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 40);
                                        _sumb = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 44);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        _sum0 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj);
                                        _sum1 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj);
                                        _sum2 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj);
                                        _sum3 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj);
                                        _sum4 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj + 4);
                                        _sum5 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj + 4);
                                        _sum6 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj + 4);
                                        _sum7 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj + 4);
                                        _sum8 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj + 8);
                                        _sum9 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj + 8);
                                        _suma = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj + 8);
                                        _sumb = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj + 8);

                                        _MM_TRANSPOSE4_PS(_sum0, _sum1, _sum2, _sum3);
                                        _MM_TRANSPOSE4_PS(_sum4, _sum5, _sum6, _sum7);
                                        _MM_TRANSPOSE4_PS(_sum8, _sum9, _suma, _sumb);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm_set1_ps(pC[j + jj]);
                                    _sum1 = _mm_set1_ps(pC[j + jj + 1]);
                                    _sum2 = _mm_set1_ps(pC[j + jj + 2]);
                                    _sum3 = _mm_set1_ps(pC[j + jj + 3]);
                                    _sum4 = _mm_set1_ps(pC[j + jj + 4]);
                                    _sum5 = _mm_set1_ps(pC[j + jj + 5]);
                                    _sum6 = _mm_set1_ps(pC[j + jj + 6]);
                                    _sum7 = _mm_set1_ps(pC[j + jj + 7]);
                                    _sum8 = _mm_set1_ps(pC[j + jj + 8]);
                                    _sum9 = _mm_set1_ps(pC[j + jj + 9]);
                                    _suma = _mm_set1_ps(pC[j + jj + 10]);
                                    _sumb = _mm_set1_ps(pC[j + jj + 11]);
                                }

                                __m128 _beta = _mm_set1_ps(beta);
                                _sum0 = _mm_mul_ps(_sum0, _beta);
                                _sum1 = _mm_mul_ps(_sum1, _beta);
                                _sum2 = _mm_mul_ps(_sum2, _beta);
                                _sum3 = _mm_mul_ps(_sum3, _beta);
                                _sum4 = _mm_mul_ps(_sum4, _beta);
                                _sum5 = _mm_mul_ps(_sum5, _beta);
                                _sum6 = _mm_mul_ps(_sum6, _beta);
                                _sum7 = _mm_mul_ps(_sum7, _beta);
                                _sum8 = _mm_mul_ps(_sum8, _beta);
                                _sum9 = _mm_mul_ps(_sum9, _beta);
                                _suma = _mm_mul_ps(_suma, _beta);
                                _sumb = _mm_mul_ps(_sumb, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm_loadu_ps(ptmp);
                            _sum1 = _mm_loadu_ps(ptmp + 4 * 1);
                            _sum2 = _mm_loadu_ps(ptmp + 4 * 2);
                            _sum3 = _mm_loadu_ps(ptmp + 4 * 3);
                            _sum4 = _mm_loadu_ps(ptmp + 4 * 4);
                            _sum5 = _mm_loadu_ps(ptmp + 4 * 5);
                            _sum6 = _mm_loadu_ps(ptmp + 4 * 6);
                            _sum7 = _mm_loadu_ps(ptmp + 4 * 7);
                            _sum8 = _mm_loadu_ps(ptmp + 4 * 8);
                            _sum9 = _mm_loadu_ps(ptmp + 4 * 9);
                            _suma = _mm_loadu_ps(ptmp + 4 * 10);
                            _sumb = _mm_loadu_ps(ptmp + 4 * 11);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m128 _pA = _mm_loadu_ps(pA);

                            _sum0 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[1]), _sum1);
                            _sum2 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[2]), _sum2);
                            _sum3 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[3]), _sum3);
                            _sum4 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[4]), _sum4);
                            _sum5 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[5]), _sum5);
                            _sum6 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[6]), _sum6);
                            _sum7 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[7]), _sum7);
                            _sum8 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[8]), _sum8);
                            _sum9 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[9]), _sum9);
                            _suma = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[10]), _suma);
                            _sumb = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[11]), _sumb);

                            pA += 4;
                            pB += 12;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m128 _alpha = _mm_set1_ps(alpha);
                            _sum0 = _mm_mul_ps(_sum0, _alpha);
                            _sum1 = _mm_mul_ps(_sum1, _alpha);
                            _sum2 = _mm_mul_ps(_sum2, _alpha);
                            _sum3 = _mm_mul_ps(_sum3, _alpha);
                            _sum4 = _mm_mul_ps(_sum4, _alpha);
                            _sum5 = _mm_mul_ps(_sum5, _alpha);
                            _sum6 = _mm_mul_ps(_sum6, _alpha);
                            _sum7 = _mm_mul_ps(_sum7, _alpha);
                            _sum8 = _mm_mul_ps(_sum8, _alpha);
                            _sum9 = _mm_mul_ps(_sum9, _alpha);
                            _suma = _mm_mul_ps(_suma, _alpha);
                            _sumb = _mm_mul_ps(_sumb, _alpha);

                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _sum0);
                                _mm_storeu_ps(outptr0 + 4 * 1, _sum1);
                                _mm_storeu_ps(outptr0 + 4 * 2, _sum2);
                                _mm_storeu_ps(outptr0 + 4 * 3, _sum3);
                                _mm_storeu_ps(outptr0 + 4 * 4, _sum4);
                                _mm_storeu_ps(outptr0 + 4 * 5, _sum5);
                                _mm_storeu_ps(outptr0 + 4 * 6, _sum6);
                                _mm_storeu_ps(outptr0 + 4 * 7, _sum7);
                                _mm_storeu_ps(outptr0 + 4 * 8, _sum8);
                                _mm_storeu_ps(outptr0 + 4 * 9, _sum9);
                                _mm_storeu_ps(outptr0 + 4 * 10, _suma);
                                _mm_storeu_ps(outptr0 + 4 * 11, _sumb);
                                outptr0 += 48;
                            }
                            if (out_elempack == 1)
                            {
                                _MM_TRANSPOSE4_PS(_sum0, _sum1, _sum2, _sum3);
                                _MM_TRANSPOSE4_PS(_sum4, _sum5, _sum6, _sum7);
                                _MM_TRANSPOSE4_PS(_sum8, _sum9, _suma, _sumb);

                                _mm_storeu_ps(outptr0, _sum0);
                                _mm_storeu_ps(outptr0 + N * 1, _sum1);
                                _mm_storeu_ps(outptr0 + N * 2, _sum2);
                                _mm_storeu_ps(outptr0 + N * 3, _sum3);
                                _mm_storeu_ps(outptr0 + 4, _sum4);
                                _mm_storeu_ps(outptr0 + N * 1 + 4, _sum5);
                                _mm_storeu_ps(outptr0 + N * 2 + 4, _sum6);
                                _mm_storeu_ps(outptr0 + N * 3 + 4, _sum7);
                                _mm_storeu_ps(outptr0 + 8, _sum8);
                                _mm_storeu_ps(outptr0 + N * 1 + 8, _sum9);
                                _mm_storeu_ps(outptr0 + N * 2 + 8, _suma);
                                _mm_storeu_ps(outptr0 + N * 3 + 8, _sumb);
                                outptr0 += 12;
                            }
                        }
                        else
                        {
                            _mm_storeu_ps(ptmp, _sum0);
                            _mm_storeu_ps(ptmp + 4 * 1, _sum1);
                            _mm_storeu_ps(ptmp + 4 * 2, _sum2);
                            _mm_storeu_ps(ptmp + 4 * 3, _sum3);
                            _mm_storeu_ps(ptmp + 4 * 4, _sum4);
                            _mm_storeu_ps(ptmp + 4 * 5, _sum5);
                            _mm_storeu_ps(ptmp + 4 * 6, _sum6);
                            _mm_storeu_ps(ptmp + 4 * 7, _sum7);
                            _mm_storeu_ps(ptmp + 4 * 8, _sum8);
                            _mm_storeu_ps(ptmp + 4 * 9, _sum9);
                            _mm_storeu_ps(ptmp + 4 * 10, _suma);
                            _mm_storeu_ps(ptmp + 4 * 11, _sumb);
                        }

                        ptmp += 48;
                    }
                    for (; jj + 7 < max_jj; jj += 8)
                    {
                        __m128 _sum0;
                        __m128 _sum1;
                        __m128 _sum2;
                        __m128 _sum3;
                        __m128 _sum4;
                        __m128 _sum5;
                        __m128 _sum6;
                        __m128 _sum7;

                        if (k == 0)
                        {
                            _sum0 = _mm_setzero_ps();
                            _sum1 = _mm_setzero_ps();
                            _sum2 = _mm_setzero_ps();
                            _sum3 = _mm_setzero_ps();
                            _sum4 = _mm_setzero_ps();
                            _sum5 = _mm_setzero_ps();
                            _sum6 = _mm_setzero_ps();
                            _sum7 = _mm_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm_set1_ps(pC[0]);
                                    _sum1 = _mm_set1_ps(pC[0]);
                                    _sum2 = _mm_set1_ps(pC[0]);
                                    _sum3 = _mm_set1_ps(pC[0]);
                                    _sum4 = _mm_set1_ps(pC[0]);
                                    _sum5 = _mm_set1_ps(pC[0]);
                                    _sum6 = _mm_set1_ps(pC[0]);
                                    _sum7 = _mm_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                    _sum2 = _sum0;
                                    _sum3 = _sum0;
                                    _sum4 = _sum0;
                                    _sum5 = _sum0;
                                    _sum6 = _sum0;
                                    _sum7 = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 4)
                                    {
                                        _sum0 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 4);
                                        _sum2 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8);
                                        _sum3 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 12);
                                        _sum4 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 16);
                                        _sum5 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 20);
                                        _sum6 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 24);
                                        _sum7 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 28);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        _sum0 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj);
                                        _sum1 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj);
                                        _sum2 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj);
                                        _sum3 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj);
                                        _sum4 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj + 4);
                                        _sum5 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj + 4);
                                        _sum6 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj + 4);
                                        _sum7 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj + 4);

                                        _MM_TRANSPOSE4_PS(_sum0, _sum1, _sum2, _sum3);
                                        _MM_TRANSPOSE4_PS(_sum4, _sum5, _sum6, _sum7);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm_set1_ps(pC[j + jj]);
                                    _sum1 = _mm_set1_ps(pC[j + jj + 1]);
                                    _sum2 = _mm_set1_ps(pC[j + jj + 2]);
                                    _sum3 = _mm_set1_ps(pC[j + jj + 3]);
                                    _sum4 = _mm_set1_ps(pC[j + jj + 4]);
                                    _sum5 = _mm_set1_ps(pC[j + jj + 5]);
                                    _sum6 = _mm_set1_ps(pC[j + jj + 6]);
                                    _sum7 = _mm_set1_ps(pC[j + jj + 7]);
                                }

                                __m128 _beta = _mm_set1_ps(beta);
                                _sum0 = _mm_mul_ps(_sum0, _beta);
                                _sum1 = _mm_mul_ps(_sum1, _beta);
                                _sum2 = _mm_mul_ps(_sum2, _beta);
                                _sum3 = _mm_mul_ps(_sum3, _beta);
                                _sum4 = _mm_mul_ps(_sum4, _beta);
                                _sum5 = _mm_mul_ps(_sum5, _beta);
                                _sum6 = _mm_mul_ps(_sum6, _beta);
                                _sum7 = _mm_mul_ps(_sum7, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm_loadu_ps(ptmp);
                            _sum1 = _mm_loadu_ps(ptmp + 4 * 1);
                            _sum2 = _mm_loadu_ps(ptmp + 4 * 2);
                            _sum3 = _mm_loadu_ps(ptmp + 4 * 3);
                            _sum4 = _mm_loadu_ps(ptmp + 4 * 4);
                            _sum5 = _mm_loadu_ps(ptmp + 4 * 5);
                            _sum6 = _mm_loadu_ps(ptmp + 4 * 6);
                            _sum7 = _mm_loadu_ps(ptmp + 4 * 7);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m128 _pA = _mm_loadu_ps(pA);

                            _sum0 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[1]), _sum1);
                            _sum2 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[2]), _sum2);
                            _sum3 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[3]), _sum3);
                            _sum4 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[4]), _sum4);
                            _sum5 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[5]), _sum5);
                            _sum6 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[6]), _sum6);
                            _sum7 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[7]), _sum7);

                            pA += 4;
                            pB += 8;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m128 _alpha = _mm_set1_ps(alpha);
                            _sum0 = _mm_mul_ps(_sum0, _alpha);
                            _sum1 = _mm_mul_ps(_sum1, _alpha);
                            _sum2 = _mm_mul_ps(_sum2, _alpha);
                            _sum3 = _mm_mul_ps(_sum3, _alpha);
                            _sum4 = _mm_mul_ps(_sum4, _alpha);
                            _sum5 = _mm_mul_ps(_sum5, _alpha);
                            _sum6 = _mm_mul_ps(_sum6, _alpha);
                            _sum7 = _mm_mul_ps(_sum7, _alpha);

                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _sum0);
                                _mm_storeu_ps(outptr0 + 4 * 1, _sum1);
                                _mm_storeu_ps(outptr0 + 4 * 2, _sum2);
                                _mm_storeu_ps(outptr0 + 4 * 3, _sum3);
                                _mm_storeu_ps(outptr0 + 4 * 4, _sum4);
                                _mm_storeu_ps(outptr0 + 4 * 5, _sum5);
                                _mm_storeu_ps(outptr0 + 4 * 6, _sum6);
                                _mm_storeu_ps(outptr0 + 4 * 7, _sum7);
                                outptr0 += 32;
                            }
                            if (out_elempack == 1)
                            {
                                _MM_TRANSPOSE4_PS(_sum0, _sum1, _sum2, _sum3);
                                _MM_TRANSPOSE4_PS(_sum4, _sum5, _sum6, _sum7);

                                _mm_storeu_ps(outptr0, _sum0);
                                _mm_storeu_ps(outptr0 + N * 1, _sum1);
                                _mm_storeu_ps(outptr0 + N * 2, _sum2);
                                _mm_storeu_ps(outptr0 + N * 3, _sum3);
                                _mm_storeu_ps(outptr0 + 4, _sum4);
                                _mm_storeu_ps(outptr0 + N * 1 + 4, _sum5);
                                _mm_storeu_ps(outptr0 + N * 2 + 4, _sum6);
                                _mm_storeu_ps(outptr0 + N * 3 + 4, _sum7);
                                outptr0 += 8;
                            }
                        }
                        else
                        {
                            _mm_storeu_ps(ptmp, _sum0);
                            _mm_storeu_ps(ptmp + 4 * 1, _sum1);
                            _mm_storeu_ps(ptmp + 4 * 2, _sum2);
                            _mm_storeu_ps(ptmp + 4 * 3, _sum3);
                            _mm_storeu_ps(ptmp + 4 * 4, _sum4);
                            _mm_storeu_ps(ptmp + 4 * 5, _sum5);
                            _mm_storeu_ps(ptmp + 4 * 6, _sum6);
                            _mm_storeu_ps(ptmp + 4 * 7, _sum7);
                        }

                        ptmp += 32;
                    }
                    for (; jj + 3 < max_jj; jj += 4)
                    {
                        __m128 _sum0;
                        __m128 _sum1;
                        __m128 _sum2;
                        __m128 _sum3;

                        if (k == 0)
                        {
                            _sum0 = _mm_setzero_ps();
                            _sum1 = _mm_setzero_ps();
                            _sum2 = _mm_setzero_ps();
                            _sum3 = _mm_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm_set1_ps(pC[0]);
                                    _sum1 = _mm_set1_ps(pC[0]);
                                    _sum2 = _mm_set1_ps(pC[0]);
                                    _sum3 = _mm_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                    _sum2 = _sum0;
                                    _sum3 = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 4)
                                    {
                                        _sum0 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 4);
                                        _sum2 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 8);
                                        _sum3 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 12);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        _sum0 = _mm_loadu_ps(pC + (i + ii + 0) * N + j + jj);
                                        _sum1 = _mm_loadu_ps(pC + (i + ii + 1) * N + j + jj);
                                        _sum2 = _mm_loadu_ps(pC + (i + ii + 2) * N + j + jj);
                                        _sum3 = _mm_loadu_ps(pC + (i + ii + 3) * N + j + jj);

                                        _MM_TRANSPOSE4_PS(_sum0, _sum1, _sum2, _sum3);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm_set1_ps(pC[j + jj]);
                                    _sum1 = _mm_set1_ps(pC[j + jj + 1]);
                                    _sum2 = _mm_set1_ps(pC[j + jj + 2]);
                                    _sum3 = _mm_set1_ps(pC[j + jj + 3]);
                                }

                                __m128 _beta = _mm_set1_ps(beta);
                                _sum0 = _mm_mul_ps(_sum0, _beta);
                                _sum1 = _mm_mul_ps(_sum1, _beta);
                                _sum2 = _mm_mul_ps(_sum2, _beta);
                                _sum3 = _mm_mul_ps(_sum3, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm_loadu_ps(ptmp);
                            _sum1 = _mm_loadu_ps(ptmp + 4 * 1);
                            _sum2 = _mm_loadu_ps(ptmp + 4 * 2);
                            _sum3 = _mm_loadu_ps(ptmp + 4 * 3);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m128 _pA = _mm_loadu_ps(pA);

                            _sum0 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[1]), _sum1);
                            _sum2 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[2]), _sum2);
                            _sum3 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[3]), _sum3);

                            pA += 4;
                            pB += 4;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m128 _alpha = _mm_set1_ps(alpha);
                            _sum0 = _mm_mul_ps(_sum0, _alpha);
                            _sum1 = _mm_mul_ps(_sum1, _alpha);
                            _sum2 = _mm_mul_ps(_sum2, _alpha);
                            _sum3 = _mm_mul_ps(_sum3, _alpha);

                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _sum0);
                                _mm_storeu_ps(outptr0 + 4 * 1, _sum1);
                                _mm_storeu_ps(outptr0 + 4 * 2, _sum2);
                                _mm_storeu_ps(outptr0 + 4 * 3, _sum3);
                                outptr0 += 16;
                            }
                            if (out_elempack == 1)
                            {
                                _MM_TRANSPOSE4_PS(_sum0, _sum1, _sum2, _sum3);

                                _mm_storeu_ps(outptr0, _sum0);
                                _mm_storeu_ps(outptr0 + N * 1, _sum1);
                                _mm_storeu_ps(outptr0 + N * 2, _sum2);
                                _mm_storeu_ps(outptr0 + N * 3, _sum3);
                                outptr0 += 4;
                            }
                        }
                        else
                        {
                            _mm_storeu_ps(ptmp, _sum0);
                            _mm_storeu_ps(ptmp + 4 * 1, _sum1);
                            _mm_storeu_ps(ptmp + 4 * 2, _sum2);
                            _mm_storeu_ps(ptmp + 4 * 3, _sum3);
                        }

                        ptmp += 16;
                    }
                    for (; jj + 1 < max_jj; jj += 2)
                    {
                        __m128 _sum0;
                        __m128 _sum1;

                        if (k == 0)
                        {
                            _sum0 = _mm_setzero_ps();
                            _sum1 = _mm_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm_set1_ps(pC[0]);
                                    _sum1 = _mm_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm_loadu_ps(pC + i + ii);
                                    _sum1 = _sum0;
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 4)
                                    {
                                        _sum0 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                        _sum1 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack + 4);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        float sum0[4];
                                        float sum1[4];
                                        sum0[0] = pC[(i + ii + 0) * N + j + jj];
                                        sum0[1] = pC[(i + ii + 1) * N + j + jj];
                                        sum0[2] = pC[(i + ii + 2) * N + j + jj];
                                        sum0[3] = pC[(i + ii + 3) * N + j + jj];
                                        sum1[0] = pC[(i + ii + 0) * N + j + jj + 1];
                                        sum1[1] = pC[(i + ii + 1) * N + j + jj + 1];
                                        sum1[2] = pC[(i + ii + 2) * N + j + jj + 1];
                                        sum1[3] = pC[(i + ii + 3) * N + j + jj + 1];

                                        _sum0 = _mm_loadu_ps(sum0);
                                        _sum1 = _mm_loadu_ps(sum1);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm_set1_ps(pC[j + jj]);
                                    _sum1 = _mm_set1_ps(pC[j + jj + 1]);
                                }

                                __m128 _beta = _mm_set1_ps(beta);
                                _sum0 = _mm_mul_ps(_sum0, _beta);
                                _sum1 = _mm_mul_ps(_sum1, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm_loadu_ps(ptmp);
                            _sum1 = _mm_loadu_ps(ptmp + 4 * 1);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m128 _pA = _mm_loadu_ps(pA);

                            _sum0 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[0]), _sum0);
                            _sum1 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[1]), _sum1);

                            pA += 4;
                            pB += 2;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m128 _alpha = _mm_set1_ps(alpha);
                            _sum0 = _mm_mul_ps(_sum0, _alpha);
                            _sum1 = _mm_mul_ps(_sum1, _alpha);

                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _sum0);
                                _mm_storeu_ps(outptr0 + 4, _sum1);
                                outptr0 += 8;
                            }
                            if (out_elempack == 1)
                            {
                                float sum0[4];
                                float sum1[4];
                                _mm_storeu_ps(sum0, _sum0);
                                _mm_storeu_ps(sum1, _sum1);

                                outptr0[0] = sum0[0];
                                outptr0[N] = sum0[1];
                                outptr0[N * 2] = sum0[2];
                                outptr0[N * 3] = sum0[3];
                                outptr0[1] = sum1[0];
                                outptr0[N + 1] = sum1[1];
                                outptr0[N * 2 + 1] = sum1[2];
                                outptr0[N * 3 + 1] = sum1[3];
                                outptr0 += 2;
                            }
                        }
                        else
                        {
                            _mm_storeu_ps(ptmp, _sum0);
                            _mm_storeu_ps(ptmp + 4 * 1, _sum1);
                        }

                        ptmp += 8;
                    }
                    for (; jj < max_jj; jj += 1)
                    {
                        __m128 _sum0;

                        if (k == 0)
                        {
                            _sum0 = _mm_setzero_ps();

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    _sum0 = _mm_set1_ps(pC[0]);
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    _sum0 = _mm_loadu_ps(pC + i + ii);
                                }
                                if (broadcast_type_C == 3)
                                {
                                    if (out_elempack == 4)
                                    {
                                        _sum0 = _mm_loadu_ps(pC + (i + ii) * N + (j + jj) * out_elempack);
                                    }
                                    if (out_elempack == 1)
                                    {
                                        float sum0[4];
                                        sum0[0] = pC[(i + ii + 0) * N + j + jj];
                                        sum0[1] = pC[(i + ii + 1) * N + j + jj];
                                        sum0[2] = pC[(i + ii + 2) * N + j + jj];
                                        sum0[3] = pC[(i + ii + 3) * N + j + jj];

                                        _sum0 = _mm_loadu_ps(sum0);
                                    }
                                }
                                if (broadcast_type_C == 4)
                                {
                                    _sum0 = _mm_set1_ps(pC[j + jj]);
                                }

                                __m128 _beta = _mm_set1_ps(beta);
                                _sum0 = _mm_mul_ps(_sum0, _beta);
                            }
                        }
                        else
                        {
                            _sum0 = _mm_loadu_ps(ptmp);
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            __m128 _pA = _mm_loadu_ps(pA);

                            _sum0 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[0]), _sum0);

                            pA += 4;
                            pB += 1;
                        }

                        if (k + TILE_K >= K)
                        {
                            __m128 _alpha = _mm_set1_ps(alpha);
                            _sum0 = _mm_mul_ps(_sum0, _alpha);

                            if (out_elempack == 4)
                            {
                                _mm_storeu_ps(outptr0, _sum0);
                                outptr0 += 4;
                            }
                            if (out_elempack == 1)
                            {
                                float sum0[4];
                                _mm_storeu_ps(sum0, _sum0);

                                outptr0[0] = sum0[0];
                                outptr0[N] = sum0[1];
                                outptr0[N * 2] = sum0[2];
                                outptr0[N * 3] = sum0[3];
                                outptr0++;
                            }
                        }
                        else
                        {
                            _mm_storeu_ps(ptmp, _sum0);
                        }

                        ptmp += 4;
                    }

                    pA0 += max_kk * 4;
                }
#endif // __SSE2__
                for (; ii + 1 < max_ii; ii += 2)
                {
                    float* outptr0 = top_blob.row(i + ii) + j;

                    const float* pB = pB0;

                    int jj = 0;
#if __SSE2__
                    for (; jj + 11 < max_jj; jj += 12)
                    {
                        float sum00;
                        float sum01;
                        float sum02;
                        float sum03;
                        float sum04;
                        float sum05;
                        float sum06;
                        float sum07;
                        float sum08;
                        float sum09;
                        float sum0a;
                        float sum0b;
                        float sum10;
                        float sum11;
                        float sum12;
                        float sum13;
                        float sum14;
                        float sum15;
                        float sum16;
                        float sum17;
                        float sum18;
                        float sum19;
                        float sum1a;
                        float sum1b;

                        if (k == 0)
                        {
                            sum00 = 0.f;
                            sum01 = 0.f;
                            sum02 = 0.f;
                            sum03 = 0.f;
                            sum04 = 0.f;
                            sum05 = 0.f;
                            sum06 = 0.f;
                            sum07 = 0.f;
                            sum08 = 0.f;
                            sum09 = 0.f;
                            sum0a = 0.f;
                            sum0b = 0.f;
                            sum10 = 0.f;
                            sum11 = 0.f;
                            sum12 = 0.f;
                            sum13 = 0.f;
                            sum14 = 0.f;
                            sum15 = 0.f;
                            sum16 = 0.f;
                            sum17 = 0.f;
                            sum18 = 0.f;
                            sum19 = 0.f;
                            sum1a = 0.f;
                            sum1b = 0.f;

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    sum00 = pC[0];
                                    sum01 = pC[0];
                                    sum02 = pC[0];
                                    sum03 = pC[0];
                                    sum04 = pC[0];
                                    sum05 = pC[0];
                                    sum06 = pC[0];
                                    sum07 = pC[0];
                                    sum08 = pC[0];
                                    sum09 = pC[0];
                                    sum0a = pC[0];
                                    sum0b = pC[0];
                                    sum10 = pC[0];
                                    sum11 = pC[0];
                                    sum12 = pC[0];
                                    sum13 = pC[0];
                                    sum14 = pC[0];
                                    sum15 = pC[0];
                                    sum16 = pC[0];
                                    sum17 = pC[0];
                                    sum18 = pC[0];
                                    sum19 = pC[0];
                                    sum1a = pC[0];
                                    sum1b = pC[0];
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    sum00 = pC[i + ii];
                                    sum01 = pC[i + ii];
                                    sum02 = pC[i + ii];
                                    sum03 = pC[i + ii];
                                    sum04 = pC[i + ii];
                                    sum05 = pC[i + ii];
                                    sum06 = pC[i + ii];
                                    sum07 = pC[i + ii];
                                    sum08 = pC[i + ii];
                                    sum09 = pC[i + ii];
                                    sum0a = pC[i + ii];
                                    sum0b = pC[i + ii];
                                    sum10 = pC[i + ii + 1];
                                    sum11 = pC[i + ii + 1];
                                    sum12 = pC[i + ii + 1];
                                    sum13 = pC[i + ii + 1];
                                    sum14 = pC[i + ii + 1];
                                    sum15 = pC[i + ii + 1];
                                    sum16 = pC[i + ii + 1];
                                    sum17 = pC[i + ii + 1];
                                    sum18 = pC[i + ii + 1];
                                    sum19 = pC[i + ii + 1];
                                    sum1a = pC[i + ii + 1];
                                    sum1b = pC[i + ii + 1];
                                }
                                if (broadcast_type_C == 3)
                                {
                                    sum00 = pC[(i + ii + 0) * N + j + jj];
                                    sum01 = pC[(i + ii + 0) * N + j + jj + 1];
                                    sum02 = pC[(i + ii + 0) * N + j + jj + 2];
                                    sum03 = pC[(i + ii + 0) * N + j + jj + 3];
                                    sum04 = pC[(i + ii + 0) * N + j + jj + 4];
                                    sum05 = pC[(i + ii + 0) * N + j + jj + 5];
                                    sum06 = pC[(i + ii + 0) * N + j + jj + 6];
                                    sum07 = pC[(i + ii + 0) * N + j + jj + 7];
                                    sum08 = pC[(i + ii + 0) * N + j + jj + 8];
                                    sum09 = pC[(i + ii + 0) * N + j + jj + 9];
                                    sum0a = pC[(i + ii + 0) * N + j + jj + 10];
                                    sum0b = pC[(i + ii + 0) * N + j + jj + 11];
                                    sum10 = pC[(i + ii + 1) * N + j + jj];
                                    sum11 = pC[(i + ii + 1) * N + j + jj + 1];
                                    sum12 = pC[(i + ii + 1) * N + j + jj + 2];
                                    sum13 = pC[(i + ii + 1) * N + j + jj + 3];
                                    sum14 = pC[(i + ii + 1) * N + j + jj + 4];
                                    sum15 = pC[(i + ii + 1) * N + j + jj + 5];
                                    sum16 = pC[(i + ii + 1) * N + j + jj + 6];
                                    sum17 = pC[(i + ii + 1) * N + j + jj + 7];
                                    sum18 = pC[(i + ii + 1) * N + j + jj + 8];
                                    sum19 = pC[(i + ii + 1) * N + j + jj + 9];
                                    sum1a = pC[(i + ii + 1) * N + j + jj + 10];
                                    sum1b = pC[(i + ii + 1) * N + j + jj + 11];
                                }
                                if (broadcast_type_C == 4)
                                {
                                    sum00 = pC[j + jj];
                                    sum01 = pC[j + jj + 1];
                                    sum02 = pC[j + jj + 2];
                                    sum03 = pC[j + jj + 3];
                                    sum04 = pC[j + jj + 4];
                                    sum05 = pC[j + jj + 5];
                                    sum06 = pC[j + jj + 6];
                                    sum07 = pC[j + jj + 7];
                                    sum08 = pC[j + jj + 8];
                                    sum09 = pC[j + jj + 9];
                                    sum0a = pC[j + jj + 10];
                                    sum0b = pC[j + jj + 11];
                                    sum10 = pC[j + jj];
                                    sum11 = pC[j + jj + 1];
                                    sum12 = pC[j + jj + 2];
                                    sum13 = pC[j + jj + 3];
                                    sum14 = pC[j + jj + 4];
                                    sum15 = pC[j + jj + 5];
                                    sum16 = pC[j + jj + 6];
                                    sum17 = pC[j + jj + 7];
                                    sum18 = pC[j + jj + 8];
                                    sum19 = pC[j + jj + 9];
                                    sum1a = pC[j + jj + 10];
                                    sum1b = pC[j + jj + 11];
                                }

                                sum00 *= beta;
                                sum01 *= beta;
                                sum02 *= beta;
                                sum03 *= beta;
                                sum04 *= beta;
                                sum05 *= beta;
                                sum06 *= beta;
                                sum07 *= beta;
                                sum08 *= beta;
                                sum09 *= beta;
                                sum0a *= beta;
                                sum0b *= beta;
                                sum10 *= beta;
                                sum11 *= beta;
                                sum12 *= beta;
                                sum13 *= beta;
                                sum14 *= beta;
                                sum15 *= beta;
                                sum16 *= beta;
                                sum17 *= beta;
                                sum18 *= beta;
                                sum19 *= beta;
                                sum1a *= beta;
                                sum1b *= beta;
                            }
                        }
                        else
                        {
                            sum00 = ptmp[0];
                            sum01 = ptmp[1];
                            sum02 = ptmp[2];
                            sum03 = ptmp[3];
                            sum04 = ptmp[4];
                            sum05 = ptmp[5];
                            sum06 = ptmp[6];
                            sum07 = ptmp[7];
                            sum08 = ptmp[8];
                            sum09 = ptmp[9];
                            sum0a = ptmp[10];
                            sum0b = ptmp[11];
                            sum10 = ptmp[12 + 0];
                            sum11 = ptmp[12 + 1];
                            sum12 = ptmp[12 + 2];
                            sum13 = ptmp[12 + 3];
                            sum14 = ptmp[12 + 4];
                            sum15 = ptmp[12 + 5];
                            sum16 = ptmp[12 + 6];
                            sum17 = ptmp[12 + 7];
                            sum18 = ptmp[12 + 8];
                            sum19 = ptmp[12 + 9];
                            sum1a = ptmp[12 + 10];
                            sum1b = ptmp[12 + 11];
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            sum00 += pA[0] * pB[0];
                            sum01 += pA[0] * pB[1];
                            sum02 += pA[0] * pB[2];
                            sum03 += pA[0] * pB[3];
                            sum04 += pA[0] * pB[4];
                            sum05 += pA[0] * pB[5];
                            sum06 += pA[0] * pB[6];
                            sum07 += pA[0] * pB[7];
                            sum08 += pA[0] * pB[8];
                            sum09 += pA[0] * pB[9];
                            sum0a += pA[0] * pB[10];
                            sum0b += pA[0] * pB[11];
                            sum10 += pA[1] * pB[0];
                            sum11 += pA[1] * pB[1];
                            sum12 += pA[1] * pB[2];
                            sum13 += pA[1] * pB[3];
                            sum14 += pA[1] * pB[4];
                            sum15 += pA[1] * pB[5];
                            sum16 += pA[1] * pB[6];
                            sum17 += pA[1] * pB[7];
                            sum18 += pA[1] * pB[8];
                            sum19 += pA[1] * pB[9];
                            sum1a += pA[1] * pB[10];
                            sum1b += pA[1] * pB[11];

                            pA += 2;
                            pB += 12;
                        }

                        if (k + TILE_K >= K)
                        {
                            sum00 *= alpha;
                            sum01 *= alpha;
                            sum02 *= alpha;
                            sum03 *= alpha;
                            sum04 *= alpha;
                            sum05 *= alpha;
                            sum06 *= alpha;
                            sum07 *= alpha;
                            sum08 *= alpha;
                            sum09 *= alpha;
                            sum0a *= alpha;
                            sum0b *= alpha;
                            sum10 *= alpha;
                            sum11 *= alpha;
                            sum12 *= alpha;
                            sum13 *= alpha;
                            sum14 *= alpha;
                            sum15 *= alpha;
                            sum16 *= alpha;
                            sum17 *= alpha;
                            sum18 *= alpha;
                            sum19 *= alpha;
                            sum1a *= alpha;
                            sum1b *= alpha;

                            // if (out_elempack == 1)
                            {
                                outptr0[0] = sum00;
                                outptr0[1] = sum01;
                                outptr0[2] = sum02;
                                outptr0[3] = sum03;
                                outptr0[4] = sum04;
                                outptr0[5] = sum05;
                                outptr0[6] = sum06;
                                outptr0[7] = sum07;
                                outptr0[8] = sum08;
                                outptr0[9] = sum09;
                                outptr0[10] = sum0a;
                                outptr0[11] = sum0b;
                                outptr0[N] = sum10;
                                outptr0[N + 1] = sum11;
                                outptr0[N + 2] = sum12;
                                outptr0[N + 3] = sum13;
                                outptr0[N + 4] = sum14;
                                outptr0[N + 5] = sum15;
                                outptr0[N + 6] = sum16;
                                outptr0[N + 7] = sum17;
                                outptr0[N + 8] = sum18;
                                outptr0[N + 9] = sum19;
                                outptr0[N + 10] = sum1a;
                                outptr0[N + 11] = sum1b;
                                outptr0 += 12;
                            }
                        }
                        else
                        {
                            ptmp[0] = sum00;
                            ptmp[1] = sum01;
                            ptmp[2] = sum02;
                            ptmp[3] = sum03;
                            ptmp[4] = sum04;
                            ptmp[5] = sum05;
                            ptmp[6] = sum06;
                            ptmp[7] = sum07;
                            ptmp[8] = sum08;
                            ptmp[9] = sum09;
                            ptmp[10] = sum0a;
                            ptmp[11] = sum0b;
                            ptmp[12 + 0] = sum10;
                            ptmp[12 + 1] = sum11;
                            ptmp[12 + 2] = sum12;
                            ptmp[12 + 3] = sum13;
                            ptmp[12 + 4] = sum14;
                            ptmp[12 + 5] = sum15;
                            ptmp[12 + 6] = sum16;
                            ptmp[12 + 7] = sum17;
                            ptmp[12 + 8] = sum18;
                            ptmp[12 + 9] = sum19;
                            ptmp[12 + 10] = sum1a;
                            ptmp[12 + 11] = sum1b;
                        }

                        ptmp += 24;
                    }
                    for (; jj + 7 < max_jj; jj += 8)
                    {
                        float sum00;
                        float sum01;
                        float sum02;
                        float sum03;
                        float sum04;
                        float sum05;
                        float sum06;
                        float sum07;
                        float sum10;
                        float sum11;
                        float sum12;
                        float sum13;
                        float sum14;
                        float sum15;
                        float sum16;
                        float sum17;

                        if (k == 0)
                        {
                            sum00 = 0.f;
                            sum01 = 0.f;
                            sum02 = 0.f;
                            sum03 = 0.f;
                            sum04 = 0.f;
                            sum05 = 0.f;
                            sum06 = 0.f;
                            sum07 = 0.f;
                            sum10 = 0.f;
                            sum11 = 0.f;
                            sum12 = 0.f;
                            sum13 = 0.f;
                            sum14 = 0.f;
                            sum15 = 0.f;
                            sum16 = 0.f;
                            sum17 = 0.f;

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    sum00 = pC[0];
                                    sum01 = pC[0];
                                    sum02 = pC[0];
                                    sum03 = pC[0];
                                    sum04 = pC[0];
                                    sum05 = pC[0];
                                    sum06 = pC[0];
                                    sum07 = pC[0];
                                    sum10 = pC[0];
                                    sum11 = pC[0];
                                    sum12 = pC[0];
                                    sum13 = pC[0];
                                    sum14 = pC[0];
                                    sum15 = pC[0];
                                    sum16 = pC[0];
                                    sum17 = pC[0];
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    sum00 = pC[i + ii];
                                    sum01 = pC[i + ii];
                                    sum02 = pC[i + ii];
                                    sum03 = pC[i + ii];
                                    sum04 = pC[i + ii];
                                    sum05 = pC[i + ii];
                                    sum06 = pC[i + ii];
                                    sum07 = pC[i + ii];
                                    sum10 = pC[i + ii + 1];
                                    sum11 = pC[i + ii + 1];
                                    sum12 = pC[i + ii + 1];
                                    sum13 = pC[i + ii + 1];
                                    sum14 = pC[i + ii + 1];
                                    sum15 = pC[i + ii + 1];
                                    sum16 = pC[i + ii + 1];
                                    sum17 = pC[i + ii + 1];
                                }
                                if (broadcast_type_C == 3)
                                {
                                    sum00 = pC[(i + ii) * N + j + jj];
                                    sum01 = pC[(i + ii) * N + j + jj + 1];
                                    sum02 = pC[(i + ii) * N + j + jj + 2];
                                    sum03 = pC[(i + ii) * N + j + jj + 3];
                                    sum04 = pC[(i + ii) * N + j + jj + 4];
                                    sum05 = pC[(i + ii) * N + j + jj + 5];
                                    sum06 = pC[(i + ii) * N + j + jj + 6];
                                    sum07 = pC[(i + ii) * N + j + jj + 7];
                                    sum10 = pC[(i + ii + 1) * N + j + jj];
                                    sum11 = pC[(i + ii + 1) * N + j + jj + 1];
                                    sum12 = pC[(i + ii + 1) * N + j + jj + 2];
                                    sum13 = pC[(i + ii + 1) * N + j + jj + 3];
                                    sum14 = pC[(i + ii + 1) * N + j + jj + 4];
                                    sum15 = pC[(i + ii + 1) * N + j + jj + 5];
                                    sum16 = pC[(i + ii + 1) * N + j + jj + 6];
                                    sum17 = pC[(i + ii + 1) * N + j + jj + 7];
                                }
                                if (broadcast_type_C == 4)
                                {
                                    sum00 = pC[j + jj];
                                    sum01 = pC[j + jj + 1];
                                    sum02 = pC[j + jj + 2];
                                    sum03 = pC[j + jj + 3];
                                    sum04 = pC[j + jj + 4];
                                    sum05 = pC[j + jj + 5];
                                    sum06 = pC[j + jj + 6];
                                    sum07 = pC[j + jj + 7];
                                    sum10 = pC[j + jj];
                                    sum11 = pC[j + jj + 1];
                                    sum12 = pC[j + jj + 2];
                                    sum13 = pC[j + jj + 3];
                                    sum14 = pC[j + jj + 4];
                                    sum15 = pC[j + jj + 5];
                                    sum16 = pC[j + jj + 6];
                                    sum17 = pC[j + jj + 7];
                                }

                                sum00 *= beta;
                                sum01 *= beta;
                                sum02 *= beta;
                                sum03 *= beta;
                                sum04 *= beta;
                                sum05 *= beta;
                                sum06 *= beta;
                                sum07 *= beta;
                                sum10 *= beta;
                                sum11 *= beta;
                                sum12 *= beta;
                                sum13 *= beta;
                                sum14 *= beta;
                                sum15 *= beta;
                                sum16 *= beta;
                                sum17 *= beta;
                            }
                        }
                        else
                        {
                            sum00 = ptmp[0];
                            sum01 = ptmp[1];
                            sum02 = ptmp[2];
                            sum03 = ptmp[3];
                            sum04 = ptmp[4];
                            sum05 = ptmp[5];
                            sum06 = ptmp[6];
                            sum07 = ptmp[7];
                            sum10 = ptmp[8 + 0];
                            sum11 = ptmp[8 + 1];
                            sum12 = ptmp[8 + 2];
                            sum13 = ptmp[8 + 3];
                            sum14 = ptmp[8 + 4];
                            sum15 = ptmp[8 + 5];
                            sum16 = ptmp[8 + 6];
                            sum17 = ptmp[8 + 7];
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            sum00 += pA[0] * pB[0];
                            sum01 += pA[0] * pB[1];
                            sum02 += pA[0] * pB[2];
                            sum03 += pA[0] * pB[3];
                            sum04 += pA[0] * pB[4];
                            sum05 += pA[0] * pB[5];
                            sum06 += pA[0] * pB[6];
                            sum07 += pA[0] * pB[7];
                            sum10 += pA[1] * pB[0];
                            sum11 += pA[1] * pB[1];
                            sum12 += pA[1] * pB[2];
                            sum13 += pA[1] * pB[3];
                            sum14 += pA[1] * pB[4];
                            sum15 += pA[1] * pB[5];
                            sum16 += pA[1] * pB[6];
                            sum17 += pA[1] * pB[7];

                            pA += 2;
                            pB += 8;
                        }

                        if (k + TILE_K >= K)
                        {
                            sum00 *= alpha;
                            sum01 *= alpha;
                            sum02 *= alpha;
                            sum03 *= alpha;
                            sum04 *= alpha;
                            sum05 *= alpha;
                            sum06 *= alpha;
                            sum07 *= alpha;
                            sum10 *= alpha;
                            sum11 *= alpha;
                            sum12 *= alpha;
                            sum13 *= alpha;
                            sum14 *= alpha;
                            sum15 *= alpha;
                            sum16 *= alpha;
                            sum17 *= alpha;

                            // if (out_elempack == 1)
                            {
                                outptr0[0] = sum00;
                                outptr0[1] = sum01;
                                outptr0[2] = sum02;
                                outptr0[3] = sum03;
                                outptr0[4] = sum04;
                                outptr0[5] = sum05;
                                outptr0[6] = sum06;
                                outptr0[7] = sum07;
                                outptr0[N] = sum10;
                                outptr0[N + 1] = sum11;
                                outptr0[N + 2] = sum12;
                                outptr0[N + 3] = sum13;
                                outptr0[N + 4] = sum14;
                                outptr0[N + 5] = sum15;
                                outptr0[N + 6] = sum16;
                                outptr0[N + 7] = sum17;
                                outptr0 += 8;
                            }
                        }
                        else
                        {
                            ptmp[0] = sum00;
                            ptmp[1] = sum01;
                            ptmp[2] = sum02;
                            ptmp[3] = sum03;
                            ptmp[4] = sum04;
                            ptmp[5] = sum05;
                            ptmp[6] = sum06;
                            ptmp[7] = sum07;
                            ptmp[8 + 0] = sum10;
                            ptmp[8 + 1] = sum11;
                            ptmp[8 + 2] = sum12;
                            ptmp[8 + 3] = sum13;
                            ptmp[8 + 4] = sum14;
                            ptmp[8 + 5] = sum15;
                            ptmp[8 + 6] = sum16;
                            ptmp[8 + 7] = sum17;
                        }

                        ptmp += 16;
                    }
                    for (; jj + 3 < max_jj; jj += 4)
                    {
                        float sum00;
                        float sum01;
                        float sum02;
                        float sum03;
                        float sum10;
                        float sum11;
                        float sum12;
                        float sum13;

                        if (k == 0)
                        {
                            sum00 = 0.f;
                            sum01 = 0.f;
                            sum02 = 0.f;
                            sum03 = 0.f;
                            sum10 = 0.f;
                            sum11 = 0.f;
                            sum12 = 0.f;
                            sum13 = 0.f;

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    sum00 = pC[0];
                                    sum01 = pC[0];
                                    sum02 = pC[0];
                                    sum03 = pC[0];
                                    sum10 = pC[0];
                                    sum11 = pC[0];
                                    sum12 = pC[0];
                                    sum13 = pC[0];
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    sum00 = pC[i + ii];
                                    sum01 = pC[i + ii];
                                    sum02 = pC[i + ii];
                                    sum03 = pC[i + ii];
                                    sum10 = pC[i + ii + 1];
                                    sum11 = pC[i + ii + 1];
                                    sum12 = pC[i + ii + 1];
                                    sum13 = pC[i + ii + 1];
                                }
                                if (broadcast_type_C == 3)
                                {
                                    sum00 = pC[(i + ii) * N + j + jj];
                                    sum01 = pC[(i + ii) * N + j + jj + 1];
                                    sum02 = pC[(i + ii) * N + j + jj + 2];
                                    sum03 = pC[(i + ii) * N + j + jj + 3];
                                    sum10 = pC[(i + ii + 1) * N + j + jj];
                                    sum11 = pC[(i + ii + 1) * N + j + jj + 1];
                                    sum12 = pC[(i + ii + 1) * N + j + jj + 2];
                                    sum13 = pC[(i + ii + 1) * N + j + jj + 3];
                                }
                                if (broadcast_type_C == 4)
                                {
                                    sum00 = pC[j + jj];
                                    sum01 = pC[j + jj + 1];
                                    sum02 = pC[j + jj + 2];
                                    sum03 = pC[j + jj + 3];
                                    sum10 = pC[j + jj];
                                    sum11 = pC[j + jj + 1];
                                    sum12 = pC[j + jj + 2];
                                    sum13 = pC[j + jj + 3];
                                }

                                sum00 *= beta;
                                sum01 *= beta;
                                sum02 *= beta;
                                sum03 *= beta;
                                sum10 *= beta;
                                sum11 *= beta;
                                sum12 *= beta;
                                sum13 *= beta;
                            }
                        }
                        else
                        {
                            sum00 = ptmp[0];
                            sum01 = ptmp[1];
                            sum02 = ptmp[2];
                            sum03 = ptmp[3];
                            sum10 = ptmp[4];
                            sum11 = ptmp[5];
                            sum12 = ptmp[6];
                            sum13 = ptmp[7];
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            sum00 += pA[0] * pB[0];
                            sum01 += pA[0] * pB[1];
                            sum02 += pA[0] * pB[2];
                            sum03 += pA[0] * pB[3];
                            sum10 += pA[1] * pB[0];
                            sum11 += pA[1] * pB[1];
                            sum12 += pA[1] * pB[2];
                            sum13 += pA[1] * pB[3];

                            pA += 2;
                            pB += 4;
                        }

                        if (k + TILE_K >= K)
                        {
                            sum00 *= alpha;
                            sum01 *= alpha;
                            sum02 *= alpha;
                            sum03 *= alpha;
                            sum10 *= alpha;
                            sum11 *= alpha;
                            sum12 *= alpha;
                            sum13 *= alpha;

                            // if (out_elempack == 1)
                            {
                                outptr0[0] = sum00;
                                outptr0[1] = sum01;
                                outptr0[2] = sum02;
                                outptr0[3] = sum03;
                                outptr0[N] = sum10;
                                outptr0[N + 1] = sum11;
                                outptr0[N + 2] = sum12;
                                outptr0[N + 3] = sum13;
                                outptr0 += 4;
                            }
                        }
                        else
                        {
                            ptmp[0] = sum00;
                            ptmp[1] = sum01;
                            ptmp[2] = sum02;
                            ptmp[3] = sum03;
                            ptmp[4] = sum10;
                            ptmp[5] = sum11;
                            ptmp[6] = sum12;
                            ptmp[7] = sum13;
                        }

                        ptmp += 8;
                    }
#endif // __SSE2__
                    for (; jj + 1 < max_jj; jj += 2)
                    {
                        float sum00;
                        float sum01;
                        float sum10;
                        float sum11;

                        if (k == 0)
                        {
                            sum00 = 0.f;
                            sum01 = 0.f;
                            sum10 = 0.f;
                            sum11 = 0.f;

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    sum00 = pC[0];
                                    sum01 = pC[0];
                                    sum10 = pC[0];
                                    sum11 = pC[0];
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    sum00 = pC[i + ii];
                                    sum01 = pC[i + ii];
                                    sum10 = pC[i + ii + 1];
                                    sum11 = pC[i + ii + 1];
                                }
                                if (broadcast_type_C == 3)
                                {
                                    sum00 = pC[(i + ii) * N + j + jj];
                                    sum01 = pC[(i + ii) * N + j + jj + 1];
                                    sum10 = pC[(i + ii + 1) * N + j + jj];
                                    sum11 = pC[(i + ii + 1) * N + j + jj + 1];
                                }
                                if (broadcast_type_C == 4)
                                {
                                    sum00 = pC[j + jj];
                                    sum01 = pC[j + jj + 1];
                                    sum10 = pC[j + jj];
                                    sum11 = pC[j + jj + 1];
                                }

                                sum00 *= beta;
                                sum01 *= beta;
                                sum10 *= beta;
                                sum11 *= beta;
                            }
                        }
                        else
                        {
                            sum00 = ptmp[0];
                            sum01 = ptmp[1];
                            sum10 = ptmp[2];
                            sum11 = ptmp[3];
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            sum00 += pA[0] * pB[0];
                            sum01 += pA[0] * pB[1];
                            sum10 += pA[1] * pB[0];
                            sum11 += pA[1] * pB[1];

                            pA += 2;
                            pB += 2;
                        }

                        if (k + TILE_K >= K)
                        {
                            sum00 *= alpha;
                            sum01 *= alpha;
                            sum10 *= alpha;
                            sum11 *= alpha;

                            // if (out_elempack == 1)
                            {
                                outptr0[0] = sum00;
                                outptr0[1] = sum01;
                                outptr0[N] = sum10;
                                outptr0[N + 1] = sum11;
                                outptr0 += 2;
                            }
                        }
                        else
                        {
                            ptmp[0] = sum00;
                            ptmp[1] = sum01;
                            ptmp[2] = sum10;
                            ptmp[3] = sum11;
                        }

                        ptmp += 4;
                    }
                    for (; jj < max_jj; jj += 1)
                    {
                        float sum0;
                        float sum1;

                        if (k == 0)
                        {
                            sum0 = 0.f;
                            sum1 = 0.f;

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    sum0 = pC[0];
                                    sum1 = pC[0];
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    sum0 = pC[i + ii];
                                    sum1 = pC[i + ii + 1];
                                }
                                if (broadcast_type_C == 3)
                                {
                                    sum0 = pC[(i + ii) * N + j + jj];
                                    sum1 = pC[(i + ii + 1) * N + j + jj];
                                }
                                if (broadcast_type_C == 4)
                                {
                                    sum0 = pC[j + jj];
                                    sum1 = pC[j + jj];
                                }

                                sum0 *= beta;
                                sum1 *= beta;
                            }
                        }
                        else
                        {
                            sum0 = ptmp[0];
                            sum1 = ptmp[1];
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            sum0 += pA[0] * pB[0];
                            sum1 += pA[1] * pB[0];
                            pA += 2;
                            pB += 1;
                        }

                        if (k + TILE_K >= K)
                        {
                            sum0 *= alpha;
                            sum1 *= alpha;

                            // if (out_elempack == 1)
                            {
                                outptr0[0] = sum0;
                                outptr0[N] = sum1;
                                outptr0++;
                            }
                        }
                        else
                        {
                            ptmp[0] = sum0;
                            ptmp[1] = sum1;
                        }

                        ptmp += 2;
                    }

                    pA0 += max_kk * 2;
                }
                for (; ii < max_ii; ii += 1)
                {
                    float* outptr0 = top_blob.row(i + ii) + j;

                    const float* pB = pB0;

                    int jj = 0;
#if __SSE2__
                    for (; jj + 11 < max_jj; jj += 12)
                    {
                        float sum0;
                        float sum1;
                        float sum2;
                        float sum3;
                        float sum4;
                        float sum5;
                        float sum6;
                        float sum7;
                        float sum8;
                        float sum9;
                        float suma;
                        float sumb;

                        if (k == 0)
                        {
                            sum0 = 0.f;
                            sum1 = 0.f;
                            sum2 = 0.f;
                            sum3 = 0.f;
                            sum4 = 0.f;
                            sum5 = 0.f;
                            sum6 = 0.f;
                            sum7 = 0.f;
                            sum8 = 0.f;
                            sum9 = 0.f;
                            suma = 0.f;
                            sumb = 0.f;

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    sum0 = pC[0];
                                    sum1 = pC[0];
                                    sum2 = pC[0];
                                    sum3 = pC[0];
                                    sum4 = pC[0];
                                    sum5 = pC[0];
                                    sum6 = pC[0];
                                    sum7 = pC[0];
                                    sum8 = pC[0];
                                    sum9 = pC[0];
                                    suma = pC[0];
                                    sumb = pC[0];
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    sum0 = pC[i + ii];
                                    sum1 = pC[i + ii];
                                    sum2 = pC[i + ii];
                                    sum3 = pC[i + ii];
                                    sum4 = pC[i + ii];
                                    sum5 = pC[i + ii];
                                    sum6 = pC[i + ii];
                                    sum7 = pC[i + ii];
                                    sum8 = pC[i + ii];
                                    sum9 = pC[i + ii];
                                    suma = pC[i + ii];
                                    sumb = pC[i + ii];
                                }
                                if (broadcast_type_C == 3)
                                {
                                    sum0 = pC[(i + ii) * N + j + jj];
                                    sum1 = pC[(i + ii) * N + j + jj + 1];
                                    sum2 = pC[(i + ii) * N + j + jj + 2];
                                    sum3 = pC[(i + ii) * N + j + jj + 3];
                                    sum4 = pC[(i + ii) * N + j + jj + 4];
                                    sum5 = pC[(i + ii) * N + j + jj + 5];
                                    sum6 = pC[(i + ii) * N + j + jj + 6];
                                    sum7 = pC[(i + ii) * N + j + jj + 7];
                                    sum8 = pC[(i + ii) * N + j + jj + 8];
                                    sum9 = pC[(i + ii) * N + j + jj + 9];
                                    suma = pC[(i + ii) * N + j + jj + 10];
                                    sumb = pC[(i + ii) * N + j + jj + 11];
                                }
                                if (broadcast_type_C == 4)
                                {
                                    sum0 = pC[j + jj];
                                    sum1 = pC[j + jj + 1];
                                    sum2 = pC[j + jj + 2];
                                    sum3 = pC[j + jj + 3];
                                    sum4 = pC[j + jj + 4];
                                    sum5 = pC[j + jj + 5];
                                    sum6 = pC[j + jj + 6];
                                    sum7 = pC[j + jj + 7];
                                    sum8 = pC[j + jj + 8];
                                    sum9 = pC[j + jj + 9];
                                    suma = pC[j + jj + 10];
                                    sumb = pC[j + jj + 11];
                                }

                                sum0 *= beta;
                                sum1 *= beta;
                                sum2 *= beta;
                                sum3 *= beta;
                                sum4 *= beta;
                                sum5 *= beta;
                                sum6 *= beta;
                                sum7 *= beta;
                                sum8 *= beta;
                                sum9 *= beta;
                                suma *= beta;
                                sumb *= beta;
                            }
                        }
                        else
                        {
                            sum0 = ptmp[0];
                            sum1 = ptmp[1];
                            sum2 = ptmp[2];
                            sum3 = ptmp[3];
                            sum4 = ptmp[4];
                            sum5 = ptmp[5];
                            sum6 = ptmp[6];
                            sum7 = ptmp[7];
                            sum8 = ptmp[8];
                            sum9 = ptmp[9];
                            suma = ptmp[10];
                            sumb = ptmp[11];
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            sum0 += pA[0] * pB[0];
                            sum1 += pA[0] * pB[1];
                            sum2 += pA[0] * pB[2];
                            sum3 += pA[0] * pB[3];
                            sum4 += pA[0] * pB[4];
                            sum5 += pA[0] * pB[5];
                            sum6 += pA[0] * pB[6];
                            sum7 += pA[0] * pB[7];
                            sum8 += pA[0] * pB[8];
                            sum9 += pA[0] * pB[9];
                            suma += pA[0] * pB[10];
                            sumb += pA[0] * pB[11];

                            pA += 1;
                            pB += 12;
                        }

                        if (k + TILE_K >= K)
                        {
                            sum0 *= alpha;
                            sum1 *= alpha;
                            sum2 *= alpha;
                            sum3 *= alpha;
                            sum4 *= alpha;
                            sum5 *= alpha;
                            sum6 *= alpha;
                            sum7 *= alpha;
                            sum8 *= alpha;
                            sum9 *= alpha;
                            suma *= alpha;
                            sumb *= alpha;

                            // if (out_elempack == 1)
                            {
                                outptr0[0] = sum0;
                                outptr0[1] = sum1;
                                outptr0[2] = sum2;
                                outptr0[3] = sum3;
                                outptr0[4] = sum4;
                                outptr0[5] = sum5;
                                outptr0[6] = sum6;
                                outptr0[7] = sum7;
                                outptr0[8] = sum8;
                                outptr0[9] = sum9;
                                outptr0[10] = suma;
                                outptr0[11] = sumb;
                                outptr0 += 12;
                            }
                        }
                        else
                        {
                            ptmp[0] = sum0;
                            ptmp[1] = sum1;
                            ptmp[2] = sum2;
                            ptmp[3] = sum3;
                            ptmp[4] = sum4;
                            ptmp[5] = sum5;
                            ptmp[6] = sum6;
                            ptmp[7] = sum7;
                            ptmp[8] = sum8;
                            ptmp[9] = sum9;
                            ptmp[10] = suma;
                            ptmp[11] = sumb;
                        }

                        ptmp += 12;
                    }
                    for (; jj + 7 < max_jj; jj += 8)
                    {
                        float sum0;
                        float sum1;
                        float sum2;
                        float sum3;
                        float sum4;
                        float sum5;
                        float sum6;
                        float sum7;

                        if (k == 0)
                        {
                            sum0 = 0.f;
                            sum1 = 0.f;
                            sum2 = 0.f;
                            sum3 = 0.f;
                            sum4 = 0.f;
                            sum5 = 0.f;
                            sum6 = 0.f;
                            sum7 = 0.f;

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    sum0 = pC[0];
                                    sum1 = pC[0];
                                    sum2 = pC[0];
                                    sum3 = pC[0];
                                    sum4 = pC[0];
                                    sum5 = pC[0];
                                    sum6 = pC[0];
                                    sum7 = pC[0];
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    sum0 = pC[i + ii];
                                    sum1 = pC[i + ii];
                                    sum2 = pC[i + ii];
                                    sum3 = pC[i + ii];
                                    sum4 = pC[i + ii];
                                    sum5 = pC[i + ii];
                                    sum6 = pC[i + ii];
                                    sum7 = pC[i + ii];
                                }
                                if (broadcast_type_C == 3)
                                {
                                    sum0 = pC[(i + ii) * N + j + jj];
                                    sum1 = pC[(i + ii) * N + j + jj + 1];
                                    sum2 = pC[(i + ii) * N + j + jj + 2];
                                    sum3 = pC[(i + ii) * N + j + jj + 3];
                                    sum4 = pC[(i + ii) * N + j + jj + 4];
                                    sum5 = pC[(i + ii) * N + j + jj + 5];
                                    sum6 = pC[(i + ii) * N + j + jj + 6];
                                    sum7 = pC[(i + ii) * N + j + jj + 7];
                                }
                                if (broadcast_type_C == 4)
                                {
                                    sum0 = pC[j + jj];
                                    sum1 = pC[j + jj + 1];
                                    sum2 = pC[j + jj + 2];
                                    sum3 = pC[j + jj + 3];
                                    sum4 = pC[j + jj + 4];
                                    sum5 = pC[j + jj + 5];
                                    sum6 = pC[j + jj + 6];
                                    sum7 = pC[j + jj + 7];
                                }

                                sum0 *= beta;
                                sum1 *= beta;
                                sum2 *= beta;
                                sum3 *= beta;
                                sum4 *= beta;
                                sum5 *= beta;
                                sum6 *= beta;
                                sum7 *= beta;
                            }
                        }
                        else
                        {
                            sum0 = ptmp[0];
                            sum1 = ptmp[1];
                            sum2 = ptmp[2];
                            sum3 = ptmp[3];
                            sum4 = ptmp[4];
                            sum5 = ptmp[5];
                            sum6 = ptmp[6];
                            sum7 = ptmp[7];
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            sum0 += pA[0] * pB[0];
                            sum1 += pA[0] * pB[1];
                            sum2 += pA[0] * pB[2];
                            sum3 += pA[0] * pB[3];
                            sum4 += pA[0] * pB[4];
                            sum5 += pA[0] * pB[5];
                            sum6 += pA[0] * pB[6];
                            sum7 += pA[0] * pB[7];

                            pA += 1;
                            pB += 8;
                        }

                        if (k + TILE_K >= K)
                        {
                            sum0 *= alpha;
                            sum1 *= alpha;
                            sum2 *= alpha;
                            sum3 *= alpha;
                            sum4 *= alpha;
                            sum5 *= alpha;
                            sum6 *= alpha;
                            sum7 *= alpha;

                            // if (out_elempack == 1)
                            {
                                outptr0[0] = sum0;
                                outptr0[1] = sum1;
                                outptr0[2] = sum2;
                                outptr0[3] = sum3;
                                outptr0[4] = sum4;
                                outptr0[5] = sum5;
                                outptr0[6] = sum6;
                                outptr0[7] = sum7;
                                outptr0 += 8;
                            }
                        }
                        else
                        {
                            ptmp[0] = sum0;
                            ptmp[1] = sum1;
                            ptmp[2] = sum2;
                            ptmp[3] = sum3;
                            ptmp[4] = sum4;
                            ptmp[5] = sum5;
                            ptmp[6] = sum6;
                            ptmp[7] = sum7;
                        }

                        ptmp += 8;
                    }
                    for (; jj + 3 < max_jj; jj += 4)
                    {
                        float sum0;
                        float sum1;
                        float sum2;
                        float sum3;

                        if (k == 0)
                        {
                            sum0 = 0.f;
                            sum1 = 0.f;
                            sum2 = 0.f;
                            sum3 = 0.f;

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    sum0 = pC[0];
                                    sum1 = pC[0];
                                    sum2 = pC[0];
                                    sum3 = pC[0];
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    sum0 = pC[i + ii];
                                    sum1 = pC[i + ii];
                                    sum2 = pC[i + ii];
                                    sum3 = pC[i + ii];
                                }
                                if (broadcast_type_C == 3)
                                {
                                    sum0 = pC[(i + ii) * N + j + jj];
                                    sum1 = pC[(i + ii) * N + j + jj + 1];
                                    sum2 = pC[(i + ii) * N + j + jj + 2];
                                    sum3 = pC[(i + ii) * N + j + jj + 3];
                                }
                                if (broadcast_type_C == 4)
                                {
                                    sum0 = pC[j + jj];
                                    sum1 = pC[j + jj + 1];
                                    sum2 = pC[j + jj + 2];
                                    sum3 = pC[j + jj + 3];
                                }

                                sum0 *= beta;
                                sum1 *= beta;
                                sum2 *= beta;
                                sum3 *= beta;
                            }
                        }
                        else
                        {
                            sum0 = ptmp[0];
                            sum1 = ptmp[1];
                            sum2 = ptmp[2];
                            sum3 = ptmp[3];
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            sum0 += pA[0] * pB[0];
                            sum1 += pA[0] * pB[1];
                            sum2 += pA[0] * pB[2];
                            sum3 += pA[0] * pB[3];

                            pA += 1;
                            pB += 4;
                        }

                        if (k + TILE_K >= K)
                        {
                            sum0 *= alpha;
                            sum1 *= alpha;
                            sum2 *= alpha;
                            sum3 *= alpha;

                            // if (out_elempack == 1)
                            {
                                outptr0[0] = sum0;
                                outptr0[1] = sum1;
                                outptr0[2] = sum2;
                                outptr0[3] = sum3;
                                outptr0 += 4;
                            }
                        }
                        else
                        {
                            ptmp[0] = sum0;
                            ptmp[1] = sum1;
                            ptmp[2] = sum2;
                            ptmp[3] = sum3;
                        }

                        ptmp += 4;
                    }
#endif // __SSE2__
                    for (; jj + 1 < max_jj; jj += 2)
                    {
                        float sum0;
                        float sum1;

                        if (k == 0)
                        {
                            sum0 = 0.f;
                            sum1 = 0.f;

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    sum0 = pC[0];
                                    sum1 = pC[0];
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    sum0 = pC[i + ii];
                                    sum1 = pC[i + ii];
                                }
                                if (broadcast_type_C == 3)
                                {
                                    sum0 = pC[(i + ii) * N + j + jj];
                                    sum1 = pC[(i + ii) * N + j + jj + 1];
                                }
                                if (broadcast_type_C == 4)
                                {
                                    sum0 = pC[j + jj];
                                    sum1 = pC[j + jj + 1];
                                }

                                sum0 *= beta;
                                sum1 *= beta;
                            }
                        }
                        else
                        {
                            sum0 = ptmp[0];
                            sum1 = ptmp[1];
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            sum0 += pA[0] * pB[0];
                            sum1 += pA[0] * pB[1];

                            pA += 1;
                            pB += 2;
                        }

                        if (k + TILE_K >= K)
                        {
                            sum0 *= alpha;
                            sum1 *= alpha;

                            // if (out_elempack == 1)
                            {
                                outptr0[0] = sum0;
                                outptr0[1] = sum1;
                                outptr0 += 2;
                            }
                        }
                        else
                        {
                            ptmp[0] = sum0;
                            ptmp[1] = sum1;
                        }

                        ptmp += 2;
                    }
                    for (; jj < max_jj; jj += 1)
                    {
                        float sum;

                        if (k == 0)
                        {
                            sum = 0.f;

                            if (pC)
                            {
                                if (broadcast_type_C == 0)
                                {
                                    sum = pC[0];
                                }
                                if (broadcast_type_C == 1 || broadcast_type_C == 2)
                                {
                                    sum = pC[i + ii];
                                }
                                if (broadcast_type_C == 3)
                                {
                                    sum = pC[(i + ii) * N + j + jj];
                                }
                                if (broadcast_type_C == 4)
                                {
                                    sum = pC[j + jj];
                                }

                                sum *= beta;
                            }
                        }
                        else
                        {
                            sum = ptmp[0];
                        }

                        const float* pA = pA0;
                        int kk = 0;
                        for (; kk < max_kk; kk += 1)
                        {
                            sum += pA[0] * pB[0];
                            pA += 1;
                            pB += 1;
                        }

                        if (k + TILE_K >= K)
                        {
                            sum *= alpha;

                            // if (out_elempack == 1)
                            {
                                outptr0[0] = sum;
                                outptr0++;
                            }
                        }
                        else
                        {
                            ptmp[0] = sum;
                        }

                        ptmp += 1;
                    }

                    pA0 += max_kk;
                }
            }
        }
    }

    return 0;
}

int Gemm_x86::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    const Mat& A = bottom_blobs[0];
    const Mat& B = bottom_blobs[1];
    const Mat& C = bottom_blobs.size() == 3 ? bottom_blobs[2] : Mat();

    Mat& top_blob = top_blobs[0];

    int ret = gemm_x86(A, B, C, top_blob, transA, transB, alpha, beta, opt);

    return ret;
}

} // namespace ncnn
