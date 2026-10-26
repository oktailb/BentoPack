/**
 * Copyright (c) 2026 Vincent LECOQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "simd/simd_kernels.h"

#if defined(BENTOPACK_HAS_NEON)

#if (defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON) || defined(__ARM_NEON__)) && !defined(__x86_64__) && !defined(_M_X64)
#  define BENTOPACK_NEON_INTRINSICS_AVAILABLE 1
#  include <arm_neon.h>
#endif

#include <QColor>
#include <algorithm>
#include <cmath>

int SimdKernels::colorSwapRow_neon(const QRgb *src, QRgb *dst, int count,
                                   int sr, int sg, int sb,
                                   int maxDistSq, QRgb dstRgbBase,
                                   bool preserveShading,
                                   int sV, int sS, int dH, int dS, int dV)
{
    return colorSwapRow_scalar(src, dst, count, sr, sg, sb, maxDistSq, dstRgbBase,
                               preserveShading, sV, sS, dH, dS, dV);
}

int SimdKernels::despillChromaRow_neon(const QRgb *src, QRgb *dst, int count,
                                      bool isGreenFringe, bool isBlueFringe,
                                      double spillFactor)
{
    return despillChromaRow_scalar(src, dst, count, isGreenFringe, isBlueFringe, spillFactor);
}

#endif // BENTOPACK_HAS_NEON
