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

#include "packer/deltapatchslicer.h"
#include "image/pixelhash.h"
#include <QPainter>

DeltaPatch DeltaPatchSlicer::computeDelta(const QImage &currentFrame,
                                         const QImage &referenceFrame,
                                         int currentIdx,
                                         int refIdx,
                                         float threshold)
{
    DeltaPatch patch;
    patch.frameIndex = currentIdx;
    patch.referenceFrameIndex = refIdx;

    if (currentFrame.isNull()) {
        return patch;
    }

    if (referenceFrame.isNull() || currentFrame.size() != referenceFrame.size()) {
        patch.isDelta = false;
        patch.isDuplicate = false;
        patch.dirtyRect = QRect(0, 0, currentFrame.width(), currentFrame.height());
        patch.patchImage = currentFrame;
        patch.deltaOffset = QPoint(0, 0);
        patch.deltaAreaRatio = 1.0f;
        return patch;
    }

    // Fast-path: Check if frames are visually identical
    if (PixelHash::areImagesIdentical(currentFrame, referenceFrame)) {
        patch.isDuplicate = true;
        patch.isDelta = false;
        patch.dirtyRect = QRect();
        patch.patchImage = QImage();
        patch.deltaOffset = QPoint(0, 0);
        patch.deltaAreaRatio = 0.0f;
        return patch;
    }

    const int w = currentFrame.width();
    const int h = currentFrame.height();
    const qint64 totalArea = static_cast<qint64>(w) * h;

    QImage curImg = (currentFrame.format() == QImage::Format_ARGB32) ? currentFrame : currentFrame.convertToFormat(QImage::Format_ARGB32);
    QImage refImg = (referenceFrame.format() == QImage::Format_ARGB32) ? referenceFrame : referenceFrame.convertToFormat(QImage::Format_ARGB32);

    int minX = w;
    int maxX = -1;
    int minY = h;
    int maxY = -1;

    for (int y = 0; y < h; ++y) {
        const QRgb *curLine = reinterpret_cast<const QRgb*>(curImg.constScanLine(y));
        const QRgb *refLine = reinterpret_cast<const QRgb*>(refImg.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            if (curLine[x] != refLine[x]) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        // No pixels actually changed
        patch.isDuplicate = true;
        patch.isDelta = false;
        patch.dirtyRect = QRect();
        patch.patchImage = QImage();
        patch.deltaOffset = QPoint(0, 0);
        patch.deltaAreaRatio = 0.0f;
        return patch;
    }

    const QRect dirtyRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
    const qint64 dirtyArea = static_cast<qint64>(dirtyRect.width()) * dirtyRect.height();
    const float areaRatio = (totalArea > 0) ? static_cast<float>(dirtyArea) / static_cast<float>(totalArea) : 1.0f;

    if (areaRatio <= threshold && dirtyArea < totalArea) {
        patch.isDelta = true;
        patch.isDuplicate = false;
        patch.dirtyRect = dirtyRect;
        patch.patchImage = currentFrame.copy(dirtyRect);
        patch.deltaOffset = dirtyRect.topLeft();
        patch.deltaAreaRatio = areaRatio;
    } else {
        patch.isDelta = false;
        patch.isDuplicate = false;
        patch.dirtyRect = QRect(0, 0, w, h);
        patch.patchImage = currentFrame;
        patch.deltaOffset = QPoint(0, 0);
        patch.deltaAreaRatio = 1.0f;
    }

    return patch;
}

DeltaSlicingResult DeltaPatchSlicer::sliceAnimation(const QList<QImage> &frames,
                                                    float threshold,
                                                    bool chainDeltas)
{
    DeltaSlicingResult result;
    result.totalFrames = frames.size();
    if (frames.isEmpty()) {
        return result;
    }

    result.patches.reserve(frames.size());

    for (int i = 0; i < frames.size(); ++i) {
        const QImage &cur = frames.at(i);
        const qint64 frameArea = static_cast<qint64>(cur.width()) * cur.height();
        result.originalTotalArea += frameArea;

        if (i == 0) {
            // First frame is always an anchor keyframe
            DeltaPatch keyframe;
            keyframe.frameIndex = 0;
            keyframe.referenceFrameIndex = -1;
            keyframe.isDelta = false;
            keyframe.isDuplicate = false;
            keyframe.dirtyRect = QRect(0, 0, cur.width(), cur.height());
            keyframe.patchImage = cur;
            keyframe.deltaOffset = QPoint(0, 0);
            keyframe.deltaAreaRatio = 1.0f;

            result.patches.append(keyframe);
            result.keyFramesCount++;
            result.packedPatchArea += frameArea;
        } else {
            int refIdx = chainDeltas ? (i - 1) : 0;
            const QImage &ref = frames.at(refIdx);

            DeltaPatch patch = computeDelta(cur, ref, i, refIdx, threshold);
            result.patches.append(patch);

            if (patch.isDuplicate) {
                result.duplicateFramesCount++;
                // 0 extra atlas area consumed
            } else if (patch.isDelta) {
                result.deltaFramesCount++;
                result.packedPatchArea += static_cast<qint64>(patch.dirtyRect.width()) * patch.dirtyRect.height();
            } else {
                result.keyFramesCount++;
                result.packedPatchArea += frameArea;
            }
        }
    }

    if (result.originalTotalArea > 0) {
        qint64 saved = result.originalTotalArea - result.packedPatchArea;
        result.areaSavingsPercentage = (static_cast<float>(saved) / static_cast<float>(result.originalTotalArea)) * 100.0f;
    }

    return result;
}

QImage DeltaPatchSlicer::reconstructFrame(const QImage &referenceFrame,
                                         const DeltaPatch &patch,
                                         const QSize &frameSize)
{
    if (patch.isDuplicate) {
        return referenceFrame;
    }

    if (!patch.isDelta) {
        return patch.patchImage;
    }

    QSize targetSize = frameSize.isValid() ? frameSize : referenceFrame.size();
    if (!targetSize.isValid()) {
        targetSize = patch.dirtyRect.size();
    }

    QImage reconstructed(targetSize, QImage::Format_ARGB32_Premultiplied);
    reconstructed.fill(Qt::transparent);

    {
        QPainter p(&reconstructed);
        if (!referenceFrame.isNull()) {
            p.drawImage(0, 0, referenceFrame);
        }
        // CompositionMode_Source replaces existing pixels inside the patch rect,
        // correctly writing alpha 0 if pixels were deleted or became transparent.
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(patch.deltaOffset, patch.patchImage);
    }

    return reconstructed.convertToFormat(QImage::Format_ARGB32);
}
