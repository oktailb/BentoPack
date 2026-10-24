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
#include "simd/cpu_features.h"
#include <QColor>
#include <algorithm>
#include <cmath>

int SimdKernels::colorSwapRow(const QRgb *src, QRgb *dst, int count,
                              int sr, int sg, int sb,
                              int maxDistSq, QRgb dstRgbBase,
                              bool preserveShading,
                              int sV, int sS, int dH, int dS, int dV)
{
#if defined(BENTOPACK_HAS_AVX2)
    if (CpuFeatures::hasAvx2()) {
        return colorSwapRow_avx2(src, dst, count, sr, sg, sb, maxDistSq, dstRgbBase,
                                 preserveShading, sV, sS, dH, dS, dV);
    }
#endif
#if defined(BENTOPACK_HAS_NEON)
    if (CpuFeatures::hasNeon()) {
        return colorSwapRow_neon(src, dst, count, sr, sg, sb, maxDistSq, dstRgbBase,
                                 preserveShading, sV, sS, dH, dS, dV);
    }
#endif
    return colorSwapRow_scalar(src, dst, count, sr, sg, sb, maxDistSq, dstRgbBase,
                               preserveShading, sV, sS, dH, dS, dV);
}

int SimdKernels::despillChromaRow(const QRgb *src, QRgb *dst, int count,
                                  bool isGreenFringe, bool isBlueFringe,
                                  double spillFactor)
{
#if defined(BENTOPACK_HAS_AVX2)
    if (CpuFeatures::hasAvx2()) {
        return despillChromaRow_avx2(src, dst, count, isGreenFringe, isBlueFringe, spillFactor);
    }
#endif
#if defined(BENTOPACK_HAS_NEON)
    if (CpuFeatures::hasNeon()) {
        return despillChromaRow_neon(src, dst, count, isGreenFringe, isBlueFringe, spillFactor);
    }
#endif
    return despillChromaRow_scalar(src, dst, count, isGreenFringe, isBlueFringe, spillFactor);
}

int SimdKernels::colorSwapRow_scalar(const QRgb *src, QRgb *dst, int count,
                                     int sr, int sg, int sb,
                                     int maxDistSq, QRgb dstRgbBase,
                                     bool preserveShading,
                                     int sV, int sS, int dH, int dS, int dV)
{
    int modified = 0;
    for (int i = 0; i < count; ++i) {
        const QRgb p = src[i];
        const int a = qAlpha(p);
        if (a == 0) continue;

        const int dR = qRed(p) - sr;
        const int dG = qGreen(p) - sg;
        const int dB = qBlue(p) - sb;
        const int distSq = dR * dR + dG * dG + dB * dB;

        if (distSq <= maxDistSq) {
            if (preserveShading) {
                QColor pCol(p);
                int pV = pCol.value();
                int pS = pCol.hsvSaturation();

                int newV = (sV > 0) ? qBound(0, (pV * dV) / sV, 255) : dV;
                int newS = (sS > 0) ? qBound(0, (pS * dS) / sS, 255) : dS;

                QColor swapped;
                swapped.setHsv(dH >= 0 ? dH : 0, newS, newV, a);
                dst[i] = swapped.rgba();
            } else {
                dst[i] = (static_cast<uint>(a) << 24) | (dstRgbBase & 0x00FFFFFF);
            }
            modified++;
        }
    }
    return modified;
}

int SimdKernels::despillChromaRow_scalar(const QRgb *src, QRgb *dst, int count,
                                         bool isGreenFringe, bool isBlueFringe,
                                         double spillFactor)
{
    int modified = 0;
    for (int i = 0; i < count; ++i) {
        const QRgb p = src[i];
        const int a = qAlpha(p);
        if (a == 0) continue;

        const int pR = qRed(p);
        const int pG = qGreen(p);
        const int pB = qBlue(p);

        if (isGreenFringe) {
            const int maxOther = std::max(pR, pB);
            if (pG > maxOther) {
                int newG = static_cast<int>(std::round(pG * (1.0 - spillFactor) + maxOther * spillFactor));
                dst[i] = qRgba(pR, qBound(0, newG, 255), pB, a);
                modified++;
            }
        } else if (isBlueFringe) {
            const int maxOther = std::max(pR, pG);
            if (pB > maxOther) {
                int newB = static_cast<int>(std::round(pB * (1.0 - spillFactor) + maxOther * spillFactor));
                dst[i] = qRgba(pR, pG, qBound(0, newB, 255), a);
                modified++;
            }
        }
    }
    return modified;
}
