/*
Copyright (c) 2015-2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "./cgemm_array_kernels.h"
#include <hc_math.hpp>

hcblasStatus cgemm_TransAB_rMajor_batch_loopunroll(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
#define THREADS 16
#define TILE_DIM 16
  hc::extent<3> grdExt(batchSize, (M + (THREADS - 1)) & ~(THREADS - 1),
                       (N + (THREADS - 1)) & ~(THREADS - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, THREADS, THREADS);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    float CValue = 0, CValue1 = 0;
    int elt = tidx.tile[0];
    int Row = tidx.tile[2] * TILE_DIM + tidx.local[2];
    int Col = tidx.tile[1] * TILE_DIM + tidx.local[1];
    tile_static float Asreal[TILE_DIM][TILE_DIM];
    tile_static float Asimg[TILE_DIM][TILE_DIM];
    tile_static float Bsreal[TILE_DIM][TILE_DIM];
    tile_static float Bsimg[TILE_DIM][TILE_DIM];
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;

    for (int k = 0; k < ((K + (TILE_DIM - 1)) / TILE_DIM); k++) {
      if (k * TILE_DIM + tidx.local[1] < K && Row < N) {
        Bsreal[tidx.local[2]][tidx.local[1]] =
            B[elt][bOffset + Row * ldb + (k * TILE_DIM + tidx.local[1])].x;
        Bsimg[tidx.local[2]][tidx.local[1]] =
            B[elt][bOffset + Row * ldb + (k * TILE_DIM + tidx.local[1])].y;
      } else {
        Bsreal[tidx.local[2]][tidx.local[1]] = 0.0;
        Bsimg[tidx.local[2]][tidx.local[1]] = 0.0;
      }

      if (k * TILE_DIM + tidx.local[2] < K && Col < M) {
        Asreal[tidx.local[2]][tidx.local[1]] =
            A[elt][aOffset + (k * TILE_DIM + tidx.local[2]) * lda + Col].x;
        Asimg[tidx.local[2]][tidx.local[1]] =
            A[elt][aOffset + (k * TILE_DIM + tidx.local[2]) * lda + Col].y;
      } else {
        Asreal[tidx.local[2]][tidx.local[1]] = 0.0;
        Asimg[tidx.local[2]][tidx.local[1]] = 0.0;
      }

      tidx.barrier.wait();
      // Unrolled Matrix Mul operation
      CValue += ((Bsreal[tidx.local[2]][0] * Asreal[0][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][0] * Asimg[0][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][1] * Asreal[1][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][1] * Asimg[1][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][2] * Asreal[2][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][2] * Asimg[2][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][3] * Asreal[3][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][3] * Asimg[3][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][4] * Asreal[4][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][4] * Asimg[4][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][5] * Asreal[5][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][5] * Asimg[5][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][6] * Asreal[6][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][6] * Asimg[6][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][7] * Asreal[7][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][7] * Asimg[7][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][8] * Asreal[8][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][8] * Asimg[8][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][9] * Asreal[9][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][9] * Asimg[9][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][10] * Asreal[10][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][10] * Asimg[10][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][11] * Asreal[11][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][11] * Asimg[11][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][12] * Asreal[12][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][12] * Asimg[12][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][13] * Asreal[13][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][13] * Asimg[13][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][14] * Asreal[14][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][14] * Asimg[14][tidx.local[1]]) +
                 (Bsreal[tidx.local[2]][15] * Asreal[15][tidx.local[1]]) -
                 (Bsimg[tidx.local[2]][15] * Asimg[15][tidx.local[1]]));
      CValue1 += ((Bsreal[tidx.local[2]][0] * Asimg[0][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][0] * Asreal[0][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][1] * Asimg[1][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][1] * Asreal[1][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][2] * Asimg[2][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][2] * Asreal[2][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][3] * Asimg[3][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][3] * Asreal[3][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][4] * Asimg[4][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][4] * Asreal[4][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][5] * Asimg[5][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][5] * Asreal[5][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][6] * Asimg[6][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][6] * Asreal[6][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][7] * Asimg[7][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][7] * Asreal[7][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][8] * Asimg[8][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][8] * Asreal[8][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][9] * Asimg[9][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][9] * Asreal[9][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][10] * Asimg[10][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][10] * Asreal[10][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][11] * Asimg[11][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][11] * Asreal[11][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][12] * Asimg[12][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][12] * Asreal[12][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][13] * Asimg[13][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][13] * Asreal[13][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][14] * Asimg[14][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][14] * Asreal[14][tidx.local[1]]) +
                  (Bsreal[tidx.local[2]][15] * Asimg[15][tidx.local[1]]) +
                  (Bsimg[tidx.local[2]][15] * Asreal[15][tidx.local[1]]));
      tidx.barrier.wait();
    }

    if (Row < N && Col < M) {
      CReal = C[elt][cOffset + tidx.global[2] + (tidx.global[1] * ldc)].x;
      CImg = C[elt][cOffset + tidx.global[2] + (tidx.global[1] * ldc)].y;
      CReal = (hc::fast_math::isnan(CReal) ||
               hc::fast_math::isinf(static_cast<float>(CReal)))
                  ? 0
                  : CReal;
      CImg = (hc::fast_math::isnan(CImg) ||
              hc::fast_math::isinf(static_cast<float>(CImg)))
                 ? 0
                 : CImg;
      tempReal = ((CReal * beta.x) - (CImg * beta.y));
      tempImg = ((CReal * beta.y) + (CImg * beta.x));
      C[elt][cOffset + tidx.global[2] + (tidx.global[1] * ldc)].x =
          tempReal + ((CValue * alpha.x) - (CValue1 * alpha.y));
      C[elt][cOffset + tidx.global[2] + (tidx.global[1] * ldc)].y =
          tempImg + ((CValue * alpha.y) + (CValue1 * alpha.x));
    }
  }) ;
#undef THREADS
#undef TILE_DIM
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_TransAB_rMajor_batch_MICRO_TS16XMTS2(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
#define TILESIZE 16
#define MICROTILESIZE 1
  hc::extent<3> grdExt(batchSize, (M + (TILESIZE - 1)) & ~(TILESIZE - 1),
                       (N + (TILESIZE - 1)) & ~(TILESIZE - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, TILESIZE, TILESIZE);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    int elt = tidx.tile[0];
    int shiftTS = hc::fast_math::log2f(TILESIZE);
    int shiftMTP = hc::fast_math::log2f(MICROTILEPROD);
    float rCreal[MICROTILESIZE][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rCimg[MICROTILESIZE][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rAreal[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rAimg[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rBreal[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rBimg[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    tile_static float lAreal[TOTMICROTILEPROD + TILESIZE];
    tile_static float lAimg[TOTMICROTILEPROD + TILESIZE];
    tile_static float lBreal[TOTMICROTILEPROD + TILESIZE];
    tile_static float lBimg[TOTMICROTILEPROD + TILESIZE];
    int gidx = tidx.tile[1];
    int gidy = tidx.tile[2];
    int idx = tidx.local[1];
    int idy = tidx.local[2];
    int idt = (idy * TILESIZE) + idx;
    int idxT = idt & (TILESIZE - 1);
    int idyT = idt / TILESIZE;
    int block_k = 0;
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;

    do {
      int colIndex = (block_k * TILESIZE) + idyT;
      int lIndex = (idyT * BANKMICROTILESIZE) + idxT;
      tidx.barrier.wait();

      for (int sec = 0; sec < MICROTILESIZE; ++sec) {
        int secVal = sec * TILESIZE;
        int BrowIndex = (gidy * MICROTILEPROD) + idxT + secVal;
        int ArowIndex = (gidx * MICROTILEPROD) + idxT + secVal;

        if (BrowIndex < N && colIndex < K) {
          lBreal[lIndex + secVal] =
              B[elt][bOffset + BrowIndex * ldb + colIndex].x;
          lBimg[lIndex + secVal] =
              B[elt][bOffset + BrowIndex * ldb + colIndex].y;
        } else {
          lBreal[lIndex + secVal] = 0;
          lBimg[lIndex + secVal] = 0;
        }

        if (ArowIndex < M && colIndex < K) {
          lAreal[lIndex + secVal] =
              A[elt][aOffset + ArowIndex + colIndex * lda].x;
          lAimg[lIndex + secVal] =
              A[elt][aOffset + ArowIndex + colIndex * lda].y;
        } else {
          lAreal[lIndex + secVal] = 0;
          lAimg[lIndex + secVal] = 0;
        }
      }

      tidx.barrier.wait();
      int offA = idx;
      int offB = idy;

      for (int iter = 0; iter < TILESIZE; ++iter) {
        MTS;
      }

      tidx.barrier.wait();
    } while (++block_k < (((K + TILESIZE - 1) & ~(TILESIZE - 1)) / TILESIZE));

    int xIndex = ((gidx << shiftMTP) + idx) * ldc;
    int yIndex = (gidy << shiftMTP) + idy;

    for (int row = 0; row < MICROTILESIZE; row++) {
      for (int col = 0; col < MICROTILESIZE; col++) {
        if ((xIndex / ldc) + (col << shiftTS) < M &&
            yIndex + (row << shiftTS) < N) {
          CReal = C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                         (row << shiftTS)]
                      .x;
          CImg = C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                        (row * TILESIZE)]
                     .y;
          CReal = (hc::fast_math::isnan(CReal) ||
                   hc::fast_math::isinf(static_cast<float>(CReal)))
                      ? 0
                      : CReal;
          CImg = (hc::fast_math::isnan(CImg) ||
                  hc::fast_math::isinf(static_cast<float>(CImg)))
                     ? 0
                     : CImg;
          tempReal = ((CReal * beta.x) - (CImg * beta.y));
          tempImg = ((CReal * beta.y) + (CImg * beta.x));
          C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                 (row << shiftTS)]
              .x = tempReal +
                   ((rCreal[col][row] * alpha.x) - (rCimg[col][row] * alpha.y));
          C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                 (row * TILESIZE)]
              .y = tempImg +
                   ((rCreal[col][row] * alpha.y) + (rCimg[col][row] * alpha.x));
        }
      }
    }
  }) ;
#undef TILESIZE
#undef MICROTILESIZE
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_TransAB_rMajor_batch_STEP_TS8XSS8(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize)

{
#define TILESIZE 8
#define STEPSIZE 8
  hc::extent<3> grdExt(batchSize, (M + (TILESIZE - 1)) & ~(TILESIZE - 1),
                       (N + (TILESIZE - 1)) & ~(TILESIZE - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, TILESIZE, TILESIZE);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    int elt = tidx.tile[0];
    int shiftFactor = hc::fast_math::log2f(STEPSIZE);
    float rCreal[1][1];
    float rAreal[1][STEPSIZE / TILESIZE];
    float rBreal[1][STEPSIZE / TILESIZE];
    float rCimg[1][1];
    float rAimg[1][STEPSIZE / TILESIZE];
    float rBimg[1][STEPSIZE / TILESIZE];
    tile_static float lAreal[TILESIZE * STEPSIZE];  // 8*8+8
    tile_static float lBreal[TILESIZE * STEPSIZE];
    tile_static float lAimg[TILESIZE * STEPSIZE];  // 8*8+8
    tile_static float lBimg[TILESIZE * STEPSIZE];
    rCreal[0][0] = 0;
    rCimg[0][0] = 0;
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;
    int gidx = tidx.tile[1];
    int gidy = tidx.tile[2];
    int idx = tidx.local[1];
    int idy = tidx.local[2];
    int idt = TILESIZE * idy + idx;
    int idxT = idt % TILESIZE;
    int idyT = idt / TILESIZE;
    int block_k = ((K + (STEPSIZE - 1)) & ~(STEPSIZE - 1)) >> shiftFactor;
    int i = 0;

    do {
      tidx.barrier.wait();

      // Load Sections of A and B into respective shared memory slots
      for (int sec = 0; sec < STEPSIZE / TILESIZE; ++sec) {
        // Load Section 'sec' from global memory B onto shared lB
        if (gidy * TILESIZE + idxT < N &&
            (idyT + i * STEPSIZE + (TILESIZE * sec)) < K) {
          lBreal[idxT * TILESIZE + idyT + (TILESIZE * TILESIZE * sec)] =
              B[elt][bOffset + (gidy * TILESIZE + idxT) * ldb + idyT +
                     i * STEPSIZE + (TILESIZE * sec)]
                  .x;
          lBimg[idxT * TILESIZE + idyT + (TILESIZE * TILESIZE * sec)] =
              B[elt][bOffset + (gidy * TILESIZE + idxT) * ldb + idyT +
                     i * STEPSIZE + (TILESIZE * sec)]
                  .y;
        } else {
          lBreal[idxT * TILESIZE + idyT + (TILESIZE * TILESIZE * sec)] = 0;
          lBimg[idxT * TILESIZE + idyT + (TILESIZE * TILESIZE * sec)] = 0;
        }

        // Load Section 'sec' from global memory A onto shared lA
        if (gidx * TILESIZE + idxT < M &&
            (i * STEPSIZE + idyT + (TILESIZE * sec)) < K) {
          lAreal[idxT * TILESIZE + idyT + (TILESIZE * TILESIZE * sec)] =
              A[elt][aOffset + gidx * TILESIZE + idxT + idyT * lda +
                     i * (lda << shiftFactor) + (TILESIZE * sec) * lda]
                  .x;
          lAimg[idxT * TILESIZE + idyT + (TILESIZE * TILESIZE * sec)] =
              A[elt][aOffset + gidx * TILESIZE + idxT + idyT * lda +
                     i * (lda << shiftFactor) + (TILESIZE * sec) * lda]
                  .y;
        } else {
          lAreal[idxT * TILESIZE + idyT + (TILESIZE * TILESIZE * sec)] = 0;
          lAimg[idxT * TILESIZE + idyT + (TILESIZE * TILESIZE * sec)] = 0;
        }
      }

      tidx.barrier.wait();
      int offA = idx * TILESIZE;
      int offB = idy * TILESIZE;
      int offset = 1;

      for (int iter = 0; iter < TILESIZE; ++iter) {
        MS1x1(offset);
      }

      i++;
    } while (--block_k > 0);

    tidx.barrier.wait();

    if (gidx * TILESIZE + idx < M && gidy * TILESIZE + idy < N) {
      CReal = C[elt]
               [cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
                   .x;
      CImg = C[elt]
              [cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
                  .y;
      CReal = (hc::fast_math::isnan(CReal) ||
               hc::fast_math::isinf(static_cast<float>(CReal)))
                  ? 0
                  : CReal;
      CImg = (hc::fast_math::isnan(CImg) ||
              hc::fast_math::isinf(static_cast<float>(CImg)))
                 ? 0
                 : CImg;
      tempReal = ((CReal * beta.x) - (CImg * beta.y));
      tempImg = ((CReal * beta.y) + (CImg * beta.x));
      C[elt][cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
          .x = tempReal + ((rCreal[0][0] * alpha.x) - (rCimg[0][0] * alpha.y));
      C[elt][cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
          .y = tempImg + ((rCreal[0][0] * alpha.y) + (rCimg[0][0] * alpha.x));
    }
  }) ;
#undef TILESIZE
#undef STEPSIZE
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_TransAB_rMajor_batch_MICRO_TS8XMTS2(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
#define TILESIZE 8
#define MICROTILESIZE 1
  hc::extent<3> grdExt(batchSize, (M + (TILESIZE - 1)) & ~(TILESIZE - 1),
                       (N + (TILESIZE - 1)) & ~(TILESIZE - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, TILESIZE, TILESIZE);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    int elt = tidx.tile[0];
    int shiftTS = hc::fast_math::log2f(TILESIZE);
    int shiftMTP = hc::fast_math::log2f(MICROTILEPROD);
    float rCreal[MICROTILESIZE][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rCimg[MICROTILESIZE][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rAreal[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rAimg[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rBreal[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rBimg[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    tile_static float lAreal[TOTMICROTILEPROD + TILESIZE];
    tile_static float lAimg[TOTMICROTILEPROD + TILESIZE];
    tile_static float lBreal[TOTMICROTILEPROD + TILESIZE];
    tile_static float lBimg[TOTMICROTILEPROD + TILESIZE];
    int gidx = tidx.tile[1];
    int gidy = tidx.tile[2];
    int idx = tidx.local[1];
    int idy = tidx.local[2];
    int idt = (idy * TILESIZE) + idx;
    int idxT = idt & (TILESIZE - 1);
    int idyT = idt / TILESIZE;
    int block_k = 0;
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;

    do {
      int colIndex = (block_k * TILESIZE) + idyT;
      int lIndex = (idyT * BANKMICROTILESIZE) + idxT;
      tidx.barrier.wait();

      for (int sec = 0; sec < MICROTILESIZE; ++sec) {
        int secVal = sec * TILESIZE;
        int BrowIndex = (gidy * MICROTILEPROD) + idxT + secVal;
        int ArowIndex = (gidx * MICROTILEPROD) + idxT + secVal;

        if (BrowIndex < N && colIndex < K) {
          lBreal[lIndex + secVal] =
              B[elt][bOffset + BrowIndex * ldb + colIndex].x;
          lBimg[lIndex + secVal] =
              B[elt][bOffset + BrowIndex * ldb + colIndex].y;
        } else {
          lBreal[lIndex + secVal] = 0;
          lBimg[lIndex + secVal] = 0;
        }

        if (ArowIndex < M && colIndex < K) {
          lAreal[lIndex + secVal] =
              A[elt][aOffset + ArowIndex + colIndex * lda].x;
          lAimg[lIndex + secVal] =
              A[elt][aOffset + ArowIndex + colIndex * lda].y;
        } else {
          lAreal[lIndex + secVal] = 0;
          lAimg[lIndex + secVal] = 0;
        }
      }

      tidx.barrier.wait();
      int offA = idx;
      int offB = idy;

      for (int iter = 0; iter < TILESIZE; ++iter) {
        MTS;
      }

      tidx.barrier.wait();
    } while (++block_k < (((K + TILESIZE - 1) & ~(TILESIZE - 1)) / TILESIZE));

    int xIndex = ((gidx << shiftMTP) + idx) * ldc;
    int yIndex = (gidy << shiftMTP) + idy;

    for (int row = 0; row < MICROTILESIZE; row++) {
      for (int col = 0; col < MICROTILESIZE; col++) {
        if ((xIndex / ldc) + (col << shiftTS) < M &&
            yIndex + (row << shiftTS) < N) {
          CReal = C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                         (row << shiftTS)]
                      .x;
          CImg = C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                        (row * TILESIZE)]
                     .y;
          CReal = (hc::fast_math::isnan(CReal) ||
                   hc::fast_math::isinf(static_cast<float>(CReal)))
                      ? 0
                      : CReal;
          CImg = (hc::fast_math::isnan(CImg) ||
                  hc::fast_math::isinf(static_cast<float>(CImg)))
                     ? 0
                     : CImg;
          tempReal = ((CReal * beta.x) - (CImg * beta.y));
          tempImg = ((CReal * beta.y) + (CImg * beta.x));
          C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                 (row << shiftTS)]
              .x = tempReal +
                   ((rCreal[col][row] * alpha.x) - (rCimg[col][row] * alpha.y));
          C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                 (row * TILESIZE)]
              .y = tempImg +
                   ((rCreal[col][row] * alpha.y) + (rCimg[col][row] * alpha.x));
        }
      }
    }
  }) ;
#undef TILESIZE
#undef MICROTILESIZE
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_NoTransB_rMajor_batch_MICRO_TS16XMTS2(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
#define TILESIZE 16
#define MICROTILESIZE 1
  hc::extent<3> grdExt(batchSize, (M + (TILESIZE - 1)) & ~(TILESIZE - 1),
                       (N + (TILESIZE - 1)) & ~(TILESIZE - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, TILESIZE, TILESIZE);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    int elt = tidx.tile[0];
    int shiftTS = hc::fast_math::log2f(TILESIZE);
    int shiftMTP = hc::fast_math::log2f(MICROTILEPROD);
    float rCreal[MICROTILESIZE][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rCimg[MICROTILESIZE][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rAreal[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rAimg[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rBreal[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rBimg[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    tile_static float lAreal[TOTMICROTILEPROD + TILESIZE];
    tile_static float lAimg[TOTMICROTILEPROD + TILESIZE];
    tile_static float lBreal[TOTMICROTILEPROD + TILESIZE];
    tile_static float lBimg[TOTMICROTILEPROD + TILESIZE];
    int gidx = tidx.tile[1];
    int gidy = tidx.tile[2];
    int idx = tidx.local[1];
    int idy = tidx.local[2];
    int idt = (idy << shiftTS) + idx;
    int idxT = idt & (TILESIZE - 1);
    int idyT = idt >> shiftTS;
    int block_k = 0;
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;

    do {
      int colIndex = (block_k << shiftTS) + idyT;
      int lIndex = (idyT * BANKMICROTILESIZE) + idxT;
      tidx.barrier.wait();

      for (int sec = 0; sec < MICROTILESIZE; ++sec) {
        int secVal = sec << shiftTS;
        int BrowIndex = (gidy << shiftMTP) + idxT + secVal;
        int ArowIndex = (gidx << shiftMTP) + idxT + secVal;

        if (BrowIndex < N && colIndex < K) {
          lBreal[lIndex + secVal] =
              B[elt][bOffset + BrowIndex + colIndex * ldb].x;
          lBimg[lIndex + secVal] =
              B[elt][bOffset + BrowIndex + colIndex * ldb].y;
        } else {
          lBreal[lIndex + secVal] = 0;
          lBimg[lIndex + secVal] = 0;
        }

        if (ArowIndex < M && colIndex < K) {
          lAreal[lIndex + secVal] =
              A[elt][aOffset + ArowIndex + colIndex * lda].x;
          lAimg[lIndex + secVal] =
              A[elt][aOffset + ArowIndex + colIndex * lda].y;
        } else {
          lAreal[lIndex + secVal] = 0;
          lAimg[lIndex + secVal] = 0;
        }
      }

      tidx.barrier.wait();
      int offA = idx;
      int offB = idy;

      for (int iter = 0; iter < TILESIZE; ++iter) {
        MTS;
      }

      tidx.barrier.wait();
    } while (++block_k < (((K + TILESIZE - 1) & ~(TILESIZE - 1)) >> shiftTS));

    int xIndex = ((gidx << shiftMTP) + idx) * ldc;
    int yIndex = (gidy << shiftMTP) + idy;

    for (int row = 0; row < MICROTILESIZE; row++) {
      for (int col = 0; col < MICROTILESIZE; col++) {
        if ((xIndex / ldc) + (col << shiftTS) < M &&
            yIndex + (row << shiftTS) < N) {
          CReal = C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                         (row << shiftTS)]
                      .x;
          CImg = C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                        (row * TILESIZE)]
                     .y;
          CReal = (hc::fast_math::isnan(CReal) ||
                   hc::fast_math::isinf(static_cast<float>(CReal)))
                      ? 0
                      : CReal;
          CImg = (hc::fast_math::isnan(CImg) ||
                  hc::fast_math::isinf(static_cast<float>(CImg)))
                     ? 0
                     : CImg;
          tempReal = ((CReal * beta.x) - (CImg * beta.y));
          tempImg = ((CReal * beta.y) + (CImg * beta.x));
          C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                 (row << shiftTS)]
              .x = tempReal +
                   ((rCreal[col][row] * alpha.x) - (rCimg[col][row] * alpha.y));
          C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                 (row * TILESIZE)]
              .y = tempImg +
                   ((rCreal[col][row] * alpha.y) + (rCimg[col][row] * alpha.x));
        }
      }
    }
  }) ;
#undef TILESIZE
#undef MICROTILESIZE
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_NoTransA_rMajor_batch_STEP_TS8XSS8(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
#define TILESIZE 8
#define STEPSIZE 8
  hc::extent<3> grdExt(batchSize, (M + (TILESIZE - 1)) & ~(TILESIZE - 1),
                       (N + (TILESIZE - 1)) & ~(TILESIZE - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, TILESIZE, TILESIZE);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    int elt = tidx.tile[0];
    int shiftFactor = hc::fast_math::log2f(TILESIZE);
    float rCreal[1][1];
    float rAreal[1][1];
    float rBreal[1][1];
    float rCimg[1][1];
    float rAimg[1][1];
    float rBimg[1][1];
    tile_static float lAreal[TILESIZE * TILESIZE];  // 8*8+8
    tile_static float lBreal[TILESIZE * TILESIZE];
    tile_static float lAimg[TILESIZE * TILESIZE];  // 8*8+8
    tile_static float lBimg[TILESIZE * TILESIZE];
    rCreal[0][0] = 0;
    rCimg[0][0] = 0;
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;
    int gidx = tidx.tile[1];
    int gidy = tidx.tile[2];
    int idx = tidx.local[1];
    int idy = tidx.local[2];
    int idt = TILESIZE * idy + idx;
    int idxT = idt % TILESIZE;
    int idyT = idt / TILESIZE;
    int block_k = ((K + (TILESIZE - 1)) & ~(TILESIZE - 1)) >> shiftFactor;
    int i = 0;

    do {
      // barrier(CLK_LOCAL_MEM_FENCE);
      tidx.barrier.wait();

      if (gidy * TILESIZE + idxT < N && i * TILESIZE + idyT < K) {
        lBreal[idyT + idxT * TILESIZE] =
            B[elt]
             [bOffset + (gidy * TILESIZE + idxT) * ldb + idyT + i * TILESIZE]
                 .x;
        lBimg[idyT + idxT * TILESIZE] =
            B[elt]
             [bOffset + (gidy * TILESIZE + idxT) * ldb + idyT + i * TILESIZE]
                 .y;
      } else {
        lBreal[idyT + idxT * TILESIZE] = 0;
        lBimg[idyT + idxT * TILESIZE] = 0;
      }

      if (gidx * TILESIZE + idxT < M && i * TILESIZE + idyT < K) {
        lAreal[idyT + idxT * TILESIZE] =
            A[elt]
             [aOffset + (gidx * TILESIZE + idxT) * lda + idyT + i * TILESIZE]
                 .x;
        lAimg[idyT + idxT * TILESIZE] =
            A[elt]
             [aOffset + (gidx * TILESIZE + idxT) * lda + idyT + i * TILESIZE]
                 .y;
      } else {
        lAreal[idyT + idxT * TILESIZE] = 0;
        lAimg[idyT + idxT * TILESIZE] = 0;
      }

      tidx.barrier.wait();
      // barrier(CLK_LOCAL_MEM_FENCE);
      int offA = idx * TILESIZE;
      int offB = idy * TILESIZE;
      int offset = 1;

      for (int iter = 0; iter < TILESIZE; ++iter) {
        M1x1(offset);
      }

      i++;
    } while (--block_k > 0);

    tidx.barrier.wait();

    if (gidx * TILESIZE + idx < M && gidy * TILESIZE + idy < N) {
      CReal = C[elt]
               [cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
                   .x;
      CImg = C[elt]
              [cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
                  .y;
      CReal = (hc::fast_math::isnan(CReal) ||
               hc::fast_math::isinf(static_cast<float>(CReal)))
                  ? 0
                  : CReal;
      CImg = (hc::fast_math::isnan(CImg) ||
              hc::fast_math::isinf(static_cast<float>(CImg)))
                 ? 0
                 : CImg;
      tempReal = ((CReal * beta.x) - (CImg * beta.y));
      tempImg = ((CReal * beta.y) + (CImg * beta.x));
      C[elt][cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
          .x = tempReal + ((rCreal[0][0] * alpha.x) - (rCimg[0][0] * alpha.y));
      C[elt][cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
          .y = tempImg + ((rCreal[0][0] * alpha.y) + (rCimg[0][0] * alpha.x));
    }
  }) ;
#undef TILESIZE
#undef STEPSIZE
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_NoTransA_rMajor_batch_MICRO_TS16XMTS2(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
#define TILESIZE 16
#define MICROTILESIZE 1
  hc::extent<3> grdExt(batchSize, (M + (TILESIZE - 1)) & ~(TILESIZE - 1),
                       (N + (TILESIZE - 1)) & ~(TILESIZE - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, TILESIZE, TILESIZE);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    int elt = tidx.tile[0];
    int shiftTS = hc::fast_math::log2f(TILESIZE);
    int shiftMTP = hc::fast_math::log2f(MICROTILEPROD);
    float rCreal[MICROTILESIZE][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rCimg[MICROTILESIZE][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rAreal[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rAimg[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rBreal[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rBimg[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    tile_static float lAreal[TOTMICROTILEPROD + TILESIZE];
    tile_static float lAimg[TOTMICROTILEPROD + TILESIZE];
    tile_static float lBreal[TOTMICROTILEPROD + TILESIZE];
    tile_static float lBimg[TOTMICROTILEPROD + TILESIZE];
    int gidx = tidx.tile[1];
    int gidy = tidx.tile[2];
    int idx = tidx.local[1];
    int idy = tidx.local[2];
    int idt = (idy << shiftTS) + idx;
    int idxT = idt % TILESIZE;
    int idyT = idt / TILESIZE;
    int block_k = 0;
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;

    do {
      int colIndex = (block_k << shiftTS) + idyT;
      int lIndex = (idyT * BANKMICROTILESIZE) + idxT;
      tidx.barrier.wait();

      for (int sec = 0; sec < MICROTILESIZE; ++sec) {
        int secVal = sec << shiftTS;
        int BrowIndex = (gidy << shiftMTP) + idxT + secVal;
        int ArowIndex = (gidx << shiftMTP) + idxT + secVal;
        tidx.barrier.wait();

        if (BrowIndex < N && colIndex < K) {
          lBreal[lIndex + secVal] =
              B[elt][bOffset + BrowIndex * ldb + colIndex].x;
          lBimg[lIndex + secVal] =
              B[elt][bOffset + BrowIndex * ldb + colIndex].y;
        } else {
          lBreal[lIndex + secVal] = 0;
          lBimg[lIndex + secVal] = 0;
        }

        if (ArowIndex < M && colIndex < K) {
          lAreal[lIndex + secVal] =
              A[elt][aOffset + ArowIndex * lda + colIndex].x;
          lAimg[lIndex + secVal] =
              A[elt][aOffset + ArowIndex * lda + colIndex].y;
        } else {
          lAreal[lIndex + secVal] = 0;
          lAimg[lIndex + secVal] = 0;
        }
      }

      tidx.barrier.wait();
      int offA = idx;
      int offB = idy;

      for (int iter = 0; iter < TILESIZE; ++iter) {
        MTS;
      }

      tidx.barrier.wait();
    } while (++block_k < (((K + TILESIZE - 1) & ~(TILESIZE - 1)) >> shiftTS));

    int xIndex = ((gidx << shiftMTP) + idx) * ldc;
    int yIndex = (gidy << shiftMTP) + idy;

    for (int row = 0; row < MICROTILESIZE; row++) {
      for (int col = 0; col < MICROTILESIZE; col++) {
        if ((xIndex / ldc) + (col << shiftTS) < M &&
            yIndex + (row << shiftTS) < N) {
          CReal = C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                         (row << shiftTS)]
                      .x;
          CImg = C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                        (row * TILESIZE)]
                     .y;
          CReal = (hc::fast_math::isnan(CReal) ||
                   hc::fast_math::isinf(static_cast<float>(CReal)))
                      ? 0
                      : CReal;
          CImg = (hc::fast_math::isnan(CImg) ||
                  hc::fast_math::isinf(static_cast<float>(CImg)))
                     ? 0
                     : CImg;
          tempReal = ((CReal * beta.x) - (CImg * beta.y));
          tempImg = ((CReal * beta.y) + (CImg * beta.x));
          C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                 (row << shiftTS)]
              .x = tempReal +
                   ((rCreal[col][row] * alpha.x) - (rCimg[col][row] * alpha.y));
          C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                 (row * TILESIZE)]
              .y = tempImg +
                   ((rCreal[col][row] * alpha.y) + (rCimg[col][row] * alpha.x));
        }
      }
    }
  }) ;
#undef TILESIZE
#undef MICROTILESIZE
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_NoTransA_rMajor_batch_loopunroll(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
#define THREADS 16
#define TILE_DIM 16
  hc::extent<3> grdExt(batchSize, (M + (THREADS - 1)) & ~(THREADS - 1),
                       (N + (THREADS - 1)) & ~(THREADS - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, THREADS, THREADS);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    int elt = tidx.tile[0];
    float CValue = 0, CValue1 = 0;
    int Row = tidx.global[2];
    int Col = tidx.global[1];
    tile_static float Asreal[TILE_DIM][TILE_DIM];
    tile_static float Asimg[TILE_DIM][TILE_DIM];
    tile_static float Bsreal[TILE_DIM][TILE_DIM];
    tile_static float Bsimg[TILE_DIM][TILE_DIM];
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;

    for (int k = 0; k < ((K + (TILE_DIM - 1)) & ~(TILE_DIM - 1));
         k += TILE_DIM) {
      if (k + tidx.local[1] < K && Row < N) {
        Bsreal[tidx.local[2]][tidx.local[1]] =
            B[elt][bOffset + Row * ldb + k + tidx.local[1]].x;
        Bsimg[tidx.local[2]][tidx.local[1]] =
            B[elt][bOffset + Row * ldb + k + tidx.local[1]].y;
      } else {
        Bsreal[tidx.local[2]][tidx.local[1]] = 0.0;
        Bsimg[tidx.local[2]][tidx.local[1]] = 0.0;
      }

      if (k + tidx.local[1] < K &&
          (tidx.tile[1] * TILE_DIM + tidx.local[2]) < M) {
        Asreal[tidx.local[2]][tidx.local[1]] =
            A[elt][aOffset + ((tidx.tile[1] * TILE_DIM + tidx.local[2]) * lda) +
                   k + tidx.local[1]]
                .x;
        Asimg[tidx.local[2]][tidx.local[1]] =
            A[elt][aOffset + ((tidx.tile[1] * TILE_DIM + tidx.local[2]) * lda) +
                   k + tidx.local[1]]
                .y;
      } else {
        Asreal[tidx.local[2]][tidx.local[1]] = 0.0;
        Asimg[tidx.local[2]][tidx.local[1]] = 0.0;
      }

      tidx.barrier.wait();
      // Unrolled Matrix Mul operation
      CValue += ((Bsreal[tidx.local[2]][0] * Asreal[tidx.local[1]][0]) -
                 (Bsimg[tidx.local[2]][0] * Asimg[tidx.local[1]][0]) +
                 (Bsreal[tidx.local[2]][1] * Asreal[tidx.local[1]][1]) -
                 (Bsimg[tidx.local[2]][1] * Asimg[tidx.local[1]][1]) +
                 (Bsreal[tidx.local[2]][2] * Asreal[tidx.local[1]][2]) -
                 (Bsimg[tidx.local[2]][2] * Asimg[tidx.local[1]][2]) +
                 (Bsreal[tidx.local[2]][3] * Asreal[tidx.local[1]][3]) -
                 (Bsimg[tidx.local[2]][3] * Asimg[tidx.local[1]][3]) +
                 (Bsreal[tidx.local[2]][4] * Asreal[tidx.local[1]][4]) -
                 (Bsimg[tidx.local[2]][4] * Asimg[tidx.local[1]][4]) +
                 (Bsreal[tidx.local[2]][5] * Asreal[tidx.local[1]][5]) -
                 (Bsimg[tidx.local[2]][5] * Asimg[tidx.local[1]][5]) +
                 (Bsreal[tidx.local[2]][6] * Asreal[tidx.local[1]][6]) -
                 (Bsimg[tidx.local[2]][6] * Asimg[tidx.local[1]][6]) +
                 (Bsreal[tidx.local[2]][7] * Asreal[tidx.local[1]][7]) -
                 (Bsimg[tidx.local[2]][7] * Asimg[tidx.local[1]][7]) +
                 (Bsreal[tidx.local[2]][8] * Asreal[tidx.local[1]][8]) -
                 (Bsimg[tidx.local[2]][8] * Asimg[tidx.local[1]][8]) +
                 (Bsreal[tidx.local[2]][9] * Asreal[tidx.local[1]][9]) -
                 (Bsimg[tidx.local[2]][9] * Asimg[tidx.local[1]][9]) +
                 (Bsreal[tidx.local[2]][10] * Asreal[tidx.local[1]][10]) -
                 (Bsimg[tidx.local[2]][10] * Asimg[tidx.local[1]][10]) +
                 (Bsreal[tidx.local[2]][11] * Asreal[tidx.local[1]][11]) -
                 (Bsimg[tidx.local[2]][11] * Asimg[tidx.local[1]][11]) +
                 (Bsreal[tidx.local[2]][12] * Asreal[tidx.local[1]][12]) -
                 (Bsimg[tidx.local[2]][12] * Asimg[tidx.local[1]][12]) +
                 (Bsreal[tidx.local[2]][13] * Asreal[tidx.local[1]][13]) -
                 (Bsimg[tidx.local[2]][13] * Asimg[tidx.local[1]][13]) +
                 (Bsreal[tidx.local[2]][14] * Asreal[tidx.local[1]][14]) -
                 (Bsimg[tidx.local[2]][14] * Asimg[tidx.local[1]][14]) +
                 (Bsreal[tidx.local[2]][15] * Asreal[tidx.local[1]][15]) -
                 (Bsimg[tidx.local[2]][15] * Asimg[tidx.local[1]][15]));
      CValue1 += ((Bsreal[tidx.local[2]][0] * Asimg[tidx.local[1]][0]) +
                  (Bsimg[tidx.local[2]][0] * Asreal[tidx.local[1]][0]) +
                  (Bsreal[tidx.local[2]][1] * Asimg[tidx.local[1]][1]) +
                  (Bsimg[tidx.local[2]][1] * Asreal[tidx.local[1]][1]) +
                  (Bsreal[tidx.local[2]][2] * Asimg[tidx.local[1]][2]) +
                  (Bsimg[tidx.local[2]][2] * Asreal[tidx.local[1]][2]) +
                  (Bsreal[tidx.local[2]][3] * Asimg[tidx.local[1]][3]) +
                  (Bsimg[tidx.local[2]][3] * Asreal[tidx.local[1]][3]) +
                  (Bsreal[tidx.local[2]][4] * Asimg[tidx.local[1]][4]) +
                  (Bsimg[tidx.local[2]][4] * Asreal[tidx.local[1]][4]) +
                  (Bsreal[tidx.local[2]][5] * Asimg[tidx.local[1]][5]) +
                  (Bsimg[tidx.local[2]][5] * Asreal[tidx.local[1]][5]) +
                  (Bsreal[tidx.local[2]][6] * Asimg[tidx.local[1]][6]) +
                  (Bsimg[tidx.local[2]][6] * Asreal[tidx.local[1]][6]) +
                  (Bsreal[tidx.local[2]][7] * Asimg[tidx.local[1]][7]) +
                  (Bsimg[tidx.local[2]][7] * Asreal[tidx.local[1]][7]) +
                  (Bsreal[tidx.local[2]][8] * Asimg[tidx.local[1]][8]) +
                  (Bsimg[tidx.local[2]][8] * Asreal[tidx.local[1]][8]) +
                  (Bsreal[tidx.local[2]][9] * Asimg[tidx.local[1]][9]) +
                  (Bsimg[tidx.local[2]][9] * Asreal[tidx.local[1]][9]) +
                  (Bsreal[tidx.local[2]][10] * Asimg[tidx.local[1]][10]) +
                  (Bsimg[tidx.local[2]][10] * Asreal[tidx.local[1]][10]) +
                  (Bsreal[tidx.local[2]][11] * Asimg[tidx.local[1]][11]) +
                  (Bsimg[tidx.local[2]][11] * Asreal[tidx.local[1]][11]) +
                  (Bsreal[tidx.local[2]][12] * Asimg[tidx.local[1]][12]) +
                  (Bsimg[tidx.local[2]][12] * Asreal[tidx.local[1]][12]) +
                  (Bsreal[tidx.local[2]][13] * Asimg[tidx.local[1]][13]) +
                  (Bsimg[tidx.local[2]][13] * Asreal[tidx.local[1]][13]) +
                  (Bsreal[tidx.local[2]][14] * Asimg[tidx.local[1]][14]) +
                  (Bsimg[tidx.local[2]][14] * Asreal[tidx.local[1]][14]) +
                  (Bsreal[tidx.local[2]][15] * Asimg[tidx.local[1]][15]) +
                  (Bsimg[tidx.local[2]][15] * Asreal[tidx.local[1]][15]));
      tidx.barrier.wait();
    }

    if (Row < N && Col < M) {
      CReal = C[elt][cOffset + tidx.global[2] + (tidx.global[1] * ldc)].x;
      CImg = C[elt][cOffset + tidx.global[2] + (tidx.global[1] * ldc)].y;
      CReal = (hc::fast_math::isnan(CReal) ||
               hc::fast_math::isinf(static_cast<float>(CReal)))
                  ? 0
                  : CReal;
      CImg = (hc::fast_math::isnan(CImg) ||
              hc::fast_math::isinf(static_cast<float>(CImg)))
                 ? 0
                 : CImg;
      tempReal = ((CReal * beta.x) - (CImg * beta.y));
      tempImg = ((CReal * beta.y) + (CImg * beta.x));
      C[elt][cOffset + tidx.global[2] + (tidx.global[1] * ldc)].x =
          tempReal + ((CValue * alpha.x) - (CValue1 * alpha.y));
      C[elt][cOffset + tidx.global[2] + (tidx.global[1] * ldc)].y =
          tempImg + ((CValue * alpha.y) + (CValue1 * alpha.x));
    }
  }) ;
#undef THREADS
#undef TILE_DIM
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_NoTransAB_rMajor_batch_STEP_TS8XSS8(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
#define TILESIZE 8
#define STEPSIZE 8
  hc::extent<3> grdExt(batchSize, (M + (TILESIZE - 1)) & ~(TILESIZE - 1),
                       (N + (TILESIZE - 1)) & ~(TILESIZE - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, TILESIZE, TILESIZE);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    int elt = tidx.tile[0];
    int shiftFactor = hc::fast_math::log2f(TILESIZE);
    float rCreal[1][1];
    float rAreal[1][1];
    float rBreal[1][1];
    float rCimg[1][1];
    float rAimg[1][1];
    float rBimg[1][1];
    tile_static float lAreal[TILESIZE * TILESIZE];
    tile_static float lBreal[TILESIZE * TILESIZE];
    tile_static float lAimg[TILESIZE * TILESIZE];
    tile_static float lBimg[TILESIZE * TILESIZE];
    rCreal[0][0] = 0;
    rCimg[0][0] = 0;
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;
    int gidx = tidx.tile[1];
    int gidy = tidx.tile[2];
    int idx = tidx.local[1];
    int idy = tidx.local[2];
    int idt = TILESIZE * idy + idx;
    int idxT = idt % TILESIZE;
    int idyT = idt / TILESIZE;
    int block_k = ((K + (TILESIZE - 1)) & ~(TILESIZE - 1)) >> shiftFactor;
    int i = 0;

    do {
      tidx.barrier.wait();

      if (gidy * TILESIZE + idxT < N && i * TILESIZE + idyT < K) {
        lBreal[idyT * TILESIZE + idxT] =
            B[elt][bOffset + gidy * TILESIZE + idxT + idyT * ldb +
                   i * (ldb << shiftFactor)]
                .x;
        lBimg[idyT * TILESIZE + idxT] =
            B[elt][bOffset + gidy * TILESIZE + idxT + idyT * ldb +
                   i * (ldb << shiftFactor)]
                .y;
      } else {
        lBreal[idyT * TILESIZE + idxT] = 0;
        lBimg[idyT * TILESIZE + idxT] = 0;
      }

      if (gidx * TILESIZE + idxT < M && i * TILESIZE + idyT < K) {
        lAreal[idyT * TILESIZE + idxT] =
            A[elt]
             [aOffset + (gidx * TILESIZE + idxT) * lda + idyT + i * TILESIZE]
                 .x;
        lAimg[idyT * TILESIZE + idxT] =
            A[elt]
             [aOffset + (gidx * TILESIZE + idxT) * lda + idyT + i * TILESIZE]
                 .y;
      } else {
        lAreal[idyT * TILESIZE + idxT] = 0;
        lAimg[idyT * TILESIZE + idxT] = 0;
      }

      tidx.barrier.wait();
      int offA = idx;
      int offB = idy;

      for (int iter = 0; iter < TILESIZE; iter++) {
        M1x1(TILESIZE);
      }

      i++;
    } while (--block_k > 0);

    tidx.barrier.wait();

    if (gidx * TILESIZE + idx < M && gidy * TILESIZE + idy < N) {
      CReal = C[elt]
               [cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
                   .x;
      CImg = C[elt]
              [cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
                  .y;
      CReal = (hc::fast_math::isnan(CReal) ||
               hc::fast_math::isinf(static_cast<float>(CReal)))
                  ? 0
                  : CReal;
      CImg = (hc::fast_math::isnan(CImg) ||
              hc::fast_math::isinf(static_cast<float>(CImg)))
                 ? 0
                 : CImg;
      tempReal = ((CReal * beta.x) - (CImg * beta.y));
      tempImg = ((CReal * beta.y) + (CImg * beta.x));
      C[elt][cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
          .x = tempReal + ((rCreal[0][0] * alpha.x) - (rCimg[0][0] * alpha.y));
      C[elt][cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
          .y = tempImg + ((rCreal[0][0] * alpha.y) + (rCimg[0][0] * alpha.x));
    }
  }) ;
#undef TILESIZE
#undef STEPSIZE
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_NoTransAB_rMajor_batch_STEP_TS16XSS16(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
#define TILESIZE 16
#define STEPSIZE 16
  hc::extent<3> grdExt(batchSize, (M + (TILESIZE - 1)) & ~(TILESIZE - 1),
                       (N + (TILESIZE - 1)) & ~(TILESIZE - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, TILESIZE, TILESIZE);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    int elt = tidx.tile[0];
    int shiftFactor = hc::fast_math::log2f(TILESIZE);
    float rCreal[1][1];
    float rAreal[1][1];
    float rBreal[1][1];
    float rCimg[1][1];
    float rAimg[1][1];
    float rBimg[1][1];
    tile_static float lAreal[TILESIZE * TILESIZE];
    tile_static float lBreal[TILESIZE * TILESIZE];
    tile_static float lAimg[TILESIZE * TILESIZE];
    tile_static float lBimg[TILESIZE * TILESIZE];
    rCreal[0][0] = 0;
    rCimg[0][0] = 0;
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;
    int gidx = tidx.tile[1];
    int gidy = tidx.tile[2];
    int idx = tidx.local[1];
    int idy = tidx.local[2];
    int idt = TILESIZE * idy + idx;
    int idxT = idt % TILESIZE;
    int idyT = idt / TILESIZE;
    int block_k = ((K + (TILESIZE - 1)) & ~(TILESIZE - 1)) >> shiftFactor;
    int i = 0;

    do {
      tidx.barrier.wait();

      if (gidy * TILESIZE + idxT < N && i * TILESIZE + idyT < K) {
        lBreal[idyT * TILESIZE + idxT] =
            B[elt][bOffset + gidy * TILESIZE + idxT + idyT * ldb +
                   i * (ldb << shiftFactor)]
                .x;
        lBimg[idyT * TILESIZE + idxT] =
            B[elt][bOffset + gidy * TILESIZE + idxT + idyT * ldb +
                   i * (ldb << shiftFactor)]
                .y;
      } else {
        lBreal[idyT * TILESIZE + idxT] = 0;
        lBimg[idyT * TILESIZE + idxT] = 0;
      }

      if (gidx * TILESIZE + idxT < M && i * TILESIZE + idyT < K) {
        lAreal[idyT * TILESIZE + idxT] =
            A[elt]
             [aOffset + (gidx * TILESIZE + idxT) * lda + idyT + i * TILESIZE]
                 .x;
        lAimg[idyT * TILESIZE + idxT] =
            A[elt]
             [aOffset + (gidx * TILESIZE + idxT) * lda + idyT + i * TILESIZE]
                 .y;
      } else {
        lAreal[idyT * TILESIZE + idxT] = 0;
        lAimg[idyT * TILESIZE + idxT] = 0;
      }

      tidx.barrier.wait();
      int offA = idx;
      int offB = idy;

      for (int iter = 0; iter < TILESIZE; iter++) {
        M1x1(TILESIZE);
      }

      i++;
    } while (--block_k > 0);

    tidx.barrier.wait();

    if (gidx * TILESIZE + idx < M && gidy * TILESIZE + idy < N) {
      CReal = C[elt]
               [cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
                   .x;
      CImg = C[elt]
              [cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
                  .y;
      CReal = (hc::fast_math::isnan(CReal) ||
               hc::fast_math::isinf(static_cast<float>(CReal)))
                  ? 0
                  : CReal;
      CImg = (hc::fast_math::isnan(CImg) ||
              hc::fast_math::isinf(static_cast<float>(CImg)))
                 ? 0
                 : CImg;
      tempReal = ((CReal * beta.x) - (CImg * beta.y));
      tempImg = ((CReal * beta.y) + (CImg * beta.x));
      C[elt][cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
          .x = tempReal + ((rCreal[0][0] * alpha.x) - (rCimg[0][0] * alpha.y));
      C[elt][cOffset + (gidx * TILESIZE + idx) * ldc + gidy * TILESIZE + idy]
          .y = tempImg + ((rCreal[0][0] * alpha.y) + (rCimg[0][0] * alpha.x));
    }
  }) ;
#undef TILESIZE
#undef STEPSIZE
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_NoTransAB_rMajor_batch_MICRO_TS16XMTS2(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
#define TILESIZE 16
#define MICROTILESIZE 1
  hc::extent<3> grdExt(batchSize, (M + (TILESIZE - 1)) & ~(TILESIZE - 1),
                       (N + (TILESIZE - 1)) & ~(TILESIZE - 1));
  hc::tiled_extent<3> t_ext = grdExt.tile(1, TILESIZE, TILESIZE);
  hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<3> & tidx)[[hc]] {
    int elt = tidx.tile[0];
    int shiftTS = hc::fast_math::log2f(TILESIZE);
    int shiftMTP = hc::fast_math::log2f(MICROTILEPROD);
    float rCreal[MICROTILESIZE][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rCimg[MICROTILESIZE][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rAreal[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rAimg[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rBreal[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    float rBimg[1][MICROTILESIZE] = {{static_cast<float>(0)}};
    tile_static float lAreal[TOTMICROTILEPROD + TILESIZE];
    tile_static float lAimg[TOTMICROTILEPROD + TILESIZE];
    tile_static float lBreal[TOTMICROTILEPROD + TILESIZE];
    tile_static float lBimg[TOTMICROTILEPROD + TILESIZE];
    int gidx = tidx.tile[1];
    int gidy = tidx.tile[2];
    int idx = tidx.local[1];
    int idy = tidx.local[2];
    int idt = (idy << shiftTS) + idx;
    int idxT = idt & (TILESIZE - 1);
    int idyT = idt >> shiftTS;
    int block_k = 0;
    float tempReal = 0.0;
    float tempImg = 0.0;
    float CReal = 0.0;
    float CImg = 0.0;

    do {
      int colIndex = (block_k << shiftTS) + idyT;
      int lIndex = (idyT * BANKMICROTILESIZE) + idxT;
      tidx.barrier.wait();

      for (int sec = 0; sec < MICROTILESIZE; ++sec) {
        int secVal = sec << shiftTS;
        int BrowIndex = (gidy << shiftMTP) + idxT + secVal;
        int ArowIndex = (gidx << shiftMTP) + idxT + secVal;

        if (BrowIndex < N && colIndex < K) {
          lBreal[lIndex + secVal] =
              B[elt][bOffset + BrowIndex + colIndex * ldb].x;
          lBimg[lIndex + secVal] =
              B[elt][bOffset + BrowIndex + colIndex * ldb].y;
        } else {
          lBreal[lIndex + secVal] = 0;
          lBimg[lIndex + secVal] = 0;
        }

        if (ArowIndex < M && colIndex < K) {
          lAreal[lIndex + secVal] =
              A[elt][aOffset + ArowIndex * lda + colIndex].x;
          lAimg[lIndex + secVal] =
              A[elt][aOffset + ArowIndex * lda + colIndex].y;
        } else {
          lAreal[lIndex + secVal] = 0;
          lAimg[lIndex + secVal] = 0;
        }
      }

      tidx.barrier.wait();
      int offA = idx;
      int offB = idy;

      for (int iter = 0; iter < TILESIZE; ++iter) {
        MTS;
      }

      tidx.barrier.wait();
    } while (++block_k < (((K + TILESIZE - 1) & ~(TILESIZE - 1)) >> shiftTS));

    int xIndex = ((gidx << shiftMTP) + idx) * ldc;
    int yIndex = (gidy << shiftMTP) + idy;

    for (int row = 0; row < MICROTILESIZE; row++) {
      for (int col = 0; col < MICROTILESIZE; col++) {
        if ((xIndex / ldc) + (col << shiftTS) < M &&
            yIndex + (row << shiftTS) < N) {
          CReal = C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                         (row << shiftTS)]
                      .x;
          CImg = C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                        (row * TILESIZE)]
                     .y;
          CReal = (hc::fast_math::isnan(CReal) ||
                   hc::fast_math::isinf(static_cast<float>(CReal)))
                      ? 0
                      : CReal;
          CImg = (hc::fast_math::isnan(CImg) ||
                  hc::fast_math::isinf(static_cast<float>(CImg)))
                     ? 0
                     : CImg;
          tempReal = ((CReal * beta.x) - (CImg * beta.y));
          tempImg = ((CReal * beta.y) + (CImg * beta.x));
          C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                 (row << shiftTS)]
              .x = tempReal +
                   ((rCreal[col][row] * alpha.x) - (rCimg[col][row] * alpha.y));
          C[elt][cOffset + (xIndex + (col << shiftTS) * ldc) + yIndex +
                 (row * TILESIZE)]
              .y = tempImg +
                   ((rCreal[col][row] * alpha.y) + (rCimg[col][row] * alpha.x));
        }
      }
    }
  }) ;
#undef TILESIZE
#undef MICROTILESIZE
  return HCBLAS_SUCCEEDS;
}

hcblasStatus cgemm_TransAB_rMajor(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
  if (M < 600 && N < 600 && K >= 600 && K < 1800) {
    return cgemm_TransAB_rMajor_batch_loopunroll(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  } else if (M >= 600 && M < 6000 && N < 600 && K < 1800) {
    return cgemm_TransAB_rMajor_batch_MICRO_TS16XMTS2(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  } else if (M >= 1800 && M < 6000 && N < 10 && K >= 600 && K < 6000) {
    return cgemm_TransAB_rMajor_batch_STEP_TS8XSS8(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  } else if (M >= 6000 && M < 10000 && N < 10 && K >= 1800 && K < 6000) {
    return cgemm_TransAB_rMajor_batch_MICRO_TS8XMTS2(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  } else {
    return cgemm_TransAB_rMajor_batch_MICRO_TS16XMTS2(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  }
}

hcblasStatus cgemm_NoTransB_rMajor(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
  /*if ((M < 600 && N >= 600 && N < 1800 && K < 600)||(M >= 600 && M < 6000 && N
  <6000 && K < 600))
  { */
  return cgemm_NoTransB_rMajor_batch_MICRO_TS16XMTS2(
      accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
      cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  //}
}

hcblasStatus cgemm_NoTransA_rMajor(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
  if ((M >= 10 && M < 6000 && N < 600 && K < 10) ||
      (M >= 600 && M < 1800 && N < 10 && K >= 1800 && K < 6000) ||
      (M < 600 && N < 600 && K > 1800 && K < 6000)) {
    return cgemm_NoTransA_rMajor_batch_STEP_TS8XSS8(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  } else if (M >= 600 && M < 6000 && N < 600 && K < 600) {
    return cgemm_NoTransA_rMajor_batch_MICRO_TS16XMTS2(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  } else if (M > 1800 && M < 6000 && N < 600 && K >= 1800 && K < 10000) {
    return cgemm_NoTransA_rMajor_batch_loopunroll(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  } else {
    return cgemm_NoTransA_rMajor_batch_MICRO_TS16XMTS2(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  }
}

hcblasStatus cgemm_NoTransAB_rMajor(
    hc::accelerator_view accl_view, hc::short_vector::float_2 *A[],
    __int64_t aOffset, __int64_t A_batchOffset, hc::short_vector::float_2 *B[],
    __int64_t bOffset, __int64_t B_batchOffset, hc::short_vector::float_2 *C[],
    __int64_t cOffset, __int64_t C_batchOffset, int M, int N, int K, int lda,
    int ldb, int ldc, hc::short_vector::float_2 alpha,
    hc::short_vector::float_2 beta, int batchSize) {
  if ((M < 600 && N < 600 && K < 10) || (M < 1800 && N < 600 && K < 600)) {
    return cgemm_NoTransAB_rMajor_batch_STEP_TS8XSS8(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  } else if ((M < 600 && N < 600 && K < 1800) ||
             (M < 1800 && ((N < 600 && K < 1800) || (N < 1800 && K < 10)))) {
    return cgemm_NoTransAB_rMajor_batch_STEP_TS16XSS16(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  } else { /*if (M < 1800 && N < 1800 && K < 1800)*/
    return cgemm_NoTransAB_rMajor_batch_MICRO_TS16XMTS2(
        accl_view, A, aOffset, A_batchOffset, B, bOffset, B_batchOffset, C,
        cOffset, C_batchOffset, M, N, K, lda, ldb, ldc, alpha, beta, batchSize);
  }
}

