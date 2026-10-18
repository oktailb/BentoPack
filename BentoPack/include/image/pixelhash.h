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

#ifndef PIXELHASH_H
#define PIXELHASH_H

#include <QImage>
#include <QtCore/qglobal.h>
#include "bentopackcore_export.h"

struct SpriteCel;

/**
 * @brief High-performance 64-bit XXH3 image, sprite, and cel hashing utility.
 *
 * Enables O(1) deduplication lookups for sprite frames, modular cels, and delta patches,
 * replacing O(N^2) pairwise pixel comparisons with hardware-accelerated 64-bit hashing.
 */
class BENTOPACK_CORE_EXPORT PixelHash
{
public:
    /**
     * @brief Computes a 64-bit XXH3-based hash of a QImage.
     * Takes dimensions, color format, and raw pixel scanlines into account.
     */
    static quint64 hashImage(const QImage &image);

    /**
     * @brief Computes a 64-bit XXH3-based hash of a SpriteCel.
     * Takes image content, layer ID, layer index, local position (x, y), and opacity into account.
     */
    static quint64 hashCel(const SpriteCel &cel);

    /**
     * @brief Fast visual image equality check.
     * Evaluates dimensions and 64-bit hashes before performing full scanline comparisons.
     */
    static bool areImagesIdentical(const QImage &a, const QImage &b);

    /**
     * @brief Fast visual cel equality check.
     * Evaluates layer index, layer ID, coordinates, opacity, and image contents.
     */
    static bool areCelsIdentical(const SpriteCel &a, const SpriteCel &b);

    /**
     * @brief Generic 64-bit XXH3 data hashing helper.
     */
    static quint64 hashData(const void *data, size_t length, quint64 seed = 0);
};

#endif // PIXELHASH_H
