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

#include "cpu.h"

namespace ncnn {

Gemm_x86::Gemm_x86()
{
#if __SSE2__
    support_packing = true;
#endif // __SSE2__
}

static void pack_A_tile(const Mat& A, Mat& AT, int i, int max_ii, int k, int max_kk)
{
    // const int M = A.h * A.elempack;
    // const int K = A.w;
    const int elempack = A.elempack;

    float* pp = AT;

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
                _mm512_store_ps(pp, _mm512_load_ps(p0));
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
                _mm256_store_ps(pp, _mm256_load_ps(p0));
                _mm256_store_ps(pp + 8, _mm256_load_ps(p1));
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
                _mm_store_ps(pp, _mm_load_ps(p0));
                _mm_store_ps(pp + 4, _mm_load_ps(p1));
                _mm_store_ps(pp + 8, _mm_load_ps(p2));
                _mm_store_ps(pp + 12, _mm_load_ps(p3));
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

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                __m512 _r0 = _mm512_loadu_ps(p0);
                __m512 _r1 = _mm512_loadu_ps(p1);
                __m512 _r2 = _mm512_loadu_ps(p2);
                __m512 _r3 = _mm512_loadu_ps(p3);
                __m512 _r4 = _mm512_loadu_ps(p4);
                __m512 _r5 = _mm512_loadu_ps(p5);
                __m512 _r6 = _mm512_loadu_ps(p6);
                __m512 _r7 = _mm512_loadu_ps(p7);
                __m512 _r8 = _mm512_loadu_ps(p8);
                __m512 _r9 = _mm512_loadu_ps(p9);
                __m512 _ra = _mm512_loadu_ps(pa);
                __m512 _rb = _mm512_loadu_ps(pb);
                __m512 _rc = _mm512_loadu_ps(pc);
                __m512 _rd = _mm512_loadu_ps(pd);
                __m512 _re = _mm512_loadu_ps(pe);
                __m512 _rf = _mm512_loadu_ps(pf);
                transpose16x16_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7, _r8, _r9, _ra, _rb, _rc, _rd, _re, _rf);
                _mm512_store_ps(pp, _r0);
                _mm512_store_ps(pp + 16 * 1, _r1);
                _mm512_store_ps(pp + 16 * 2, _r2);
                _mm512_store_ps(pp + 16 * 3, _r3);
                _mm512_store_ps(pp + 16 * 4, _r4);
                _mm512_store_ps(pp + 16 * 5, _r5);
                _mm512_store_ps(pp + 16 * 6, _r6);
                _mm512_store_ps(pp + 16 * 7, _r7);
                _mm512_store_ps(pp + 16 * 8, _r8);
                _mm512_store_ps(pp + 16 * 9, _r9);
                _mm512_store_ps(pp + 16 * 10, _ra);
                _mm512_store_ps(pp + 16 * 11, _rb);
                _mm512_store_ps(pp + 16 * 12, _rc);
                _mm512_store_ps(pp + 16 * 13, _rd);
                _mm512_store_ps(pp + 16 * 14, _re);
                _mm512_store_ps(pp + 16 * 15, _rf);
                pp += 256;
                p0 += 16;
                p1 += 16;
                p2 += 16;
                p3 += 16;
                p4 += 16;
                p5 += 16;
                p6 += 16;
                p7 += 16;
                p8 += 16;
                p9 += 16;
                pa += 16;
                pb += 16;
                pc += 16;
                pd += 16;
                pe += 16;
                pf += 16;
            }
            for (; kk < max_kk; kk++)
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
                _mm_store_ps(pp, _mm_load_ps(p0));
                _mm_store_ps(pp + 4, _mm_load_ps(p1));
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

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_loadu_ps(p0);
                __m256 _r1 = _mm256_loadu_ps(p1);
                __m256 _r2 = _mm256_loadu_ps(p2);
                __m256 _r3 = _mm256_loadu_ps(p3);
                __m256 _r4 = _mm256_loadu_ps(p4);
                __m256 _r5 = _mm256_loadu_ps(p5);
                __m256 _r6 = _mm256_loadu_ps(p6);
                __m256 _r7 = _mm256_loadu_ps(p7);
                transpose8x8_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7);
                _mm256_store_ps(pp, _r0);
                _mm256_store_ps(pp + 8, _r1);
                _mm256_store_ps(pp + 8 * 2, _r2);
                _mm256_store_ps(pp + 8 * 3, _r3);
                _mm256_store_ps(pp + 8 * 4, _r4);
                _mm256_store_ps(pp + 8 * 5, _r5);
                _mm256_store_ps(pp + 8 * 6, _r6);
                _mm256_store_ps(pp + 8 * 7, _r7);
                pp += 64;
                p0 += 8;
                p1 += 8;
                p2 += 8;
                p3 += 8;
                p4 += 8;
                p5 += 8;
                p6 += 8;
                p7 += 8;
            }
            for (; kk < max_kk; kk++)
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
                _mm_store_ps(pp, _mm_load_ps(p0));
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

            int kk = 0;
#if __AVX__
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_loadu_ps(p0);
                __m256 _r1 = _mm256_loadu_ps(p1);
                __m256 _r2 = _mm256_loadu_ps(p2);
                __m256 _r3 = _mm256_loadu_ps(p3);
                transpose8x4_ps(_r0, _r1, _r2, _r3);
                _mm256_store_ps(pp, _r0);
                _mm256_store_ps(pp + 8, _r1);
                _mm256_store_ps(pp + 16, _r2);
                _mm256_store_ps(pp + 24, _r3);
                pp += 32;
                p0 += 8;
                p1 += 8;
                p2 += 8;
                p3 += 8;
            }
#endif // __AVX__
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_loadu_ps(p0);
                __m128 _r1 = _mm_loadu_ps(p1);
                __m128 _r2 = _mm_loadu_ps(p2);
                __m128 _r3 = _mm_loadu_ps(p3);
                _MM_TRANSPOSE4_PS(_r0, _r1, _r2, _r3);
                _mm_store_ps(pp, _r0);
                _mm_store_ps(pp + 4, _r1);
                _mm_store_ps(pp + 8, _r2);
                _mm_store_ps(pp + 12, _r3);
                pp += 16;
                p0 += 4;
                p1 += 4;
                p2 += 4;
                p3 += 4;
            }
            for (; kk < max_kk; kk++)
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

            int kk = 0;
#if __SSE2__
#if __AVX__
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_loadu_ps(p0);
                __m256 _r1 = _mm256_loadu_ps(p1);
                transpose8x2_ps(_r0, _r1);
                _mm256_storeu_ps(pp, _r0);
                _mm256_storeu_ps(pp + 8, _r1);
                pp += 16;
                p0 += 8;
                p1 += 8;
            }
#endif // __AVX__
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_loadu_ps(p0);
                __m128 _r1 = _mm_loadu_ps(p1);
                __m128 _tmp0 = _mm_unpacklo_ps(_r0, _r1);
                __m128 _tmp1 = _mm_unpackhi_ps(_r0, _r1);
                _mm_store_ps(pp, _tmp0);
                _mm_store_ps(pp + 4, _tmp1);
                pp += 8;
                p0 += 4;
                p1 += 4;
            }
#endif // __SSE2__
            for (; kk < max_kk; kk++)
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

            int kk = 0;
#if __SSE2__
#if __AVX__
            for (; kk + 7 < max_kk; kk += 8)
            {
                _mm256_storeu_ps(pp, _mm256_loadu_ps(p0));
                pp += 8;
                p0 += 8;
            }
#endif // __AVX__
            for (; kk + 3 < max_kk; kk += 4)
            {
                _mm_storeu_ps(pp, _mm_loadu_ps(p0));
                pp += 4;
                p0 += 4;
            }
#endif // __SSE2__
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp += 1;
                p0++;
            }
        }
    }
}

static void transpose_pack_A_tile(const Mat& A, Mat& AT, int i, int max_ii, int k, int max_kk)
{
    const int M = A.w;
    // const int K = A.h * A.elempack;
    const int elempack = A.elempack;

    float* pp = AT;

    int ii = 0;
#if __SSE2__
#if __AVX__
#if __AVX512F__
    for (; ii + 15 < max_ii; ii += 16)
    {
        if (elempack == 16)
        {
            const float* p0 = A.row(k / 16) + (i + ii) * 16;

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                __m512 _r0 = _mm512_load_ps(p0);
                __m512 _r1 = _mm512_load_ps(p0 + 16 * 1);
                __m512 _r2 = _mm512_load_ps(p0 + 16 * 2);
                __m512 _r3 = _mm512_load_ps(p0 + 16 * 3);
                __m512 _r4 = _mm512_load_ps(p0 + 16 * 4);
                __m512 _r5 = _mm512_load_ps(p0 + 16 * 5);
                __m512 _r6 = _mm512_load_ps(p0 + 16 * 6);
                __m512 _r7 = _mm512_load_ps(p0 + 16 * 7);
                __m512 _r8 = _mm512_load_ps(p0 + 16 * 8);
                __m512 _r9 = _mm512_load_ps(p0 + 16 * 9);
                __m512 _ra = _mm512_load_ps(p0 + 16 * 10);
                __m512 _rb = _mm512_load_ps(p0 + 16 * 11);
                __m512 _rc = _mm512_load_ps(p0 + 16 * 12);
                __m512 _rd = _mm512_load_ps(p0 + 16 * 13);
                __m512 _re = _mm512_load_ps(p0 + 16 * 14);
                __m512 _rf = _mm512_load_ps(p0 + 16 * 15);
                transpose16x16_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7, _r8, _r9, _ra, _rb, _rc, _rd, _re, _rf);
                _mm512_store_ps(pp, _r0);
                _mm512_store_ps(pp + 16 * 1, _r1);
                _mm512_store_ps(pp + 16 * 2, _r2);
                _mm512_store_ps(pp + 16 * 3, _r3);
                _mm512_store_ps(pp + 16 * 4, _r4);
                _mm512_store_ps(pp + 16 * 5, _r5);
                _mm512_store_ps(pp + 16 * 6, _r6);
                _mm512_store_ps(pp + 16 * 7, _r7);
                _mm512_store_ps(pp + 16 * 8, _r8);
                _mm512_store_ps(pp + 16 * 9, _r9);
                _mm512_store_ps(pp + 16 * 10, _ra);
                _mm512_store_ps(pp + 16 * 11, _rb);
                _mm512_store_ps(pp + 16 * 12, _rc);
                _mm512_store_ps(pp + 16 * 13, _rd);
                _mm512_store_ps(pp + 16 * 14, _re);
                _mm512_store_ps(pp + 16 * 15, _rf);
                pp += 256;
                p0 += M * 16;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[16 * 1];
                pp[2] = p0[16 * 2];
                pp[3] = p0[16 * 3];
                pp[4] = p0[16 * 4];
                pp[5] = p0[16 * 5];
                pp[6] = p0[16 * 6];
                pp[7] = p0[16 * 7];
                pp[8] = p0[16 * 8];
                pp[9] = p0[16 * 9];
                pp[10] = p0[16 * 10];
                pp[11] = p0[16 * 11];
                pp[12] = p0[16 * 12];
                pp[13] = p0[16 * 13];
                pp[14] = p0[16 * 14];
                pp[15] = p0[16 * 15];
                pp += 16;
                p0 += 1;
            }
        }
        if (elempack == 8)
        {
            const float* p0 = A.row(k / 8) + (i + ii) * 8;

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_load_ps(p0);
                __m256 _r1 = _mm256_load_ps(p0 + 8 * 1);
                __m256 _r2 = _mm256_load_ps(p0 + 8 * 2);
                __m256 _r3 = _mm256_load_ps(p0 + 8 * 3);
                __m256 _r4 = _mm256_load_ps(p0 + 8 * 4);
                __m256 _r5 = _mm256_load_ps(p0 + 8 * 5);
                __m256 _r6 = _mm256_load_ps(p0 + 8 * 6);
                __m256 _r7 = _mm256_load_ps(p0 + 8 * 7);
                __m256 _r8 = _mm256_load_ps(p0 + 8 * 8);
                __m256 _r9 = _mm256_load_ps(p0 + 8 * 9);
                __m256 _ra = _mm256_load_ps(p0 + 8 * 10);
                __m256 _rb = _mm256_load_ps(p0 + 8 * 11);
                __m256 _rc = _mm256_load_ps(p0 + 8 * 12);
                __m256 _rd = _mm256_load_ps(p0 + 8 * 13);
                __m256 _re = _mm256_load_ps(p0 + 8 * 14);
                __m256 _rf = _mm256_load_ps(p0 + 8 * 15);

                transpose8x16_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7, _r8, _r9, _ra, _rb, _rc, _rd, _re, _rf);
                __m512 _rr0 = _mm512_insertf32x8(_mm512_castps256_ps512(_r0), _r1, 1);
                __m512 _rr1 = _mm512_insertf32x8(_mm512_castps256_ps512(_r2), _r3, 1);
                __m512 _rr2 = _mm512_insertf32x8(_mm512_castps256_ps512(_r4), _r5, 1);
                __m512 _rr3 = _mm512_insertf32x8(_mm512_castps256_ps512(_r6), _r7, 1);
                __m512 _rr4 = _mm512_insertf32x8(_mm512_castps256_ps512(_r8), _r9, 1);
                __m512 _rr5 = _mm512_insertf32x8(_mm512_castps256_ps512(_ra), _rb, 1);
                __m512 _rr6 = _mm512_insertf32x8(_mm512_castps256_ps512(_rc), _rd, 1);
                __m512 _rr7 = _mm512_insertf32x8(_mm512_castps256_ps512(_re), _rf, 1);

                _mm512_store_ps(pp, _rr0);
                _mm512_store_ps(pp + 16 * 1, _rr1);
                _mm512_store_ps(pp + 16 * 2, _rr2);
                _mm512_store_ps(pp + 16 * 3, _rr3);
                _mm512_store_ps(pp + 16 * 4, _rr4);
                _mm512_store_ps(pp + 16 * 5, _rr5);
                _mm512_store_ps(pp + 16 * 6, _rr6);
                _mm512_store_ps(pp + 16 * 7, _rr7);
                pp += 128;
                p0 += M * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[8 * 1];
                pp[2] = p0[8 * 2];
                pp[3] = p0[8 * 3];
                pp[4] = p0[8 * 4];
                pp[5] = p0[8 * 5];
                pp[6] = p0[8 * 6];
                pp[7] = p0[8 * 7];
                pp[8] = p0[8 * 8];
                pp[9] = p0[8 * 9];
                pp[10] = p0[8 * 10];
                pp[11] = p0[8 * 11];
                pp[12] = p0[8 * 12];
                pp[13] = p0[8 * 13];
                pp[14] = p0[8 * 14];
                pp[15] = p0[8 * 15];
                pp += 16;
                p0 += 1;
            }
        }
        if (elempack == 4)
        {
            const float* p0 = A.row(k / 4) + (i + ii) * 4;

            int kk = 0;
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_load_ps(p0);
                __m128 _r1 = _mm_load_ps(p0 + 4 * 1);
                __m128 _r2 = _mm_load_ps(p0 + 4 * 2);
                __m128 _r3 = _mm_load_ps(p0 + 4 * 3);
                __m128 _r4 = _mm_load_ps(p0 + 4 * 4);
                __m128 _r5 = _mm_load_ps(p0 + 4 * 5);
                __m128 _r6 = _mm_load_ps(p0 + 4 * 6);
                __m128 _r7 = _mm_load_ps(p0 + 4 * 7);
                __m128 _r8 = _mm_load_ps(p0 + 4 * 8);
                __m128 _r9 = _mm_load_ps(p0 + 4 * 9);
                __m128 _ra = _mm_load_ps(p0 + 4 * 10);
                __m128 _rb = _mm_load_ps(p0 + 4 * 11);
                __m128 _rc = _mm_load_ps(p0 + 4 * 12);
                __m128 _rd = _mm_load_ps(p0 + 4 * 13);
                __m128 _re = _mm_load_ps(p0 + 4 * 14);
                __m128 _rf = _mm_load_ps(p0 + 4 * 15);
                _MM_TRANSPOSE4_PS(_r0, _r1, _r2, _r3);
                _MM_TRANSPOSE4_PS(_r4, _r5, _r6, _r7);
                _MM_TRANSPOSE4_PS(_r8, _r9, _ra, _rb);
                _MM_TRANSPOSE4_PS(_rc, _rd, _re, _rf);
                _mm_store_ps(pp, _r0);
                _mm_store_ps(pp + 4 * 1, _r4);
                _mm_store_ps(pp + 4 * 2, _r8);
                _mm_store_ps(pp + 4 * 3, _rc);
                _mm_store_ps(pp + 4 * 4, _r1);
                _mm_store_ps(pp + 4 * 5, _r5);
                _mm_store_ps(pp + 4 * 6, _r9);
                _mm_store_ps(pp + 4 * 7, _rd);
                _mm_store_ps(pp + 4 * 8, _r2);
                _mm_store_ps(pp + 4 * 9, _r6);
                _mm_store_ps(pp + 4 * 10, _ra);
                _mm_store_ps(pp + 4 * 11, _re);
                _mm_store_ps(pp + 4 * 12, _r3);
                _mm_store_ps(pp + 4 * 13, _r7);
                _mm_store_ps(pp + 4 * 14, _rb);
                _mm_store_ps(pp + 4 * 15, _rf);
                pp += 64;
                p0 += M * 4;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[4 * 1];
                pp[2] = p0[4 * 2];
                pp[3] = p0[4 * 3];
                pp[4] = p0[4 * 4];
                pp[5] = p0[4 * 5];
                pp[6] = p0[4 * 6];
                pp[7] = p0[4 * 7];
                pp[8] = p0[4 * 8];
                pp[9] = p0[4 * 9];
                pp[10] = p0[4 * 10];
                pp[11] = p0[4 * 11];
                pp[12] = p0[4 * 12];
                pp[13] = p0[4 * 13];
                pp[14] = p0[4 * 14];
                pp[15] = p0[4 * 15];
                pp += 16;
                p0 += 1;
            }
        }
        if (elempack == 1)
        {
            const float* p0 = A.row(k) + (i + ii);

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                _mm512_store_ps(pp, _mm512_loadu_ps(p0));
                pp += 16;
                p0 += M;
            }
        }
    }
#endif // __AVX512F__
    for (; ii + 7 < max_ii; ii += 8)
    {
#if __AVX512F__
        if (elempack == 16)
        {
            const float* p0 = A.row(k / 16) + (i + ii) * 16;

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                __m512 _r0 = _mm512_load_ps(p0);
                __m512 _r1 = _mm512_load_ps(p0 + 16 * 1);
                __m512 _r2 = _mm512_load_ps(p0 + 16 * 2);
                __m512 _r3 = _mm512_load_ps(p0 + 16 * 3);
                __m512 _r4 = _mm512_load_ps(p0 + 16 * 4);
                __m512 _r5 = _mm512_load_ps(p0 + 16 * 5);
                __m512 _r6 = _mm512_load_ps(p0 + 16 * 6);
                __m512 _r7 = _mm512_load_ps(p0 + 16 * 7);
                transpose16x8_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7);
                _mm512_store_ps(pp, _r0);
                _mm512_store_ps(pp + 16 * 1, _r1);
                _mm512_store_ps(pp + 16 * 2, _r2);
                _mm512_store_ps(pp + 16 * 3, _r3);
                _mm512_store_ps(pp + 16 * 4, _r4);
                _mm512_store_ps(pp + 16 * 5, _r5);
                _mm512_store_ps(pp + 16 * 6, _r6);
                _mm512_store_ps(pp + 16 * 7, _r7);
                pp += 128;
                p0 += M * 16;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[16 * 1];
                pp[2] = p0[16 * 2];
                pp[3] = p0[16 * 3];
                pp[4] = p0[16 * 4];
                pp[5] = p0[16 * 5];
                pp[6] = p0[16 * 6];
                pp[7] = p0[16 * 7];
                pp += 8;
                p0 += 1;
            }
        }
#endif // __AVX512F__
        if (elempack == 8)
        {
            const float* p0 = A.row(k / 8) + (i + ii) * 8;

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_load_ps(p0);
                __m256 _r1 = _mm256_load_ps(p0 + 8 * 1);
                __m256 _r2 = _mm256_load_ps(p0 + 8 * 2);
                __m256 _r3 = _mm256_load_ps(p0 + 8 * 3);
                __m256 _r4 = _mm256_load_ps(p0 + 8 * 4);
                __m256 _r5 = _mm256_load_ps(p0 + 8 * 5);
                __m256 _r6 = _mm256_load_ps(p0 + 8 * 6);
                __m256 _r7 = _mm256_load_ps(p0 + 8 * 7);
                transpose8x8_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7);
                _mm256_store_ps(pp, _r0);
                _mm256_store_ps(pp + 8 * 1, _r1);
                _mm256_store_ps(pp + 8 * 2, _r2);
                _mm256_store_ps(pp + 8 * 3, _r3);
                _mm256_store_ps(pp + 8 * 4, _r4);
                _mm256_store_ps(pp + 8 * 5, _r5);
                _mm256_store_ps(pp + 8 * 6, _r6);
                _mm256_store_ps(pp + 8 * 7, _r7);
                pp += 64;
                p0 += M * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[8 * 1];
                pp[2] = p0[8 * 2];
                pp[3] = p0[8 * 3];
                pp[4] = p0[8 * 4];
                pp[5] = p0[8 * 5];
                pp[6] = p0[8 * 6];
                pp[7] = p0[8 * 7];
                pp += 8;
                p0 += 1;
            }
        }
        if (elempack == 4)
        {
            const float* p0 = A.row(k / 4) + (i + ii) * 4;

            int kk = 0;
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_load_ps(p0);
                __m128 _r1 = _mm_load_ps(p0 + 4 * 1);
                __m128 _r2 = _mm_load_ps(p0 + 4 * 2);
                __m128 _r3 = _mm_load_ps(p0 + 4 * 3);
                __m128 _r4 = _mm_load_ps(p0 + 4 * 4);
                __m128 _r5 = _mm_load_ps(p0 + 4 * 5);
                __m128 _r6 = _mm_load_ps(p0 + 4 * 6);
                __m128 _r7 = _mm_load_ps(p0 + 4 * 7);
                _MM_TRANSPOSE4_PS(_r0, _r1, _r2, _r3);
                _MM_TRANSPOSE4_PS(_r4, _r5, _r6, _r7);
                _mm_store_ps(pp, _r0);
                _mm_store_ps(pp + 4 * 1, _r4);
                _mm_store_ps(pp + 4 * 2, _r1);
                _mm_store_ps(pp + 4 * 3, _r5);
                _mm_store_ps(pp + 4 * 4, _r2);
                _mm_store_ps(pp + 4 * 5, _r6);
                _mm_store_ps(pp + 4 * 6, _r3);
                _mm_store_ps(pp + 4 * 7, _r7);
                pp += 32;
                p0 += M * 4;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[4 * 1];
                pp[2] = p0[4 * 2];
                pp[3] = p0[4 * 3];
                pp[4] = p0[4 * 4];
                pp[5] = p0[4 * 5];
                pp[6] = p0[4 * 6];
                pp[7] = p0[4 * 7];
                pp += 8;
                p0 += 1;
            }
        }
        if (elempack == 1)
        {
            const float* p0 = A.row(k) + (i + ii);

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                _mm256_store_ps(pp, _mm256_loadu_ps(p0));
                pp += 8;
                p0 += M;
            }
        }
    }
#endif // __AVX__
    for (; ii + 3 < max_ii; ii += 4)
    {
#if __AVX__
#if __AVX512F__
        if (elempack == 16)
        {
            const float* p0 = A.row(k / 16) + (i + ii) * 16;

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                __m512 _r0 = _mm512_load_ps(p0);
                __m512 _r1 = _mm512_load_ps(p0 + 16 * 1);
                __m512 _r2 = _mm512_load_ps(p0 + 16 * 2);
                __m512 _r3 = _mm512_load_ps(p0 + 16 * 3);
                transpose16x4_ps(_r0, _r1, _r2, _r3);
                _mm512_store_ps(pp, _r0);
                _mm512_store_ps(pp + 16 * 1, _r1);
                _mm512_store_ps(pp + 16 * 2, _r2);
                _mm512_store_ps(pp + 16 * 3, _r3);
                pp += 64;
                p0 += M * 16;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[16 * 1];
                pp[2] = p0[16 * 2];
                pp[3] = p0[16 * 3];
                pp += 4;
                p0 += 1;
            }
        }
#endif // __AVX512F__
        if (elempack == 8)
        {
            const float* p0 = A.row(k / 8) + (i + ii) * 8;

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_load_ps(p0);
                __m256 _r1 = _mm256_load_ps(p0 + 8 * 1);
                __m256 _r2 = _mm256_load_ps(p0 + 8 * 2);
                __m256 _r3 = _mm256_load_ps(p0 + 8 * 3);
                transpose8x4_ps(_r0, _r1, _r2, _r3);
                _mm256_store_ps(pp, _r0);
                _mm256_store_ps(pp + 8 * 1, _r1);
                _mm256_store_ps(pp + 8 * 2, _r2);
                _mm256_store_ps(pp + 8 * 3, _r3);
                pp += 32;
                p0 += M * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[8 * 1];
                pp[2] = p0[8 * 2];
                pp[3] = p0[8 * 3];
                pp += 4;
                p0 += 1;
            }
        }
#endif // __AVX__
        if (elempack == 4)
        {
            const float* p0 = A.row(k / 4) + (i + ii) * 4;

            int kk = 0;
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_load_ps(p0);
                __m128 _r1 = _mm_load_ps(p0 + 4 * 1);
                __m128 _r2 = _mm_load_ps(p0 + 4 * 2);
                __m128 _r3 = _mm_load_ps(p0 + 4 * 3);
                _MM_TRANSPOSE4_PS(_r0, _r1, _r2, _r3);
                _mm_store_ps(pp, _r0);
                _mm_store_ps(pp + 4 * 1, _r1);
                _mm_store_ps(pp + 4 * 2, _r2);
                _mm_store_ps(pp + 4 * 3, _r3);
                pp += 16;
                p0 += M * 4;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[4 * 1];
                pp[2] = p0[4 * 2];
                pp[3] = p0[4 * 3];
                pp += 4;
                p0 += 1;
            }
        }
        if (elempack == 1)
        {
            const float* p0 = A.row(k) + (i + ii);

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                _mm_store_ps(pp, _mm_loadu_ps(p0));
                pp += 4;
                p0 += M;
            }
        }
    }
#endif // __SSE2__
    for (; ii + 1 < max_ii; ii += 2)
    {
#if __SSE2__
#if __AVX__
#if __AVX512F__
        if (elempack == 16)
        {
            const float* p0 = A.row(k / 16) + (i + ii) * 16;

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                __m512 _r0 = _mm512_load_ps(p0);
                __m512 _r1 = _mm512_load_ps(p0 + 16);
                transpose16x2_ps(_r0, _r1);
                _mm512_store_ps(pp, _r0);
                _mm512_store_ps(pp + 16, _r1);
                pp += 32;
                p0 += M * 16;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[16];
                pp += 2;
                p0 += 1;
            }
        }
#endif // __AVX512F__
        if (elempack == 8)
        {
            const float* p0 = A.row(k / 8) + (i + ii) * 8;

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_load_ps(p0);
                __m256 _r1 = _mm256_load_ps(p0 + 8);
                transpose8x2_ps(_r0, _r1);
                _mm256_store_ps(pp, _r0);
                _mm256_store_ps(pp + 8, _r1);
                pp += 16;
                p0 += M * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[8];
                pp += 2;
                p0 += 1;
            }
        }
#endif // __AVX__
        if (elempack == 4)
        {
            const float* p0 = A.row(k / 4) + (i + ii) * 4;

            int kk = 0;
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_load_ps(p0);
                __m128 _r1 = _mm_load_ps(p0 + 4);
                __m128 _tmp0 = _mm_unpacklo_ps(_r0, _r1);
                __m128 _tmp1 = _mm_unpackhi_ps(_r0, _r1);
                _mm_store_ps(pp, _tmp0);
                _mm_store_ps(pp + 4, _tmp1);
                pp += 8;
                p0 += M * 4;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[4];
                pp += 2;
                p0 += 1;
            }
        }
#endif // __SSE2__
        if (elempack == 1)
        {
            const float* p0 = A.row(k) + (i + ii);

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[1];
                pp += 2;
                p0 += M;
            }
        }
    }
    for (; ii < max_ii; ii += 1)
    {
#if __SSE2__
#if __AVX__
#if __AVX512F__
        if (elempack == 16)
        {
            const float* p0 = A.row(k / 16) + (i + ii) * 16;

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                _mm512_store_ps(pp, _mm512_load_ps(p0));
                pp += 16;
                p0 += M * 16;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp += 1;
                p0 += 1;
            }
        }
#endif // __AVX512F__
        if (elempack == 8)
        {
            const float* p0 = A.row(k / 8) + (i + ii) * 8;

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                _mm256_store_ps(pp, _mm256_load_ps(p0));
                pp += 8;
                p0 += M * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp += 1;
                p0 += 1;
            }
        }
#endif // __AVX__
        if (elempack == 4)
        {
            const float* p0 = A.row(k / 4) + (i + ii) * 4;

            int kk = 0;
            for (; kk + 3 < max_kk; kk += 4)
            {
                _mm_store_ps(pp, _mm_load_ps(p0));
                pp += 4;
                p0 += M * 4;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp += 1;
                p0 += 1;
            }
        }
#endif // __SSE2__
        if (elempack == 1)
        {
            const float* p0 = A.row(k) + (i + ii);

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp += 1;
                p0 += M;
            }
        }
    }
}

static void pack_B_tile(const Mat& B, Mat& BT, int j, int max_jj, int k, int max_kk)
{
    // const int N = B.h * B.elempack;
    // const int K = B.w;
    const int elempack = B.elempack;

    float* pp = BT;

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
                    _mm256_storeu_ps(pp, _mm256_load_ps(p0));
                    _mm_store_ps(pp + 8, _mm_load_ps(p0 + 8));
                    pp += 12;
                    p0 += 16;
                }
            }
            if ((j + jj) % 16 == 4)
            {
                for (int kk = 0; kk < max_kk; kk++)
                {
                    _mm256_storeu_ps(pp, _mm256_loadu_ps(p0 + 4));
                    _mm_store_ps(pp + 8, _mm_load_ps(p0 + 12));
                    pp += 12;
                    p0 += 16;
                }
            }
            if ((j + jj) % 16 == 8)
            {
                for (int kk = 0; kk < max_kk; kk++)
                {
                    _mm256_storeu_ps(pp, _mm256_load_ps(p0 + 8));
                    _mm_store_ps(pp + 8, _mm_load_ps(p1));
                    pp += 12;
                    p0 += 16;
                    p1 += 16;
                }
            }
            if ((j + jj) % 16 == 12)
            {
                for (int kk = 0; kk < max_kk; kk++)
                {
                    _mm_store_ps(pp, _mm_load_ps(p0 + 12));
                    _mm256_storeu_ps(pp + 4, _mm256_load_ps(p1));
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
                    _mm256_storeu_ps(pp, _mm256_load_ps(p0));
                    _mm_store_ps(pp + 8, _mm_load_ps(p1));
                    pp += 12;
                    p0 += 8;
                    p1 += 8;
                }
            }
            if ((j + jj) % 8 == 4)
            {
                for (int kk = 0; kk < max_kk; kk++)
                {
                    _mm_store_ps(pp, _mm_load_ps(p0 + 4));
                    _mm256_storeu_ps(pp + 4, _mm256_load_ps(p1));
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
                _mm_store_ps(pp, _mm_load_ps(p0));
                _mm_store_ps(pp + 4, _mm_load_ps(p1));
                _mm_store_ps(pp + 8, _mm_load_ps(p2));
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

            int kk = 0;
#if __AVX__
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_loadu_ps(p0);
                __m256 _r1 = _mm256_loadu_ps(p1);
                __m256 _r2 = _mm256_loadu_ps(p2);
                __m256 _r3 = _mm256_loadu_ps(p3);
                __m256 _r4 = _mm256_loadu_ps(p4);
                __m256 _r5 = _mm256_loadu_ps(p5);
                __m256 _r6 = _mm256_loadu_ps(p6);
                __m256 _r7 = _mm256_loadu_ps(p7);
                __m256 _r8 = _mm256_loadu_ps(p8);
                __m256 _r9 = _mm256_loadu_ps(p9);
                __m256 _ra = _mm256_loadu_ps(pa);
                __m256 _rb = _mm256_loadu_ps(pb);
                transpose8x12_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7, _r8, _r9, _ra, _rb);
                _mm256_storeu_ps(pp, _r0);
                _mm256_storeu_ps(pp + 8, _r1);
                _mm256_storeu_ps(pp + 8 * 2, _r2);
                _mm256_storeu_ps(pp + 8 * 3, _r3);
                _mm256_storeu_ps(pp + 8 * 4, _r4);
                _mm256_storeu_ps(pp + 8 * 5, _r5);
                _mm256_storeu_ps(pp + 8 * 6, _r6);
                _mm256_storeu_ps(pp + 8 * 7, _r7);
                _mm256_storeu_ps(pp + 8 * 8, _r8);
                _mm256_storeu_ps(pp + 8 * 9, _r9);
                _mm256_storeu_ps(pp + 8 * 10, _ra);
                _mm256_storeu_ps(pp + 8 * 11, _rb);
                pp += 96;
                p0 += 8;
                p1 += 8;
                p2 += 8;
                p3 += 8;
                p4 += 8;
                p5 += 8;
                p6 += 8;
                p7 += 8;
                p8 += 8;
                p9 += 8;
                pa += 8;
                pb += 8;
            }
#endif // __AVX__
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_loadu_ps(p0);
                __m128 _r1 = _mm_loadu_ps(p1);
                __m128 _r2 = _mm_loadu_ps(p2);
                __m128 _r3 = _mm_loadu_ps(p3);
                __m128 _r4 = _mm_loadu_ps(p4);
                __m128 _r5 = _mm_loadu_ps(p5);
                __m128 _r6 = _mm_loadu_ps(p6);
                __m128 _r7 = _mm_loadu_ps(p7);
                __m128 _r8 = _mm_loadu_ps(p8);
                __m128 _r9 = _mm_loadu_ps(p9);
                __m128 _ra = _mm_loadu_ps(pa);
                __m128 _rb = _mm_loadu_ps(pb);
                _MM_TRANSPOSE4_PS(_r0, _r1, _r2, _r3);
                _MM_TRANSPOSE4_PS(_r4, _r5, _r6, _r7);
                _MM_TRANSPOSE4_PS(_r8, _r9, _ra, _rb);
                _mm_store_ps(pp, _r0);
                _mm_store_ps(pp + 4, _r4);
                _mm_store_ps(pp + 4 * 2, _r8);
                _mm_store_ps(pp + 4 * 3, _r1);
                _mm_store_ps(pp + 4 * 4, _r5);
                _mm_store_ps(pp + 4 * 5, _r9);
                _mm_store_ps(pp + 4 * 6, _r2);
                _mm_store_ps(pp + 4 * 7, _r6);
                _mm_store_ps(pp + 4 * 8, _ra);
                _mm_store_ps(pp + 4 * 9, _r3);
                _mm_store_ps(pp + 4 * 10, _r7);
                _mm_store_ps(pp + 4 * 11, _rb);
                pp += 48;
                p0 += 4;
                p1 += 4;
                p2 += 4;
                p3 += 4;
                p4 += 4;
                p5 += 4;
                p6 += 4;
                p7 += 4;
                p8 += 4;
                p9 += 4;
                pa += 4;
                pb += 4;
            }
            for (; kk < max_kk; kk++)
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
                    _mm256_storeu_ps(pp, _mm256_load_ps(p0));
                    pp += 8;
                    p0 += 8;
                }
            }
            if ((j + jj) % 8 == 4)
            {
                for (int kk = 0; kk < max_kk; kk++)
                {
                    _mm_store_ps(pp, _mm_load_ps(p0 + 4));
                    _mm_store_ps(pp + 4, _mm_load_ps(p1));
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
                _mm_store_ps(pp, _mm_load_ps(p0));
                _mm_store_ps(pp + 4, _mm_load_ps(p1));
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

            int kk = 0;
#if __AVX__
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_loadu_ps(p0);
                __m256 _r1 = _mm256_loadu_ps(p1);
                __m256 _r2 = _mm256_loadu_ps(p2);
                __m256 _r3 = _mm256_loadu_ps(p3);
                __m256 _r4 = _mm256_loadu_ps(p4);
                __m256 _r5 = _mm256_loadu_ps(p5);
                __m256 _r6 = _mm256_loadu_ps(p6);
                __m256 _r7 = _mm256_loadu_ps(p7);
                transpose8x8_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7);
                _mm256_storeu_ps(pp, _r0);
                _mm256_storeu_ps(pp + 8, _r1);
                _mm256_storeu_ps(pp + 8 * 2, _r2);
                _mm256_storeu_ps(pp + 8 * 3, _r3);
                _mm256_storeu_ps(pp + 8 * 4, _r4);
                _mm256_storeu_ps(pp + 8 * 5, _r5);
                _mm256_storeu_ps(pp + 8 * 6, _r6);
                _mm256_storeu_ps(pp + 8 * 7, _r7);
                pp += 64;
                p0 += 8;
                p1 += 8;
                p2 += 8;
                p3 += 8;
                p4 += 8;
                p5 += 8;
                p6 += 8;
                p7 += 8;
            }
#endif // __AVX__
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_loadu_ps(p0);
                __m128 _r1 = _mm_loadu_ps(p1);
                __m128 _r2 = _mm_loadu_ps(p2);
                __m128 _r3 = _mm_loadu_ps(p3);
                __m128 _r4 = _mm_loadu_ps(p4);
                __m128 _r5 = _mm_loadu_ps(p5);
                __m128 _r6 = _mm_loadu_ps(p6);
                __m128 _r7 = _mm_loadu_ps(p7);
                _MM_TRANSPOSE4_PS(_r0, _r1, _r2, _r3);
                _MM_TRANSPOSE4_PS(_r4, _r5, _r6, _r7);
                _mm_store_ps(pp, _r0);
                _mm_store_ps(pp + 4, _r4);
                _mm_store_ps(pp + 4 * 2, _r1);
                _mm_store_ps(pp + 4 * 3, _r5);
                _mm_store_ps(pp + 4 * 4, _r2);
                _mm_store_ps(pp + 4 * 5, _r6);
                _mm_store_ps(pp + 4 * 6, _r3);
                _mm_store_ps(pp + 4 * 7, _r7);
                pp += 32;
                p0 += 4;
                p1 += 4;
                p2 += 4;
                p3 += 4;
                p4 += 4;
                p5 += 4;
                p6 += 4;
                p7 += 4;
            }
            for (; kk < max_kk; kk++)
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
                    _mm_store_ps(pp, _mm_load_ps(p0));
                    pp += 4;
                    p0 += 8;
                }
            }
            if ((j + jj) % 8 == 4)
            {
                for (int kk = 0; kk < max_kk; kk++)
                {
                    _mm_store_ps(pp, _mm_load_ps(p0 + 4));
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
                _mm_store_ps(pp, _mm_load_ps(p0));
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

            int kk = 0;
#if __AVX__
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_loadu_ps(p0);
                __m256 _r1 = _mm256_loadu_ps(p1);
                __m256 _r2 = _mm256_loadu_ps(p2);
                __m256 _r3 = _mm256_loadu_ps(p3);
                transpose8x4_ps(_r0, _r1, _r2, _r3);
                _mm256_storeu_ps(pp, _r0);
                _mm256_storeu_ps(pp + 8, _r1);
                _mm256_storeu_ps(pp + 16, _r2);
                _mm256_storeu_ps(pp + 24, _r3);
                pp += 32;
                p0 += 8;
                p1 += 8;
                p2 += 8;
                p3 += 8;
            }
#endif // __AVX__
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_loadu_ps(p0);
                __m128 _r1 = _mm_loadu_ps(p1);
                __m128 _r2 = _mm_loadu_ps(p2);
                __m128 _r3 = _mm_loadu_ps(p3);
                _MM_TRANSPOSE4_PS(_r0, _r1, _r2, _r3);
                _mm_store_ps(pp, _r0);
                _mm_store_ps(pp + 4, _r1);
                _mm_store_ps(pp + 8, _r2);
                _mm_store_ps(pp + 12, _r3);
                pp += 16;
                p0 += 4;
                p1 += 4;
                p2 += 4;
                p3 += 4;
            }
            for (; kk < max_kk; kk++)
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

            int kk = 0;
#if __SSE2__
#if __AVX__
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_loadu_ps(p0);
                __m256 _r1 = _mm256_loadu_ps(p1);
                transpose8x2_ps(_r0, _r1);
                _mm256_storeu_ps(pp, _r0);
                _mm256_storeu_ps(pp + 8, _r1);
                pp += 16;
                p0 += 8;
                p1 += 8;
            }
#endif // __AVX__
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_loadu_ps(p0);
                __m128 _r1 = _mm_loadu_ps(p1);
                __m128 _tmp0 = _mm_unpacklo_ps(_r0, _r1);
                __m128 _tmp1 = _mm_unpackhi_ps(_r0, _r1);
                _mm_store_ps(pp, _tmp0);
                _mm_store_ps(pp + 4, _tmp1);
                pp += 8;
                p0 += 4;
                p1 += 4;
            }
#endif // __SSE2__
            for (; kk < max_kk; kk++)
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

            int kk = 0;
#if __SSE2__
#if __AVX__
            for (; kk + 7 < max_kk; kk += 8)
            {
                _mm256_storeu_ps(pp, _mm256_loadu_ps(p0));
                pp += 8;
                p0 += 8;
            }
#endif // __AVX__
            for (; kk + 3 < max_kk; kk += 4)
            {
                _mm_storeu_ps(pp, _mm_loadu_ps(p0));
                pp += 4;
                p0 += 4;
            }
#endif // __SSE2__
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp += 1;
                p0++;
            }
        }
    }
}

static void transpose_pack_B_tile(const Mat& B, Mat& BT, int j, int max_jj, int k, int max_kk)
{
    const int N = B.w;
    // const int K = B.h * B.elempack;
    const int elempack = B.elempack;

    float* pp = BT;

    int jj = 0;
#if __SSE2__
    for (; jj + 11 < max_jj; jj += 12)
    {
#if __AVX__
#if __AVX512F__
        if (elempack == 16)
        {
            const float* p0 = B.row(k / 16) + (j + jj) * 16;

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                __m512 _r0 = _mm512_load_ps(p0);
                __m512 _r1 = _mm512_load_ps(p0 + 16 * 1);
                __m512 _r2 = _mm512_load_ps(p0 + 16 * 2);
                __m512 _r3 = _mm512_load_ps(p0 + 16 * 3);
                __m512 _r4 = _mm512_load_ps(p0 + 16 * 4);
                __m512 _r5 = _mm512_load_ps(p0 + 16 * 5);
                __m512 _r6 = _mm512_load_ps(p0 + 16 * 6);
                __m512 _r7 = _mm512_load_ps(p0 + 16 * 7);
                __m512 _r8 = _mm512_load_ps(p0 + 16 * 8);
                __m512 _r9 = _mm512_load_ps(p0 + 16 * 9);
                __m512 _ra = _mm512_load_ps(p0 + 16 * 10);
                __m512 _rb = _mm512_load_ps(p0 + 16 * 11);
                transpose16x12_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7, _r8, _r9, _ra, _rb);
                _mm512_store_ps(pp, _r0);
                _mm512_store_ps(pp + 16 * 1, _r1);
                _mm512_store_ps(pp + 16 * 2, _r2);
                _mm512_store_ps(pp + 16 * 3, _r3);
                _mm512_store_ps(pp + 16 * 4, _r4);
                _mm512_store_ps(pp + 16 * 5, _r5);
                _mm512_store_ps(pp + 16 * 6, _r6);
                _mm512_store_ps(pp + 16 * 7, _r7);
                _mm512_store_ps(pp + 16 * 8, _r8);
                _mm512_store_ps(pp + 16 * 9, _r9);
                _mm512_store_ps(pp + 16 * 10, _ra);
                _mm512_store_ps(pp + 16 * 11, _rb);
                pp += 192;
                p0 += N * 16;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[16 * 1];
                pp[2] = p0[16 * 2];
                pp[3] = p0[16 * 3];
                pp[4] = p0[16 * 4];
                pp[5] = p0[16 * 5];
                pp[6] = p0[16 * 6];
                pp[7] = p0[16 * 7];
                pp[8] = p0[16 * 8];
                pp[9] = p0[16 * 9];
                pp[10] = p0[16 * 10];
                pp[11] = p0[16 * 11];
                pp += 12;
                p0 += 1;
            }
        }
#endif // __AVX512F__
        if (elempack == 8)
        {
            const float* p0 = B.row(k / 8) + (j + jj) * 8;

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_load_ps(p0);
                __m256 _r1 = _mm256_load_ps(p0 + 8 * 1);
                __m256 _r2 = _mm256_load_ps(p0 + 8 * 2);
                __m256 _r3 = _mm256_load_ps(p0 + 8 * 3);
                __m256 _r4 = _mm256_load_ps(p0 + 8 * 4);
                __m256 _r5 = _mm256_load_ps(p0 + 8 * 5);
                __m256 _r6 = _mm256_load_ps(p0 + 8 * 6);
                __m256 _r7 = _mm256_load_ps(p0 + 8 * 7);
                __m256 _r8 = _mm256_load_ps(p0 + 8 * 8);
                __m256 _r9 = _mm256_load_ps(p0 + 8 * 9);
                __m256 _ra = _mm256_load_ps(p0 + 8 * 10);
                __m256 _rb = _mm256_load_ps(p0 + 8 * 11);
                transpose8x12_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7, _r8, _r9, _ra, _rb);
                _mm256_store_ps(pp, _r0);
                _mm256_store_ps(pp + 8 * 1, _r1);
                _mm256_store_ps(pp + 8 * 2, _r2);
                _mm256_store_ps(pp + 8 * 3, _r3);
                _mm256_store_ps(pp + 8 * 4, _r4);
                _mm256_store_ps(pp + 8 * 5, _r5);
                _mm256_store_ps(pp + 8 * 6, _r6);
                _mm256_store_ps(pp + 8 * 7, _r7);
                _mm256_store_ps(pp + 8 * 8, _r8);
                _mm256_store_ps(pp + 8 * 9, _r9);
                _mm256_store_ps(pp + 8 * 10, _ra);
                _mm256_store_ps(pp + 8 * 11, _rb);
                pp += 96;
                p0 += N * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[8 * 1];
                pp[2] = p0[8 * 2];
                pp[3] = p0[8 * 3];
                pp[4] = p0[8 * 4];
                pp[5] = p0[8 * 5];
                pp[6] = p0[8 * 6];
                pp[7] = p0[8 * 7];
                pp[8] = p0[8 * 8];
                pp[9] = p0[8 * 9];
                pp[10] = p0[8 * 10];
                pp[11] = p0[8 * 11];
                pp += 12;
                p0 += 1;
            }
        }
#endif // __AVX__
        if (elempack == 4)
        {
            const float* p0 = B.row(k / 4) + (j + jj) * 4;

            int kk = 0;
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_load_ps(p0);
                __m128 _r1 = _mm_load_ps(p0 + 4 * 1);
                __m128 _r2 = _mm_load_ps(p0 + 4 * 2);
                __m128 _r3 = _mm_load_ps(p0 + 4 * 3);
                __m128 _r4 = _mm_load_ps(p0 + 4 * 4);
                __m128 _r5 = _mm_load_ps(p0 + 4 * 5);
                __m128 _r6 = _mm_load_ps(p0 + 4 * 6);
                __m128 _r7 = _mm_load_ps(p0 + 4 * 7);
                __m128 _r8 = _mm_load_ps(p0 + 4 * 8);
                __m128 _r9 = _mm_load_ps(p0 + 4 * 9);
                __m128 _ra = _mm_load_ps(p0 + 4 * 10);
                __m128 _rb = _mm_load_ps(p0 + 4 * 11);
                _MM_TRANSPOSE4_PS(_r0, _r1, _r2, _r3);
                _MM_TRANSPOSE4_PS(_r4, _r5, _r6, _r7);
                _MM_TRANSPOSE4_PS(_r8, _r9, _ra, _rb);
                _mm_store_ps(pp, _r0);
                _mm_store_ps(pp + 4 * 1, _r4);
                _mm_store_ps(pp + 4 * 2, _r8);
                _mm_store_ps(pp + 4 * 3, _r1);
                _mm_store_ps(pp + 4 * 4, _r5);
                _mm_store_ps(pp + 4 * 5, _r9);
                _mm_store_ps(pp + 4 * 6, _r2);
                _mm_store_ps(pp + 4 * 7, _r6);
                _mm_store_ps(pp + 4 * 8, _ra);
                _mm_store_ps(pp + 4 * 9, _r3);
                _mm_store_ps(pp + 4 * 10, _r7);
                _mm_store_ps(pp + 4 * 11, _rb);
                pp += 48;
                p0 += N * 4;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[4 * 1];
                pp[2] = p0[4 * 2];
                pp[3] = p0[4 * 3];
                pp[4] = p0[4 * 4];
                pp[5] = p0[4 * 5];
                pp[6] = p0[4 * 6];
                pp[7] = p0[4 * 7];
                pp[8] = p0[4 * 8];
                pp[9] = p0[4 * 9];
                pp[10] = p0[4 * 10];
                pp[11] = p0[4 * 11];
                pp += 12;
                p0 += 1;
            }
        }
        if (elempack == 1)
        {
            const float* p0 = B.row(k) + (j + jj);

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                _mm_store_ps(pp, _mm_loadu_ps(p0));
                _mm_store_ps(pp + 4, _mm_loadu_ps(p0 + 4));
                _mm_store_ps(pp + 8, _mm_loadu_ps(p0 + 8));
                pp += 12;
                p0 += N;
            }
        }
    }
    for (; jj + 7 < max_jj; jj += 8)
    {
#if __AVX__
#if __AVX512F__
        if (elempack == 16)
        {
            const float* p0 = B.row(k / 16) + (j + jj) * 16;

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                __m512 _r0 = _mm512_load_ps(p0);
                __m512 _r1 = _mm512_load_ps(p0 + 16 * 1);
                __m512 _r2 = _mm512_load_ps(p0 + 16 * 2);
                __m512 _r3 = _mm512_load_ps(p0 + 16 * 3);
                __m512 _r4 = _mm512_load_ps(p0 + 16 * 4);
                __m512 _r5 = _mm512_load_ps(p0 + 16 * 5);
                __m512 _r6 = _mm512_load_ps(p0 + 16 * 6);
                __m512 _r7 = _mm512_load_ps(p0 + 16 * 7);
                transpose16x8_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7);
                _mm512_store_ps(pp, _r0);
                _mm512_store_ps(pp + 16 * 1, _r1);
                _mm512_store_ps(pp + 16 * 2, _r2);
                _mm512_store_ps(pp + 16 * 3, _r3);
                _mm512_store_ps(pp + 16 * 4, _r4);
                _mm512_store_ps(pp + 16 * 5, _r5);
                _mm512_store_ps(pp + 16 * 6, _r6);
                _mm512_store_ps(pp + 16 * 7, _r7);
                pp += 128;
                p0 += N * 16;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[16 * 1];
                pp[2] = p0[16 * 2];
                pp[3] = p0[16 * 3];
                pp[4] = p0[16 * 4];
                pp[5] = p0[16 * 5];
                pp[6] = p0[16 * 6];
                pp[7] = p0[16 * 7];
                pp += 8;
                p0 += 1;
            }
        }
#endif // __AVX512F__
        if (elempack == 8)
        {
            const float* p0 = B.row(k / 8) + (j + jj) * 8;

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_load_ps(p0);
                __m256 _r1 = _mm256_load_ps(p0 + 8 * 1);
                __m256 _r2 = _mm256_load_ps(p0 + 8 * 2);
                __m256 _r3 = _mm256_load_ps(p0 + 8 * 3);
                __m256 _r4 = _mm256_load_ps(p0 + 8 * 4);
                __m256 _r5 = _mm256_load_ps(p0 + 8 * 5);
                __m256 _r6 = _mm256_load_ps(p0 + 8 * 6);
                __m256 _r7 = _mm256_load_ps(p0 + 8 * 7);
                transpose8x8_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7);
                _mm256_store_ps(pp, _r0);
                _mm256_store_ps(pp + 8 * 1, _r1);
                _mm256_store_ps(pp + 8 * 2, _r2);
                _mm256_store_ps(pp + 8 * 3, _r3);
                _mm256_store_ps(pp + 8 * 4, _r4);
                _mm256_store_ps(pp + 8 * 5, _r5);
                _mm256_store_ps(pp + 8 * 6, _r6);
                _mm256_store_ps(pp + 8 * 7, _r7);
                pp += 64;
                p0 += N * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[8 * 1];
                pp[2] = p0[8 * 2];
                pp[3] = p0[8 * 3];
                pp[4] = p0[8 * 4];
                pp[5] = p0[8 * 5];
                pp[6] = p0[8 * 6];
                pp[7] = p0[8 * 7];
                pp += 8;
                p0 += 1;
            }
        }
#endif // __AVX__
        if (elempack == 4)
        {
            const float* p0 = B.row(k / 4) + (j + jj) * 4;

            int kk = 0;
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_load_ps(p0);
                __m128 _r1 = _mm_load_ps(p0 + 4 * 1);
                __m128 _r2 = _mm_load_ps(p0 + 4 * 2);
                __m128 _r3 = _mm_load_ps(p0 + 4 * 3);
                __m128 _r4 = _mm_load_ps(p0 + 4 * 4);
                __m128 _r5 = _mm_load_ps(p0 + 4 * 5);
                __m128 _r6 = _mm_load_ps(p0 + 4 * 6);
                __m128 _r7 = _mm_load_ps(p0 + 4 * 7);
                _MM_TRANSPOSE4_PS(_r0, _r1, _r2, _r3);
                _MM_TRANSPOSE4_PS(_r4, _r5, _r6, _r7);
                _mm_store_ps(pp, _r0);
                _mm_store_ps(pp + 4 * 1, _r4);
                _mm_store_ps(pp + 4 * 2, _r1);
                _mm_store_ps(pp + 4 * 3, _r5);
                _mm_store_ps(pp + 4 * 4, _r2);
                _mm_store_ps(pp + 4 * 5, _r6);
                _mm_store_ps(pp + 4 * 6, _r3);
                _mm_store_ps(pp + 4 * 7, _r7);
                pp += 32;
                p0 += N * 4;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[4 * 1];
                pp[2] = p0[4 * 2];
                pp[3] = p0[4 * 3];
                pp[4] = p0[4 * 4];
                pp[5] = p0[4 * 5];
                pp[6] = p0[4 * 6];
                pp[7] = p0[4 * 7];
                pp += 8;
                p0 += 1;
            }
        }
        if (elempack == 1)
        {
            const float* p0 = B.row(k) + (j + jj);

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                _mm_store_ps(pp, _mm_loadu_ps(p0));
                _mm_store_ps(pp + 4, _mm_loadu_ps(p0 + 4));
                pp += 8;
                p0 += N;
            }
        }
    }
    for (; jj + 3 < max_jj; jj += 4)
    {
#if __AVX__
#if __AVX512F__
        if (elempack == 16)
        {
            const float* p0 = B.row(k / 16) + (j + jj) * 16;

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                __m512 _r0 = _mm512_load_ps(p0);
                __m512 _r1 = _mm512_load_ps(p0 + 16 * 1);
                __m512 _r2 = _mm512_load_ps(p0 + 16 * 2);
                __m512 _r3 = _mm512_load_ps(p0 + 16 * 3);
                transpose16x4_ps(_r0, _r1, _r2, _r3);
                _mm512_store_ps(pp, _r0);
                _mm512_store_ps(pp + 16 * 1, _r1);
                _mm512_store_ps(pp + 16 * 2, _r2);
                _mm512_store_ps(pp + 16 * 3, _r3);
                pp += 64;
                p0 += N * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[16 * 1];
                pp[2] = p0[16 * 2];
                pp[3] = p0[16 * 3];
                pp += 4;
                p0 += 1;
            }
        }
#endif // __AVX512F__
        if (elempack == 8)
        {
            const float* p0 = B.row(k / 8) + (j + jj) * 8;

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_load_ps(p0);
                __m256 _r1 = _mm256_load_ps(p0 + 8 * 1);
                __m256 _r2 = _mm256_load_ps(p0 + 8 * 2);
                __m256 _r3 = _mm256_load_ps(p0 + 8 * 3);
                transpose8x4_ps(_r0, _r1, _r2, _r3);
                _mm256_store_ps(pp, _r0);
                _mm256_store_ps(pp + 8 * 1, _r1);
                _mm256_store_ps(pp + 8 * 2, _r2);
                _mm256_store_ps(pp + 8 * 3, _r3);
                pp += 32;
                p0 += N * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[8 * 1];
                pp[2] = p0[8 * 2];
                pp[3] = p0[8 * 3];
                pp += 4;
                p0 += 1;
            }
        }
#endif // __AVX__
        if (elempack == 4)
        {
            const float* p0 = B.row(k / 4) + (j + jj) * 4;

            int kk = 0;
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_load_ps(p0);
                __m128 _r1 = _mm_load_ps(p0 + 4 * 1);
                __m128 _r2 = _mm_load_ps(p0 + 4 * 2);
                __m128 _r3 = _mm_load_ps(p0 + 4 * 3);
                _MM_TRANSPOSE4_PS(_r0, _r1, _r2, _r3);
                _mm_store_ps(pp, _r0);
                _mm_store_ps(pp + 4 * 1, _r1);
                _mm_store_ps(pp + 4 * 2, _r2);
                _mm_store_ps(pp + 4 * 3, _r3);
                pp += 16;
                p0 += N * 4;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[4 * 1];
                pp[2] = p0[4 * 2];
                pp[3] = p0[4 * 3];
                pp += 4;
                p0 += 1;
            }
        }
        if (elempack == 1)
        {
            const float* p0 = B.row(k) + (j + jj);

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                _mm_store_ps(pp, _mm_loadu_ps(p0));
                pp += 4;
                p0 += N;
            }
        }
    }
#endif // __SSE2__
    for (; jj + 1 < max_jj; jj += 2)
    {
#if __SSE2__
#if __AVX__
#if __AVX512F__
        if (elempack == 16)
        {
            const float* p0 = B.row(k / 16) + (j + jj) * 16;

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                __m512 _r0 = _mm512_load_ps(p0);
                __m512 _r1 = _mm512_load_ps(p0 + 16);
                transpose16x2_ps(_r0, _r1);
                _mm512_store_ps(pp, _r0);
                _mm512_store_ps(pp + 16, _r1);
                pp += 32;
                p0 += N * 16;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[16];
                pp += 2;
                p0 += 1;
            }
        }
#endif // __AVX512F__
        if (elempack == 8)
        {
            const float* p0 = B.row(k / 8) + (j + jj) * 8;

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                __m256 _r0 = _mm256_load_ps(p0);
                __m256 _r1 = _mm256_load_ps(p0 + 8);
                transpose8x2_ps(_r0, _r1);
                _mm256_store_ps(pp, _r0);
                _mm256_store_ps(pp + 8, _r1);
                pp += 16;
                p0 += N * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[8];
                pp += 2;
                p0 += 1;
            }
        }
#endif // __AVX__
        if (elempack == 4)
        {
            const float* p0 = B.row(k / 4) + (j + jj) * 4;

            int kk = 0;
            for (; kk + 3 < max_kk; kk += 4)
            {
                __m128 _r0 = _mm_load_ps(p0);
                __m128 _r1 = _mm_load_ps(p0 + 4);
                __m128 _tmp0 = _mm_unpacklo_ps(_r0, _r1);
                __m128 _tmp1 = _mm_unpackhi_ps(_r0, _r1);
                _mm_store_ps(pp, _tmp0);
                _mm_store_ps(pp + 4, _tmp1);
                pp += 8;
                p0 += N * 4;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[4];
                pp += 2;
                p0 += 1;
            }
        }
#endif // __SSE2__
        if (elempack == 1)
        {
            const float* p0 = B.row(k) + (j + jj);

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[1];
                pp += 2;
                p0 += N;
            }
        }
    }
    for (; jj < max_jj; jj += 1)
    {
#if __SSE2__
#if __AVX__
#if __AVX512F__
        if (elempack == 16)
        {
            const float* p0 = B.row(k / 16) + (j + jj) * 16;

            int kk = 0;
            for (; kk + 15 < max_kk; kk += 16)
            {
                _mm512_store_ps(pp, _mm512_load_ps(p0));
                pp += 16;
                p0 += N * 16;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp += 1;
                p0 += 1;
            }
        }
#endif // __AVX512F__
        if (elempack == 8)
        {
            const float* p0 = B.row(k / 8) + (j + jj) * 8;

            int kk = 0;
            for (; kk + 7 < max_kk; kk += 8)
            {
                _mm256_store_ps(pp, _mm256_load_ps(p0));
                pp += 8;
                p0 += N * 8;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp += 1;
                p0 += 1;
            }
        }
#endif // __AVX__
        if (elempack == 4)
        {
            const float* p0 = B.row(k / 4) + (j + jj) * 4;

            int kk = 0;
            for (; kk + 3 < max_kk; kk += 4)
            {
                _mm_store_ps(pp, _mm_load_ps(p0));
                pp += 4;
                p0 += N * 4;
            }
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp += 1;
                p0 += 1;
            }
        }
#endif // __SSE2__
        if (elempack == 1)
        {
            const float* p0 = B.row(k) + (j + jj);

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp += 1;
                p0 += N;
            }
        }
    }
}

static void matmul_packed_transB_tile(const Mat& AT_tile, const Mat& BT_tile, const Mat& C, Mat& top_blob, int broadcast_type_C, Mat& tmp, float alpha, float beta, int i, int max_ii, int j, int max_jj, int k, int max_kk, bool k_end)
{
    const int out_elempack = top_blob.elempack;
    const int N = top_blob.w;

    const float* pA0 = AT_tile;
    const float* pB0 = BT_tile;

    float* ptmp = tmp;

    int ii = 0;
#if __SSE2__
#if __AVX__
#if __AVX512F__
    for (; ii + 15 < max_ii; ii += 16)
    {
        float* outptr0 = top_blob.row((i + ii) / out_elempack) + j * out_elempack;

        const float* pB = pB0;

        const float* pC = C;
        if (pC)
        {
            if (broadcast_type_C == 1 || broadcast_type_C == 2)
            {
                pC = pC + i + ii;
            }
            if (broadcast_type_C == 3)
            {
                pC = C.row((i + ii) / out_elempack) + j * out_elempack;
            }
            if (broadcast_type_C == 4)
            {
                pC = pC + j;
            }
        }

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
                        _sum0 = _mm512_loadu_ps(pC);
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
                            _sum0 = _mm512_loadu_ps(pC);
                            _sum1 = _mm512_loadu_ps(pC + 16);
                            _sum2 = _mm512_loadu_ps(pC + 16 * 2);
                            _sum3 = _mm512_loadu_ps(pC + 16 * 3);
                            _sum4 = _mm512_loadu_ps(pC + 16 * 4);
                            _sum5 = _mm512_loadu_ps(pC + 16 * 5);
                            _sum6 = _mm512_loadu_ps(pC + 16 * 6);
                            _sum7 = _mm512_loadu_ps(pC + 16 * 7);
                            _sum8 = _mm512_loadu_ps(pC + 16 * 8);
                            _sum9 = _mm512_loadu_ps(pC + 16 * 9);
                            _suma = _mm512_loadu_ps(pC + 16 * 10);
                            _sumb = _mm512_loadu_ps(pC + 16 * 11);
                            pC += 192;
                        }
                        if (out_elempack == 8)
                        {
                            __m256 _sum0_0 = _mm256_loadu_ps(pC);
                            __m256 _sum1_0 = _mm256_loadu_ps(pC + 8);
                            __m256 _sum2_0 = _mm256_loadu_ps(pC + 8 * 2);
                            __m256 _sum3_0 = _mm256_loadu_ps(pC + 8 * 3);
                            __m256 _sum4_0 = _mm256_loadu_ps(pC + 8 * 4);
                            __m256 _sum5_0 = _mm256_loadu_ps(pC + 8 * 5);
                            __m256 _sum6_0 = _mm256_loadu_ps(pC + 8 * 6);
                            __m256 _sum7_0 = _mm256_loadu_ps(pC + 8 * 7);
                            __m256 _sum8_0 = _mm256_loadu_ps(pC + 8 * 8);
                            __m256 _sum9_0 = _mm256_loadu_ps(pC + 8 * 9);
                            __m256 _suma_0 = _mm256_loadu_ps(pC + 8 * 10);
                            __m256 _sumb_0 = _mm256_loadu_ps(pC + 8 * 11);
                            __m256 _sum0_1 = _mm256_loadu_ps(pC + N * 8);
                            __m256 _sum1_1 = _mm256_loadu_ps(pC + N * 8 + 8);
                            __m256 _sum2_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 2);
                            __m256 _sum3_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 3);
                            __m256 _sum4_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 4);
                            __m256 _sum5_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 5);
                            __m256 _sum6_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 6);
                            __m256 _sum7_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 7);
                            __m256 _sum8_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 8);
                            __m256 _sum9_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 9);
                            __m256 _suma_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 10);
                            __m256 _sumb_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 11);
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
                            pC += 96;
                        }
                        if (out_elempack == 4)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + 4);
                            __m128 _sum2_0 = _mm_loadu_ps(pC + 4 * 2);
                            __m128 _sum3_0 = _mm_loadu_ps(pC + 4 * 3);
                            __m128 _sum4_0 = _mm_loadu_ps(pC + 4 * 4);
                            __m128 _sum5_0 = _mm_loadu_ps(pC + 4 * 5);
                            __m128 _sum6_0 = _mm_loadu_ps(pC + 4 * 6);
                            __m128 _sum7_0 = _mm_loadu_ps(pC + 4 * 7);
                            __m128 _sum8_0 = _mm_loadu_ps(pC + 4 * 8);
                            __m128 _sum9_0 = _mm_loadu_ps(pC + 4 * 9);
                            __m128 _suma_0 = _mm_loadu_ps(pC + 4 * 10);
                            __m128 _sumb_0 = _mm_loadu_ps(pC + 4 * 11);
                            __m128 _sum0_1 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum1_1 = _mm_loadu_ps(pC + N * 4 + 4);
                            __m128 _sum2_1 = _mm_loadu_ps(pC + N * 4 + 4 * 2);
                            __m128 _sum3_1 = _mm_loadu_ps(pC + N * 4 + 4 * 3);
                            __m128 _sum4_1 = _mm_loadu_ps(pC + N * 4 + 4 * 4);
                            __m128 _sum5_1 = _mm_loadu_ps(pC + N * 4 + 4 * 5);
                            __m128 _sum6_1 = _mm_loadu_ps(pC + N * 4 + 4 * 6);
                            __m128 _sum7_1 = _mm_loadu_ps(pC + N * 4 + 4 * 7);
                            __m128 _sum8_1 = _mm_loadu_ps(pC + N * 4 + 4 * 8);
                            __m128 _sum9_1 = _mm_loadu_ps(pC + N * 4 + 4 * 9);
                            __m128 _suma_1 = _mm_loadu_ps(pC + N * 4 + 4 * 10);
                            __m128 _sumb_1 = _mm_loadu_ps(pC + N * 4 + 4 * 11);
                            __m128 _sum0_2 = _mm_loadu_ps(pC + N * 8);
                            __m128 _sum1_2 = _mm_loadu_ps(pC + N * 8 + 4);
                            __m128 _sum2_2 = _mm_loadu_ps(pC + N * 8 + 4 * 2);
                            __m128 _sum3_2 = _mm_loadu_ps(pC + N * 8 + 4 * 3);
                            __m128 _sum4_2 = _mm_loadu_ps(pC + N * 8 + 4 * 4);
                            __m128 _sum5_2 = _mm_loadu_ps(pC + N * 8 + 4 * 5);
                            __m128 _sum6_2 = _mm_loadu_ps(pC + N * 8 + 4 * 6);
                            __m128 _sum7_2 = _mm_loadu_ps(pC + N * 8 + 4 * 7);
                            __m128 _sum8_2 = _mm_loadu_ps(pC + N * 8 + 4 * 8);
                            __m128 _sum9_2 = _mm_loadu_ps(pC + N * 8 + 4 * 9);
                            __m128 _suma_2 = _mm_loadu_ps(pC + N * 8 + 4 * 10);
                            __m128 _sumb_2 = _mm_loadu_ps(pC + N * 8 + 4 * 11);
                            __m128 _sum0_3 = _mm_loadu_ps(pC + N * 12);
                            __m128 _sum1_3 = _mm_loadu_ps(pC + N * 12 + 4);
                            __m128 _sum2_3 = _mm_loadu_ps(pC + N * 12 + 4 * 2);
                            __m128 _sum3_3 = _mm_loadu_ps(pC + N * 12 + 4 * 3);
                            __m128 _sum4_3 = _mm_loadu_ps(pC + N * 12 + 4 * 4);
                            __m128 _sum5_3 = _mm_loadu_ps(pC + N * 12 + 4 * 5);
                            __m128 _sum6_3 = _mm_loadu_ps(pC + N * 12 + 4 * 6);
                            __m128 _sum7_3 = _mm_loadu_ps(pC + N * 12 + 4 * 7);
                            __m128 _sum8_3 = _mm_loadu_ps(pC + N * 12 + 4 * 8);
                            __m128 _sum9_3 = _mm_loadu_ps(pC + N * 12 + 4 * 9);
                            __m128 _suma_3 = _mm_loadu_ps(pC + N * 12 + 4 * 10);
                            __m128 _sumb_3 = _mm_loadu_ps(pC + N * 12 + 4 * 11);
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
                            pC += 48;
                        }
                        if (out_elempack == 1)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + N);
                            __m128 _sum2_0 = _mm_loadu_ps(pC + N * 2);
                            __m128 _sum3_0 = _mm_loadu_ps(pC + N * 3);
                            __m128 _sum4_0 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum5_0 = _mm_loadu_ps(pC + N * 5);
                            __m128 _sum6_0 = _mm_loadu_ps(pC + N * 6);
                            __m128 _sum7_0 = _mm_loadu_ps(pC + N * 7);
                            __m128 _sum8_0 = _mm_loadu_ps(pC + N * 8);
                            __m128 _sum9_0 = _mm_loadu_ps(pC + N * 9);
                            __m128 _suma_0 = _mm_loadu_ps(pC + N * 10);
                            __m128 _sumb_0 = _mm_loadu_ps(pC + N * 11);
                            __m128 _sumc_0 = _mm_loadu_ps(pC + N * 12);
                            __m128 _sumd_0 = _mm_loadu_ps(pC + N * 13);
                            __m128 _sume_0 = _mm_loadu_ps(pC + N * 14);
                            __m128 _sumf_0 = _mm_loadu_ps(pC + N * 15);

                            __m128 _sum0_1 = _mm_loadu_ps(pC + 4);
                            __m128 _sum1_1 = _mm_loadu_ps(pC + N + 4);
                            __m128 _sum2_1 = _mm_loadu_ps(pC + N * 2 + 4);
                            __m128 _sum3_1 = _mm_loadu_ps(pC + N * 3 + 4);
                            __m128 _sum4_1 = _mm_loadu_ps(pC + N * 4 + 4);
                            __m128 _sum5_1 = _mm_loadu_ps(pC + N * 5 + 4);
                            __m128 _sum6_1 = _mm_loadu_ps(pC + N * 6 + 4);
                            __m128 _sum7_1 = _mm_loadu_ps(pC + N * 7 + 4);
                            __m128 _sum8_1 = _mm_loadu_ps(pC + N * 8 + 4);
                            __m128 _sum9_1 = _mm_loadu_ps(pC + N * 9 + 4);
                            __m128 _suma_1 = _mm_loadu_ps(pC + N * 10 + 4);
                            __m128 _sumb_1 = _mm_loadu_ps(pC + N * 11 + 4);
                            __m128 _sumc_1 = _mm_loadu_ps(pC + N * 12 + 4);
                            __m128 _sumd_1 = _mm_loadu_ps(pC + N * 13 + 4);
                            __m128 _sume_1 = _mm_loadu_ps(pC + N * 14 + 4);
                            __m128 _sumf_1 = _mm_loadu_ps(pC + N * 15 + 4);

                            __m128 _sum0_2 = _mm_loadu_ps(pC + 8);
                            __m128 _sum1_2 = _mm_loadu_ps(pC + N + 8);
                            __m128 _sum2_2 = _mm_loadu_ps(pC + N * 2 + 8);
                            __m128 _sum3_2 = _mm_loadu_ps(pC + N * 3 + 8);
                            __m128 _sum4_2 = _mm_loadu_ps(pC + N * 4 + 8);
                            __m128 _sum5_2 = _mm_loadu_ps(pC + N * 5 + 8);
                            __m128 _sum6_2 = _mm_loadu_ps(pC + N * 6 + 8);
                            __m128 _sum7_2 = _mm_loadu_ps(pC + N * 7 + 8);
                            __m128 _sum8_2 = _mm_loadu_ps(pC + N * 8 + 8);
                            __m128 _sum9_2 = _mm_loadu_ps(pC + N * 9 + 8);
                            __m128 _suma_2 = _mm_loadu_ps(pC + N * 10 + 8);
                            __m128 _sumb_2 = _mm_loadu_ps(pC + N * 11 + 8);
                            __m128 _sumc_2 = _mm_loadu_ps(pC + N * 12 + 8);
                            __m128 _sumd_2 = _mm_loadu_ps(pC + N * 13 + 8);
                            __m128 _sume_2 = _mm_loadu_ps(pC + N * 14 + 8);
                            __m128 _sumf_2 = _mm_loadu_ps(pC + N * 15 + 8);

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
                            pC += 12;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm512_set1_ps(pC[0]);
                        _sum1 = _mm512_set1_ps(pC[1]);
                        _sum2 = _mm512_set1_ps(pC[2]);
                        _sum3 = _mm512_set1_ps(pC[3]);
                        _sum4 = _mm512_set1_ps(pC[4]);
                        _sum5 = _mm512_set1_ps(pC[5]);
                        _sum6 = _mm512_set1_ps(pC[6]);
                        _sum7 = _mm512_set1_ps(pC[7]);
                        _sum8 = _mm512_set1_ps(pC[8]);
                        _sum9 = _mm512_set1_ps(pC[9]);
                        _suma = _mm512_set1_ps(pC[10]);
                        _sumb = _mm512_set1_ps(pC[11]);
                        pC += 12;
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

            if (k_end)
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
                    _mm512_store_ps(outptr0, _sum0);
                    _mm512_store_ps(outptr0 + 16 * 1, _sum1);
                    _mm512_store_ps(outptr0 + 16 * 2, _sum2);
                    _mm512_store_ps(outptr0 + 16 * 3, _sum3);
                    _mm512_store_ps(outptr0 + 16 * 4, _sum4);
                    _mm512_store_ps(outptr0 + 16 * 5, _sum5);
                    _mm512_store_ps(outptr0 + 16 * 6, _sum6);
                    _mm512_store_ps(outptr0 + 16 * 7, _sum7);
                    _mm512_store_ps(outptr0 + 16 * 8, _sum8);
                    _mm512_store_ps(outptr0 + 16 * 9, _sum9);
                    _mm512_store_ps(outptr0 + 16 * 10, _suma);
                    _mm512_store_ps(outptr0 + 16 * 11, _sumb);
                    outptr0 += 192;
                }
                if (out_elempack == 8)
                {
                    _mm256_store_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                    _mm256_store_ps(outptr0 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 0));
                    _mm256_store_ps(outptr0 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 0));
                    _mm256_store_ps(outptr0 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 0));
                    _mm256_store_ps(outptr0 + 8 * 4, _mm512_extractf32x8_ps(_sum4, 0));
                    _mm256_store_ps(outptr0 + 8 * 5, _mm512_extractf32x8_ps(_sum5, 0));
                    _mm256_store_ps(outptr0 + 8 * 6, _mm512_extractf32x8_ps(_sum6, 0));
                    _mm256_store_ps(outptr0 + 8 * 7, _mm512_extractf32x8_ps(_sum7, 0));
                    _mm256_store_ps(outptr0 + 8 * 8, _mm512_extractf32x8_ps(_sum8, 0));
                    _mm256_store_ps(outptr0 + 8 * 9, _mm512_extractf32x8_ps(_sum9, 0));
                    _mm256_store_ps(outptr0 + 8 * 10, _mm512_extractf32x8_ps(_suma, 0));
                    _mm256_store_ps(outptr0 + 8 * 11, _mm512_extractf32x8_ps(_sumb, 0));

                    _mm256_store_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum0, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 4, _mm512_extractf32x8_ps(_sum4, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 5, _mm512_extractf32x8_ps(_sum5, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 6, _mm512_extractf32x8_ps(_sum6, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 7, _mm512_extractf32x8_ps(_sum7, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 8, _mm512_extractf32x8_ps(_sum8, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 9, _mm512_extractf32x8_ps(_sum9, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 10, _mm512_extractf32x8_ps(_suma, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 11, _mm512_extractf32x8_ps(_sumb, 1));

                    outptr0 += 96;
                }
                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _mm512_extractf32x4_ps(_sum0, 0));
                    _mm_store_ps(outptr0 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 0));
                    _mm_store_ps(outptr0 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 0));
                    _mm_store_ps(outptr0 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 0));
                    _mm_store_ps(outptr0 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 0));
                    _mm_store_ps(outptr0 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 0));
                    _mm_store_ps(outptr0 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 0));
                    _mm_store_ps(outptr0 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 0));
                    _mm_store_ps(outptr0 + 4 * 8, _mm512_extractf32x4_ps(_sum8, 0));
                    _mm_store_ps(outptr0 + 4 * 9, _mm512_extractf32x4_ps(_sum9, 0));
                    _mm_store_ps(outptr0 + 4 * 10, _mm512_extractf32x4_ps(_suma, 0));
                    _mm_store_ps(outptr0 + 4 * 11, _mm512_extractf32x4_ps(_sumb, 0));

                    _mm_store_ps(outptr0 + N * 4, _mm512_extractf32x4_ps(_sum0, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 8, _mm512_extractf32x4_ps(_sum8, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 9, _mm512_extractf32x4_ps(_sum9, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 10, _mm512_extractf32x4_ps(_suma, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 11, _mm512_extractf32x4_ps(_sumb, 1));

                    _mm_store_ps(outptr0 + N * 8, _mm512_extractf32x4_ps(_sum0, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 8, _mm512_extractf32x4_ps(_sum8, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 9, _mm512_extractf32x4_ps(_sum9, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 10, _mm512_extractf32x4_ps(_suma, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 11, _mm512_extractf32x4_ps(_sumb, 2));

                    _mm_store_ps(outptr0 + N * 12, _mm512_extractf32x4_ps(_sum0, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 8, _mm512_extractf32x4_ps(_sum8, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 9, _mm512_extractf32x4_ps(_sum9, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 10, _mm512_extractf32x4_ps(_suma, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 11, _mm512_extractf32x4_ps(_sumb, 3));

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
                        _sum0 = _mm512_loadu_ps(pC);
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
                            _sum0 = _mm512_loadu_ps(pC);
                            _sum1 = _mm512_loadu_ps(pC + 16);
                            _sum2 = _mm512_loadu_ps(pC + 16 * 2);
                            _sum3 = _mm512_loadu_ps(pC + 16 * 3);
                            _sum4 = _mm512_loadu_ps(pC + 16 * 4);
                            _sum5 = _mm512_loadu_ps(pC + 16 * 5);
                            _sum6 = _mm512_loadu_ps(pC + 16 * 6);
                            _sum7 = _mm512_loadu_ps(pC + 16 * 7);
                            pC += 128;
                        }
                        if (out_elempack == 8)
                        {
                            __m256 _sum0_0 = _mm256_loadu_ps(pC);
                            __m256 _sum1_0 = _mm256_loadu_ps(pC + 8);
                            __m256 _sum2_0 = _mm256_loadu_ps(pC + 8 * 2);
                            __m256 _sum3_0 = _mm256_loadu_ps(pC + 8 * 3);
                            __m256 _sum4_0 = _mm256_loadu_ps(pC + 8 * 4);
                            __m256 _sum5_0 = _mm256_loadu_ps(pC + 8 * 5);
                            __m256 _sum6_0 = _mm256_loadu_ps(pC + 8 * 6);
                            __m256 _sum7_0 = _mm256_loadu_ps(pC + 8 * 7);
                            __m256 _sum0_1 = _mm256_loadu_ps(pC + N * 8);
                            __m256 _sum1_1 = _mm256_loadu_ps(pC + N * 8 + 8);
                            __m256 _sum2_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 2);
                            __m256 _sum3_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 3);
                            __m256 _sum4_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 4);
                            __m256 _sum5_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 5);
                            __m256 _sum6_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 6);
                            __m256 _sum7_1 = _mm256_loadu_ps(pC + N * 8 + 8 * 7);
                            _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum0_0), _sum0_1, 1);
                            _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum1_0), _sum1_1, 1);
                            _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum2_0), _sum2_1, 1);
                            _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum3_0), _sum3_1, 1);
                            _sum4 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum4_0), _sum4_1, 1);
                            _sum5 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum5_0), _sum5_1, 1);
                            _sum6 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum6_0), _sum6_1, 1);
                            _sum7 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum7_0), _sum7_1, 1);
                            pC += 64;
                        }
                        if (out_elempack == 4)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + 4);
                            __m128 _sum2_0 = _mm_loadu_ps(pC + 4 * 2);
                            __m128 _sum3_0 = _mm_loadu_ps(pC + 4 * 3);
                            __m128 _sum4_0 = _mm_loadu_ps(pC + 4 * 4);
                            __m128 _sum5_0 = _mm_loadu_ps(pC + 4 * 5);
                            __m128 _sum6_0 = _mm_loadu_ps(pC + 4 * 6);
                            __m128 _sum7_0 = _mm_loadu_ps(pC + 4 * 7);
                            __m128 _sum0_1 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum1_1 = _mm_loadu_ps(pC + N * 4 + 4);
                            __m128 _sum2_1 = _mm_loadu_ps(pC + N * 4 + 4 * 2);
                            __m128 _sum3_1 = _mm_loadu_ps(pC + N * 4 + 4 * 3);
                            __m128 _sum4_1 = _mm_loadu_ps(pC + N * 4 + 4 * 4);
                            __m128 _sum5_1 = _mm_loadu_ps(pC + N * 4 + 4 * 5);
                            __m128 _sum6_1 = _mm_loadu_ps(pC + N * 4 + 4 * 6);
                            __m128 _sum7_1 = _mm_loadu_ps(pC + N * 4 + 4 * 7);
                            __m128 _sum0_2 = _mm_loadu_ps(pC + N * 8);
                            __m128 _sum1_2 = _mm_loadu_ps(pC + N * 8 + 4);
                            __m128 _sum2_2 = _mm_loadu_ps(pC + N * 8 + 4 * 2);
                            __m128 _sum3_2 = _mm_loadu_ps(pC + N * 8 + 4 * 3);
                            __m128 _sum4_2 = _mm_loadu_ps(pC + N * 8 + 4 * 4);
                            __m128 _sum5_2 = _mm_loadu_ps(pC + N * 8 + 4 * 5);
                            __m128 _sum6_2 = _mm_loadu_ps(pC + N * 8 + 4 * 6);
                            __m128 _sum7_2 = _mm_loadu_ps(pC + N * 8 + 4 * 7);
                            __m128 _sum0_3 = _mm_loadu_ps(pC + N * 12);
                            __m128 _sum1_3 = _mm_loadu_ps(pC + N * 12 + 4);
                            __m128 _sum2_3 = _mm_loadu_ps(pC + N * 12 + 4 * 2);
                            __m128 _sum3_3 = _mm_loadu_ps(pC + N * 12 + 4 * 3);
                            __m128 _sum4_3 = _mm_loadu_ps(pC + N * 12 + 4 * 4);
                            __m128 _sum5_3 = _mm_loadu_ps(pC + N * 12 + 4 * 5);
                            __m128 _sum6_3 = _mm_loadu_ps(pC + N * 12 + 4 * 6);
                            __m128 _sum7_3 = _mm_loadu_ps(pC + N * 12 + 4 * 7);
                            _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_2), _sum0_3, 1), 1);
                            _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_2), _sum1_3, 1), 1);
                            _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum2_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_2), _sum2_3, 1), 1);
                            _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum3_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_2), _sum3_3, 1), 1);
                            _sum4 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum4_0), _sum4_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum4_2), _sum4_3, 1), 1);
                            _sum5 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum5_0), _sum5_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum5_2), _sum5_3, 1), 1);
                            _sum6 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum6_0), _sum6_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum6_2), _sum6_3, 1), 1);
                            _sum7 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum7_0), _sum7_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum7_2), _sum7_3, 1), 1);
                            pC += 32;
                        }
                        if (out_elempack == 1)
                        {
                            __m256 _r0 = _mm256_loadu_ps(pC);
                            __m256 _r1 = _mm256_loadu_ps(pC + N);
                            __m256 _r2 = _mm256_loadu_ps(pC + N * 2);
                            __m256 _r3 = _mm256_loadu_ps(pC + N * 3);
                            __m256 _r4 = _mm256_loadu_ps(pC + N * 4);
                            __m256 _r5 = _mm256_loadu_ps(pC + N * 5);
                            __m256 _r6 = _mm256_loadu_ps(pC + N * 6);
                            __m256 _r7 = _mm256_loadu_ps(pC + N * 7);
                            __m256 _r8 = _mm256_loadu_ps(pC + N * 8);
                            __m256 _r9 = _mm256_loadu_ps(pC + N * 9);
                            __m256 _ra = _mm256_loadu_ps(pC + N * 10);
                            __m256 _rb = _mm256_loadu_ps(pC + N * 11);
                            __m256 _rc = _mm256_loadu_ps(pC + N * 12);
                            __m256 _rd = _mm256_loadu_ps(pC + N * 13);
                            __m256 _re = _mm256_loadu_ps(pC + N * 14);
                            __m256 _rf = _mm256_loadu_ps(pC + N * 15);

                            transpose8x16_ps(_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7, _r8, _r9, _ra, _rb, _rc, _rd, _re, _rf);

                            _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_r0), _r1, 1);
                            _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_r2), _r3, 1);
                            _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_r4), _r5, 1);
                            _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_r6), _r7, 1);
                            _sum4 = _mm512_insertf32x8(_mm512_castps256_ps512(_r8), _r9, 1);
                            _sum5 = _mm512_insertf32x8(_mm512_castps256_ps512(_ra), _rb, 1);
                            _sum6 = _mm512_insertf32x8(_mm512_castps256_ps512(_rc), _rd, 1);
                            _sum7 = _mm512_insertf32x8(_mm512_castps256_ps512(_re), _rf, 1);
                            pC += 8;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm512_set1_ps(pC[0]);
                        _sum1 = _mm512_set1_ps(pC[1]);
                        _sum2 = _mm512_set1_ps(pC[2]);
                        _sum3 = _mm512_set1_ps(pC[3]);
                        _sum4 = _mm512_set1_ps(pC[4]);
                        _sum5 = _mm512_set1_ps(pC[5]);
                        _sum6 = _mm512_set1_ps(pC[6]);
                        _sum7 = _mm512_set1_ps(pC[7]);
                        pC += 8;
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
                _sum0 = _mm512_load_ps(ptmp);
                _sum1 = _mm512_load_ps(ptmp + 16 * 1);
                _sum2 = _mm512_load_ps(ptmp + 16 * 2);
                _sum3 = _mm512_load_ps(ptmp + 16 * 3);
                _sum4 = _mm512_load_ps(ptmp + 16 * 4);
                _sum5 = _mm512_load_ps(ptmp + 16 * 5);
                _sum6 = _mm512_load_ps(ptmp + 16 * 6);
                _sum7 = _mm512_load_ps(ptmp + 16 * 7);
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

                pA += 16;
                pB += 8;
            }

            if (k_end)
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
                    _mm512_store_ps(outptr0, _sum0);
                    _mm512_store_ps(outptr0 + 16 * 1, _sum1);
                    _mm512_store_ps(outptr0 + 16 * 2, _sum2);
                    _mm512_store_ps(outptr0 + 16 * 3, _sum3);
                    _mm512_store_ps(outptr0 + 16 * 4, _sum4);
                    _mm512_store_ps(outptr0 + 16 * 5, _sum5);
                    _mm512_store_ps(outptr0 + 16 * 6, _sum6);
                    _mm512_store_ps(outptr0 + 16 * 7, _sum7);
                    outptr0 += 128;
                }
                if (out_elempack == 8)
                {
                    _mm256_store_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                    _mm256_store_ps(outptr0 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 0));
                    _mm256_store_ps(outptr0 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 0));
                    _mm256_store_ps(outptr0 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 0));
                    _mm256_store_ps(outptr0 + 8 * 4, _mm512_extractf32x8_ps(_sum4, 0));
                    _mm256_store_ps(outptr0 + 8 * 5, _mm512_extractf32x8_ps(_sum5, 0));
                    _mm256_store_ps(outptr0 + 8 * 6, _mm512_extractf32x8_ps(_sum6, 0));
                    _mm256_store_ps(outptr0 + 8 * 7, _mm512_extractf32x8_ps(_sum7, 0));

                    _mm256_store_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum0, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 4, _mm512_extractf32x8_ps(_sum4, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 5, _mm512_extractf32x8_ps(_sum5, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 6, _mm512_extractf32x8_ps(_sum6, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 7, _mm512_extractf32x8_ps(_sum7, 1));

                    outptr0 += 64;
                }
                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _mm512_extractf32x4_ps(_sum0, 0));
                    _mm_store_ps(outptr0 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 0));
                    _mm_store_ps(outptr0 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 0));
                    _mm_store_ps(outptr0 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 0));
                    _mm_store_ps(outptr0 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 0));
                    _mm_store_ps(outptr0 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 0));
                    _mm_store_ps(outptr0 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 0));
                    _mm_store_ps(outptr0 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 0));

                    _mm_store_ps(outptr0 + N * 4, _mm512_extractf32x4_ps(_sum0, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 1));

                    _mm_store_ps(outptr0 + N * 8, _mm512_extractf32x4_ps(_sum0, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 2));

                    _mm_store_ps(outptr0 + N * 12, _mm512_extractf32x4_ps(_sum0, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 4, _mm512_extractf32x4_ps(_sum4, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 5, _mm512_extractf32x4_ps(_sum5, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 6, _mm512_extractf32x4_ps(_sum6, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 7, _mm512_extractf32x4_ps(_sum7, 3));

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
                _mm512_store_ps(ptmp, _sum0);
                _mm512_store_ps(ptmp + 16 * 1, _sum1);
                _mm512_store_ps(ptmp + 16 * 2, _sum2);
                _mm512_store_ps(ptmp + 16 * 3, _sum3);
                _mm512_store_ps(ptmp + 16 * 4, _sum4);
                _mm512_store_ps(ptmp + 16 * 5, _sum5);
                _mm512_store_ps(ptmp + 16 * 6, _sum6);
                _mm512_store_ps(ptmp + 16 * 7, _sum7);
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
                        _sum0 = _mm512_loadu_ps(pC);
                        _sum1 = _sum0;
                        _sum2 = _sum0;
                        _sum3 = _sum0;
                    }
                    if (broadcast_type_C == 3)
                    {
                        if (out_elempack == 16)
                        {
                            _sum0 = _mm512_loadu_ps(pC);
                            _sum1 = _mm512_loadu_ps(pC + 16);
                            _sum2 = _mm512_loadu_ps(pC + 32);
                            _sum3 = _mm512_loadu_ps(pC + 48);
                            pC += 64;
                        }
                        if (out_elempack == 8)
                        {
                            __m256 _sum0_0 = _mm256_loadu_ps(pC);
                            __m256 _sum1_0 = _mm256_loadu_ps(pC + 8);
                            __m256 _sum2_0 = _mm256_loadu_ps(pC + 16);
                            __m256 _sum3_0 = _mm256_loadu_ps(pC + 24);
                            __m256 _sum0_1 = _mm256_loadu_ps(pC + N * 8);
                            __m256 _sum1_1 = _mm256_loadu_ps(pC + N * 8 + 8);
                            __m256 _sum2_1 = _mm256_loadu_ps(pC + N * 8 + 16);
                            __m256 _sum3_1 = _mm256_loadu_ps(pC + N * 8 + 24);
                            _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum0_0), _sum0_1, 1);
                            _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum1_0), _sum1_1, 1);
                            _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum2_0), _sum2_1, 1);
                            _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum3_0), _sum3_1, 1);
                            pC += 32;
                        }
                        if (out_elempack == 4)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + 4);
                            __m128 _sum2_0 = _mm_loadu_ps(pC + 8);
                            __m128 _sum3_0 = _mm_loadu_ps(pC + 12);
                            __m128 _sum0_1 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum1_1 = _mm_loadu_ps(pC + N * 4 + 4);
                            __m128 _sum2_1 = _mm_loadu_ps(pC + N * 4 + 8);
                            __m128 _sum3_1 = _mm_loadu_ps(pC + N * 4 + 12);
                            __m128 _sum0_2 = _mm_loadu_ps(pC + N * 8);
                            __m128 _sum1_2 = _mm_loadu_ps(pC + N * 8 + 4);
                            __m128 _sum2_2 = _mm_loadu_ps(pC + N * 8 + 8);
                            __m128 _sum3_2 = _mm_loadu_ps(pC + N * 8 + 12);
                            __m128 _sum0_3 = _mm_loadu_ps(pC + N * 12);
                            __m128 _sum1_3 = _mm_loadu_ps(pC + N * 12 + 4);
                            __m128 _sum2_3 = _mm_loadu_ps(pC + N * 12 + 8);
                            __m128 _sum3_3 = _mm_loadu_ps(pC + N * 12 + 12);
                            _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_2), _sum0_3, 1), 1);
                            _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_2), _sum1_3, 1), 1);
                            _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum2_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_2), _sum2_3, 1), 1);
                            _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum3_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_2), _sum3_3, 1), 1);
                            pC += 16;
                        }
                        if (out_elempack == 1)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + N);
                            __m128 _sum2_0 = _mm_loadu_ps(pC + N * 2);
                            __m128 _sum3_0 = _mm_loadu_ps(pC + N * 3);
                            __m128 _sum4_0 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum5_0 = _mm_loadu_ps(pC + N * 5);
                            __m128 _sum6_0 = _mm_loadu_ps(pC + N * 6);
                            __m128 _sum7_0 = _mm_loadu_ps(pC + N * 7);
                            __m128 _sum8_0 = _mm_loadu_ps(pC + N * 8);
                            __m128 _sum9_0 = _mm_loadu_ps(pC + N * 9);
                            __m128 _suma_0 = _mm_loadu_ps(pC + N * 10);
                            __m128 _sumb_0 = _mm_loadu_ps(pC + N * 11);
                            __m128 _sumc_0 = _mm_loadu_ps(pC + N * 12);
                            __m128 _sumd_0 = _mm_loadu_ps(pC + N * 13);
                            __m128 _sume_0 = _mm_loadu_ps(pC + N * 14);
                            __m128 _sumf_0 = _mm_loadu_ps(pC + N * 15);

                            _MM_TRANSPOSE4_PS(_sum0_0, _sum1_0, _sum2_0, _sum3_0);
                            _MM_TRANSPOSE4_PS(_sum4_0, _sum5_0, _sum6_0, _sum7_0);
                            _MM_TRANSPOSE4_PS(_sum8_0, _sum9_0, _suma_0, _sumb_0);
                            _MM_TRANSPOSE4_PS(_sumc_0, _sumd_0, _sume_0, _sumf_0);

                            _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum4_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum8_0), _sumc_0, 1), 1);
                            _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum5_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum9_0), _sumd_0, 1), 1);
                            _sum2 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum6_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_suma_0), _sume_0, 1), 1);
                            _sum3 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum7_0, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sumb_0), _sumf_0, 1), 1);
                            pC += 4;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm512_set1_ps(pC[0]);
                        _sum1 = _mm512_set1_ps(pC[1]);
                        _sum2 = _mm512_set1_ps(pC[2]);
                        _sum3 = _mm512_set1_ps(pC[3]);
                        pC += 4;
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
                _sum0 = _mm512_load_ps(ptmp);
                _sum1 = _mm512_load_ps(ptmp + 16 * 1);
                _sum2 = _mm512_load_ps(ptmp + 16 * 2);
                _sum3 = _mm512_load_ps(ptmp + 16 * 3);
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

                pA += 16;
                pB += 4;
            }

            if (k_end)
            {
                __m512 _alpha = _mm512_set1_ps(alpha);
                _sum0 = _mm512_mul_ps(_sum0, _alpha);
                _sum1 = _mm512_mul_ps(_sum1, _alpha);
                _sum2 = _mm512_mul_ps(_sum2, _alpha);
                _sum3 = _mm512_mul_ps(_sum3, _alpha);

                if (out_elempack == 16)
                {
                    _mm512_store_ps(outptr0, _sum0);
                    _mm512_store_ps(outptr0 + 16 * 1, _sum1);
                    _mm512_store_ps(outptr0 + 16 * 2, _sum2);
                    _mm512_store_ps(outptr0 + 16 * 3, _sum3);
                    outptr0 += 64;
                }
                if (out_elempack == 8)
                {
                    _mm256_store_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                    _mm256_store_ps(outptr0 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 0));
                    _mm256_store_ps(outptr0 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 0));
                    _mm256_store_ps(outptr0 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 0));

                    _mm256_store_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum0, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 1, _mm512_extractf32x8_ps(_sum1, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 2, _mm512_extractf32x8_ps(_sum2, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8 * 3, _mm512_extractf32x8_ps(_sum3, 1));

                    outptr0 += 32;
                }
                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _mm512_extractf32x4_ps(_sum0, 0));
                    _mm_store_ps(outptr0 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 0));
                    _mm_store_ps(outptr0 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 0));
                    _mm_store_ps(outptr0 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 0));

                    _mm_store_ps(outptr0 + N * 4, _mm512_extractf32x4_ps(_sum0, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 1));

                    _mm_store_ps(outptr0 + N * 8, _mm512_extractf32x4_ps(_sum0, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 2));

                    _mm_store_ps(outptr0 + N * 12, _mm512_extractf32x4_ps(_sum0, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 1, _mm512_extractf32x4_ps(_sum1, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 2, _mm512_extractf32x4_ps(_sum2, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4 * 3, _mm512_extractf32x4_ps(_sum3, 3));

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
                _mm512_store_ps(ptmp, _sum0);
                _mm512_store_ps(ptmp + 16 * 1, _sum1);
                _mm512_store_ps(ptmp + 16 * 2, _sum2);
                _mm512_store_ps(ptmp + 16 * 3, _sum3);
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
                        _sum0 = _mm512_loadu_ps(pC);
                        _sum1 = _sum0;
                    }
                    if (broadcast_type_C == 3)
                    {
                        if (out_elempack == 16)
                        {
                            _sum0 = _mm512_loadu_ps(pC);
                            _sum1 = _mm512_loadu_ps(pC + 16);
                            pC += 32;
                        }
                        if (out_elempack == 8)
                        {
                            __m256 _sum0_0 = _mm256_loadu_ps(pC);
                            __m256 _sum1_0 = _mm256_loadu_ps(pC + 8);
                            __m256 _sum0_1 = _mm256_loadu_ps(pC + N * 8);
                            __m256 _sum1_1 = _mm256_loadu_ps(pC + N * 8 + 8);
                            _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum0_0), _sum0_1, 1);
                            _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum1_0), _sum1_1, 1);
                            pC += 16;
                        }
                        if (out_elempack == 4)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + 4);
                            __m128 _sum0_1 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum1_1 = _mm_loadu_ps(pC + N * 4 + 4);
                            __m128 _sum0_2 = _mm_loadu_ps(pC + N * 8);
                            __m128 _sum1_2 = _mm_loadu_ps(pC + N * 8 + 4);
                            __m128 _sum0_3 = _mm_loadu_ps(pC + N * 12);
                            __m128 _sum1_3 = _mm_loadu_ps(pC + N * 12 + 4);
                            _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_2), _sum0_3, 1), 1);
                            _sum1 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_2), _sum1_3, 1), 1);
                            pC += 8;
                        }
                        if (out_elempack == 1)
                        {
                            float sum0[16];
                            float sum1[16];
                            sum0[0] = pC[0];
                            sum0[1] = pC[N];
                            sum0[2] = pC[N * 2];
                            sum0[3] = pC[N * 3];
                            sum0[4] = pC[N * 4];
                            sum0[5] = pC[N * 5];
                            sum0[6] = pC[N * 6];
                            sum0[7] = pC[N * 7];
                            sum0[8] = pC[N * 8];
                            sum0[9] = pC[N * 9];
                            sum0[10] = pC[N * 10];
                            sum0[11] = pC[N * 11];
                            sum0[12] = pC[N * 12];
                            sum0[13] = pC[N * 13];
                            sum0[14] = pC[N * 14];
                            sum0[15] = pC[N * 15];
                            sum1[0] = pC[1];
                            sum1[1] = pC[N + 1];
                            sum1[2] = pC[N * 2 + 1];
                            sum1[3] = pC[N * 3 + 1];
                            sum1[4] = pC[N * 4 + 1];
                            sum1[5] = pC[N * 5 + 1];
                            sum1[6] = pC[N * 6 + 1];
                            sum1[7] = pC[N * 7 + 1];
                            sum1[8] = pC[N * 8 + 1];
                            sum1[9] = pC[N * 9 + 1];
                            sum1[10] = pC[N * 10 + 1];
                            sum1[11] = pC[N * 11 + 1];
                            sum1[12] = pC[N * 12 + 1];
                            sum1[13] = pC[N * 13 + 1];
                            sum1[14] = pC[N * 14 + 1];
                            sum1[15] = pC[N * 15 + 1];

                            _sum0 = _mm512_loadu_ps(sum0);
                            _sum1 = _mm512_loadu_ps(sum1);
                            pC += 2;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm512_set1_ps(pC[0]);
                        _sum1 = _mm512_set1_ps(pC[1]);
                        pC += 2;
                    }

                    __m512 _beta = _mm512_set1_ps(beta);
                    _sum0 = _mm512_mul_ps(_sum0, _beta);
                    _sum1 = _mm512_mul_ps(_sum1, _beta);
                }
            }
            else
            {
                _sum0 = _mm512_load_ps(ptmp);
                _sum1 = _mm512_load_ps(ptmp + 16);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m512 _pA = _mm512_load_ps(pA);

                _sum0 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[0]), _sum0);
                _sum1 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[1]), _sum1);

                pA += 16;
                pB += 2;
            }

            if (k_end)
            {
                __m512 _alpha = _mm512_set1_ps(alpha);
                _sum0 = _mm512_mul_ps(_sum0, _alpha);
                _sum1 = _mm512_mul_ps(_sum1, _alpha);

                if (out_elempack == 16)
                {
                    _mm512_store_ps(outptr0, _sum0);
                    _mm512_store_ps(outptr0 + 16, _sum1);
                    outptr0 += 32;
                }
                if (out_elempack == 8)
                {
                    _mm256_store_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                    _mm256_store_ps(outptr0 + 8, _mm512_extractf32x8_ps(_sum1, 0));

                    _mm256_store_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum0, 1));
                    _mm256_store_ps(outptr0 + N * 8 + 8, _mm512_extractf32x8_ps(_sum1, 1));
                    outptr0 += 16;
                }
                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _mm512_extractf32x4_ps(_sum0, 0));
                    _mm_store_ps(outptr0 + 4, _mm512_extractf32x4_ps(_sum1, 0));

                    _mm_store_ps(outptr0 + N * 4, _mm512_extractf32x4_ps(_sum0, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4, _mm512_extractf32x4_ps(_sum1, 1));

                    _mm_store_ps(outptr0 + N * 8, _mm512_extractf32x4_ps(_sum0, 2));
                    _mm_store_ps(outptr0 + N * 8 + 4, _mm512_extractf32x4_ps(_sum1, 2));

                    _mm_store_ps(outptr0 + N * 12, _mm512_extractf32x4_ps(_sum0, 3));
                    _mm_store_ps(outptr0 + N * 12 + 4, _mm512_extractf32x4_ps(_sum1, 3));
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
                _mm512_store_ps(ptmp, _sum0);
                _mm512_store_ps(ptmp + 16, _sum1);
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
                        _sum0 = _mm512_loadu_ps(pC);
                    }
                    if (broadcast_type_C == 3)
                    {
                        if (out_elempack == 16)
                        {
                            _sum0 = _mm512_loadu_ps(pC);
                            pC += 16;
                        }
                        if (out_elempack == 8)
                        {
                            __m256 _sum0_0 = _mm256_loadu_ps(pC);
                            __m256 _sum0_1 = _mm256_loadu_ps(pC + N * 8);
                            _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_sum0_0), _sum0_1, 1);
                            pC += 8;
                        }
                        if (out_elempack == 4)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum0_1 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum0_2 = _mm_loadu_ps(pC + N * 8);
                            __m128 _sum0_3 = _mm_loadu_ps(pC + N * 12);
                            _sum0 = _mm512_insertf32x8(_mm512_castps256_ps512(_mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1)), _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_2), _sum0_3, 1), 1);
                            pC += 4;
                        }
                        if (out_elempack == 1)
                        {
                            float sum0[16];
                            sum0[0] = pC[0];
                            sum0[1] = pC[N];
                            sum0[2] = pC[N * 2];
                            sum0[3] = pC[N * 3];
                            sum0[4] = pC[N * 4];
                            sum0[5] = pC[N * 5];
                            sum0[6] = pC[N * 6];
                            sum0[7] = pC[N * 7];
                            sum0[8] = pC[N * 8];
                            sum0[9] = pC[N * 9];
                            sum0[10] = pC[N * 10];
                            sum0[11] = pC[N * 11];
                            sum0[12] = pC[N * 12];
                            sum0[13] = pC[N * 13];
                            sum0[14] = pC[N * 14];
                            sum0[15] = pC[N * 15];

                            _sum0 = _mm512_loadu_ps(sum0);
                            pC += 1;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm512_set1_ps(pC[0]);
                        pC += 1;
                    }

                    __m512 _beta = _mm512_set1_ps(beta);
                    _sum0 = _mm512_mul_ps(_sum0, _beta);
                }
            }
            else
            {
                _sum0 = _mm512_load_ps(ptmp);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m512 _pA = _mm512_load_ps(pA);

                _sum0 = _mm512_fmadd_ps(_pA, _mm512_set1_ps(pB[0]), _sum0);

                pA += 16;
                pB += 1;
            }

            if (k_end)
            {
                __m512 _alpha = _mm512_set1_ps(alpha);
                _sum0 = _mm512_mul_ps(_sum0, _alpha);

                if (out_elempack == 16)
                {
                    _mm512_store_ps(outptr0, _sum0);
                    outptr0 += 16;
                }
                if (out_elempack == 8)
                {
                    _mm256_store_ps(outptr0, _mm512_extractf32x8_ps(_sum0, 0));
                    _mm256_store_ps(outptr0 + N * 8, _mm512_extractf32x8_ps(_sum0, 1));
                    outptr0 += 8;
                }
                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _mm512_extractf32x4_ps(_sum0, 0));
                    _mm_store_ps(outptr0 + N * 4, _mm512_extractf32x4_ps(_sum0, 1));
                    _mm_store_ps(outptr0 + N * 8, _mm512_extractf32x4_ps(_sum0, 2));
                    _mm_store_ps(outptr0 + N * 12, _mm512_extractf32x4_ps(_sum0, 3));
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
                _mm512_store_ps(ptmp, _sum0);
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

        const float* pC = C;
        if (pC)
        {
            if (broadcast_type_C == 1 || broadcast_type_C == 2)
            {
                pC = pC + i + ii;
            }
            if (broadcast_type_C == 3)
            {
                pC = C.row((i + ii) / out_elempack) + j * out_elempack;
            }
            if (broadcast_type_C == 4)
            {
                pC = pC + j;
            }
        }

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
                        _sum0 = _mm256_loadu_ps(pC);
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
                            _sum0 = _mm256_loadu_ps(pC);
                            _sum1 = _mm256_loadu_ps(pC + 8);
                            _sum2 = _mm256_loadu_ps(pC + 8 * 2);
                            _sum3 = _mm256_loadu_ps(pC + 8 * 3);
                            _sum4 = _mm256_loadu_ps(pC + 8 * 4);
                            _sum5 = _mm256_loadu_ps(pC + 8 * 5);
                            _sum6 = _mm256_loadu_ps(pC + 8 * 6);
                            _sum7 = _mm256_loadu_ps(pC + 8 * 7);
                            _sum8 = _mm256_loadu_ps(pC + 8 * 8);
                            _sum9 = _mm256_loadu_ps(pC + 8 * 9);
                            _suma = _mm256_loadu_ps(pC + 8 * 10);
                            _sumb = _mm256_loadu_ps(pC + 8 * 11);
                            pC += 96;
                        }
                        if (out_elempack == 4)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + 4);
                            __m128 _sum2_0 = _mm_loadu_ps(pC + 4 * 2);
                            __m128 _sum3_0 = _mm_loadu_ps(pC + 4 * 3);
                            __m128 _sum4_0 = _mm_loadu_ps(pC + 4 * 4);
                            __m128 _sum5_0 = _mm_loadu_ps(pC + 4 * 5);
                            __m128 _sum6_0 = _mm_loadu_ps(pC + 4 * 6);
                            __m128 _sum7_0 = _mm_loadu_ps(pC + 4 * 7);
                            __m128 _sum8_0 = _mm_loadu_ps(pC + 4 * 8);
                            __m128 _sum9_0 = _mm_loadu_ps(pC + 4 * 9);
                            __m128 _suma_0 = _mm_loadu_ps(pC + 4 * 10);
                            __m128 _sumb_0 = _mm_loadu_ps(pC + 4 * 11);
                            __m128 _sum0_1 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum1_1 = _mm_loadu_ps(pC + N * 4 + 4);
                            __m128 _sum2_1 = _mm_loadu_ps(pC + N * 4 + 4 * 2);
                            __m128 _sum3_1 = _mm_loadu_ps(pC + N * 4 + 4 * 3);
                            __m128 _sum4_1 = _mm_loadu_ps(pC + N * 4 + 4 * 4);
                            __m128 _sum5_1 = _mm_loadu_ps(pC + N * 4 + 4 * 5);
                            __m128 _sum6_1 = _mm_loadu_ps(pC + N * 4 + 4 * 6);
                            __m128 _sum7_1 = _mm_loadu_ps(pC + N * 4 + 4 * 7);
                            __m128 _sum8_1 = _mm_loadu_ps(pC + N * 4 + 4 * 8);
                            __m128 _sum9_1 = _mm_loadu_ps(pC + N * 4 + 4 * 9);
                            __m128 _suma_1 = _mm_loadu_ps(pC + N * 4 + 4 * 10);
                            __m128 _sumb_1 = _mm_loadu_ps(pC + N * 4 + 4 * 11);
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
                            pC += 48;
                        }
                        if (out_elempack == 1)
                        {
                            _sum0 = _mm256_loadu_ps(pC);
                            _sum1 = _mm256_loadu_ps(pC + N);
                            _sum2 = _mm256_loadu_ps(pC + N * 2);
                            _sum3 = _mm256_loadu_ps(pC + N * 3);
                            _sum4 = _mm256_loadu_ps(pC + N * 4);
                            _sum5 = _mm256_loadu_ps(pC + N * 5);
                            _sum6 = _mm256_loadu_ps(pC + N * 6);
                            _sum7 = _mm256_loadu_ps(pC + N * 7);

                            transpose8x8_ps(_sum0, _sum1, _sum2, _sum3, _sum4, _sum5, _sum6, _sum7);

                            __m128 _sum0_8 = _mm_loadu_ps(pC + 8);
                            __m128 _sum1_8 = _mm_loadu_ps(pC + N + 8);
                            __m128 _sum2_8 = _mm_loadu_ps(pC + N * 2 + 8);
                            __m128 _sum3_8 = _mm_loadu_ps(pC + N * 3 + 8);
                            __m128 _sum4_8 = _mm_loadu_ps(pC + N * 4 + 8);
                            __m128 _sum5_8 = _mm_loadu_ps(pC + N * 5 + 8);
                            __m128 _sum6_8 = _mm_loadu_ps(pC + N * 6 + 8);
                            __m128 _sum7_8 = _mm_loadu_ps(pC + N * 7 + 8);

                            _MM_TRANSPOSE4_PS(_sum0_8, _sum1_8, _sum2_8, _sum3_8);
                            _MM_TRANSPOSE4_PS(_sum4_8, _sum5_8, _sum6_8, _sum7_8);

                            _sum8 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_8), _sum4_8, 1);
                            _sum9 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_8), _sum5_8, 1);
                            _suma = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_8), _sum6_8, 1);
                            _sumb = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_8), _sum7_8, 1);
                            pC += 12;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm256_set1_ps(pC[0]);
                        _sum1 = _mm256_set1_ps(pC[1]);
                        _sum2 = _mm256_set1_ps(pC[2]);
                        _sum3 = _mm256_set1_ps(pC[3]);
                        _sum4 = _mm256_set1_ps(pC[4]);
                        _sum5 = _mm256_set1_ps(pC[5]);
                        _sum6 = _mm256_set1_ps(pC[6]);
                        _sum7 = _mm256_set1_ps(pC[7]);
                        _sum8 = _mm256_set1_ps(pC[8]);
                        _sum9 = _mm256_set1_ps(pC[9]);
                        _suma = _mm256_set1_ps(pC[10]);
                        _sumb = _mm256_set1_ps(pC[11]);
                        pC += 12;
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

            if (k_end)
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
                    _mm256_store_ps(outptr0, _sum0);
                    _mm256_store_ps(outptr0 + 8 * 1, _sum1);
                    _mm256_store_ps(outptr0 + 8 * 2, _sum2);
                    _mm256_store_ps(outptr0 + 8 * 3, _sum3);
                    _mm256_store_ps(outptr0 + 8 * 4, _sum4);
                    _mm256_store_ps(outptr0 + 8 * 5, _sum5);
                    _mm256_store_ps(outptr0 + 8 * 6, _sum6);
                    _mm256_store_ps(outptr0 + 8 * 7, _sum7);
                    _mm256_store_ps(outptr0 + 8 * 8, _sum8);
                    _mm256_store_ps(outptr0 + 8 * 9, _sum9);
                    _mm256_store_ps(outptr0 + 8 * 10, _suma);
                    _mm256_store_ps(outptr0 + 8 * 11, _sumb);
                    outptr0 += 96;
                }
                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _mm256_extractf128_ps(_sum0, 0));
                    _mm_store_ps(outptr0 + 4 * 1, _mm256_extractf128_ps(_sum1, 0));
                    _mm_store_ps(outptr0 + 4 * 2, _mm256_extractf128_ps(_sum2, 0));
                    _mm_store_ps(outptr0 + 4 * 3, _mm256_extractf128_ps(_sum3, 0));
                    _mm_store_ps(outptr0 + 4 * 4, _mm256_extractf128_ps(_sum4, 0));
                    _mm_store_ps(outptr0 + 4 * 5, _mm256_extractf128_ps(_sum5, 0));
                    _mm_store_ps(outptr0 + 4 * 6, _mm256_extractf128_ps(_sum6, 0));
                    _mm_store_ps(outptr0 + 4 * 7, _mm256_extractf128_ps(_sum7, 0));
                    _mm_store_ps(outptr0 + 4 * 8, _mm256_extractf128_ps(_sum8, 0));
                    _mm_store_ps(outptr0 + 4 * 9, _mm256_extractf128_ps(_sum9, 0));
                    _mm_store_ps(outptr0 + 4 * 10, _mm256_extractf128_ps(_suma, 0));
                    _mm_store_ps(outptr0 + 4 * 11, _mm256_extractf128_ps(_sumb, 0));

                    _mm_store_ps(outptr0 + N * 4, _mm256_extractf128_ps(_sum0, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 1, _mm256_extractf128_ps(_sum1, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 2, _mm256_extractf128_ps(_sum2, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 3, _mm256_extractf128_ps(_sum3, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 4, _mm256_extractf128_ps(_sum4, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 5, _mm256_extractf128_ps(_sum5, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 6, _mm256_extractf128_ps(_sum6, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 7, _mm256_extractf128_ps(_sum7, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 8, _mm256_extractf128_ps(_sum8, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 9, _mm256_extractf128_ps(_sum9, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 10, _mm256_extractf128_ps(_suma, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 11, _mm256_extractf128_ps(_sumb, 1));

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
                        _sum0 = _mm256_loadu_ps(pC);
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
                            _sum0 = _mm256_loadu_ps(pC);
                            _sum1 = _mm256_loadu_ps(pC + 8);
                            _sum2 = _mm256_loadu_ps(pC + 8 * 2);
                            _sum3 = _mm256_loadu_ps(pC + 8 * 3);
                            _sum4 = _mm256_loadu_ps(pC + 8 * 4);
                            _sum5 = _mm256_loadu_ps(pC + 8 * 5);
                            _sum6 = _mm256_loadu_ps(pC + 8 * 6);
                            _sum7 = _mm256_loadu_ps(pC + 8 * 7);
                            pC += 64;
                        }
                        if (out_elempack == 4)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + 4);
                            __m128 _sum2_0 = _mm_loadu_ps(pC + 4 * 2);
                            __m128 _sum3_0 = _mm_loadu_ps(pC + 4 * 3);
                            __m128 _sum4_0 = _mm_loadu_ps(pC + 4 * 4);
                            __m128 _sum5_0 = _mm_loadu_ps(pC + 4 * 5);
                            __m128 _sum6_0 = _mm_loadu_ps(pC + 4 * 6);
                            __m128 _sum7_0 = _mm_loadu_ps(pC + 4 * 7);
                            __m128 _sum0_1 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum1_1 = _mm_loadu_ps(pC + N * 4 + 4);
                            __m128 _sum2_1 = _mm_loadu_ps(pC + N * 4 + 4 * 2);
                            __m128 _sum3_1 = _mm_loadu_ps(pC + N * 4 + 4 * 3);
                            __m128 _sum4_1 = _mm_loadu_ps(pC + N * 4 + 4 * 4);
                            __m128 _sum5_1 = _mm_loadu_ps(pC + N * 4 + 4 * 5);
                            __m128 _sum6_1 = _mm_loadu_ps(pC + N * 4 + 4 * 6);
                            __m128 _sum7_1 = _mm_loadu_ps(pC + N * 4 + 4 * 7);
                            _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1);
                            _sum1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1);
                            _sum2 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum2_1, 1);
                            _sum3 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum3_1, 1);
                            _sum4 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum4_0), _sum4_1, 1);
                            _sum5 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum5_0), _sum5_1, 1);
                            _sum6 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum6_0), _sum6_1, 1);
                            _sum7 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum7_0), _sum7_1, 1);
                            pC += 32;
                        }
                        if (out_elempack == 1)
                        {
                            _sum0 = _mm256_loadu_ps(pC);
                            _sum1 = _mm256_loadu_ps(pC + N);
                            _sum2 = _mm256_loadu_ps(pC + N * 2);
                            _sum3 = _mm256_loadu_ps(pC + N * 3);
                            _sum4 = _mm256_loadu_ps(pC + N * 4);
                            _sum5 = _mm256_loadu_ps(pC + N * 5);
                            _sum6 = _mm256_loadu_ps(pC + N * 6);
                            _sum7 = _mm256_loadu_ps(pC + N * 7);

                            transpose8x8_ps(_sum0, _sum1, _sum2, _sum3, _sum4, _sum5, _sum6, _sum7);
                            pC += 8;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm256_set1_ps(pC[0]);
                        _sum1 = _mm256_set1_ps(pC[1]);
                        _sum2 = _mm256_set1_ps(pC[2]);
                        _sum3 = _mm256_set1_ps(pC[3]);
                        _sum4 = _mm256_set1_ps(pC[4]);
                        _sum5 = _mm256_set1_ps(pC[5]);
                        _sum6 = _mm256_set1_ps(pC[6]);
                        _sum7 = _mm256_set1_ps(pC[7]);
                        pC += 8;
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
                _sum0 = _mm256_load_ps(ptmp);
                _sum1 = _mm256_load_ps(ptmp + 8 * 1);
                _sum2 = _mm256_load_ps(ptmp + 8 * 2);
                _sum3 = _mm256_load_ps(ptmp + 8 * 3);
                _sum4 = _mm256_load_ps(ptmp + 8 * 4);
                _sum5 = _mm256_load_ps(ptmp + 8 * 5);
                _sum6 = _mm256_load_ps(ptmp + 8 * 6);
                _sum7 = _mm256_load_ps(ptmp + 8 * 7);
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

                pA += 8;
                pB += 8;
            }

            if (k_end)
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
                    _mm256_store_ps(outptr0, _sum0);
                    _mm256_store_ps(outptr0 + 8 * 1, _sum1);
                    _mm256_store_ps(outptr0 + 8 * 2, _sum2);
                    _mm256_store_ps(outptr0 + 8 * 3, _sum3);
                    _mm256_store_ps(outptr0 + 8 * 4, _sum4);
                    _mm256_store_ps(outptr0 + 8 * 5, _sum5);
                    _mm256_store_ps(outptr0 + 8 * 6, _sum6);
                    _mm256_store_ps(outptr0 + 8 * 7, _sum7);
                    outptr0 += 64;
                }
                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _mm256_extractf128_ps(_sum0, 0));
                    _mm_store_ps(outptr0 + 4 * 1, _mm256_extractf128_ps(_sum1, 0));
                    _mm_store_ps(outptr0 + 4 * 2, _mm256_extractf128_ps(_sum2, 0));
                    _mm_store_ps(outptr0 + 4 * 3, _mm256_extractf128_ps(_sum3, 0));
                    _mm_store_ps(outptr0 + 4 * 4, _mm256_extractf128_ps(_sum4, 0));
                    _mm_store_ps(outptr0 + 4 * 5, _mm256_extractf128_ps(_sum5, 0));
                    _mm_store_ps(outptr0 + 4 * 6, _mm256_extractf128_ps(_sum6, 0));
                    _mm_store_ps(outptr0 + 4 * 7, _mm256_extractf128_ps(_sum7, 0));

                    _mm_store_ps(outptr0 + N * 4, _mm256_extractf128_ps(_sum0, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 1, _mm256_extractf128_ps(_sum1, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 2, _mm256_extractf128_ps(_sum2, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 3, _mm256_extractf128_ps(_sum3, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 4, _mm256_extractf128_ps(_sum4, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 5, _mm256_extractf128_ps(_sum5, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 6, _mm256_extractf128_ps(_sum6, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 7, _mm256_extractf128_ps(_sum7, 1));

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
                _mm256_store_ps(ptmp, _sum0);
                _mm256_store_ps(ptmp + 8 * 1, _sum1);
                _mm256_store_ps(ptmp + 8 * 2, _sum2);
                _mm256_store_ps(ptmp + 8 * 3, _sum3);
                _mm256_store_ps(ptmp + 8 * 4, _sum4);
                _mm256_store_ps(ptmp + 8 * 5, _sum5);
                _mm256_store_ps(ptmp + 8 * 6, _sum6);
                _mm256_store_ps(ptmp + 8 * 7, _sum7);
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
                        _sum0 = _mm256_loadu_ps(pC);
                        _sum1 = _sum0;
                        _sum2 = _sum0;
                        _sum3 = _sum0;
                    }
                    if (broadcast_type_C == 3)
                    {
                        if (out_elempack == 8)
                        {
                            _sum0 = _mm256_loadu_ps(pC);
                            _sum1 = _mm256_loadu_ps(pC + 8);
                            _sum2 = _mm256_loadu_ps(pC + 16);
                            _sum3 = _mm256_loadu_ps(pC + 24);
                            pC += 32;
                        }
                        if (out_elempack == 4)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + 4);
                            __m128 _sum2_0 = _mm_loadu_ps(pC + 8);
                            __m128 _sum3_0 = _mm_loadu_ps(pC + 12);
                            __m128 _sum0_1 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum1_1 = _mm_loadu_ps(pC + N * 4 + 4);
                            __m128 _sum2_1 = _mm_loadu_ps(pC + N * 4 + 8);
                            __m128 _sum3_1 = _mm_loadu_ps(pC + N * 4 + 12);
                            _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1);
                            _sum1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1);
                            _sum2 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum2_1, 1);
                            _sum3 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum3_1, 1);
                            pC += 16;
                        }
                        if (out_elempack == 1)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + N);
                            __m128 _sum2_0 = _mm_loadu_ps(pC + N * 2);
                            __m128 _sum3_0 = _mm_loadu_ps(pC + N * 3);
                            __m128 _sum4_0 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum5_0 = _mm_loadu_ps(pC + N * 5);
                            __m128 _sum6_0 = _mm_loadu_ps(pC + N * 6);
                            __m128 _sum7_0 = _mm_loadu_ps(pC + N * 7);

                            _MM_TRANSPOSE4_PS(_sum0_0, _sum1_0, _sum2_0, _sum3_0);
                            _MM_TRANSPOSE4_PS(_sum4_0, _sum5_0, _sum6_0, _sum7_0);

                            _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum4_0, 1);
                            _sum1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum5_0, 1);
                            _sum2 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum2_0), _sum6_0, 1);
                            _sum3 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum3_0), _sum7_0, 1);
                            pC += 4;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm256_set1_ps(pC[0]);
                        _sum1 = _mm256_set1_ps(pC[1]);
                        _sum2 = _mm256_set1_ps(pC[2]);
                        _sum3 = _mm256_set1_ps(pC[3]);
                        pC += 4;
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
                _sum0 = _mm256_load_ps(ptmp);
                _sum1 = _mm256_load_ps(ptmp + 8 * 1);
                _sum2 = _mm256_load_ps(ptmp + 8 * 2);
                _sum3 = _mm256_load_ps(ptmp + 8 * 3);
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

                pA += 8;
                pB += 4;
            }

            if (k_end)
            {
                __m256 _alpha = _mm256_set1_ps(alpha);
                _sum0 = _mm256_mul_ps(_sum0, _alpha);
                _sum1 = _mm256_mul_ps(_sum1, _alpha);
                _sum2 = _mm256_mul_ps(_sum2, _alpha);
                _sum3 = _mm256_mul_ps(_sum3, _alpha);

                if (out_elempack == 8)
                {
                    _mm256_store_ps(outptr0, _sum0);
                    _mm256_store_ps(outptr0 + 8 * 1, _sum1);
                    _mm256_store_ps(outptr0 + 8 * 2, _sum2);
                    _mm256_store_ps(outptr0 + 8 * 3, _sum3);
                    outptr0 += 32;
                }
                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _mm256_extractf128_ps(_sum0, 0));
                    _mm_store_ps(outptr0 + 4 * 1, _mm256_extractf128_ps(_sum1, 0));
                    _mm_store_ps(outptr0 + 4 * 2, _mm256_extractf128_ps(_sum2, 0));
                    _mm_store_ps(outptr0 + 4 * 3, _mm256_extractf128_ps(_sum3, 0));

                    _mm_store_ps(outptr0 + N * 4, _mm256_extractf128_ps(_sum0, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 1, _mm256_extractf128_ps(_sum1, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 2, _mm256_extractf128_ps(_sum2, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4 * 3, _mm256_extractf128_ps(_sum3, 1));

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
                _mm256_store_ps(ptmp, _sum0);
                _mm256_store_ps(ptmp + 8 * 1, _sum1);
                _mm256_store_ps(ptmp + 8 * 2, _sum2);
                _mm256_store_ps(ptmp + 8 * 3, _sum3);
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
                        _sum0 = _mm256_loadu_ps(pC);
                        _sum1 = _sum0;
                    }
                    if (broadcast_type_C == 3)
                    {
                        if (out_elempack == 8)
                        {
                            _sum0 = _mm256_loadu_ps(pC);
                            _sum1 = _mm256_loadu_ps(pC + 8);
                            pC += 16;
                        }
                        if (out_elempack == 4)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum1_0 = _mm_loadu_ps(pC + 4);
                            __m128 _sum0_1 = _mm_loadu_ps(pC + N * 4);
                            __m128 _sum1_1 = _mm_loadu_ps(pC + N * 4 + 4);
                            _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1);
                            _sum1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum1_0), _sum1_1, 1);
                            pC += 8;
                        }
                        if (out_elempack == 1)
                        {
                            float sum0[8];
                            float sum1[8];
                            sum0[0] = pC[0];
                            sum0[1] = pC[N];
                            sum0[2] = pC[N * 2];
                            sum0[3] = pC[N * 3];
                            sum0[4] = pC[N * 4];
                            sum0[5] = pC[N * 5];
                            sum0[6] = pC[N * 6];
                            sum0[7] = pC[N * 7];
                            sum1[0] = pC[1];
                            sum1[1] = pC[N + 1];
                            sum1[2] = pC[N * 2 + 1];
                            sum1[3] = pC[N * 3 + 1];
                            sum1[4] = pC[N * 4 + 1];
                            sum1[5] = pC[N * 5 + 1];
                            sum1[6] = pC[N * 6 + 1];
                            sum1[7] = pC[N * 7 + 1];

                            _sum0 = _mm256_loadu_ps(sum0);
                            _sum1 = _mm256_loadu_ps(sum1);
                            pC += 2;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm256_set1_ps(pC[0]);
                        _sum1 = _mm256_set1_ps(pC[1]);
                        pC += 2;
                    }

                    __m256 _beta = _mm256_set1_ps(beta);
                    _sum0 = _mm256_mul_ps(_sum0, _beta);
                    _sum1 = _mm256_mul_ps(_sum1, _beta);
                }
            }
            else
            {
                _sum0 = _mm256_load_ps(ptmp);
                _sum1 = _mm256_load_ps(ptmp + 8);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m256 _pA = _mm256_load_ps(pA);

                _sum0 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[0]), _sum0);
                _sum1 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[1]), _sum1);

                pA += 8;
                pB += 2;
            }

            if (k_end)
            {
                __m256 _alpha = _mm256_set1_ps(alpha);
                _sum0 = _mm256_mul_ps(_sum0, _alpha);
                _sum1 = _mm256_mul_ps(_sum1, _alpha);

                if (out_elempack == 8)
                {
                    _mm256_store_ps(outptr0, _sum0);
                    _mm256_store_ps(outptr0 + 8, _sum1);
                    outptr0 += 16;
                }
                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _mm256_extractf128_ps(_sum0, 0));
                    _mm_store_ps(outptr0 + 4, _mm256_extractf128_ps(_sum1, 0));

                    _mm_store_ps(outptr0 + N * 4, _mm256_extractf128_ps(_sum0, 1));
                    _mm_store_ps(outptr0 + N * 4 + 4, _mm256_extractf128_ps(_sum1, 1));
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
                _mm256_store_ps(ptmp, _sum0);
                _mm256_store_ps(ptmp + 8, _sum1);
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
                        _sum0 = _mm256_loadu_ps(pC);
                    }
                    if (broadcast_type_C == 3)
                    {
                        if (out_elempack == 8)
                        {
                            _sum0 = _mm256_loadu_ps(pC);
                            pC += 8;
                        }
                        if (out_elempack == 4)
                        {
                            __m128 _sum0_0 = _mm_loadu_ps(pC);
                            __m128 _sum0_1 = _mm_loadu_ps(pC + N * 4);
                            _sum0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_sum0_0), _sum0_1, 1);
                            pC += 4;
                        }
                        if (out_elempack == 1)
                        {
                            float sum0[8];
                            sum0[0] = pC[0];
                            sum0[1] = pC[N];
                            sum0[2] = pC[N * 2];
                            sum0[3] = pC[N * 3];
                            sum0[4] = pC[N * 4];
                            sum0[5] = pC[N * 5];
                            sum0[6] = pC[N * 6];
                            sum0[7] = pC[N * 7];

                            _sum0 = _mm256_loadu_ps(sum0);
                            pC += 1;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm256_set1_ps(pC[0]);
                        pC += 1;
                    }

                    __m256 _beta = _mm256_set1_ps(beta);
                    _sum0 = _mm256_mul_ps(_sum0, _beta);
                }
            }
            else
            {
                _sum0 = _mm256_load_ps(ptmp);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m256 _pA = _mm256_load_ps(pA);

                _sum0 = _mm256_comp_fmadd_ps(_pA, _mm256_set1_ps(pB[0]), _sum0);

                pA += 8;
                pB += 1;
            }

            if (k_end)
            {
                __m256 _alpha = _mm256_set1_ps(alpha);
                _sum0 = _mm256_mul_ps(_sum0, _alpha);

                if (out_elempack == 8)
                {
                    _mm256_store_ps(outptr0, _sum0);
                    outptr0 += 8;
                }
                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _mm256_extractf128_ps(_sum0, 0));
                    _mm_store_ps(outptr0 + N * 4, _mm256_extractf128_ps(_sum0, 1));
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
                _mm256_store_ps(ptmp, _sum0);
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

        const float* pC = C;
        if (pC)
        {
            if (broadcast_type_C == 1 || broadcast_type_C == 2)
            {
                pC = pC + i + ii;
            }
            if (broadcast_type_C == 3)
            {
                pC = C.row((i + ii) / out_elempack) + j * out_elempack;
            }
            if (broadcast_type_C == 4)
            {
                pC = pC + j;
            }
        }

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
                        _sum0 = _mm_loadu_ps(pC);
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
                            _sum0 = _mm_loadu_ps(pC);
                            _sum1 = _mm_loadu_ps(pC + 4);
                            _sum2 = _mm_loadu_ps(pC + 8);
                            _sum3 = _mm_loadu_ps(pC + 12);
                            _sum4 = _mm_loadu_ps(pC + 16);
                            _sum5 = _mm_loadu_ps(pC + 20);
                            _sum6 = _mm_loadu_ps(pC + 24);
                            _sum7 = _mm_loadu_ps(pC + 28);
                            _sum8 = _mm_loadu_ps(pC + 32);
                            _sum9 = _mm_loadu_ps(pC + 36);
                            _suma = _mm_loadu_ps(pC + 40);
                            _sumb = _mm_loadu_ps(pC + 44);
                            pC += 48;
                        }
                        if (out_elempack == 1)
                        {
                            _sum0 = _mm_loadu_ps(pC);
                            _sum1 = _mm_loadu_ps(pC + N);
                            _sum2 = _mm_loadu_ps(pC + N * 2);
                            _sum3 = _mm_loadu_ps(pC + N * 3);
                            _sum4 = _mm_loadu_ps(pC + 4);
                            _sum5 = _mm_loadu_ps(pC + N + 4);
                            _sum6 = _mm_loadu_ps(pC + N * 2 + 4);
                            _sum7 = _mm_loadu_ps(pC + N * 3 + 4);
                            _sum8 = _mm_loadu_ps(pC + 8);
                            _sum9 = _mm_loadu_ps(pC + N + 8);
                            _suma = _mm_loadu_ps(pC + N * 2 + 8);
                            _sumb = _mm_loadu_ps(pC + N * 3 + 8);

                            _MM_TRANSPOSE4_PS(_sum0, _sum1, _sum2, _sum3);
                            _MM_TRANSPOSE4_PS(_sum4, _sum5, _sum6, _sum7);
                            _MM_TRANSPOSE4_PS(_sum8, _sum9, _suma, _sumb);
                            pC += 12;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm_set1_ps(pC[0]);
                        _sum1 = _mm_set1_ps(pC[1]);
                        _sum2 = _mm_set1_ps(pC[2]);
                        _sum3 = _mm_set1_ps(pC[3]);
                        _sum4 = _mm_set1_ps(pC[4]);
                        _sum5 = _mm_set1_ps(pC[5]);
                        _sum6 = _mm_set1_ps(pC[6]);
                        _sum7 = _mm_set1_ps(pC[7]);
                        _sum8 = _mm_set1_ps(pC[8]);
                        _sum9 = _mm_set1_ps(pC[9]);
                        _suma = _mm_set1_ps(pC[10]);
                        _sumb = _mm_set1_ps(pC[11]);
                        pC += 12;
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
                _sum0 = _mm_load_ps(ptmp);
                _sum1 = _mm_load_ps(ptmp + 4 * 1);
                _sum2 = _mm_load_ps(ptmp + 4 * 2);
                _sum3 = _mm_load_ps(ptmp + 4 * 3);
                _sum4 = _mm_load_ps(ptmp + 4 * 4);
                _sum5 = _mm_load_ps(ptmp + 4 * 5);
                _sum6 = _mm_load_ps(ptmp + 4 * 6);
                _sum7 = _mm_load_ps(ptmp + 4 * 7);
                _sum8 = _mm_load_ps(ptmp + 4 * 8);
                _sum9 = _mm_load_ps(ptmp + 4 * 9);
                _suma = _mm_load_ps(ptmp + 4 * 10);
                _sumb = _mm_load_ps(ptmp + 4 * 11);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pA = _mm_load_ps(pA);

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

            if (k_end)
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
                    _mm_store_ps(outptr0, _sum0);
                    _mm_store_ps(outptr0 + 4 * 1, _sum1);
                    _mm_store_ps(outptr0 + 4 * 2, _sum2);
                    _mm_store_ps(outptr0 + 4 * 3, _sum3);
                    _mm_store_ps(outptr0 + 4 * 4, _sum4);
                    _mm_store_ps(outptr0 + 4 * 5, _sum5);
                    _mm_store_ps(outptr0 + 4 * 6, _sum6);
                    _mm_store_ps(outptr0 + 4 * 7, _sum7);
                    _mm_store_ps(outptr0 + 4 * 8, _sum8);
                    _mm_store_ps(outptr0 + 4 * 9, _sum9);
                    _mm_store_ps(outptr0 + 4 * 10, _suma);
                    _mm_store_ps(outptr0 + 4 * 11, _sumb);
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
                _mm_store_ps(ptmp, _sum0);
                _mm_store_ps(ptmp + 4 * 1, _sum1);
                _mm_store_ps(ptmp + 4 * 2, _sum2);
                _mm_store_ps(ptmp + 4 * 3, _sum3);
                _mm_store_ps(ptmp + 4 * 4, _sum4);
                _mm_store_ps(ptmp + 4 * 5, _sum5);
                _mm_store_ps(ptmp + 4 * 6, _sum6);
                _mm_store_ps(ptmp + 4 * 7, _sum7);
                _mm_store_ps(ptmp + 4 * 8, _sum8);
                _mm_store_ps(ptmp + 4 * 9, _sum9);
                _mm_store_ps(ptmp + 4 * 10, _suma);
                _mm_store_ps(ptmp + 4 * 11, _sumb);
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
                        _sum0 = _mm_loadu_ps(pC);
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
                            _sum0 = _mm_loadu_ps(pC);
                            _sum1 = _mm_loadu_ps(pC + 4);
                            _sum2 = _mm_loadu_ps(pC + 8);
                            _sum3 = _mm_loadu_ps(pC + 12);
                            _sum4 = _mm_loadu_ps(pC + 16);
                            _sum5 = _mm_loadu_ps(pC + 20);
                            _sum6 = _mm_loadu_ps(pC + 24);
                            _sum7 = _mm_loadu_ps(pC + 28);
                            pC += 32;
                        }
                        if (out_elempack == 1)
                        {
                            _sum0 = _mm_loadu_ps(pC);
                            _sum1 = _mm_loadu_ps(pC + N);
                            _sum2 = _mm_loadu_ps(pC + N * 2);
                            _sum3 = _mm_loadu_ps(pC + N * 3);
                            _sum4 = _mm_loadu_ps(pC + 4);
                            _sum5 = _mm_loadu_ps(pC + N + 4);
                            _sum6 = _mm_loadu_ps(pC + N * 2 + 4);
                            _sum7 = _mm_loadu_ps(pC + N * 3 + 4);

                            _MM_TRANSPOSE4_PS(_sum0, _sum1, _sum2, _sum3);
                            _MM_TRANSPOSE4_PS(_sum4, _sum5, _sum6, _sum7);
                            pC += 8;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm_set1_ps(pC[0]);
                        _sum1 = _mm_set1_ps(pC[1]);
                        _sum2 = _mm_set1_ps(pC[2]);
                        _sum3 = _mm_set1_ps(pC[3]);
                        _sum4 = _mm_set1_ps(pC[4]);
                        _sum5 = _mm_set1_ps(pC[5]);
                        _sum6 = _mm_set1_ps(pC[6]);
                        _sum7 = _mm_set1_ps(pC[7]);
                        pC += 8;
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
                _sum0 = _mm_load_ps(ptmp);
                _sum1 = _mm_load_ps(ptmp + 4 * 1);
                _sum2 = _mm_load_ps(ptmp + 4 * 2);
                _sum3 = _mm_load_ps(ptmp + 4 * 3);
                _sum4 = _mm_load_ps(ptmp + 4 * 4);
                _sum5 = _mm_load_ps(ptmp + 4 * 5);
                _sum6 = _mm_load_ps(ptmp + 4 * 6);
                _sum7 = _mm_load_ps(ptmp + 4 * 7);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pA = _mm_load_ps(pA);

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

            if (k_end)
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
                    _mm_store_ps(outptr0, _sum0);
                    _mm_store_ps(outptr0 + 4 * 1, _sum1);
                    _mm_store_ps(outptr0 + 4 * 2, _sum2);
                    _mm_store_ps(outptr0 + 4 * 3, _sum3);
                    _mm_store_ps(outptr0 + 4 * 4, _sum4);
                    _mm_store_ps(outptr0 + 4 * 5, _sum5);
                    _mm_store_ps(outptr0 + 4 * 6, _sum6);
                    _mm_store_ps(outptr0 + 4 * 7, _sum7);
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
                _mm_store_ps(ptmp, _sum0);
                _mm_store_ps(ptmp + 4 * 1, _sum1);
                _mm_store_ps(ptmp + 4 * 2, _sum2);
                _mm_store_ps(ptmp + 4 * 3, _sum3);
                _mm_store_ps(ptmp + 4 * 4, _sum4);
                _mm_store_ps(ptmp + 4 * 5, _sum5);
                _mm_store_ps(ptmp + 4 * 6, _sum6);
                _mm_store_ps(ptmp + 4 * 7, _sum7);
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
                        _sum0 = _mm_loadu_ps(pC);
                        _sum1 = _sum0;
                        _sum2 = _sum0;
                        _sum3 = _sum0;
                    }
                    if (broadcast_type_C == 3)
                    {
                        if (out_elempack == 4)
                        {
                            _sum0 = _mm_loadu_ps(pC);
                            _sum1 = _mm_loadu_ps(pC + 4);
                            _sum2 = _mm_loadu_ps(pC + 8);
                            _sum3 = _mm_loadu_ps(pC + 12);
                            pC += 16;
                        }
                        if (out_elempack == 1)
                        {
                            _sum0 = _mm_loadu_ps(pC);
                            _sum1 = _mm_loadu_ps(pC + N);
                            _sum2 = _mm_loadu_ps(pC + N * 2);
                            _sum3 = _mm_loadu_ps(pC + N * 3);

                            _MM_TRANSPOSE4_PS(_sum0, _sum1, _sum2, _sum3);
                            pC += 4;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm_set1_ps(pC[0]);
                        _sum1 = _mm_set1_ps(pC[1]);
                        _sum2 = _mm_set1_ps(pC[2]);
                        _sum3 = _mm_set1_ps(pC[3]);
                        pC += 4;
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
                _sum0 = _mm_load_ps(ptmp);
                _sum1 = _mm_load_ps(ptmp + 4 * 1);
                _sum2 = _mm_load_ps(ptmp + 4 * 2);
                _sum3 = _mm_load_ps(ptmp + 4 * 3);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pA = _mm_load_ps(pA);

                _sum0 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[0]), _sum0);
                _sum1 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[1]), _sum1);
                _sum2 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[2]), _sum2);
                _sum3 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[3]), _sum3);

                pA += 4;
                pB += 4;
            }

            if (k_end)
            {
                __m128 _alpha = _mm_set1_ps(alpha);
                _sum0 = _mm_mul_ps(_sum0, _alpha);
                _sum1 = _mm_mul_ps(_sum1, _alpha);
                _sum2 = _mm_mul_ps(_sum2, _alpha);
                _sum3 = _mm_mul_ps(_sum3, _alpha);

                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _sum0);
                    _mm_store_ps(outptr0 + 4 * 1, _sum1);
                    _mm_store_ps(outptr0 + 4 * 2, _sum2);
                    _mm_store_ps(outptr0 + 4 * 3, _sum3);
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
                _mm_store_ps(ptmp, _sum0);
                _mm_store_ps(ptmp + 4 * 1, _sum1);
                _mm_store_ps(ptmp + 4 * 2, _sum2);
                _mm_store_ps(ptmp + 4 * 3, _sum3);
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
                        _sum0 = _mm_loadu_ps(pC);
                        _sum1 = _sum0;
                    }
                    if (broadcast_type_C == 3)
                    {
                        if (out_elempack == 4)
                        {
                            _sum0 = _mm_loadu_ps(pC);
                            _sum1 = _mm_loadu_ps(pC + 4);
                            pC += 8;
                        }
                        if (out_elempack == 1)
                        {
                            float sum0[4];
                            float sum1[4];
                            sum0[0] = pC[0];
                            sum0[1] = pC[N];
                            sum0[2] = pC[N * 2];
                            sum0[3] = pC[N * 3];
                            sum1[0] = pC[1];
                            sum1[1] = pC[N + 1];
                            sum1[2] = pC[N * 2 + 1];
                            sum1[3] = pC[N * 3 + 1];

                            _sum0 = _mm_loadu_ps(sum0);
                            _sum1 = _mm_loadu_ps(sum1);
                            pC += 2;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm_set1_ps(pC[0]);
                        _sum1 = _mm_set1_ps(pC[1]);
                        pC += 2;
                    }

                    __m128 _beta = _mm_set1_ps(beta);
                    _sum0 = _mm_mul_ps(_sum0, _beta);
                    _sum1 = _mm_mul_ps(_sum1, _beta);
                }
            }
            else
            {
                _sum0 = _mm_load_ps(ptmp);
                _sum1 = _mm_load_ps(ptmp + 4);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pA = _mm_load_ps(pA);

                _sum0 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[0]), _sum0);
                _sum1 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[1]), _sum1);

                pA += 4;
                pB += 2;
            }

            if (k_end)
            {
                __m128 _alpha = _mm_set1_ps(alpha);
                _sum0 = _mm_mul_ps(_sum0, _alpha);
                _sum1 = _mm_mul_ps(_sum1, _alpha);

                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _sum0);
                    _mm_store_ps(outptr0 + 4, _sum1);
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
                _mm_store_ps(ptmp, _sum0);
                _mm_store_ps(ptmp + 4, _sum1);
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
                        _sum0 = _mm_loadu_ps(pC);
                    }
                    if (broadcast_type_C == 3)
                    {
                        if (out_elempack == 4)
                        {
                            _sum0 = _mm_loadu_ps(pC);
                            pC += 4;
                        }
                        if (out_elempack == 1)
                        {
                            float sum0[4];
                            sum0[0] = pC[0];
                            sum0[1] = pC[N];
                            sum0[2] = pC[N * 2];
                            sum0[3] = pC[N * 3];

                            _sum0 = _mm_loadu_ps(sum0);
                            pC += 1;
                        }
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm_set1_ps(pC[0]);
                        pC += 1;
                    }

                    __m128 _beta = _mm_set1_ps(beta);
                    _sum0 = _mm_mul_ps(_sum0, _beta);
                }
            }
            else
            {
                _sum0 = _mm_load_ps(ptmp);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pA = _mm_load_ps(pA);

                _sum0 = _mm_comp_fmadd_ps(_pA, _mm_set1_ps(pB[0]), _sum0);

                pA += 4;
                pB += 1;
            }

            if (k_end)
            {
                __m128 _alpha = _mm_set1_ps(alpha);
                _sum0 = _mm_mul_ps(_sum0, _alpha);

                if (out_elempack == 4)
                {
                    _mm_store_ps(outptr0, _sum0);
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
                _mm_store_ps(ptmp, _sum0);
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

        const float* pC = C;
        if (pC)
        {
            if (broadcast_type_C == 1 || broadcast_type_C == 2)
            {
                pC = pC + i + ii;
            }
            if (broadcast_type_C == 3)
            {
                pC = C.row(i + ii) + j;
            }
            if (broadcast_type_C == 4)
            {
                pC = pC + j;
            }
        }

        int jj = 0;
#if __SSE2__
        for (; jj + 11 < max_jj; jj += 12)
        {
            __m128 _sum00;
            __m128 _sum01;
            __m128 _sum02;
            __m128 _sum10;
            __m128 _sum11;
            __m128 _sum12;

            if (k == 0)
            {
                _sum00 = _mm_setzero_ps();
                _sum01 = _mm_setzero_ps();
                _sum02 = _mm_setzero_ps();
                _sum10 = _mm_setzero_ps();
                _sum11 = _mm_setzero_ps();
                _sum12 = _mm_setzero_ps();

                if (pC)
                {
                    if (broadcast_type_C == 0)
                    {
                        _sum00 = _mm_set1_ps(pC[0]);
                        _sum01 = _mm_set1_ps(pC[0]);
                        _sum02 = _mm_set1_ps(pC[0]);
                        _sum10 = _mm_set1_ps(pC[0]);
                        _sum11 = _mm_set1_ps(pC[0]);
                        _sum12 = _mm_set1_ps(pC[0]);
                    }
                    if (broadcast_type_C == 1 || broadcast_type_C == 2)
                    {
                        _sum00 = _mm_set1_ps(pC[0]);
                        _sum01 = _mm_set1_ps(pC[0]);
                        _sum02 = _mm_set1_ps(pC[0]);
                        _sum10 = _mm_set1_ps(pC[1]);
                        _sum11 = _mm_set1_ps(pC[1]);
                        _sum12 = _mm_set1_ps(pC[1]);
                    }
                    if (broadcast_type_C == 3)
                    {
                        _sum00 = _mm_loadu_ps(pC);
                        _sum01 = _mm_loadu_ps(pC + 4);
                        _sum02 = _mm_loadu_ps(pC + 8);
                        _sum10 = _mm_loadu_ps(pC + N);
                        _sum11 = _mm_loadu_ps(pC + N + 4);
                        _sum12 = _mm_loadu_ps(pC + N + 8);
                        pC += 12;
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum00 = _mm_loadu_ps(pC);
                        _sum01 = _mm_loadu_ps(pC + 4);
                        _sum02 = _mm_loadu_ps(pC + 8);
                        _sum10 = _sum00;
                        _sum11 = _sum01;
                        _sum12 = _sum02;
                        pC += 12;
                    }

                    __m128 _beta = _mm_set1_ps(beta);
                    _sum00 = _mm_mul_ps(_sum00, _beta);
                    _sum01 = _mm_mul_ps(_sum01, _beta);
                    _sum02 = _mm_mul_ps(_sum02, _beta);
                    _sum10 = _mm_mul_ps(_sum10, _beta);
                    _sum11 = _mm_mul_ps(_sum11, _beta);
                    _sum12 = _mm_mul_ps(_sum12, _beta);
                }
            }
            else
            {
                _sum00 = _mm_load_ps(ptmp);
                _sum01 = _mm_load_ps(ptmp + 4);
                _sum02 = _mm_load_ps(ptmp + 8);
                _sum10 = _mm_load_ps(ptmp + 12);
                _sum11 = _mm_load_ps(ptmp + 16);
                _sum12 = _mm_load_ps(ptmp + 20);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pB0 = _mm_load_ps(pB);
                __m128 _pB1 = _mm_load_ps(pB + 4);
                __m128 _pB2 = _mm_load_ps(pB + 8);

                __m128 _pA0 = _mm_set1_ps(pA[0]);
                _sum00 = _mm_comp_fmadd_ps(_pA0, _pB0, _sum00);
                _sum01 = _mm_comp_fmadd_ps(_pA0, _pB1, _sum01);
                _sum02 = _mm_comp_fmadd_ps(_pA0, _pB2, _sum02);
                __m128 _pA1 = _mm_set1_ps(pA[1]);
                _sum10 = _mm_comp_fmadd_ps(_pA1, _pB0, _sum10);
                _sum11 = _mm_comp_fmadd_ps(_pA1, _pB1, _sum11);
                _sum12 = _mm_comp_fmadd_ps(_pA1, _pB2, _sum12);

                pA += 2;
                pB += 12;
            }

            if (k_end)
            {
                __m128 _alpha = _mm_set1_ps(alpha);
                _sum00 = _mm_mul_ps(_sum00, _alpha);
                _sum01 = _mm_mul_ps(_sum01, _alpha);
                _sum02 = _mm_mul_ps(_sum02, _alpha);
                _sum10 = _mm_mul_ps(_sum10, _alpha);
                _sum11 = _mm_mul_ps(_sum11, _alpha);
                _sum12 = _mm_mul_ps(_sum12, _alpha);

                // if (out_elempack == 1)
                {
                    _mm_storeu_ps(outptr0, _sum00);
                    _mm_storeu_ps(outptr0 + 4, _sum01);
                    _mm_storeu_ps(outptr0 + 8, _sum02);
                    _mm_storeu_ps(outptr0 + N, _sum10);
                    _mm_storeu_ps(outptr0 + N + 4, _sum11);
                    _mm_storeu_ps(outptr0 + N + 8, _sum12);
                    outptr0 += 12;
                }
            }
            else
            {
                _mm_store_ps(ptmp, _sum00);
                _mm_store_ps(ptmp + 4, _sum01);
                _mm_store_ps(ptmp + 8, _sum02);
                _mm_store_ps(ptmp + 12, _sum10);
                _mm_store_ps(ptmp + 16, _sum11);
                _mm_store_ps(ptmp + 20, _sum12);
            }

            ptmp += 24;
        }
        for (; jj + 7 < max_jj; jj += 8)
        {
            __m128 _sum00;
            __m128 _sum01;
            __m128 _sum10;
            __m128 _sum11;

            if (k == 0)
            {
                _sum00 = _mm_setzero_ps();
                _sum01 = _mm_setzero_ps();
                _sum10 = _mm_setzero_ps();
                _sum11 = _mm_setzero_ps();

                if (pC)
                {
                    if (broadcast_type_C == 0)
                    {
                        _sum00 = _mm_set1_ps(pC[0]);
                        _sum01 = _mm_set1_ps(pC[0]);
                        _sum10 = _mm_set1_ps(pC[0]);
                        _sum11 = _mm_set1_ps(pC[0]);
                    }
                    if (broadcast_type_C == 1 || broadcast_type_C == 2)
                    {
                        _sum00 = _mm_set1_ps(pC[0]);
                        _sum01 = _mm_set1_ps(pC[0]);
                        _sum10 = _mm_set1_ps(pC[1]);
                        _sum11 = _mm_set1_ps(pC[1]);
                    }
                    if (broadcast_type_C == 3)
                    {
                        _sum00 = _mm_loadu_ps(pC);
                        _sum01 = _mm_loadu_ps(pC + 4);
                        _sum10 = _mm_loadu_ps(pC + N);
                        _sum11 = _mm_loadu_ps(pC + N + 4);
                        pC += 8;
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum00 = _mm_loadu_ps(pC);
                        _sum01 = _mm_loadu_ps(pC + 4);
                        _sum10 = _sum00;
                        _sum11 = _sum01;
                        pC += 8;
                    }

                    __m128 _beta = _mm_set1_ps(beta);
                    _sum00 = _mm_mul_ps(_sum00, _beta);
                    _sum01 = _mm_mul_ps(_sum01, _beta);
                    _sum10 = _mm_mul_ps(_sum10, _beta);
                    _sum11 = _mm_mul_ps(_sum11, _beta);
                }
            }
            else
            {
                _sum00 = _mm_load_ps(ptmp);
                _sum01 = _mm_load_ps(ptmp + 4);
                _sum10 = _mm_load_ps(ptmp + 8);
                _sum11 = _mm_load_ps(ptmp + 12);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pB0 = _mm_load_ps(pB);
                __m128 _pB1 = _mm_load_ps(pB + 4);

                __m128 _pA0 = _mm_set1_ps(pA[0]);
                _sum00 = _mm_comp_fmadd_ps(_pA0, _pB0, _sum00);
                _sum01 = _mm_comp_fmadd_ps(_pA0, _pB1, _sum01);
                __m128 _pA1 = _mm_set1_ps(pA[1]);
                _sum10 = _mm_comp_fmadd_ps(_pA1, _pB0, _sum10);
                _sum11 = _mm_comp_fmadd_ps(_pA1, _pB1, _sum11);

                pA += 2;
                pB += 8;
            }

            if (k_end)
            {
                __m128 _alpha = _mm_set1_ps(alpha);
                _sum00 = _mm_mul_ps(_sum00, _alpha);
                _sum01 = _mm_mul_ps(_sum01, _alpha);
                _sum10 = _mm_mul_ps(_sum10, _alpha);
                _sum11 = _mm_mul_ps(_sum11, _alpha);

                // if (out_elempack == 1)
                {
                    _mm_storeu_ps(outptr0, _sum00);
                    _mm_storeu_ps(outptr0 + 4, _sum01);
                    _mm_storeu_ps(outptr0 + N, _sum10);
                    _mm_storeu_ps(outptr0 + N + 4, _sum11);
                    outptr0 += 8;
                }
            }
            else
            {
                _mm_store_ps(ptmp, _sum00);
                _mm_store_ps(ptmp + 4, _sum01);
                _mm_store_ps(ptmp + 8, _sum10);
                _mm_store_ps(ptmp + 12, _sum11);
            }

            ptmp += 16;
        }
        for (; jj + 3 < max_jj; jj += 4)
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
                        _sum0 = _mm_set1_ps(pC[0]);
                        _sum1 = _mm_set1_ps(pC[1]);
                    }
                    if (broadcast_type_C == 3)
                    {
                        _sum0 = _mm_loadu_ps(pC);
                        _sum1 = _mm_loadu_ps(pC + N);
                        pC += 4;
                    }
                    if (broadcast_type_C == 4)
                    {
                        _sum0 = _mm_loadu_ps(pC);
                        _sum1 = _sum0;
                        pC += 4;
                    }

                    __m128 _beta = _mm_set1_ps(beta);
                    _sum0 = _mm_mul_ps(_sum0, _beta);
                    _sum1 = _mm_mul_ps(_sum1, _beta);
                }
            }
            else
            {
                _sum0 = _mm_load_ps(ptmp);
                _sum1 = _mm_load_ps(ptmp + 4);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pB = _mm_load_ps(pB);

                _sum0 = _mm_comp_fmadd_ps(_mm_set1_ps(pA[0]), _pB, _sum0);
                _sum1 = _mm_comp_fmadd_ps(_mm_set1_ps(pA[1]), _pB, _sum1);

                pA += 2;
                pB += 4;
            }

            if (k_end)
            {
                __m128 _alpha = _mm_set1_ps(alpha);
                _sum0 = _mm_mul_ps(_sum0, _alpha);
                _sum1 = _mm_mul_ps(_sum1, _alpha);

                // if (out_elempack == 1)
                {
                    _mm_storeu_ps(outptr0, _sum0);
                    _mm_storeu_ps(outptr0 + N, _sum1);
                    outptr0 += 4;
                }
            }
            else
            {
                _mm_store_ps(ptmp, _sum0);
                _mm_store_ps(ptmp + 4, _sum1);
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
                        sum00 = pC[0];
                        sum01 = pC[0];
                        sum10 = pC[1];
                        sum11 = pC[1];
                    }
                    if (broadcast_type_C == 3)
                    {
                        sum00 = pC[0];
                        sum01 = pC[1];
                        sum10 = pC[N];
                        sum11 = pC[N + 1];
                        pC += 2;
                    }
                    if (broadcast_type_C == 4)
                    {
                        sum00 = pC[0];
                        sum01 = pC[1];
                        sum10 = pC[0];
                        sum11 = pC[1];
                        pC += 2;
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

            if (k_end)
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
                        sum0 = pC[0];
                        sum1 = pC[1];
                    }
                    if (broadcast_type_C == 3)
                    {
                        sum0 = pC[0];
                        sum1 = pC[N];
                        pC += 1;
                    }
                    if (broadcast_type_C == 4)
                    {
                        sum0 = pC[0];
                        sum1 = pC[0];
                        pC += 1;
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

            if (k_end)
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

        const float* pC = C;
        if (pC)
        {
            if (broadcast_type_C == 1 || broadcast_type_C == 2)
            {
                pC = pC + i + ii;
            }
            if (broadcast_type_C == 3)
            {
                pC = C.row(i + ii) + j;
            }
            if (broadcast_type_C == 4)
            {
                pC = pC + j;
            }
        }

        int jj = 0;
#if __SSE2__
        for (; jj + 11 < max_jj; jj += 12)
        {
            __m128 _sum0;
            __m128 _sum1;
            __m128 _sum2;

            if (k == 0)
            {
                _sum0 = _mm_setzero_ps();
                _sum1 = _mm_setzero_ps();
                _sum2 = _mm_setzero_ps();

                if (pC)
                {
                    if (broadcast_type_C == 0 || broadcast_type_C == 1 || broadcast_type_C == 2)
                    {
                        _sum0 = _mm_set1_ps(pC[0]);
                        _sum1 = _mm_set1_ps(pC[0]);
                        _sum2 = _mm_set1_ps(pC[0]);
                    }
                    if (broadcast_type_C == 3 || broadcast_type_C == 4)
                    {
                        _sum0 = _mm_loadu_ps(pC);
                        _sum1 = _mm_loadu_ps(pC + 4);
                        _sum2 = _mm_loadu_ps(pC + 8);
                        pC += 12;
                    }

                    __m128 _beta = _mm_set1_ps(beta);
                    _sum0 = _mm_mul_ps(_sum0, _beta);
                    _sum1 = _mm_mul_ps(_sum1, _beta);
                    _sum2 = _mm_mul_ps(_sum2, _beta);
                }
            }
            else
            {
                _sum0 = _mm_loadu_ps(ptmp);
                _sum1 = _mm_loadu_ps(ptmp + 4);
                _sum2 = _mm_loadu_ps(ptmp + 8);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pB0 = _mm_load_ps(pB);
                __m128 _pB1 = _mm_load_ps(pB + 4);
                __m128 _pB2 = _mm_load_ps(pB + 8);

                __m128 _pA0 = _mm_set1_ps(pA[0]);
                _sum0 = _mm_comp_fmadd_ps(_pA0, _pB0, _sum0);
                _sum1 = _mm_comp_fmadd_ps(_pA0, _pB1, _sum1);
                _sum2 = _mm_comp_fmadd_ps(_pA0, _pB2, _sum2);

                pA += 1;
                pB += 12;
            }

            if (k_end)
            {
                __m128 _alpha = _mm_set1_ps(alpha);
                _sum0 = _mm_mul_ps(_sum0, _alpha);
                _sum1 = _mm_mul_ps(_sum1, _alpha);
                _sum2 = _mm_mul_ps(_sum2, _alpha);

                // if (out_elempack == 1)
                {
                    _mm_storeu_ps(outptr0, _sum0);
                    _mm_storeu_ps(outptr0 + 4, _sum1);
                    _mm_storeu_ps(outptr0 + 8, _sum2);
                    outptr0 += 12;
                }
            }
            else
            {
                _mm_storeu_ps(ptmp, _sum0);
                _mm_storeu_ps(ptmp + 4, _sum1);
                _mm_storeu_ps(ptmp + 8, _sum2);
            }

            ptmp += 12;
        }
        for (; jj + 7 < max_jj; jj += 8)
        {
            __m128 _sum0;
            __m128 _sum1;

            if (k == 0)
            {
                _sum0 = _mm_setzero_ps();
                _sum1 = _mm_setzero_ps();

                if (pC)
                {
                    if (broadcast_type_C == 0 || broadcast_type_C == 1 || broadcast_type_C == 2)
                    {
                        _sum0 = _mm_set1_ps(pC[0]);
                        _sum1 = _mm_set1_ps(pC[0]);
                    }
                    if (broadcast_type_C == 3 || broadcast_type_C == 4)
                    {
                        _sum0 = _mm_loadu_ps(pC);
                        _sum1 = _mm_loadu_ps(pC + 4);
                        pC += 8;
                    }

                    __m128 _beta = _mm_set1_ps(beta);
                    _sum0 = _mm_mul_ps(_sum0, _beta);
                    _sum1 = _mm_mul_ps(_sum1, _beta);
                }
            }
            else
            {
                _sum0 = _mm_loadu_ps(ptmp);
                _sum1 = _mm_loadu_ps(ptmp + 4);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pB0 = _mm_load_ps(pB);
                __m128 _pB1 = _mm_load_ps(pB + 4);

                __m128 _pA0 = _mm_set1_ps(pA[0]);
                _sum0 = _mm_comp_fmadd_ps(_pA0, _pB0, _sum0);
                _sum1 = _mm_comp_fmadd_ps(_pA0, _pB1, _sum1);

                pA += 1;
                pB += 8;
            }

            if (k_end)
            {
                __m128 _alpha = _mm_set1_ps(alpha);
                _sum0 = _mm_mul_ps(_sum0, _alpha);
                _sum1 = _mm_mul_ps(_sum1, _alpha);

                // if (out_elempack == 1)
                {
                    _mm_storeu_ps(outptr0, _sum0);
                    _mm_storeu_ps(outptr0 + 4, _sum1);
                    outptr0 += 8;
                }
            }
            else
            {
                _mm_storeu_ps(ptmp, _sum0);
                _mm_storeu_ps(ptmp + 4, _sum1);
            }

            ptmp += 8;
        }
        for (; jj + 3 < max_jj; jj += 4)
        {
            __m128 _sum;

            if (k == 0)
            {
                _sum = _mm_setzero_ps();

                if (pC)
                {
                    if (broadcast_type_C == 0 || broadcast_type_C == 1 || broadcast_type_C == 2)
                    {
                        _sum = _mm_set1_ps(pC[0]);
                    }
                    if (broadcast_type_C == 3 || broadcast_type_C == 4)
                    {
                        _sum = _mm_loadu_ps(pC);
                        pC += 4;
                    }

                    __m128 _beta = _mm_set1_ps(beta);
                    _sum = _mm_mul_ps(_sum, _beta);
                }
            }
            else
            {
                _sum = _mm_loadu_ps(ptmp);
            }

            const float* pA = pA0;
            int kk = 0;
            for (; kk < max_kk; kk += 1)
            {
                __m128 _pB = _mm_load_ps(pB);

                _sum = _mm_comp_fmadd_ps(_mm_set1_ps(pA[0]), _pB, _sum);

                pA += 1;
                pB += 4;
            }

            if (k_end)
            {
                __m128 _alpha = _mm_set1_ps(alpha);
                _sum = _mm_mul_ps(_sum, _alpha);

                // if (out_elempack == 1)
                {
                    _mm_storeu_ps(outptr0, _sum);
                    outptr0 += 4;
                }
            }
            else
            {
                _mm_storeu_ps(ptmp, _sum);
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
                    if (broadcast_type_C == 0 || broadcast_type_C == 1 || broadcast_type_C == 2)
                    {
                        sum0 = pC[0];
                        sum1 = pC[0];
                    }
                    if (broadcast_type_C == 3 || broadcast_type_C == 4)
                    {
                        sum0 = pC[0];
                        sum1 = pC[1];
                        pC += 2;
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

            if (k_end)
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
                    if (broadcast_type_C == 0 || broadcast_type_C == 1 || broadcast_type_C == 2)
                    {
                        sum = pC[0];
                    }
                    if (broadcast_type_C == 3 || broadcast_type_C == 4)
                    {
                        sum = pC[0];
                        pC += 1;
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

            if (k_end)
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

static int gemm_x86(const Mat& A, const Mat& B, const Mat& C, Mat& top_blob, int broadcast_type_C, int transA, int transB, float alpha, float beta, const Option& opt)
{
    const int M = transA ? A.w : A.h * A.elempack;
    const int K = transA ? A.h * A.elempack : A.w;
    const int N = transB ? B.h * B.elempack : B.w;

    // NCNN_LOGE("M/N/K = %d %d %d", M, N, K);

    // TODO do not hardcode
    int TILE_M = 16 * 16 * get_physical_cpu_count(); // 256
    int TILE_N = 12 * 20;                            // 240
    int TILE_K = 16 * 16;

    {
        int nn_M = (M + TILE_M - 1) / TILE_M;
        int nn_N = (N + TILE_N - 1) / TILE_N;
        int nn_K = (K + TILE_K - 1) / TILE_K;

        TILE_M = std::min(TILE_M, ((M + nn_M - 1) / nn_M + 15) / 16 * 16);
        TILE_N = std::min(TILE_N, ((N + nn_N - 1) / nn_N + 11) / 12 * 12);
        TILE_K = std::min(TILE_K, ((K + nn_K - 1) / nn_K + 15) / 16 * 16);

        if (opt.num_threads > 1)
        {
            TILE_M = std::min(TILE_M, (TILE_M / opt.num_threads + 15) / 16 * 16);
            TILE_N = std::min(TILE_N, (TILE_N / opt.num_threads + 11) / 12 * 12);
            TILE_K = std::min(TILE_K, (TILE_K / opt.num_threads + 15) / 16 * 16);
        }
    }

    // NCNN_LOGE("TILE M/N/K = %d %d %d", TILE_M, TILE_N, TILE_K);

    int nn_M = (M + TILE_M - 1) / TILE_M;
    int nn_N = (N + TILE_N - 1) / TILE_N;

    Mat ATX(TILE_K * TILE_M, (K + TILE_K - 1) / TILE_K, opt.num_threads, 4u, opt.blob_allocator);
    Mat BT(TILE_K * TILE_N, (K + TILE_K - 1) / TILE_K, (N + TILE_N - 1) / TILE_N, 4u, opt.blob_allocator);

    Mat tmpX;
    if (K > TILE_K)
        tmpX.create(TILE_N, TILE_M, opt.num_threads, 4u, opt.blob_allocator);

    // pack B
    #pragma omp parallel for num_threads(opt.num_threads)
    for (int ppj = 0; ppj < nn_N; ppj++)
    {
        const int j = ppj * TILE_N;

        for (int k = 0; k < K; k += TILE_K)
        {
            const int max_jj = std::min((N - j), TILE_N);
            const int max_kk = std::min((K - k), TILE_K);

            Mat BT_tile = BT.channel(j / TILE_N).row_range(k / TILE_K, 1);

            if (transB)
            {
                pack_B_tile(B, BT_tile, j, max_jj, k, max_kk);
            }
            else
            {
                transpose_pack_B_tile(B, BT_tile, j, max_jj, k, max_kk);
            }
        }
    }

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int ppi = 0; ppi < nn_M; ppi++)
    {
        const int i = ppi * TILE_M;

        Mat tmp;
        if (K > TILE_K)
            tmp = tmpX.channel(get_omp_thread_num());

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

                Mat AT_tile = ATX.channel(get_omp_thread_num()).row_range(k / TILE_K, 1);

                Mat BT_tile = BT.channel(j / TILE_N).row_range(k / TILE_K, 1);

                if (j == 0)
                {
                    if (transA)
                    {
                        transpose_pack_A_tile(A, AT_tile, i, max_ii, k, max_kk);
                    }
                    else
                    {
                        pack_A_tile(A, AT_tile, i, max_ii, k, max_kk);
                    }
                }

                bool k_end = k + TILE_K >= K;

                matmul_packed_transB_tile(AT_tile, BT_tile, C, top_blob, broadcast_type_C, tmp, alpha, beta, i, max_ii, j, max_jj, k, max_kk, k_end);
            }
        }
    }

    return 0;
}

static int gemm_AT_x86(const Mat& AT, const Mat& B, const Mat& C, Mat& top_blob, int broadcast_type_C, int M, int K, int transB, float alpha, float beta, const Option& opt)
{
    const int N = transB ? B.h * B.elempack : B.w;

    // NCNN_LOGE("M/N/K = %d %d %d", M, N, K);

    // TODO do not hardcode
    int TILE_M = 16 * 16 * get_physical_cpu_count(); // 256
    int TILE_N = 12 * 20;                            // 240
    int TILE_K = 16 * 16;

    {
        int nn_M = (M + TILE_M - 1) / TILE_M;
        int nn_N = (N + TILE_N - 1) / TILE_N;
        int nn_K = (K + TILE_K - 1) / TILE_K;

        TILE_M = std::min(TILE_M, ((M + nn_M - 1) / nn_M + 15) / 16 * 16);
        TILE_N = std::min(TILE_N, ((N + nn_N - 1) / nn_N + 11) / 12 * 12);
        TILE_K = std::min(TILE_K, ((K + nn_K - 1) / nn_K + 15) / 16 * 16);

        if (opt.num_threads > 1)
        {
            TILE_M = std::min(TILE_M, (TILE_M / opt.num_threads + 15) / 16 * 16);
            TILE_N = std::min(TILE_N, (TILE_N / opt.num_threads + 11) / 12 * 12);
            TILE_K = std::min(TILE_K, (TILE_K / opt.num_threads + 15) / 16 * 16);
        }
    }

    // NCNN_LOGE("TILE M/N/K = %d %d %d", TILE_M, TILE_N, TILE_K);

    int nn_M = (M + TILE_M - 1) / TILE_M;
    int nn_N = (N + TILE_N - 1) / TILE_N;

    Mat BT(TILE_K * TILE_N, (K + TILE_K - 1) / TILE_K, (N + TILE_N - 1) / TILE_N, 4u, opt.blob_allocator);

    Mat tmpX;
    if (K > TILE_K)
        tmpX.create(TILE_N, TILE_M, opt.num_threads, 4u, opt.blob_allocator);

    // pack B
    #pragma omp parallel for num_threads(opt.num_threads)
    for (int ppj = 0; ppj < nn_N; ppj++)
    {
        const int j = ppj * TILE_N;

        for (int k = 0; k < K; k += TILE_K)
        {
            const int max_jj = std::min((N - j), TILE_N);
            const int max_kk = std::min((K - k), TILE_K);

            Mat BT_tile = BT.channel(j / TILE_N).row_range(k / TILE_K, 1);

            if (transB)
            {
                pack_B_tile(B, BT_tile, j, max_jj, k, max_kk);
            }
            else
            {
                transpose_pack_B_tile(B, BT_tile, j, max_jj, k, max_kk);
            }
        }
    }

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int ppi = 0; ppi < nn_M; ppi++)
    {
        const int i = ppi * TILE_M;

        Mat tmp;
        if (K > TILE_K)
            tmp = tmpX.channel(get_omp_thread_num());

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

                Mat AT_tile = AT.channel(i / TILE_M).row_range(k / TILE_K, 1);

                Mat BT_tile = BT.channel(j / TILE_N).row_range(k / TILE_K, 1);

                bool k_end = k + TILE_K >= K;

                matmul_packed_transB_tile(AT_tile, BT_tile, C, top_blob, broadcast_type_C, tmp, alpha, beta, i, max_ii, j, max_jj, k, max_kk, k_end);
            }
        }
    }

    return 0;
}

static int gemm_BT_x86(const Mat& A, const Mat& BT, const Mat& C, Mat& top_blob, int broadcast_type_C, int N, int K, int transA, float alpha, float beta, const Option& opt)
{
    const int M = transA ? A.w : A.h * A.elempack;

    // NCNN_LOGE("M/N/K = %d %d %d", M, N, K);

    // TODO do not hardcode
    int TILE_M = 16 * 16 * get_physical_cpu_count(); // 256
    int TILE_N = 12 * 20;                            // 240
    int TILE_K = 16 * 16;

    {
        int nn_M = (M + TILE_M - 1) / TILE_M;
        int nn_N = (N + TILE_N - 1) / TILE_N;
        int nn_K = (K + TILE_K - 1) / TILE_K;

        TILE_M = std::min(TILE_M, ((M + nn_M - 1) / nn_M + 15) / 16 * 16);
        TILE_N = std::min(TILE_N, ((N + nn_N - 1) / nn_N + 11) / 12 * 12);
        TILE_K = std::min(TILE_K, ((K + nn_K - 1) / nn_K + 15) / 16 * 16);

        if (opt.num_threads > 1)
        {
            TILE_M = std::min(TILE_M, (TILE_M / opt.num_threads + 15) / 16 * 16);
            TILE_N = std::min(TILE_N, (TILE_N / opt.num_threads + 11) / 12 * 12);
            TILE_K = std::min(TILE_K, (TILE_K / opt.num_threads + 15) / 16 * 16);
        }
    }

    // NCNN_LOGE("TILE M/N/K = %d %d %d", TILE_M, TILE_N, TILE_K);

    int nn_M = (M + TILE_M - 1) / TILE_M;
    // int nn_N = (N + TILE_N - 1) / TILE_N;

    Mat ATX(TILE_K * TILE_M, (K + TILE_K - 1) / TILE_K, opt.num_threads, 4u, opt.blob_allocator);

    Mat tmpX;
    if (K > TILE_K)
        tmpX.create(TILE_N, TILE_M, opt.num_threads, 4u, opt.blob_allocator);

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int ppi = 0; ppi < nn_M; ppi++)
    {
        const int i = ppi * TILE_M;

        Mat tmp;
        if (K > TILE_K)
            tmp = tmpX.channel(get_omp_thread_num());

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

                Mat AT_tile = ATX.channel(get_omp_thread_num()).row_range(k / TILE_K, 1);

                Mat BT_tile = BT.channel(j / TILE_N).row_range(k / TILE_K, 1);

                if (j == 0)
                {
                    if (transA)
                    {
                        transpose_pack_A_tile(A, AT_tile, i, max_ii, k, max_kk);
                    }
                    else
                    {
                        pack_A_tile(A, AT_tile, i, max_ii, k, max_kk);
                    }
                }

                bool k_end = k + TILE_K >= K;

                matmul_packed_transB_tile(AT_tile, BT_tile, C, top_blob, broadcast_type_C, tmp, alpha, beta, i, max_ii, j, max_jj, k, max_kk, k_end);
            }
        }
    }

    return 0;
}

static int gemm_AT_BT_x86(const Mat& AT, const Mat& BT, const Mat& C, Mat& top_blob, int broadcast_type_C, int M, int N, int K, float alpha, float beta, const Option& opt)
{
    // NCNN_LOGE("M/N/K = %d %d %d", M, N, K);

    // TODO do not hardcode
    int TILE_M = 16 * 16 * get_physical_cpu_count(); // 256
    int TILE_N = 12 * 20;                            // 240
    int TILE_K = 16 * 16;

    {
        int nn_M = (M + TILE_M - 1) / TILE_M;
        int nn_N = (N + TILE_N - 1) / TILE_N;
        int nn_K = (K + TILE_K - 1) / TILE_K;

        TILE_M = std::min(TILE_M, ((M + nn_M - 1) / nn_M + 15) / 16 * 16);
        TILE_N = std::min(TILE_N, ((N + nn_N - 1) / nn_N + 11) / 12 * 12);
        TILE_K = std::min(TILE_K, ((K + nn_K - 1) / nn_K + 15) / 16 * 16);

        if (opt.num_threads > 1)
        {
            TILE_M = std::min(TILE_M, (TILE_M / opt.num_threads + 15) / 16 * 16);
            TILE_N = std::min(TILE_N, (TILE_N / opt.num_threads + 11) / 12 * 12);
            TILE_K = std::min(TILE_K, (TILE_K / opt.num_threads + 15) / 16 * 16);
        }
    }

    // NCNN_LOGE("TILE M/N/K = %d %d %d", TILE_M, TILE_N, TILE_K);

    int nn_M = (M + TILE_M - 1) / TILE_M;
    // int nn_N = (N + TILE_N - 1) / TILE_N;

    Mat tmpX;
    if (K > TILE_K)
        tmpX.create(TILE_N, TILE_M, opt.num_threads, 4u, opt.blob_allocator);

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int ppi = 0; ppi < nn_M; ppi++)
    {
        const int i = ppi * TILE_M;

        Mat tmp;
        if (K > TILE_K)
            tmp = tmpX.channel(get_omp_thread_num());

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

                Mat AT_tile = AT.channel(i / TILE_M).row_range(k / TILE_K, 1);

                Mat BT_tile = BT.channel(j / TILE_N).row_range(k / TILE_K, 1);

                bool k_end = k + TILE_K >= K;

                matmul_packed_transB_tile(AT_tile, BT_tile, C, top_blob, broadcast_type_C, tmp, alpha, beta, i, max_ii, j, max_jj, k, max_kk, k_end);
            }
        }
    }

    return 0;
}

int Gemm_x86::create_pipeline(const Option& opt)
{
    if (constantA)
    {
        const int M = constantM;
        const int K = constantK;

        // TODO do not hardcode
        int TILE_M = 16 * 16 * get_physical_cpu_count(); // 256
        // int TILE_N = 12 * 20;                            // 240
        int TILE_K = 16 * 16;

        {
            int nn_M = (constantM + TILE_M - 1) / TILE_M;
            // int nn_N = (constantN + TILE_N - 1) / TILE_N;
            int nn_K = (constantK + TILE_K - 1) / TILE_K;

            TILE_M = std::min(TILE_M, ((constantM + nn_M - 1) / nn_M + 15) / 16 * 16);
            // TILE_N = std::min(TILE_N, ((constantN + nn_N - 1) / nn_N + 11) / 12 * 12);
            TILE_K = std::min(TILE_K, ((constantK + nn_K - 1) / nn_K + 15) / 16 * 16);

            if (opt.num_threads > 1)
            {
                TILE_M = std::min(TILE_M, (TILE_M / opt.num_threads + 15) / 16 * 16);
                // TILE_N = std::min(TILE_N, (TILE_N / opt.num_threads + 11) / 12 * 12);
                TILE_K = std::min(TILE_K, (TILE_K / opt.num_threads + 15) / 16 * 16);
            }
        }

        const int nn_M = (M + TILE_M - 1) / TILE_M;

        AT_data.create(TILE_K * TILE_M, (K + TILE_K - 1) / TILE_K, (M + TILE_M - 1) / TILE_M, 4u, opt.blob_allocator);
        if (AT_data.empty())
            return -100;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int ppj = 0; ppj < nn_M; ppj++)
        {
            const int i = ppj * TILE_M;

            for (int k = 0; k < K; k += TILE_K)
            {
                const int max_ii = std::min((M - i), TILE_M);
                const int max_kk = std::min((K - k), TILE_K);

                Mat AT_tile = AT_data.channel(i / TILE_M).row_range(k / TILE_K, 1);

                if (transA)
                {
                    transpose_pack_A_tile(A_data, AT_tile, i, max_ii, k, max_kk);
                }
                else
                {
                    pack_A_tile(A_data, AT_tile, i, max_ii, k, max_kk);
                }
            }
        }

        if (opt.lightmode)
        {
            A_data.release();
        }
    }

    if (constantB)
    {
        const int N = constantN;
        const int K = constantK;

        // TODO do not hardcode
        // int TILE_M = 16 * 16 * get_physical_cpu_count(); // 256
        int TILE_N = 12 * 20;                            // 240
        int TILE_K = 16 * 16;

        {
            // int nn_M = (constantM + TILE_M - 1) / TILE_M;
            int nn_N = (constantN + TILE_N - 1) / TILE_N;
            int nn_K = (constantK + TILE_K - 1) / TILE_K;

            // TILE_M = std::min(TILE_M, ((constantM + nn_M - 1) / nn_M + 15) / 16 * 16);
            TILE_N = std::min(TILE_N, ((constantN + nn_N - 1) / nn_N + 11) / 12 * 12);
            TILE_K = std::min(TILE_K, ((constantK + nn_K - 1) / nn_K + 15) / 16 * 16);

            if (opt.num_threads > 1)
            {
                // TILE_M = std::min(TILE_M, (TILE_M / opt.num_threads + 15) / 16 * 16);
                TILE_N = std::min(TILE_N, (TILE_N / opt.num_threads + 11) / 12 * 12);
                TILE_K = std::min(TILE_K, (TILE_K / opt.num_threads + 15) / 16 * 16);
            }
        }

        const int nn_N = (N + TILE_N - 1) / TILE_N;

        BT_data.create(TILE_K * TILE_N, (K + TILE_K - 1) / TILE_K, (N + TILE_N - 1) / TILE_N, 4u, opt.blob_allocator);
        if (BT_data.empty())
            return -100;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int ppj = 0; ppj < nn_N; ppj++)
        {
            const int j = ppj * TILE_N;

            for (int k = 0; k < K; k += TILE_K)
            {
                const int max_jj = std::min((N - j), TILE_N);
                const int max_kk = std::min((K - k), TILE_K);

                Mat BT_tile = BT_data.channel(j / TILE_N).row_range(k / TILE_K, 1);

                if (transB)
                {
                    pack_B_tile(B_data, BT_tile, j, max_jj, k, max_kk);
                }
                else
                {
                    transpose_pack_B_tile(B_data, BT_tile, j, max_jj, k, max_kk);
                }
            }
        }

        if (opt.lightmode)
        {
            B_data.release();
        }
    }

    if (constantC)
    {
        const int M = constantM;

        int C_elempack = 1;
#if __SSE2__
        if (opt.use_packing_layout)
        {
#if __AVX512F__
            C_elempack = M % 16 == 0 ? 16 : M % 8 == 0 ? 8 : M % 4 == 0 ? 4 : 1;
#elif __AVX__
            C_elempack = M % 8 == 0 ? 8 : M % 4 == 0 ? 4 : 1;
#else
            C_elempack = M % 4 == 0 ? 4 : 1;
#endif
        }
#endif // __SSE2__

        convert_packing(C_data, CT_data, C_elempack, opt);

        if (opt.lightmode)
        {
            C_data.release();
        }
    }

    return 0;
}

int Gemm_x86::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    int M;
    int N;
    if (constantA && constantB)
    {
        M = constantM;
        N = constantN;
    }
    else if (constantA)
    {
        const Mat& B = bottom_blobs[0];
        M = constantM;
        N = transB ? B.h * B.elempack : B.w;
    }
    else if (constantB)
    {
        const Mat& A = bottom_blobs[0];
        M = transA ? A.w : A.h * A.elempack;
        N = constantN;
    }
    else
    {
        const Mat& A = bottom_blobs[0];
        const Mat& B = bottom_blobs[1];
        M = transA ? A.w : A.h * A.elempack;
        N = transB ? B.h * B.elempack : B.w;
    }

    Mat C;
    int broadcast_type_C = 0;
    if (constantC)
    {
        C = CT_data;
        broadcast_type_C = constant_broadcast_type_C;
    }
    else
    {
        if (constantA && constantB)
        {
            C = bottom_blobs.size() == 1 ? bottom_blobs[0] : Mat();
        }
        else if (constantA)
        {
            C = bottom_blobs.size() == 2 ? bottom_blobs[1] : Mat();
        }
        else if (constantB)
        {
            C = bottom_blobs.size() == 2 ? bottom_blobs[1] : Mat();
        }
        else
        {
            C = bottom_blobs.size() == 3 ? bottom_blobs[2] : Mat();
        }

        if (!C.empty())
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
    }

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

    Mat& top_blob = top_blobs[0];
    top_blob.create(N, M / out_elempack, out_elemsize, out_elempack, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    int ret = 0;
    if (constantA && constantB)
    {
        ret = gemm_AT_BT_x86(AT_data, BT_data, C, top_blob, broadcast_type_C, constantM, constantN, constantK, alpha, beta, opt);
    }
    else if (constantA)
    {
        const Mat& B = bottom_blobs[0];
        ret = gemm_AT_x86(AT_data, B, C, top_blob, broadcast_type_C, constantM, constantK, transB, alpha, beta, opt);
    }
    else if (constantB)
    {
        const Mat& A = bottom_blobs[0];
        ret = gemm_BT_x86(A, BT_data, C, top_blob, broadcast_type_C, constantN, constantK, transA, alpha, beta, opt);
    }
    else
    {
        const Mat& A = bottom_blobs[0];
        const Mat& B = bottom_blobs[1];
        ret = gemm_x86(A, B, C, top_blob, broadcast_type_C, transA, transB, alpha, beta, opt);
    }

    return ret;
}

} // namespace ncnn
