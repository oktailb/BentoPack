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

#ifndef DELTAPATCHSLICER_H
#define DELTAPATCHSLICER_H

#include <QImage>
#include <QList>
#include <QRect>
#include <QPoint>
#include <QSize>
#include "bentopackcore_export.h"

/**
 * @brief Represents a differential patch extracted from a frame relative to a reference frame.
 */
struct BENTOPACK_CORE_EXPORT DeltaPatch {
    int     frameIndex = 0;
    int     referenceFrameIndex = -1; ///< -1 if keyframe (unaltered), >= 0 if delta relative to ref
    bool    isDelta = false;          ///< true if small delta patch (area <= threshold)
    bool    isDuplicate = false;      ///< true if 100% duplicate of reference
    QRect   dirtyRect;                ///< Bounding box of changed pixels relative to frame (0,0)
    QImage  patchImage;               ///< Cropped patch image (dirtyRect.size())
    QPoint  deltaOffset;              ///< (dirtyRect.x(), dirtyRect.y()) relative to frame top-left
    float   deltaAreaRatio = 1.0f;    ///< dirtyArea / totalFrameArea (0.0 to 1.0)
};

/**
 * @brief Bundled result of delta-patch slicing across an animation sequence.
 */
struct BENTOPACK_CORE_EXPORT DeltaSlicingResult {
    QList<DeltaPatch>   patches;
    int                 totalFrames = 0;
    int                 keyFramesCount = 0;
    int                 deltaFramesCount = 0;
    int                 duplicateFramesCount = 0;
    qint64              originalTotalArea = 0;
    qint64              packedPatchArea = 0;
    float               areaSavingsPercentage = 0.0f;
};

/**
 * @brief High-performance differential frame patch extractor .
 *
 * Compares animated frames against keyframes or preceding frames, detects the dirty bounding
 * box of modified pixels, and if the changed area is <= threshold (default 25%), isolates the
 * dirty patch to eliminate up to 75% of texture waste while preserving exact 1:1 reconstruction.
 */
class BENTOPACK_CORE_EXPORT DeltaPatchSlicer
{
public:
    /**
     * @brief Computes the delta patch between a current frame and a reference frame.
     * @param currentFrame The frame to be evaluated.
     * @param referenceFrame The reference base frame.
     * @param currentIdx Index of current frame.
     * @param refIdx Index of reference frame.
     * @param threshold Maximum area ratio (0.0 to 1.0) to qualify as a delta patch (default 0.25 = 25%).
     */
    static DeltaPatch computeDelta(const QImage &currentFrame,
                                   const QImage &referenceFrame,
                                   int currentIdx,
                                   int refIdx,
                                   float threshold = 0.25f);

    /**
     * @brief Slices an entire animation sequence into keyframes, delta-patches, and duplicates.
     * @param frames List of frames belonging to an animation.
     * @param threshold Maximum area ratio for delta-patching.
     * @param chainDeltas If true, each frame is compared to the previous frame; if false, compared to frame 0 (keyframe).
     */
    static DeltaSlicingResult sliceAnimation(const QList<QImage> &frames,
                                             float threshold = 0.25f,
                                             bool chainDeltas = false);

    /**
     * @brief Perfectly reconstructs the full frame from a reference frame and its delta patch.
     * Guaranteed byte-for-byte pixel fidelity.
     */
    static QImage reconstructFrame(const QImage &referenceFrame,
                                   const DeltaPatch &patch,
                                   const QSize &frameSize);
};

#endif // DELTAPATCHSLICER_H
