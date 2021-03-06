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

#include "include/hcblaslib.h"
#include <hc.hpp>
#include <hc_am.hpp>
#include <hc_math.hpp>

#define BLOCK_SIZE 256

static void gemv_TransA(hc::accelerator_view accl_view, double *A_mat,
                        __int64_t aOffset, double *X_vec, __int64_t xOffset,
                        double *Y_vec, __int64_t yOffset, double alpha,
                        double beta, int lenX, int lenY) {
  if ((lenX - lenY) > 5000) {
    int len_X = (lenX + (BLOCK_SIZE - 1)) & ~(BLOCK_SIZE - 1);
    int len_Y = (lenY + (BLOCK_SIZE - 1)) & ~(BLOCK_SIZE - 1);
    int num_blocks = len_X / BLOCK_SIZE;
    double *temp =
        reinterpret_cast<double *>(malloc(num_blocks * len_Y * sizeof(double)));
    hc::accelerator acc = accl_view.get_accelerator();
    double *tempBuf = hc::am_alloc(sizeof(double) * num_blocks * len_Y, acc, 0);
    hc::extent<1> grdExt(len_X);
    hc::tiled_extent<1> t_ext = grdExt.tile(BLOCK_SIZE);
    hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<1> tidx)[[hc]] {
      tile_static double t[BLOCK_SIZE];

      for (int Col = 0; Col < lenY; Col++) {
        int blockIdx = tidx.tile[0];
        int threadIdx = tidx.local[0];
        tempBuf[Col * num_blocks + blockIdx] = 0;
        t[threadIdx] = 0;

        if (Col < lenY && blockIdx * BLOCK_SIZE + threadIdx < lenX) {
          t[threadIdx] =
              X_vec[xOffset + blockIdx * BLOCK_SIZE + threadIdx] *
              A_mat[aOffset + Col * lenX + blockIdx * BLOCK_SIZE + threadIdx];
        }

        tidx.barrier.wait();

        for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
          if (threadIdx < stride) {
            t[threadIdx] += t[threadIdx + stride];
          }
        }

        tempBuf[Col * num_blocks + blockIdx] = t[0];
        tidx.barrier.wait();
      }

      if (tidx.tile[0] == 0) {
        for (int Col = 0; Col < lenY; Col++) {
          tile_static double sh[BLOCK_SIZE];
          int threadId = tidx.local[0];
          sh[tidx.local[0]] = 0;

          for (int i = threadId; i < num_blocks; i += tidx.tile_dim[0]) {
            sh[threadId] += tempBuf[Col * num_blocks + i];
          }

          tidx.barrier.wait();

          for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
            if (threadId < stride) {
              sh[threadId] += sh[threadId + stride];
            }
          }

          tidx.barrier.wait();
          __int64_t Y_index = yOffset + Col;
          Y_vec[Y_index] =
              (hc::fast_math::isnan(static_cast<float>(Y_vec[Y_index])) ||
               hc::fast_math::isinf(static_cast<float>(Y_vec[Y_index])))
                  ? 0
                  : Y_vec[Y_index];
          Y_vec[Y_index] *= beta;
          Y_vec[Y_index] += alpha * sh[0];
        }
      }
    }) ;
    // free up resources
    free(temp);
    hc::am_free(tempBuf);
  } else {
    hc::extent<1> grdExt(lenY * BLOCK_SIZE);
    hc::tiled_extent<1> t_ext = grdExt.tile(BLOCK_SIZE);
    hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<1> tidx)[[hc]] {
      int threadIdx = tidx.local[0];
      int blockIdx = tidx.tile[0];
      int Col = blockIdx;
      tile_static double sh[BLOCK_SIZE];
      sh[threadIdx] = 0;

      for (int tileId = 0;
           tileId < ((lenX + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1)) / BLOCK_SIZE;
           tileId++) {
        if (tileId * BLOCK_SIZE + threadIdx < lenX && Col < lenY) {
          sh[threadIdx] +=
              X_vec[xOffset + tileId * BLOCK_SIZE + threadIdx] *
              A_mat[aOffset + Col * lenX + tileId * BLOCK_SIZE + threadIdx];
        }
      }

      tidx.barrier.wait();

      for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
        if (threadIdx < stride) {
          sh[threadIdx] += sh[threadIdx + stride];
        }

        tidx.barrier.wait();
      }

      if (threadIdx == 0 && Col < lenY) {
        __int64_t Y_index = yOffset + Col;
        Y_vec[Y_index] =
            (hc::fast_math::isnan(static_cast<float>(Y_vec[Y_index])) ||
             hc::fast_math::isinf(static_cast<float>(Y_vec[Y_index])))
                ? 0
                : Y_vec[Y_index];
        Y_vec[Y_index] *= beta;
        Y_vec[Y_index] += alpha * sh[0];
      }
    }) ;
  }
}

static void gemv_TransA(hc::accelerator_view accl_view, double *A_mat,
                        __int64_t aOffset, __int64_t A_batchOffset,
                        double *X_vec, __int64_t xOffset,
                        __int64_t X_batchOffset, double *Y_vec,
                        __int64_t yOffset, __int64_t Y_batchOffset,
                        double alpha, double beta, int lenX, int lenY,
                        int batchSize) {
  if ((lenX - lenY) > 5000) {
    int len_X = (lenX + (BLOCK_SIZE - 1)) & ~(BLOCK_SIZE - 1);
    int len_Y = (lenY + (BLOCK_SIZE - 1)) & ~(BLOCK_SIZE - 1);
    int num_blocks = len_X / BLOCK_SIZE;
    double *temp =
        reinterpret_cast<double *>(malloc(num_blocks * len_Y * sizeof(double)));
    hc::accelerator acc = accl_view.get_accelerator();
    double *tempBuf = hc::am_alloc(sizeof(double) * num_blocks * len_Y, acc, 0);
    hc::extent<2> grdExt(batchSize, len_X);
    hc::tiled_extent<2> t_ext = grdExt.tile(1, BLOCK_SIZE);
    hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<2> tidx)[[hc]] {
      tile_static double t[BLOCK_SIZE];
      int elt = tidx.tile[0];

      for (int Col = 0; Col < lenY; Col++) {
        int blockIdx = tidx.tile[1];
        int threadIdx = tidx.local[1];
        tempBuf[Col * num_blocks + blockIdx] = 0;
        t[threadIdx] = 0;

        if (Col < lenY && blockIdx * BLOCK_SIZE + threadIdx < lenX) {
          t[threadIdx] = X_vec[xOffset + X_batchOffset * elt +
                               blockIdx * BLOCK_SIZE + threadIdx] *
                         A_mat[aOffset + A_batchOffset * elt + Col * lenX +
                               blockIdx * BLOCK_SIZE + threadIdx];
        }

        tidx.barrier.wait();

        for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
          if (threadIdx < stride) {
            t[threadIdx] += t[threadIdx + stride];
          }
        }

        tempBuf[Col * num_blocks + blockIdx] = t[0];
        tidx.barrier.wait();
      }

      if (tidx.tile[1] == 0) {
        for (int Col = 0; Col < lenY; Col++) {
          tile_static double sh[BLOCK_SIZE];
          int threadId = tidx.local[1];
          sh[tidx.local[1]] = 0;

          for (int i = threadId; i < num_blocks; i += tidx.tile_dim[0]) {
            sh[threadId] += tempBuf[Col * num_blocks + i];
          }

          tidx.barrier.wait();

          for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
            if (threadId < stride) {
              sh[threadId] += sh[threadId + stride];
            }
          }

          tidx.barrier.wait();
          __int64_t Y_index = yOffset + Y_batchOffset * elt + Col;
          Y_vec[Y_index] =
              (hc::fast_math::isnan(static_cast<float>(Y_vec[Y_index])) ||
               hc::fast_math::isinf(static_cast<float>(Y_vec[Y_index])))
                  ? 0
                  : Y_vec[Y_index];
          Y_vec[Y_index] *= beta;
          Y_vec[Y_index] += alpha * sh[0];
        }
      }
    }) ;
    // free up resources
    free(temp);
    hc::am_free(tempBuf);
  } else {
    hc::extent<2> grdExt(batchSize, lenY * BLOCK_SIZE);
    hc::tiled_extent<2> t_ext = grdExt.tile(1, BLOCK_SIZE);
    hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<2> tidx)[[hc]] {
      int elt = tidx.tile[0];
      int threadIdx = tidx.local[1];
      int blockIdx = tidx.tile[1];
      int Col = blockIdx;
      tile_static double sh[BLOCK_SIZE];
      sh[threadIdx] = 0;

      for (int tileId = 0;
           tileId < ((lenX + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1)) / BLOCK_SIZE;
           tileId++) {
        if (tileId * BLOCK_SIZE + threadIdx < lenX && Col < lenY) {
          sh[threadIdx] += X_vec[xOffset + X_batchOffset * elt +
                                 tileId * BLOCK_SIZE + threadIdx] *
                           A_mat[aOffset + A_batchOffset * elt + Col * lenX +
                                 tileId * BLOCK_SIZE + threadIdx];
        }
      }

      tidx.barrier.wait();

      for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
        if (threadIdx < stride) {
          sh[threadIdx] += sh[threadIdx + stride];
        }

        tidx.barrier.wait();
      }

      if (threadIdx == 0 && Col < lenY) {
        __int64_t Y_index = yOffset + Y_batchOffset * elt + Col;
        Y_vec[Y_index] =
            (hc::fast_math::isnan(static_cast<float>(Y_vec[Y_index])) ||
             hc::fast_math::isinf(static_cast<float>(Y_vec[Y_index])))
                ? 0
                : Y_vec[Y_index];
        Y_vec[Y_index] *= beta;
        Y_vec[Y_index] += alpha * sh[0];
      }
    }) ;
  }
}

static void gemv_TransA_rMajor(hc::accelerator_view accl_view, double *A_mat,
                               __int64_t aOffset, double *X_vec,
                               __int64_t xOffset, double *Y_vec,
                               __int64_t yOffset, double alpha, double beta,
                               int lenX, int lenY) {
  if ((lenX - lenY) > 5000) {
    int len_X = (lenX + (BLOCK_SIZE - 1)) & ~(BLOCK_SIZE - 1);
    int len_Y = (lenY + (BLOCK_SIZE - 1)) & ~(BLOCK_SIZE - 1);
    int num_blocks = len_X / BLOCK_SIZE;
    double *temp =
        reinterpret_cast<double *>(malloc(num_blocks * len_Y * sizeof(double)));
    hc::accelerator acc = accl_view.get_accelerator();
    double *tempBuf = hc::am_alloc(sizeof(double) * num_blocks * len_Y, acc, 0);
    hc::extent<1> grdExt(len_X);
    hc::tiled_extent<1> t_ext = grdExt.tile(BLOCK_SIZE);
    hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<1> tidx)[[hc]] {
      tile_static double t[BLOCK_SIZE];

      for (int Col = 0; Col < lenY; Col++) {
        int blockIdx = tidx.tile[0];
        int threadIdx = tidx.local[0];
        tempBuf[Col * num_blocks + blockIdx] = 0;
        t[threadIdx] = 0;

        if (Col < lenY && blockIdx * BLOCK_SIZE + threadIdx < lenX) {
          t[threadIdx] =
              X_vec[xOffset + blockIdx * BLOCK_SIZE + threadIdx] *
              A_mat[aOffset + Col + (blockIdx * BLOCK_SIZE + threadIdx) * lenY];
        }

        tidx.barrier.wait();

        for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
          if (threadIdx < stride) {
            t[threadIdx] += t[threadIdx + stride];
          }
        }

        tempBuf[Col * num_blocks + blockIdx] = t[0];
        tidx.barrier.wait();
      }

      if (tidx.tile[0] == 0) {
        for (int Col = 0; Col < lenY; Col++) {
          tile_static double sh[BLOCK_SIZE];
          int threadId = tidx.local[0];
          sh[tidx.local[0]] = 0;

          for (int i = threadId; i < num_blocks; i += tidx.tile_dim[0]) {
            sh[threadId] += tempBuf[Col * num_blocks + i];
          }

          tidx.barrier.wait();

          for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
            if (threadId < stride) {
              sh[threadId] += sh[threadId + stride];
            }
          }

          tidx.barrier.wait();
          __int64_t Y_index = yOffset + Col;
          Y_vec[Y_index] =
              (hc::fast_math::isnan(static_cast<float>(Y_vec[Y_index])) ||
               hc::fast_math::isinf(static_cast<float>(Y_vec[Y_index])))
                  ? 0
                  : Y_vec[Y_index];
          Y_vec[Y_index] *= beta;
          Y_vec[Y_index] += alpha * sh[0];
        }
      }
    }) ;
    // Free up resources
    free(temp);
    hc::am_free(tempBuf);
  } else {
    hc::extent<1> grdExt(lenY * BLOCK_SIZE);
    hc::tiled_extent<1> t_ext = grdExt.tile(BLOCK_SIZE);
    hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<1> tidx)[[hc]] {
      int threadIdx = tidx.local[0];
      int blockIdx = tidx.tile[0];
      int Col = blockIdx;
      tile_static double sh[BLOCK_SIZE];
      sh[threadIdx] = 0;

      for (int tileId = 0;
           tileId < ((lenX + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1)) / BLOCK_SIZE;
           tileId++) {
        if (tileId * BLOCK_SIZE + threadIdx < lenX && Col < lenY) {
          sh[threadIdx] +=
              X_vec[xOffset + tileId * BLOCK_SIZE + threadIdx] *
              A_mat[aOffset + Col + (tileId * BLOCK_SIZE + threadIdx) * lenY];
        }
      }

      tidx.barrier.wait();

      for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
        if (threadIdx < stride) {
          sh[threadIdx] += sh[threadIdx + stride];
        }

        tidx.barrier.wait();
      }

      if (threadIdx == 0 && Col < lenY) {
        __int64_t Y_index = yOffset + Col;
        Y_vec[Y_index] =
            (hc::fast_math::isnan(static_cast<float>(Y_vec[Y_index])) ||
             hc::fast_math::isinf(static_cast<float>(Y_vec[Y_index])))
                ? 0
                : Y_vec[Y_index];
        Y_vec[Y_index] *= beta;
        Y_vec[Y_index] += alpha * sh[0];
      }
    }) ;
  }
}

static void gemv_TransA_rMajor(hc::accelerator_view accl_view, double *A_mat,
                               __int64_t aOffset, __int64_t A_batchOffset,
                               double *X_vec, __int64_t xOffset,
                               __int64_t X_batchOffset, double *Y_vec,
                               __int64_t yOffset, __int64_t Y_batchOffset,
                               double alpha, double beta, int lenX, int lenY,
                               int batchSize) {
  if ((lenX - lenY) > 5000) {
    int len_X = (lenX + (BLOCK_SIZE - 1)) & ~(BLOCK_SIZE - 1);
    int len_Y = (lenY + (BLOCK_SIZE - 1)) & ~(BLOCK_SIZE - 1);
    int num_blocks = len_X / BLOCK_SIZE;
    double *temp =
        reinterpret_cast<double *>(malloc(num_blocks * len_Y * sizeof(double)));
    hc::accelerator acc = accl_view.get_accelerator();
    double *tempBuf = hc::am_alloc(sizeof(double) * num_blocks * len_Y, acc, 0);
    hc::extent<2> grdExt(batchSize, len_X);
    hc::tiled_extent<2> t_ext = grdExt.tile(1, BLOCK_SIZE);
    hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<2> tidx)[[hc]] {
      tile_static double t[BLOCK_SIZE];
      int elt = tidx.tile[0];

      for (int Col = 0; Col < lenY; Col++) {
        int blockIdx = tidx.tile[1];
        int threadIdx = tidx.local[1];
        tempBuf[Col * num_blocks + blockIdx] = 0;
        t[threadIdx] = 0;

        if (Col < lenY && blockIdx * BLOCK_SIZE + threadIdx < lenX) {
          t[threadIdx] = X_vec[xOffset + X_batchOffset * elt +
                               blockIdx * BLOCK_SIZE + threadIdx] *
                         A_mat[aOffset + A_batchOffset * elt + Col +
                               (blockIdx * BLOCK_SIZE + threadIdx) * lenY];
        }

        tidx.barrier.wait();

        for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
          if (threadIdx < stride) {
            t[threadIdx] += t[threadIdx + stride];
          }
        }

        tempBuf[Col * num_blocks + blockIdx] = t[0];
        tidx.barrier.wait();
      }

      if (tidx.tile[1] == 0) {
        for (int Col = 0; Col < lenY; Col++) {
          tile_static double sh[BLOCK_SIZE];
          int threadId = tidx.local[1];
          sh[tidx.local[1]] = 0;

          for (int i = threadId; i < num_blocks; i += tidx.tile_dim[0]) {
            sh[threadId] += tempBuf[Col * num_blocks + i];
          }

          tidx.barrier.wait();

          for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
            if (threadId < stride) {
              sh[threadId] += sh[threadId + stride];
            }
          }

          tidx.barrier.wait();
          __int64_t Y_index = yOffset + Y_batchOffset * elt + Col;
          Y_vec[Y_index] =
              (hc::fast_math::isnan(static_cast<float>(Y_vec[Y_index])) ||
               hc::fast_math::isinf(static_cast<float>(Y_vec[Y_index])))
                  ? 0
                  : Y_vec[Y_index];
          Y_vec[Y_index] *= beta;
          Y_vec[Y_index] += alpha * sh[0];
        }
      }
    }) ;
    // Free up resources
    free(temp);
    hc::am_free(tempBuf);
  } else {
    hc::extent<2> grdExt(batchSize, lenY * BLOCK_SIZE);
    hc::tiled_extent<2> t_ext = grdExt.tile(1, BLOCK_SIZE);
    hc::parallel_for_each(accl_view, t_ext, [=](hc::tiled_index<2> tidx)[[hc]] {
      int elt = tidx.tile[0];
      int threadIdx = tidx.local[1];
      int blockIdx = tidx.tile[1];
      int Col = blockIdx;
      tile_static double sh[BLOCK_SIZE];
      sh[threadIdx] = 0;

      for (int tileId = 0;
           tileId < ((lenX + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1)) / BLOCK_SIZE;
           tileId++) {
        if (tileId * BLOCK_SIZE + threadIdx < lenX && Col < lenY) {
          sh[threadIdx] += X_vec[xOffset + X_batchOffset * elt +
                                 tileId * BLOCK_SIZE + threadIdx] *
                           A_mat[aOffset + A_batchOffset * elt + Col +
                                 (tileId * BLOCK_SIZE + threadIdx) * lenY];
        }
      }

      tidx.barrier.wait();

      for (int stride = BLOCK_SIZE / 2; stride >= 1; stride /= 2) {
        if (threadIdx < stride) {
          sh[threadIdx] += sh[threadIdx + stride];
        }

        tidx.barrier.wait();
      }

      if (threadIdx == 0 && Col < lenY) {
        __int64_t Y_index = yOffset + Y_batchOffset * elt + Col;
        Y_vec[Y_index] =
            (hc::fast_math::isnan(static_cast<float>(Y_vec[Y_index])) ||
             hc::fast_math::isinf(static_cast<float>(Y_vec[Y_index])))
                ? 0
                : Y_vec[Y_index];
        Y_vec[Y_index] *= beta;
        Y_vec[Y_index] += alpha * sh[0];
      }
    }) ;
  }
}

static void gemv_NoTransA(hc::accelerator_view accl_view, double *A,
                          __int64_t aOffset, double *X, __int64_t xOffset,
                          double *Y, __int64_t yOffset, double alpha,
                          double beta, int lenX, int lenY) {
  __int64_t size = (lenY + 255) & ~255;
  hc::extent<1> compute_domain(size);
  hc::parallel_for_each(accl_view, compute_domain.tile(BLOCK_SIZE), [=
  ](hc::tiled_index<1> tidx)[[hc]] {
    int bx = tidx.tile[0];
    int tx = tidx.local[0];
    tile_static double Xds[BLOCK_SIZE];
    int Col = bx * BLOCK_SIZE + tx;
    double Pvalue = 0;

    for (int m = 0; m < (lenX - 1) / BLOCK_SIZE + 1; ++m) {
      if (m * BLOCK_SIZE + tx < lenX) {
        Xds[tx] = X[xOffset + m * BLOCK_SIZE + tx];
      } else {
        Xds[tx] = 0;
      }

      tidx.barrier.wait();

      for (int k = 0; k < BLOCK_SIZE; k++)
        if (Col < lenY && m * BLOCK_SIZE + k < lenX) {
          Pvalue += Xds[k] * A[aOffset + Col + (m * BLOCK_SIZE + k) * lenY];
        }

      tidx.barrier.wait();
    }

    if (Col < lenY) {
      __int64_t Y_index = yOffset + Col;
      Y[Y_index] = (hc::fast_math::isnan(static_cast<float>(Y[Y_index])) ||
                    hc::fast_math::isinf(static_cast<float>(Y[Y_index])))
                       ? 0
                       : Y[Y_index];
      Y[Y_index] *= beta;
      Y[Y_index] += alpha * Pvalue;
    }

    tidx.barrier.wait();
  }) ;
}

static void gemv_NoTransA(hc::accelerator_view accl_view, double *A,
                          __int64_t aOffset, __int64_t A_batchOffset, double *X,
                          __int64_t xOffset, __int64_t X_batchOffset, double *Y,
                          __int64_t yOffset, __int64_t Y_batchOffset,
                          double alpha, double beta, int lenX, int lenY,
                          int batchSize) {
  __int64_t size = (lenY + 255) & ~255;
  hc::extent<2> compute_domain(batchSize, size);
  hc::parallel_for_each(accl_view, compute_domain.tile(1, BLOCK_SIZE), [=
  ](hc::tiled_index<2> tidx)[[hc]] {
    int elt = tidx.tile[0];
    int bx = tidx.tile[1];
    int tx = tidx.local[1];
    tile_static double Xds[BLOCK_SIZE];
    int Col = bx * BLOCK_SIZE + tx;
    double Pvalue = 0;

    for (int m = 0; m < (lenX - 1) / BLOCK_SIZE + 1; ++m) {
      if (m * BLOCK_SIZE + tx < lenX) {
        Xds[tx] = X[xOffset + X_batchOffset * elt + m * BLOCK_SIZE + tx];
      } else {
        Xds[tx] = 0;
      }

      tidx.barrier.wait();

      for (int k = 0; k < BLOCK_SIZE; k++)
        if (Col < lenY && m * BLOCK_SIZE + k < lenX) {
          Pvalue += Xds[k] * A[aOffset + A_batchOffset * elt + Col +
                               (m * BLOCK_SIZE + k) * lenY];
        }

      tidx.barrier.wait();
    }

    if (Col < lenY) {
      __int64_t Y_index = yOffset + Y_batchOffset * elt + Col;
      Y[Y_index] = (hc::fast_math::isnan(static_cast<float>(Y[Y_index])) ||
                    hc::fast_math::isinf(static_cast<float>(Y[Y_index])))
                       ? 0
                       : Y[Y_index];
      Y[Y_index] *= beta;
      Y[Y_index] += alpha * Pvalue;
    }

    tidx.barrier.wait();
  }) ;
}

static void gemv_NoTransA_rMajor(hc::accelerator_view accl_view, double *A,
                                 __int64_t aOffset, double *X,
                                 __int64_t xOffset, double *Y,
                                 __int64_t yOffset, double alpha, double beta,
                                 int lenX, int lenY) {
  __int64_t size = (lenY + 255) & ~255;
  hc::extent<1> compute_domain(size);
  hc::parallel_for_each(accl_view, compute_domain.tile(BLOCK_SIZE), [=
  ](hc::tiled_index<1> tidx)[[hc]] {
    int bx = tidx.tile[0];
    int tx = tidx.local[0];
    tile_static double Xds[BLOCK_SIZE];
    int Col = bx * BLOCK_SIZE + tx;
    double Pvalue = 0;

    for (int m = 0; m < (lenX - 1) / BLOCK_SIZE + 1; ++m) {
      if (m * BLOCK_SIZE + tx < lenX) {
        Xds[tx] = X[xOffset + m * BLOCK_SIZE + tx];
      } else {
        Xds[tx] = 0;
      }

      tidx.barrier.wait();

      for (int k = 0; k < BLOCK_SIZE; k++)
        if (Col < lenY && m * BLOCK_SIZE + k < lenX) {
          Pvalue += Xds[k] * A[aOffset + Col * lenX + m * BLOCK_SIZE + k];
        }

      tidx.barrier.wait();
    }

    if (Col < lenY) {
      __int64_t Y_index = yOffset + Col;
      Y[Y_index] = (hc::fast_math::isnan(static_cast<float>(Y[Y_index])) ||
                    hc::fast_math::isinf(static_cast<float>(Y[Y_index])))
                       ? 0
                       : Y[Y_index];
      Y[Y_index] *= beta;
      Y[Y_index] += alpha * Pvalue;
    }

    tidx.barrier.wait();
  }) ;
}

static void gemv_NoTransA_rMajor(hc::accelerator_view accl_view, double *A,
                                 __int64_t aOffset, __int64_t A_batchOffset,
                                 double *X, __int64_t xOffset,
                                 __int64_t X_batchOffset, double *Y,
                                 __int64_t yOffset, __int64_t Y_batchOffset,
                                 double alpha, double beta, int lenX, int lenY,
                                 int batchSize) {
  __int64_t size = (lenY + 255) & ~255;
  hc::extent<2> compute_domain(batchSize, size);
  hc::parallel_for_each(accl_view, compute_domain.tile(1, BLOCK_SIZE), [=
  ](hc::tiled_index<2> tidx)[[hc]] {
    int elt = tidx.tile[0];
    int bx = tidx.tile[1];
    int tx = tidx.local[1];
    tile_static double Xds[BLOCK_SIZE];
    int Col = bx * BLOCK_SIZE + tx;
    double Pvalue = 0;

    for (int m = 0; m < (lenX - 1) / BLOCK_SIZE + 1; ++m) {
      if (m * BLOCK_SIZE + tx < lenX) {
        Xds[tx] = X[xOffset + X_batchOffset * elt + m * BLOCK_SIZE + tx];
      } else {
        Xds[tx] = 0;
      }

      tidx.barrier.wait();

      for (int k = 0; k < BLOCK_SIZE; k++)
        if (Col < lenY && m * BLOCK_SIZE + k < lenX) {
          Pvalue += Xds[k] * A[aOffset + A_batchOffset * elt + Col * lenX +
                               m * BLOCK_SIZE + k];
        }

      tidx.barrier.wait();
    }

    if (Col < lenY) {
      __int64_t Y_index = yOffset + Y_batchOffset * elt + Col;
      Y[Y_index] = (hc::fast_math::isnan(static_cast<float>(Y[Y_index])) ||
                    hc::fast_math::isinf(static_cast<float>(Y[Y_index])))
                       ? 0
                       : Y[Y_index];
      Y[Y_index] *= beta;
      Y[Y_index] += alpha * Pvalue;
    }

    tidx.barrier.wait();
  }) ;
}

static void gemv_alpha0_col(hc::accelerator_view accl_view, double *A,
                            __int64_t aOffset, double *X, __int64_t xOffset,
                            double *Y, __int64_t yOffset, double alpha,
                            double beta, int lenX, int lenY) {
  __int64_t size = (lenY + 255) & ~255;
  hc::extent<1> compute_domain(size);
  hc::parallel_for_each(accl_view, compute_domain.tile(BLOCK_SIZE), [=
  ](hc::tiled_index<1> tidx)[[hc]] {
    int bx = tidx.tile[0];
    int tx = tidx.local[0];
    int Col = bx * BLOCK_SIZE + tx;
    if (Col < lenY) {
      __int64_t Y_index = yOffset + Col;
      Y[Y_index] = (hc::fast_math::isnan(static_cast<float>(Y[Y_index])) ||
                    hc::fast_math::isinf(static_cast<float>(Y[Y_index])))
                       ? 0
                       : Y[Y_index];
      if (alpha == 0) {
        if (beta == 0)
          Y[Y_index] = 0.0;
        else
          Y[Y_index] *= beta;
      }
    }
  }) ;
}

static void gemv_alpha0_colbatch(hc::accelerator_view accl_view, double *A,
                                 __int64_t aOffset, __int64_t A_batchOffset,
                                 double *X, __int64_t xOffset,
                                 __int64_t X_batchOffset, double *Y,
                                 __int64_t yOffset, __int64_t Y_batchOffset,
                                 double alpha, double beta, int lenX, int lenY,
                                 int batchSize) {
  __int64_t size = (lenY + 255) & ~255;
  hc::extent<2> compute_domain(batchSize, size);
  hc::parallel_for_each(accl_view, compute_domain.tile(1, BLOCK_SIZE), [=
  ](hc::tiled_index<2> tidx)[[hc]] {
    int elt = tidx.tile[0];
    int bx = tidx.tile[1];
    int tx = tidx.local[1];
    int Col = bx * BLOCK_SIZE + tx;
    if (Col < lenY) {
      __int64_t Y_index = yOffset + Y_batchOffset * elt + Col;
      Y[Y_index] = (hc::fast_math::isnan(static_cast<float>(Y[Y_index])) ||
                    hc::fast_math::isinf(static_cast<float>(Y[Y_index])))
                       ? 0
                       : Y[Y_index];
      if (alpha == 0) {
        if (beta == 0)
          Y[Y_index] = 0.0;
        else
          Y[Y_index] *= beta;
      }
    }
  }) ;
}

static void gemv_alpha0_row(hc::accelerator_view accl_view, double *A,
                            __int64_t aOffset, double *X, __int64_t xOffset,
                            double *Y, __int64_t yOffset, double alpha,
                            double beta, int lenX, int lenY) {
  __int64_t size = (lenY + 255) & ~255;
  hc::extent<1> compute_domain(size);
  hc::parallel_for_each(accl_view, compute_domain.tile(BLOCK_SIZE), [=
  ](hc::tiled_index<1> tidx)[[hc]] {
    int bx = tidx.tile[0];
    int tx = tidx.local[0];
    int Col = bx * BLOCK_SIZE + tx;
    if (Col < lenY) {
      __int64_t Y_index = yOffset + Col;
      Y[Y_index] = (hc::fast_math::isnan(static_cast<float>(Y[Y_index])) ||
                    hc::fast_math::isinf(static_cast<float>(Y[Y_index])))
                       ? 0
                       : Y[Y_index];
      if (alpha == 0) {
        if (beta == 0)
          Y[Y_index] = 0.0;
        else
          Y[Y_index] *= beta;
      }
    }
  }) ;
}

static void gemv_alpha0_rowbatch(hc::accelerator_view accl_view, double *A,
                                 __int64_t aOffset, __int64_t A_batchOffset,
                                 double *X, __int64_t xOffset,
                                 __int64_t X_batchOffset, double *Y,
                                 __int64_t yOffset, __int64_t Y_batchOffset,
                                 double alpha, double beta, int lenX, int lenY,
                                 int batchSize) {
  __int64_t size = (lenY + 255) & ~255;
  hc::extent<2> compute_domain(batchSize, size);
  hc::parallel_for_each(accl_view, compute_domain.tile(1, BLOCK_SIZE), [=
  ](hc::tiled_index<2> tidx)[[hc]] {
    int elt = tidx.tile[0];
    int bx = tidx.tile[1];
    int tx = tidx.local[1];
    int Col = bx * BLOCK_SIZE + tx;
    if (Col < lenY) {
      __int64_t Y_index = yOffset + Y_batchOffset * elt + Col;
      Y[Y_index] = (hc::fast_math::isnan(static_cast<float>(Y[Y_index])) ||
                    hc::fast_math::isinf(static_cast<float>(Y[Y_index])))
                       ? 0
                       : Y[Y_index];
      if (alpha == 0) {
        if (beta == 0)
          Y[Y_index] = 0.0;
        else
          Y[Y_index] *= beta;
      }
    }
  }) ;
}

/* DGEMV - Type I : inputs and outputs are device pointers */
hcblasStatus Hcblaslibrary::hcblas_dgemv(
    hc::accelerator_view accl_view, hcblasOrder order, hcblasTranspose type,
    const int M, const int N, const double &alpha, double *A,
    const __int64_t aOffset, const int lda, double *X, const __int64_t xOffset,
    const int incX, const double &beta, double *Y, const __int64_t yOffset,
    const int incY) {
  /*Check the conditions*/
  if (X == NULL || Y == NULL || A == NULL || M <= 0 || N <= 0 || incX <= 0 ||
      incY <= 0) {
    return HCBLAS_INVALID;
  }

  int lenX, lenY;

  if (type == 'n') {
    lenX = 1 + (N - 1) * abs(incX);
    lenY = 1 + (M - 1) * abs(incY);
  } else {
    lenX = 1 + (M - 1) * abs(incX);
    lenY = 1 + (N - 1) * abs(incY);
  }

  if (alpha == 0) {
    if (order)
      gemv_alpha0_col(accl_view, A, aOffset, X, xOffset, Y, yOffset, alpha,
                      beta, lenX, lenY);
    else
      gemv_alpha0_row(accl_view, A, aOffset, X, xOffset, Y, yOffset, alpha,
                      beta, lenX, lenY);
    return HCBLAS_SUCCEEDS;
  }

  if (order) {
    if (type == 't') {
      gemv_TransA(accl_view, A, aOffset, X, xOffset, Y, yOffset, alpha, beta,
                  lenX, lenY);
    } else if (type == 'n') {
      gemv_NoTransA(accl_view, A, aOffset, X, xOffset, Y, yOffset, alpha, beta,
                    lenX, lenY);
    }
  } else {
    if (type == 't') {
      gemv_TransA_rMajor(accl_view, A, aOffset, X, xOffset, Y, yOffset, alpha,
                         beta, lenX, lenY);
    } else if (type == 'n') {
      gemv_NoTransA_rMajor(accl_view, A, aOffset, X, xOffset, Y, yOffset, alpha,
                           beta, lenX, lenY);
    }
  }

  return HCBLAS_SUCCEEDS;
}

/* DGEMV - Type II : Inputs and outputs are device pointers with batch
 * processing */
hcblasStatus Hcblaslibrary::hcblas_dgemv(
    hc::accelerator_view accl_view, hcblasOrder order, hcblasTranspose type,
    const int M, const int N, const double &alpha, double *A,
    const __int64_t aOffset, const __int64_t A_batchOffset, const int lda,
    double *X, const __int64_t xOffset, const __int64_t X_batchOffset,
    const int incX, const double &beta, double *Y, const __int64_t yOffset,
    const __int64_t Y_batchOffset, const int incY, const int batchSize) {
  /*Check the conditions*/
  if (X == NULL || Y == NULL || A == NULL || M <= 0 || N <= 0 || incX <= 0 ||
      incY <= 0) {
    return HCBLAS_INVALID;
  }

  int lenX, lenY;

  if (type == 'n') {
    lenX = 1 + (N - 1) * abs(incX);
    lenY = 1 + (M - 1) * abs(incY);
  } else {
    lenX = 1 + (M - 1) * abs(incX);
    lenY = 1 + (N - 1) * abs(incY);
  }

  if (alpha == 0) {
    if (order)
      gemv_alpha0_colbatch(accl_view, A, aOffset, A_batchOffset, X, xOffset,
                           X_batchOffset, Y, yOffset, Y_batchOffset, alpha,
                           beta, lenX, lenY, batchSize);
    else
      gemv_alpha0_rowbatch(accl_view, A, aOffset, A_batchOffset, X, xOffset,
                           X_batchOffset, Y, yOffset, Y_batchOffset, alpha,
                           beta, lenX, lenY, batchSize);
    return HCBLAS_SUCCEEDS;
  }

  if (order) {
    if (type == 't') {
      gemv_TransA(accl_view, A, aOffset, A_batchOffset, X, xOffset,
                  X_batchOffset, Y, yOffset, Y_batchOffset, alpha, beta, lenX,
                  lenY, batchSize);
    } else if (type == 'n') {
      gemv_NoTransA(accl_view, A, aOffset, A_batchOffset, X, xOffset,
                    X_batchOffset, Y, yOffset, Y_batchOffset, alpha, beta, lenX,
                    lenY, batchSize);
    }
  } else {
    if (type == 't') {
      gemv_TransA_rMajor(accl_view, A, aOffset, A_batchOffset, X, xOffset,
                         X_batchOffset, Y, yOffset, Y_batchOffset, alpha, beta,
                         lenX, lenY, batchSize);
    } else if (type == 'n') {
      gemv_NoTransA_rMajor(accl_view, A, aOffset, A_batchOffset, X, xOffset,
                           X_batchOffset, Y, yOffset, Y_batchOffset, alpha,
                           beta, lenX, lenY, batchSize);
    }
  }
  return HCBLAS_SUCCEEDS;
}
