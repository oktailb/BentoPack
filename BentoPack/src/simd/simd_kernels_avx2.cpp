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

#if defined(BENTOPACK_HAS_AVX2)

#include <immintrin.h>
#include <QColor>
#include <algorithm>
#include <cmath>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC target("avx2")
#endif

int SimdKernels::colorSwapRow_avx2(const QRgb *src, QRgb *dst, int count,
                                   int sr, int sg, int sb,
                                   int maxDistSq, QRgb dstRgbBase,
                                   bool preserveShading,
                                   int sV, int sS, int dH, int dS, int dV)
{
    int modified = 0;
    int i = 0;

    // Target color vector (unpacked to 16-bit)
    // Little endian ARGB: Byte 0=B, 1=G, 2=R, 3=A
    const __m256i vTarget = _mm256_setr_epi16(sb, sg, sr, 0, sb, sg, sr, 0,
                                              sb, sg, sr, 0, sb, sg, sr, 0);
    const __m256i vRgbMask = _mm256_setr_epi16(-1, -1, -1, 0, -1, -1, -1, 0,
                                               -1, -1, -1, 0, -1, -1, -1, 0);
    const __m128i vMaxDistSqPlusOne = _mm_set1_epi32(maxDistSq + 1);
    const __m128i vAlphaMask = _mm_set1_epi32(static_cast<int>(0xFF000000));
    const __m128i vDstRgb = _mm_set1_epi32(static_cast<int>(dstRgbBase & 0x00FFFFFF));

    // Process in blocks of 4 pixels (16 bytes = 128 bits)
    for (; i + 4 <= count; i += 4) {
        __m128i pix128 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));

        // 1. Unpack 4x 8-bit ARGB pixels into 16-bit components in 256-bit register
        __m256i v = _mm256_cvtepu8_epi16(pix128);

        // 2. Subtract target RGB
        __m256i d = _mm256_and_si256(_mm256_sub_epi16(v, vTarget), vRgbMask);

        // 3. Multiply and pair-wise add: sq = (dB^2 + dG^2, dR^2, ...)
        __m256i sq = _mm256_madd_epi16(d, d);

        // 4. Horizontal add to get 4x 32-bit distSq
        __m128i dist128 = _mm_hadd_epi32(_mm256_castsi256_si128(sq), _mm256_extracti128_si256(sq, 1));

        // 5. Compare: distSq < maxDistSq + 1  <=>  distSq <= maxDistSq
        __m128i isWithin = _mm_cmpgt_epi32(vMaxDistSqPlusOne, dist128);

        // 6. Check non-zero alpha
        __m128i hasAlpha = _mm_xor_si128(_mm_cmpeq_epi32(_mm_and_si128(pix128, vAlphaMask), _mm_setzero_si128()), _mm_set1_epi32(-1));

        // 7. Complete match mask
        __m128i matchMask = _mm_and_si128(isWithin, hasAlpha);

        int mask = _mm_movemask_epi8(matchMask);
        if (mask == 0) {
            // No matching pixels in this 4-pixel chunk: copy as is
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), pix128);
            continue;
        }

        if (!preserveShading) {
            // Direct color swap: replace RGB with dstColor, keep original Alpha
            __m128i replacement = _mm_or_si128(_mm_and_si128(pix128, vAlphaMask), vDstRgb);
            __m128i result = _mm_blendv_epi8(pix128, replacement, matchMask);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), result);

            modified += (mask & 1 ? 1 : 0) + ((mask >> 4) & 1 ? 1 : 0) +
                        ((mask >> 8) & 1 ? 1 : 0) + ((mask >> 12) & 1 ? 1 : 0);
        } else {
            // Shading preservation: for matching pixels only, compute HSV modulation
            alignas(16) uint32_t pArr[4];
            _mm_storeu_si128(reinterpret_cast<__m128i*>(pArr), pix128);

            for (int k = 0; k < 4; ++k) {
                if ((mask >> (k * 4)) & 1) {
                    QRgb p = pArr[k];
                    int a = qAlpha(p);
                    QColor pCol(p);
                    int pV = pCol.value();
                    int pS = pCol.hsvSaturation();

                    int newV = (sV > 0) ? qBound(0, (pV * dV) / sV, 255) : dV;
                    int newS = (sS > 0) ? qBound(0, (pS * dS) / sS, 255) : dS;

                    QColor swapped;
                    swapped.setHsv(dH >= 0 ? dH : 0, newS, newV, a);
                    dst[i + k] = swapped.rgba();
                    modified++;
                } else {
                    dst[i + k] = pArr[k];
                }
            }
        }
    }

    // Scalar tail loop for remaining 0 to 3 pixels
    if (i < count) {
        modified += colorSwapRow_scalar(src + i, dst + i, count - i,
                                        sr, sg, sb, maxDistSq, dstRgbBase,
                                        preserveShading, sV, sS, dH, dS, dV);
    }

    return modified;
}

int SimdKernels::despillChromaRow_avx2(const QRgb *src, QRgb *dst, int count,
                                      bool isGreenFringe, bool isBlueFringe,
                                      double spillFactor)
{
    // If fractional spill factor, fallback to scalar to ensure identical floating-point rounding
    if (std::abs(spillFactor - 1.0) > 1e-4) {
        return despillChromaRow_scalar(src, dst, count, isGreenFringe, isBlueFringe, spillFactor);
    }

    int modified = 0;
    int i = 0;

    // Process in blocks of 8 pixels (32 bytes = 256 bits)
    for (; i + 8 <= count; i += 8) {
        __m256i vPix = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));

        // Little endian 32-bit ARGB: Byte 0=B, 1=G, 2=R, 3=A
        __m256i vB = _mm256_and_si256(vPix, _mm256_set1_epi32(0x000000FF));
        __m256i vG = _mm256_and_si256(_mm256_srli_epi32(vPix, 8), _mm256_set1_epi32(0x000000FF));
        __m256i vR = _mm256_and_si256(_mm256_srli_epi32(vPix, 16), _mm256_set1_epi32(0x000000FF));
        __m256i vA = _mm256_and_si256(vPix, _mm256_set1_epi32(static_cast<int>(0xFF000000)));

        __m256i hasAlpha = _mm256_xor_si256(_mm256_cmpeq_epi32(vA, _mm256_setzero_si256()), _mm256_set1_epi32(-1));

        __m256i vNewPix = vPix;
        __m256i changedMask = _mm256_setzero_si256();

        if (isGreenFringe) {
            __m256i maxOther = _mm256_max_epi32(vR, vB);
            __m256i isSpill = _mm256_cmpgt_epi32(vG, maxOther);
            changedMask = _mm256_and_si256(isSpill, hasAlpha);

            __m256i clampedG = maxOther;
            __m256i despilled = _mm256_or_si256(_mm256_or_si256(vB, _mm256_slli_epi32(clampedG, 8)),
                                                _mm256_or_si256(_mm256_slli_epi32(vR, 16), vA));
            vNewPix = _mm256_blendv_epi8(vPix, despilled, changedMask);
        } else if (isBlueFringe) {
            __m256i maxOther = _mm256_max_epi32(vR, vG);
            __m256i isSpill = _mm256_cmpgt_epi32(vB, maxOther);
            changedMask = _mm256_and_si256(isSpill, hasAlpha);

            __m256i clampedB = maxOther;
            __m256i despilled = _mm256_or_si256(_mm256_or_si256(clampedB, _mm256_slli_epi32(vG, 8)),
                                                _mm256_or_si256(_mm256_slli_epi32(vR, 16), vA));
            vNewPix = _mm256_blendv_epi8(vPix, despilled, changedMask);
        }

        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), vNewPix);

        int mask = _mm256_movemask_epi8(changedMask);
        if (mask != 0) {
            for (int k = 0; k < 8; ++k) {
                if ((mask >> (k * 4)) & 1) {
                    modified++;
                }
            }
        }
    }

    // Scalar tail loop
    if (i < count) {
        modified += despillChromaRow_scalar(src + i, dst + i, count - i,
                                            isGreenFringe, isBlueFringe, spillFactor);
    }

    return modified;
}

#endif // BENTOPACK_HAS_AVX2
