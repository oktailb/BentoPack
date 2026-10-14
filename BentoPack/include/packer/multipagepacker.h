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

#ifndef MULTIPAGEPACKER_H
#define MULTIPAGEPACKER_H

#include <QImage>
#include <QList>
#include <QMap>
#include <QRect>
#include <QSize>
#include <QString>
#include <QPolygonF>
#include "packer/atlaspacker.h"
#include "packer/multiatlaspacker.h"
#include "bentopackcore_export.h"

class SpriteDocument;
struct SpriteAnimation;

/**
 * @brief Represents an individual packed page inside a multi-page atlas collection.
 */
struct BENTOPACK_CORE_EXPORT AtlasPage {
    int                             pageIndex = 0;
    QImage                          atlas;
    QSize                           dimensions;
    QList<int>                      frameIndices;       ///< Global frame indices assigned to this page
    QList<QRect>                    frameRects;         ///< 1:1 local rects corresponding to frameIndices
    QMap<MaterialMapType, QImage>   auxiliaryAtlases;   ///< M21 synchronized normal, emissive, specular textures
    float                           efficiency = 0.0f;  ///< Occupancy percentage (0.0% to 100.0%)

    bool hasAuxiliaryMap(MaterialMapType type) const {
        return auxiliaryAtlases.contains(type) && !auxiliaryAtlases.value(type).isNull();
    }
};

/**
 * @brief Represents the bundled result of a multi-page atlas packing operation (M16).
 */
struct BENTOPACK_CORE_EXPORT MultiPagePackResult {
    QList<AtlasPage>    pages;              ///< Generated atlas pages
    QList<int>          framePageIndices;   ///< Maps global frame index -> pageIndex (size = total input frames)
    QList<QRect>        frameRects;         ///< Maps global frame index -> local QRect on its respective page
    QList<int>          duplicateMapping;   ///< Deduplication canonical indices
    bool                success = false;
    float               overallEfficiency = 0.0f;

    int totalPages() const { return pages.size(); }
    bool isMultiPage() const { return pages.size() > 1; }

    int pageForFrame(int frameIndex) const {
        if (frameIndex >= 0 && frameIndex < framePageIndices.size()) {
            return framePageIndices[frameIndex];
        }
        return -1;
    }

    QRect rectForFrame(int frameIndex) const {
        if (frameIndex >= 0 && frameIndex < frameRects.size()) {
            return frameRects[frameIndex];
        }
        return QRect();
    }
};

/**
 * @brief Coordinates multi-page texture atlas packing and VRAM overflow spanning (M16).
 *
 * Implements semantic animation cohesion: frames belonging to the same animation cycle
 * are grouped together on the same page to prevent runtime GPU texture swaps and state thrashing.
 */
class BENTOPACK_CORE_EXPORT MultiPagePacker
{
public:
    /**
     * @brief Packs an entire SpriteDocument across multiple pages if required by max dimensions.
     *
     * @param doc The source SpriteDocument containing frames, animations, and optional auxiliary maps.
     * @param options Packing configuration (maxWidth, maxHeight, padding, heuristic, etc.).
     * @param exportMaterialMaps If true and document has auxiliary maps, generates synchronized M21 maps per page.
     * @param normalMapYFlip Invert green channel for DirectX/Unity/Unreal normal maps.
     * @return MultiPagePackResult with allocated pages and frame mappings.
     */
    static MultiPagePackResult pack(const SpriteDocument &doc,
                                    const AtlasPacker::PackOptions &options,
                                    bool exportMaterialMaps = false,
                                    bool normalMapYFlip = false);

    /**
     * @brief Lower-level multi-page packer accepting explicit frames, animations, and polygons.
     */
    static MultiPagePackResult packFrames(const QList<QImage> &frames,
                                          const QMap<QString, SpriteAnimation> &animations,
                                          const AtlasPacker::PackOptions &options,
                                          const QList<QPolygonF> &polygons = {});

    /**
     * @brief Estimates whether the given collection of frames can fit in a single page.
     */
    static bool canFitInSinglePage(const QList<QImage> &frames, const AtlasPacker::PackOptions &options);

private:
    struct FrameCluster {
        QString     name;
        QList<int>  frameIndices;
        long long   totalEstimatedArea = 0;
    };

    static QList<FrameCluster> buildClusters(int frameCount, const QMap<QString, SpriteAnimation> &animations);
};

#endif // MULTIPAGEPACKER_H
