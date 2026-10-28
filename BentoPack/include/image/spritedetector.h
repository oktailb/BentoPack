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

#ifndef SPRITEDETECTOR_H
#define SPRITEDETECTOR_H

#include <QImage>
#include <QList>
#include <QRect>
#include <functional>
#include "model/spritedocument.h"

#include "bentopackcore_export.h"

/**
 * @brief Configuration parameters for sprite detection and automatic bounding box slicing.
 */
struct BENTOPACK_CORE_EXPORT SpriteDetectionOptions
{
    int alphaThreshold = 1;
    int verticalTolerance = 0;
    int minSliceSize = 3;
    bool smartCrop = false;
    double overlapThreshold = 0.5;

    // Advanced constraints (Retro hardware, non-overlapping, size limits)
    int maxWidth = 0;               // 0 = unlimited, >0 = maximum sprite box width (e.g. 32)
    int maxHeight = 0;              // 0 = unlimited, >0 = maximum sprite box height (e.g. 32)
    int maxDimensionPermissive = 0; // Allow one dimension up to this value if other is within limit (e.g. 48 or 64)
    bool disallowOverlaps = false;  // Disallow overlapping bounding boxes: merge or split
    bool alignToGrid = false;       // Snap bounding box boundaries to tile grid
    int gridCellSize = 8;           // 8 or 16 px tile grid
};

/**
 * @brief High-performance image segmentation engine for detecting and slicing sprites.
 *
 * Uses direct scanline memory access, 1D flat indexing, and connected components flood-fill.
 * Separated from codecs to adhere to the Single Responsibility Principle.
 */
class BENTOPACK_CORE_EXPORT SpriteDetector
{
public:
    /**
     * @brief Segments an image into sub-images and bounding boxes.
     */
    static bool detectToImages(const QImage &sourceImage,
                               QList<QImage> &outFrames,
                               QList<SpriteBox> &outBoxes,
                               const SpriteDetectionOptions &options = SpriteDetectionOptions(),
                               std::function<void(int)> progressCallback = nullptr);

    /**
     * @brief Segments an image and returns only the bounding boxes.
     */
    static bool detectBoxes(const QImage &sourceImage,
                            QList<SpriteBox> &outBoxes,
                            const SpriteDetectionOptions &options = SpriteDetectionOptions(),
                            std::function<void(int)> progressCallback = nullptr);
};

#endif // SPRITEDETECTOR_H
