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

#ifndef SIMD_KERNELS_H
#define SIMD_KERNELS_H

#include <qrgb.h>
#include "bentopackcore_export.h"

/**
 * @brief High-throughput vector kernels (AVX2 / ARM NEON / Scalar fallback)
 * for real-time pixel processing across high-resolution (4K / 8K / 16K) atlases.
 */
class BENTOPACK_CORE_EXPORT SimdKernels
{
public:
    /**
     * @brief Dispatches ColorSwap across a scanline slice using the best available SIMD instruction set.
     */
    static int colorSwapRow(const QRgb *src, QRgb *dst, int count,
                            int sr, int sg, int sb,
                            int maxDistSq, QRgb dstRgbBase,
                            bool preserveShading,
                            int sV, int sS, int dH, int dS, int dV);

    /**
     * @brief Dispatches Chroma Spill suppression across a scanline slice using the best available SIMD instruction set.
     */
    static int despillChromaRow(const QRgb *src, QRgb *dst, int count,
                                bool isGreenFringe, bool isBlueFringe,
                                double spillFactor);

    // Direct architecture-specific implementations (exposed for unit testing and profiling)
    static int colorSwapRow_scalar(const QRgb *src, QRgb *dst, int count,
                                   int sr, int sg, int sb,
                                   int maxDistSq, QRgb dstRgbBase,
                                   bool preserveShading,
                                   int sV, int sS, int dH, int dS, int dV);

    static int despillChromaRow_scalar(const QRgb *src, QRgb *dst, int count,
                                       bool isGreenFringe, bool isBlueFringe,
                                       double spillFactor);

#if defined(BENTOPACK_HAS_AVX2)
    static int colorSwapRow_avx2(const QRgb *src, QRgb *dst, int count,
                                 int sr, int sg, int sb,
                                 int maxDistSq, QRgb dstRgbBase,
                                 bool preserveShading,
                                 int sV, int sS, int dH, int dS, int dV);

    static int despillChromaRow_avx2(const QRgb *src, QRgb *dst, int count,
                                     bool isGreenFringe, bool isBlueFringe,
                                     double spillFactor);
#endif

#if defined(BENTOPACK_HAS_NEON)
    static int colorSwapRow_neon(const QRgb *src, QRgb *dst, int count,
                                 int sr, int sg, int sb,
                                 int maxDistSq, QRgb dstRgbBase,
                                 bool preserveShading,
                                 int sV, int sS, int dH, int dS, int dV);

    static int despillChromaRow_neon(const QRgb *src, QRgb *dst, int count,
                                     bool isGreenFringe, bool isBlueFringe,
                                     double spillFactor);
#endif
};

#endif // SIMD_KERNELS_H
