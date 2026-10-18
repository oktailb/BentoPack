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

#ifndef CELDEDUPLICATOR_H
#define CELDEDUPLICATOR_H

#include <QImage>
#include <QList>
#include <QMap>
#include <QPair>
#include <QRect>
#include "model/spritedocument.h"
#include "packer/atlaspacker.h"
#include "bentopackcore_export.h"

/**
 * @brief Information about a unique deduplicated cel.
 */
struct BENTOPACK_CORE_EXPORT UniqueCelInfo {
    int                     uniqueId = 0;
    SpriteCel               cel;
    quint64                 hash = 0;
    QList<QPair<int, int>>  occurrences; // (frameIndex, layerIndex)
};

/**
 * @brief Analysis result of cel deduplication across an animation sequence.
 */
struct BENTOPACK_CORE_EXPORT CelDeduplicationResult {
    QList<UniqueCelInfo>        uniqueCels;
    QMap<QPair<int, int>, int>  celMapping; // (frameIndex, layerIndex) -> uniqueCelId
    int                         totalCelsCount = 0;
    int                         uniqueCelsCount = 0;
    qint64                      totalPixelArea = 0;
    qint64                      uniquePixelArea = 0;
    float                       savingsPercentage = 0.0f;
};

/**
 * @brief High-performance cel deduplication engine for modular sprite architectures .
 *
 * Scans multi-layer frames (Aseprite or project cels), hashes them using XXH3-64,
 * and ensures that repeated or linked cels across animation frames are placed strictly
 * ONCE in the texture atlas, reducing VRAM footprint and overdraw while preserving UV mapping.
 */
class BENTOPACK_CORE_EXPORT CelDeduplicator
{
public:
    /**
     * @brief Analyzes all cels across all frames in the document and identifies identical cels.
     * Takes layer index, position, opacity, and pixel data into account.
     */
    static CelDeduplicationResult analyze(const SpriteDocument &doc);

    /**
     * @brief Packs only the unique cels into an atlas using MaxRects or TightPolygon.
     * Maps each unique cel to an atlas rectangle, and returns the result.
     */
    static AtlasPackResult packModularCels(const SpriteDocument &doc,
                                          const AtlasPacker::PackOptions &options,
                                          CelDeduplicationResult *outDedupResult = nullptr);
};

#endif // CELDEDUPLICATOR_H
