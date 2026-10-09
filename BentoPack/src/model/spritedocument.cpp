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

#include "model/spritedocument.h"
#include "geometry/polygonmerger.h"
#include <QFileInfo>
#include <QPainter>
#include <algorithm>

SpriteDocument::SpriteDocument(QObject *parent)
    : QObject(parent)
{
}

void SpriteDocument::clear()
{
    m_atlas = QImage();
    m_frames.clear();
    m_boxes.clear();
    m_selectedFrameIndices.clear();
    m_animations.clear();
    m_layers.clear();
    m_frameCels.clear();
    m_skinProfiles.clear();
    m_auxiliaryFrames.clear();
    m_filePath.clear();
    m_maxFrameWidth = 0;
    m_maxFrameHeight = 0;

    emit documentReset();
}

QString SpriteDocument::projectName() const
{
    if (m_filePath.isEmpty()) {
        return QStringLiteral("untitled");
    }
    QString clean = m_filePath;
    clean.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QFileInfo(clean).completeBaseName();
}

void SpriteDocument::setAtlas(const QImage &image)
{
    m_atlas = image;
    emit atlasChanged();
}

void SpriteDocument::patchAtlas(const QRect &rect, const QImage &patch)
{
    if (m_atlas.isNull() || rect.isEmpty() || patch.isNull()) return;

    if (m_atlas.format() != QImage::Format_ARGB32) {
        m_atlas = m_atlas.convertToFormat(QImage::Format_ARGB32);
    }

    // Determine target box and check if other boxes overlap rect (tight polygon packing / nested atlas)
    int targetBoxIndex = -1;
    for (int i = 0; i < m_boxes.size(); ++i) {
        if (m_boxes[i].rect == rect) {
            targetBoxIndex = i;
            break;
        }
    }

    QList<int> overlappingIndices;
    for (int i = 0; i < m_boxes.size(); ++i) {
        if (i != targetBoxIndex && !m_boxes[i].rect.isEmpty() && m_boxes[i].rect.intersects(rect)) {
            overlappingIndices.append(i);
        }
    }

    if (overlappingIndices.isEmpty()) {
        // Standard non-overlapping box: direct fast patch
        QPainter p(&m_atlas);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(rect.topLeft(), patch);
        p.end();
    } else {
        // Overlapping bounding boxes:
        // 1. Clip target image to its polygon if present so transparent bounding box margins don't clobber neighbor sprites
        QImage targetImg = patch;
        if (targetBoxIndex >= 0 && targetBoxIndex < m_boxes.size()) {
            const SpriteBox &targetBox = m_boxes[targetBoxIndex];
            if (targetBox.hasPolygonMesh && targetBox.polygon.size() >= 3) {
                QImage mask(targetImg.size(), QImage::Format_ARGB32_Premultiplied);
                mask.fill(Qt::transparent);
                {
                    QPainter mp(&mask);
                    mp.setRenderHint(QPainter::Antialiasing, false);
                    mp.setBrush(Qt::white);
                    mp.setPen(QPen(Qt::white, 1.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
                    mp.drawPolygon(targetBox.polygon);
                }
                QImage cleanTarget = targetImg.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                {
                    QPainter p(&cleanTarget);
                    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                    p.drawImage(0, 0, mask);
                }
                targetImg = cleanTarget;
            }
        }

        // 2. Clear rect on atlas
        {
            QPainter p(&m_atlas);
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            p.fillRect(rect.intersected(m_atlas.rect()), Qt::transparent);
        }

        // 3. Redraw overlapping neighbor sprites and target sprite with SourceOver
        {
            QPainter p(&m_atlas);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);

            // Redraw overlapping neighbor sprites
            for (int ovIdx : overlappingIndices) {
                if (ovIdx >= 0 && ovIdx < m_frames.size()) {
                    QImage ovImg = polygonClippedFrame(ovIdx);
                    if (!ovImg.isNull()) {
                        p.drawImage(m_boxes[ovIdx].rect.topLeft(), ovImg);
                    }
                }
            }

            // Draw target sprite
            p.drawImage(rect.topLeft(), targetImg);
            p.end();
        }
    }

    emit atlasRegionChanged(rect);
    emit atlasChanged();
}

void SpriteDocument::clearAtlasRegion(const QRect &rect)
{
    if (m_atlas.isNull() || rect.isEmpty()) return;

    if (m_atlas.format() != QImage::Format_ARGB32) {
        m_atlas = m_atlas.convertToFormat(QImage::Format_ARGB32);
    }

    QPainter p(&m_atlas);
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillRect(rect.intersected(m_atlas.rect()), Qt::transparent);

    // If other boxes intersect rect, restore their visible pixels
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    for (int i = 0; i < m_boxes.size(); ++i) {
        if (m_boxes[i].rect != rect && !m_boxes[i].rect.isEmpty() && m_boxes[i].rect.intersects(rect)) {
            QImage img = polygonClippedFrame(i);
            if (!img.isNull()) {
                p.drawImage(m_boxes[i].rect.topLeft(), img);
            }
        }
    }
    p.end();

    emit atlasRegionChanged(rect);
    emit atlasChanged();
}

void SpriteDocument::recompositeAtlas()
{
    if (m_atlas.isNull() || m_frames.isEmpty()) return;

    QImage newAtlas(m_atlas.size(), QImage::Format_ARGB32_Premultiplied);
    newAtlas.fill(Qt::transparent);

    QPainter p(&newAtlas);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (int i = 0; i < m_frames.size(); ++i) {
        if (i < m_boxes.size() && !m_boxes[i].rect.isEmpty()) {
            QImage img = polygonClippedFrame(i);
            if (!img.isNull()) {
                p.drawImage(m_boxes[i].rect.topLeft(), img);
            }
        }
    }
    p.end();

    m_atlas = newAtlas.convertToFormat(QImage::Format_ARGB32);
    emit atlasChanged();
}

QImage SpriteDocument::frame(int index) const
{
    if (index >= 0 && index < m_frames.size()) {
        return m_frames.at(index);
    }
    return QImage();
}

QImage SpriteDocument::polygonClippedFrame(int index) const
{
    if (index < 0 || index >= m_frames.size()) {
        return QImage();
    }

    const QImage &baseImage = m_frames.at(index);
    if (baseImage.isNull() || index >= m_boxes.size()) {
        return baseImage;
    }

    const SpriteBox &b = m_boxes.at(index);
    if (b.polygon.size() < 3) {
        return baseImage;
    }

    // Build transparent mask and render the polygon
    QImage mask(baseImage.size(), QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter mp(&mask);
        mp.setRenderHint(QPainter::Antialiasing, false);
        mp.setBrush(Qt::white);
        mp.setPen(QPen(Qt::white, 1.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        mp.drawPolygon(b.polygon);
    }

    // Clip baseImage to mask using DestinationIn
    QImage clipped = baseImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    {
        QPainter p(&clipped);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.drawImage(0, 0, mask);
    }

    return clipped;
}

QList<QImage> SpriteDocument::polygonClippedFrames() const
{
    QList<QImage> result;
    result.reserve(m_frames.size());
    for (int i = 0; i < m_frames.size(); ++i) {
        if (i < m_boxes.size() && m_boxes[i].hasPolygonMesh && m_boxes[i].polygon.size() >= 3) {
            result.append(polygonClippedFrame(i));
        } else {
            result.append(m_frames.at(i));
        }
    }
    return result;
}

void SpriteDocument::setFrames(const QList<QImage> &frames, const QList<SpriteBox> &boxes)
{
    m_frames = frames;
    m_boxes = boxes;
    m_selectedFrameIndices.clear();
    for (int i = 0; i < m_boxes.size(); ++i) {
        if (m_boxes[i].selected) {
            m_selectedFrameIndices.append(i);
        }
    }
    recalculateMaxFrameDimensions();
    emit framesChanged();
}

void SpriteDocument::addFrame(const QImage &image, const SpriteBox &box)
{
    if (!image.isNull()) {
        m_frames.append(image);
        m_boxes.append(box);
        if (image.width() > m_maxFrameWidth) m_maxFrameWidth = image.width();
        if (image.height() > m_maxFrameHeight) m_maxFrameHeight = image.height();
        emit framesChanged();
    }
}

void SpriteDocument::insertFrame(int index, const QImage &image, const SpriteBox &box)
{
    if (index < 0 || index > m_frames.size() || image.isNull()) return;

    m_frames.insert(index, image);
    m_boxes.insert(index, box);

    // Shift frame indices in animations that are >= index
    for (auto it = m_animations.begin(); it != m_animations.end(); ++it) {
        QList<int> &indices = it.value().frameIndices;
        for (int i = 0; i < indices.size(); ++i) {
            if (indices[i] >= index) {
                indices[i]++;
            }
        }
    }

    for (int i = 0; i < m_selectedFrameIndices.size(); ++i) {
        if (m_selectedFrameIndices[i] >= index) {
            m_selectedFrameIndices[i]++;
        }
    }
    if (box.selected) {
        m_selectedFrameIndices.append(index);
    }

    if (!m_frameCels.isEmpty()) {
        QMap<int, QList<SpriteCel>> shiftedCels;
        for (auto it = m_frameCels.begin(); it != m_frameCels.end(); ++it) {
            if (it.key() >= index) {
                shiftedCels[it.key() + 1] = it.value();
            } else {
                shiftedCels[it.key()] = it.value();
            }
        }
        m_frameCels = shiftedCels;
    }

    recalculateMaxFrameDimensions();
    emit framesChanged();
    emit animationsChanged();
}

void SpriteDocument::replaceFrame(int index, const QImage &image, const SpriteBox &box)
{
    if (index < 0 || index >= m_frames.size()) return;

    m_frames[index] = image;
    if (index < m_boxes.size() && !box.rect.isNull()) {
        m_boxes[index] = box;
    }

    recalculateMaxFrameDimensions();
    emit framesChanged();
}

void SpriteDocument::removeFrame(int index)
{
    if (index < 0 || index >= m_frames.size()) return;

    m_frames.removeAt(index);
    if (index < m_boxes.size()) {
        m_boxes.removeAt(index);
    }

    if (!m_frameCels.isEmpty()) {
        m_frameCels.remove(index);
        QMap<int, QList<SpriteCel>> shiftedCels;
        for (auto it = m_frameCels.begin(); it != m_frameCels.end(); ++it) {
            if (it.key() > index) {
                shiftedCels[it.key() - 1] = it.value();
            } else {
                shiftedCels[it.key()] = it.value();
            }
        }
        m_frameCels = shiftedCels;
    }

    // Update animations: remove referencing frames and shift indices down
    for (auto it = m_animations.begin(); it != m_animations.end(); ++it) {
        QList<int> updated;
        for (int frameIdx : it.value().frameIndices) {
            if (frameIdx == index) {
                continue; // Supprimé
            } else if (frameIdx > index) {
                updated.append(frameIdx - 1);
            } else {
                updated.append(frameIdx);
            }
        }
        it.value().frameIndices = updated;
    }

    QList<int> updatedSel;
    for (int idx : m_selectedFrameIndices) {
        if (idx == index) {
            continue;
        } else if (idx > index) {
            updatedSel.append(idx - 1);
        } else {
            updatedSel.append(idx);
        }
    }
    m_selectedFrameIndices = updatedSel;

    recalculateMaxFrameDimensions();
    emit framesChanged();
    emit animationsChanged();
}

void SpriteDocument::removeFrames(const QList<int> &indices)
{
    if (indices.isEmpty()) return;

    QList<int> sortedIndices = indices;
    std::sort(sortedIndices.begin(), sortedIndices.end());
    sortedIndices.erase(std::unique(sortedIndices.begin(), sortedIndices.end()), sortedIndices.end());

    for (int i = sortedIndices.size() - 1; i >= 0; --i) {
        int idx = sortedIndices[i];
        if (idx >= 0 && idx < m_frames.size()) {
            m_frames.removeAt(idx);
            if (idx < m_boxes.size()) {
                m_boxes.removeAt(idx);
            }
        }
    }

    if (!m_frameCels.isEmpty()) {
        for (int idx : sortedIndices) {
            m_frameCels.remove(idx);
        }
        QMap<int, QList<SpriteCel>> shiftedCels;
        for (auto it = m_frameCels.begin(); it != m_frameCels.end(); ++it) {
            int frameIdx = it.key();
            int shift = 0;
            for (int removedIdx : sortedIndices) {
                if (removedIdx < frameIdx) {
                    shift++;
                }
            }
            shiftedCels[frameIdx - shift] = it.value();
        }
        m_frameCels = shiftedCels;
    }

    // Update animations
    for (auto it = m_animations.begin(); it != m_animations.end(); ++it) {
        QList<int> updated;
        for (int frameIdx : it.value().frameIndices) {
            if (sortedIndices.contains(frameIdx)) {
                continue;
            }
            int shift = 0;
            for (int removedIdx : sortedIndices) {
                if (removedIdx < frameIdx) {
                    shift++;
                }
            }
            updated.append(frameIdx - shift);
        }
        it.value().frameIndices = updated;
    }

    QList<int> updatedSel;
    for (int frameIdx : m_selectedFrameIndices) {
        if (sortedIndices.contains(frameIdx)) {
            continue;
        }
        int shift = 0;
        for (int removedIdx : sortedIndices) {
            if (removedIdx < frameIdx) {
                shift++;
            }
        }
        updatedSel.append(frameIdx - shift);
    }
    m_selectedFrameIndices = updatedSel;

    recalculateMaxFrameDimensions();
    emit framesChanged();
    emit animationsChanged();
}

void SpriteDocument::reorderFrames(const QList<int> &newOrder)
{
    if (newOrder.size() != m_frames.size()) return;

    QList<QImage> reorderedFrames;
    QList<SpriteBox> reorderedBoxes;

    for (int idx : newOrder) {
        if (idx >= 0 && idx < m_frames.size()) {
            reorderedFrames.append(m_frames[idx]);
            reorderedBoxes.append(idx < m_boxes.size() ? m_boxes[idx] : SpriteBox());
        }
    }

    m_frames = reorderedFrames;
    m_boxes = reorderedBoxes;

    // Create a mapping from old index -> new index
    QMap<int, int> oldToNew;
    for (int newPos = 0; newPos < newOrder.size(); ++newPos) {
        oldToNew[newOrder[newPos]] = newPos;
    }

    if (!m_frameCels.isEmpty()) {
        QMap<int, QList<SpriteCel>> reorderedCels;
        for (auto it = m_frameCels.begin(); it != m_frameCels.end(); ++it) {
            if (oldToNew.contains(it.key())) {
                reorderedCels[oldToNew.value(it.key())] = it.value();
            }
        }
        m_frameCels = reorderedCels;
    }

    for (auto it = m_animations.begin(); it != m_animations.end(); ++it) {
        QList<int> updated;
        for (int oldIdx : it.value().frameIndices) {
            if (oldToNew.contains(oldIdx)) {
                updated.append(oldToNew[oldIdx]);
            }
        }
        it.value().frameIndices = updated;
    }

    QList<int> updatedSel;
    for (int oldIdx : m_selectedFrameIndices) {
        if (oldToNew.contains(oldIdx)) {
            updatedSel.append(oldToNew[oldIdx]);
        }
    }
    m_selectedFrameIndices = updatedSel;

    emit framesChanged();
    emit animationsChanged();
}

void SpriteDocument::mergeFrames(int sourceIndex, int targetIndex)
{
    if (sourceIndex < 0 || sourceIndex >= m_frames.size() ||
        targetIndex < 0 || targetIndex >= m_frames.size() ||
        sourceIndex == targetIndex) {
        return;
    }

    SpriteBox srcBox = m_boxes.value(sourceIndex);
    SpriteBox tgtBox = m_boxes.value(targetIndex);

    QRect unitedRect = srcBox.rect.united(tgtBox.rect);
    QImage mergedImage;

    if (!m_atlas.isNull() && unitedRect.isValid()) {
        mergedImage = m_atlas.copy(unitedRect);
    } else {
        // Fallback: composite both frames at their relative positions in unitedRect
        if (unitedRect.isValid()) {
            QImage composite(unitedRect.size(), QImage::Format_ARGB32_Premultiplied);
            composite.fill(Qt::transparent);
            QPainter p(&composite);
            QPoint tgtPos = tgtBox.rect.topLeft() - unitedRect.topLeft();
            QPoint srcPos = srcBox.rect.topLeft() - unitedRect.topLeft();
            p.drawImage(tgtPos, m_frames[targetIndex]);
            p.drawImage(srcPos, m_frames[sourceIndex]);
            p.end();
            mergedImage = composite;
        } else {
            QSize combinedSize = m_frames[targetIndex].size().expandedTo(m_frames[sourceIndex].size());
            QImage composite(combinedSize, QImage::Format_ARGB32_Premultiplied);
            composite.fill(Qt::transparent);
            QPainter p(&composite);
            p.drawImage(0, 0, m_frames[targetIndex]);
            p.drawImage(0, 0, m_frames[sourceIndex]);
            p.end();
            mergedImage = composite;
        }
    }

    SpriteBox newBox = BentoPackGeometry::PolygonMerger::mergeSpriteBoxes(
        srcBox, tgtBox, m_frames[sourceIndex], m_frames[targetIndex]);

    m_boxes[targetIndex] = newBox;
    m_frames[targetIndex] = mergedImage;

    removeFrame(sourceIndex);
}

SpriteBox SpriteDocument::box(int index) const
{
    if (index >= 0 && index < m_boxes.size()) {
        return m_boxes.at(index);
    }
    return SpriteBox();
}

void SpriteDocument::setBox(int index, const SpriteBox &box)
{
    if (index >= 0 && index < m_boxes.size()) {
        m_boxes[index] = box;
        emit frameUpdated(index);
    }
}

void SpriteDocument::updateBoxRect(int index, const QRect &newRect)
{
    if (index < 0 || index >= m_boxes.size() || m_atlas.isNull()) return;

    QRect clampedRect = newRect.intersected(m_atlas.rect());
    if (clampedRect.width() <= 0 || clampedRect.height() <= 0) return;

    m_boxes[index].rect = clampedRect;
    if (index < m_frames.size()) {
        m_frames[index] = m_atlas.copy(clampedRect);
    }
    recalculateMaxFrameDimensions();
    emit frameUpdated(index);
}

int SpriteDocument::addSlice(const QRect &rect)
{
    if (m_atlas.isNull()) return -1;

    QRect clampedRect = rect.intersected(m_atlas.rect());
    if (clampedRect.width() <= 0 || clampedRect.height() <= 0) return -1;

    int newIndex = m_frames.size();
    SpriteBox newBox;
    newBox.rect = clampedRect;
    newBox.index = newIndex;
    newBox.selected = true;

    QImage frameImage = m_atlas.copy(clampedRect);
    m_frames.append(frameImage);
    m_boxes.append(newBox);
    m_selectedFrameIndices.append(newIndex);

    recalculateMaxFrameDimensions();
    emit framesChanged();
    return newIndex;
}

QRect SpriteDocument::computeTrimmedRect(int index, int alphaThreshold) const
{
    if (index < 0 || index >= m_boxes.size() || m_atlas.isNull()) return QRect();

    QRect boxRect = m_boxes[index].rect.intersected(m_atlas.rect());
    if (boxRect.isEmpty()) return QRect();

    int minX = boxRect.right() + 1;
    int maxX = boxRect.left() - 1;
    int minY = boxRect.bottom() + 1;
    int maxY = boxRect.top() - 1;

    for (int y = boxRect.top(); y <= boxRect.bottom(); ++y) {
        for (int x = boxRect.left(); x <= boxRect.right(); ++x) {
            QRgb pixel = m_atlas.pixel(x, y);
            if (qAlpha(pixel) >= alphaThreshold) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (minX <= maxX && minY <= maxY) {
        return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
    }
    return boxRect;
}

void SpriteDocument::setBoxSelection(int index, bool selected)
{
    if (index >= 0 && index < m_boxes.size()) {
        m_boxes[index].selected = selected;
        if (selected) {
            if (!m_selectedFrameIndices.contains(index)) {
                m_selectedFrameIndices.append(index);
            }
        } else {
            m_selectedFrameIndices.removeAll(index);
        }
    }
}

void SpriteDocument::clearBoxSelections()
{
    for (int i = 0; i < m_boxes.size(); ++i) {
        m_boxes[i].selected = false;
    }
    m_selectedFrameIndices.clear();
}

QList<int> SpriteDocument::selectedFrameIndices() const
{
    return m_selectedFrameIndices;
}

void SpriteDocument::setSelectedFrameIndices(const QList<int> &indices)
{
    clearBoxSelections();
    for (int idx : indices) {
        if (idx >= 0 && idx < m_boxes.size()) {
            m_boxes[idx].selected = true;
            if (!m_selectedFrameIndices.contains(idx)) {
                m_selectedFrameIndices.append(idx);
            }
        }
    }
}

QPoint SpriteBox::calculatePresetPivot(PivotPreset preset, const QSize &size)
{
    int w = size.width();
    int h = size.height();
    switch (preset) {
    case PivotPreset::TopLeft:
        return QPoint(0, 0);
    case PivotPreset::TopCenter:
        return QPoint(w / 2, 0);
    case PivotPreset::TopRight:
        return QPoint(w, 0);
    case PivotPreset::CenterLeft:
        return QPoint(0, h / 2);
    case PivotPreset::Center:
        return QPoint(w / 2, h / 2);
    case PivotPreset::CenterRight:
        return QPoint(w, h / 2);
    case PivotPreset::BottomLeft:
        return QPoint(0, h);
    case PivotPreset::BottomCenter:
        return QPoint(w / 2, h);
    case PivotPreset::BottomRight:
        return QPoint(w, h);
    case PivotPreset::Custom:
    default:
        return QPoint(w / 2, h);
    }
}

QPoint SpriteDocument::boxPivot(int index) const
{
    if (index >= 0 && index < m_boxes.size()) {
        return m_boxes.at(index).effectivePivot();
    }
    return QPoint(0, 0);
}

bool SpriteDocument::boxHasCustomPivot(int index) const
{
    if (index >= 0 && index < m_boxes.size()) {
        return m_boxes.at(index).hasCustomPivot;
    }
    return false;
}

void SpriteDocument::setBoxPivot(int index, const QPoint &pivot, bool custom)
{
    if (index < 0 || index >= m_boxes.size()) return;
    if (m_boxes[index].pivot == pivot && m_boxes[index].hasCustomPivot == custom) return;

    m_boxes[index].pivot = pivot;
    m_boxes[index].hasCustomPivot = custom;
    emit boxPivotChanged(index, pivot);
    emit frameUpdated(index);
}

void SpriteDocument::setBoxesPivot(const QList<int> &indices, const QPoint &pivot, bool custom)
{
    for (int idx : indices) {
        if (idx >= 0 && idx < m_boxes.size()) {
            m_boxes[idx].pivot = pivot;
            m_boxes[idx].hasCustomPivot = custom;
            emit boxPivotChanged(idx, pivot);
            emit frameUpdated(idx);
        }
    }
}

void SpriteDocument::applyPivotPreset(const QList<int> &indices, PivotPreset preset)
{
    for (int idx : indices) {
        if (idx >= 0 && idx < m_boxes.size()) {
            QPoint p = SpriteBox::calculatePresetPivot(preset, m_boxes[idx].rect.size());
            m_boxes[idx].pivot = p;
            m_boxes[idx].hasCustomPivot = true;
            emit boxPivotChanged(idx, p);
            emit frameUpdated(idx);
        }
    }
}

QRect SpriteDocument::computeAnimationEnvelope(const QString &animName) const
{
    QList<int> frameIndices;
    if (m_animations.contains(animName)) {
        frameIndices = m_animations.value(animName).frameIndices;
    } else if (animName.isEmpty() || animName == QLatin1String("current")) {
        if (m_animations.contains(QLatin1String("current"))) {
            frameIndices = m_animations.value(QLatin1String("current")).frameIndices;
        } else {
            for (int i = 0; i < m_frames.size(); ++i) {
                frameIndices.append(i);
            }
        }
    }

    if (frameIndices.isEmpty()) {
        int w = qMax(1, m_maxFrameWidth);
        int h = qMax(1, m_maxFrameHeight);
        return QRect(w / 2, h, w, h);
    }

    int maxPx = 0;
    int maxPy = 0;
    int maxRightDist = 0;
    int maxBottomDist = 0;

    for (int idx : frameIndices) {
        if (idx < 0 || idx >= m_boxes.size()) continue;
        const SpriteBox &b = m_boxes.at(idx);
        QPoint p = b.effectivePivot();
        int w = b.rect.width();
        int h = b.rect.height();

        if (p.x() > maxPx) maxPx = p.x();
        if (p.y() > maxPy) maxPy = p.y();

        int rightDist = w - p.x();
        if (rightDist > maxRightDist) maxRightDist = rightDist;

        int bottomDist = h - p.y();
        if (bottomDist > maxBottomDist) maxBottomDist = bottomDist;
    }

    int originX = maxPx;
    int originY = maxPy;
    int canvasW = qMax(1, originX + maxRightDist);
    int canvasH = qMax(1, originY + maxBottomDist);

    return QRect(originX, originY, canvasW, canvasH);
}

SpriteAnimation SpriteDocument::animation(const QString &name) const
{
    return m_animations.value(name);
}

void SpriteDocument::setAnimation(const QString &name, const QList<int> &frameIndices, int fps, bool loop, SpriteAnimation::LoopMode loopMode)
{
    SpriteAnimation anim;
    anim.name = name;
    anim.frameIndices = frameIndices;
    anim.fps = fps;
    if (!loop && loopMode == SpriteAnimation::Loop) {
        loopMode = SpriteAnimation::Once;
    }
    anim.loop = (loopMode == SpriteAnimation::Loop);
    anim.loopMode = loopMode;
    m_animations[name] = anim;

    emit animationsChanged();
}

void SpriteDocument::setAnimations(const QMap<QString, SpriteAnimation> &animations)
{
    m_animations = animations;
    emit animationsChanged();
}

void SpriteDocument::setAnimation(const SpriteAnimation &anim)
{
    if (anim.name.isEmpty()) return;
    m_animations[anim.name] = anim;
    emit animationsChanged();
}

void SpriteDocument::removeAnimation(const QString &name)
{
    if (m_animations.remove(name) > 0) {
        emit animationsChanged();
    }
}

void SpriteDocument::renameAnimation(const QString &oldName, const QString &newName)
{
    if (m_animations.contains(oldName) && !newName.isEmpty() && oldName != newName) {
        SpriteAnimation anim = m_animations.take(oldName);
        anim.name = newName;
        m_animations[newName] = anim;
        emit animationsChanged();
    }
}

void SpriteDocument::duplicateAnimation(const QString &sourceName, const QString &newName)
{
    if (!m_animations.contains(sourceName) || newName.isEmpty()) return;
    SpriteAnimation anim = m_animations.value(sourceName);
    anim.name = newName;
    m_animations[newName] = anim;
    emit animationsChanged();
}

void SpriteDocument::reverseAnimationFrames(const QString &name)
{
    if (m_animations.contains(name)) {
        std::reverse(m_animations[name].frameIndices.begin(), m_animations[name].frameIndices.end());
        emit animationsChanged();
    }
}

void SpriteDocument::setAnimationLoopMode(const QString &name, SpriteAnimation::LoopMode mode)
{
    if (m_animations.contains(name)) {
        m_animations[name].loopMode = mode;
        m_animations[name].loop = (mode == SpriteAnimation::Loop);
        emit animationsChanged();
    }
}

void SpriteDocument::setAnimationFrameSequence(const QString &name, const QList<int> &frameIndices)
{
    if (m_animations.contains(name)) {
        m_animations[name].frameIndices = frameIndices;
        emit animationsChanged();
    }
}

void SpriteDocument::insertFrameInAnimation(const QString &name, int seqIndex, int globalFrameIndex)
{
    if (m_animations.contains(name)) {
        if (seqIndex < 0 || seqIndex > m_animations[name].frameIndices.size()) {
            m_animations[name].frameIndices.append(globalFrameIndex);
        } else {
            m_animations[name].frameIndices.insert(seqIndex, globalFrameIndex);
        }
        emit animationsChanged();
    }
}

void SpriteDocument::removeFrameFromAnimation(const QString &name, int seqIndex)
{
    if (m_animations.contains(name)) {
        if (seqIndex >= 0 && seqIndex < m_animations[name].frameIndices.size()) {
            m_animations[name].frameIndices.removeAt(seqIndex);
            emit animationsChanged();
        }
    }
}

void SpriteDocument::clearAtlasAreas(const QList<int> &frameIndices)
{
    if (m_atlas.isNull() || frameIndices.isEmpty()) return;

    QPainter painter(&m_atlas);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);

    for (int idx : frameIndices) {
        if (idx >= 0 && idx < m_boxes.size()) {
            painter.fillRect(m_boxes[idx].rect, Qt::transparent);
        }
    }
    painter.end();
    emit atlasChanged();
}

void SpriteDocument::recalculateMaxFrameDimensions()
{
    m_maxFrameWidth = 0;
    m_maxFrameHeight = 0;
    for (const QImage &img : m_frames) {
        if (img.width() > m_maxFrameWidth) m_maxFrameWidth = img.width();
        if (img.height() > m_maxFrameHeight) m_maxFrameHeight = img.height();
    }
}

double SpriteBox::polygonArea() const
{
    if (!hasPolygonMesh || polygon.size() < 3) {
        return static_cast<double>(rect.width() * rect.height());
    }
    double area = 0.0;
    int n = polygon.size();
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        area += polygon[i].x() * polygon[next].y() - polygon[next].x() * polygon[i].y();
    }
    return std::abs(area) * 0.5;
}

double SpriteBox::overdrawSavings() const
{
    if (!hasPolygonMesh || rect.width() <= 0 || rect.height() <= 0) {
        return 0.0;
    }
    double boxArea = static_cast<double>(rect.width() * rect.height());
    double polyArea = polygonArea();
    if (polyArea >= boxArea) {
        return 0.0;
    }
    return std::clamp((boxArea - polyArea) / boxArea * 100.0, 0.0, 100.0);
}

// ============================================================================
// M17: Layers, Cels, Compositing & Skin Profiles
// ============================================================================

void SpriteDocument::setLayers(const QList<SpriteLayer> &layers)
{
    m_layers = layers;
    emit layersChanged();
}

void SpriteDocument::addLayer(const SpriteLayer &layer)
{
    m_layers.append(layer);
    emit layersChanged();
}

void SpriteDocument::removeLayer(int layerIndex)
{
    if (layerIndex < 0 || layerIndex >= m_layers.size()) return;
    QString removedId = m_layers.at(layerIndex).id;
    m_layers.removeAt(layerIndex);

    // Remove cels on this layer across all frames, and adjust cel layerIndex
    for (auto it = m_frameCels.begin(); it != m_frameCels.end(); ++it) {
        QList<SpriteCel> &cels = it.value();
        for (int i = cels.size() - 1; i >= 0; --i) {
            if (cels[i].layerIndex == layerIndex || (!removedId.isEmpty() && cels[i].layerId == removedId)) {
                cels.removeAt(i);
            } else if (cels[i].layerIndex > layerIndex) {
                cels[i].layerIndex--;
            }
        }
    }
    emit layersChanged();
}

SpriteLayer SpriteDocument::layer(int layerIndex) const
{
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        return m_layers.at(layerIndex);
    }
    return SpriteLayer();
}

void SpriteDocument::setLayerVisible(int layerIndex, bool visible)
{
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        if (m_layers[layerIndex].visible != visible) {
            m_layers[layerIndex].visible = visible;
            emit layersChanged();
        }
    }
}

int SpriteDocument::findLayerIndexById(const QString &layerId) const
{
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i].id == layerId) return i;
    }
    return -1;
}

bool SpriteDocument::hasFrameCels(int frameIndex) const
{
    return m_frameCels.contains(frameIndex) && !m_frameCels.value(frameIndex).isEmpty();
}

QList<SpriteCel> SpriteDocument::frameCels(int frameIndex) const
{
    return m_frameCels.value(frameIndex);
}

void SpriteDocument::setFrameCels(int frameIndex, const QList<SpriteCel> &cels)
{
    m_frameCels[frameIndex] = cels;
}

void SpriteDocument::setCel(int frameIndex, int layerIndex, const SpriteCel &cel)
{
    QList<SpriteCel> &cels = m_frameCels[frameIndex];
    for (int i = 0; i < cels.size(); ++i) {
        if (cels[i].layerIndex == layerIndex) {
            cels[i] = cel;
            return;
        }
    }
    cels.append(cel);
}

SpriteCel SpriteDocument::cel(int frameIndex, int layerIndex) const
{
    if (!m_frameCels.contains(frameIndex)) return SpriteCel();
    const auto &cels = m_frameCels.value(frameIndex);
    for (const auto &c : cels) {
        if (c.layerIndex == layerIndex) return c;
    }
    return SpriteCel();
}

void SpriteDocument::removeCel(int frameIndex, int layerIndex)
{
    if (!m_frameCels.contains(frameIndex)) return;
    QList<SpriteCel> &cels = m_frameCels[frameIndex];
    for (int i = cels.size() - 1; i >= 0; --i) {
        if (cels[i].layerIndex == layerIndex) {
            cels.removeAt(i);
            break;
        }
    }
}

QImage SpriteDocument::compositeFrame(int frameIndex, const QStringList &activeLayerIds) const
{
    if (frameIndex < 0 || frameIndex >= m_frames.size()) return QImage();

    // If no layers or no cels defined, return standard frame
    if (m_layers.isEmpty() || !m_frameCels.contains(frameIndex) || m_frameCels.value(frameIndex).isEmpty()) {
        return m_frames.at(frameIndex);
    }

    QSize sz = m_frames.at(frameIndex).size();
    if (sz.isEmpty() && frameIndex < m_boxes.size()) {
        sz = m_boxes.at(frameIndex).rect.size();
    }
    if (sz.isEmpty()) sz = QSize(32, 32);

    QImage composite(sz, QImage::Format_ARGB32_Premultiplied);
    composite.fill(Qt::transparent);

    QPainter painter(&composite);

    // Sort layers by zOrder
    QList<int> sortedLayerIndices;
    sortedLayerIndices.reserve(m_layers.size());
    for (int i = 0; i < m_layers.size(); ++i) {
        sortedLayerIndices.append(i);
    }
    std::stable_sort(sortedLayerIndices.begin(), sortedLayerIndices.end(), [this](int a, int b) {
        return m_layers[a].zOrder < m_layers[b].zOrder;
    });

    const QList<SpriteCel> &cels = m_frameCels.value(frameIndex);

    for (int lIdx : sortedLayerIndices) {
        const SpriteLayer &lyr = m_layers.at(lIdx);

        // Check if layer is active/visible
        if (!activeLayerIds.isEmpty()) {
            if (!activeLayerIds.contains(lyr.id) && !activeLayerIds.contains(lyr.name)) {
                continue;
            }
        } else {
            // M21: Exclude auxiliary maps (_n, _e, _s) from diffuse albedo compositing
            if (MultiAtlasPacker::detectMapType(lyr.name) != MaterialMapType::Albedo) {
                continue;
            }
            if (!lyr.visible) {
                continue;
            }
        }

        // Find cel for this layer
        SpriteCel currentCel;
        bool found = false;
        for (const auto &c : cels) {
            if (c.layerIndex == lIdx || (!lyr.id.isEmpty() && c.layerId == lyr.id)) {
                currentCel = c;
                found = true;
                break;
            }
        }

        if (!found || currentCel.isNull()) continue;

        // Apply blend mode and opacities
        painter.setCompositionMode(lyr.blendMode);
        double layerOp = (lyr.opacity / 255.0) * (currentCel.opacity / 255.0);
        painter.setOpacity(std::clamp(layerOp, 0.0, 1.0));

        painter.drawImage(QPoint(currentCel.x, currentCel.y), currentCel.image);
    }
    painter.end();

    return composite.convertToFormat(QImage::Format_ARGB32);
}

void SpriteDocument::recompositeFrame(int frameIndex)
{
    if (frameIndex < 0 || frameIndex >= m_frames.size()) return;
    QImage comp = compositeFrame(frameIndex);
    if (!comp.isNull()) {
        m_frames[frameIndex] = comp;
        emit frameUpdated(frameIndex);
    }
}

void SpriteDocument::recompositeAllFrames()
{
    for (int i = 0; i < m_frames.size(); ++i) {
        QImage comp = compositeFrame(i);
        if (!comp.isNull()) {
            m_frames[i] = comp;
        }
    }
    emit framesChanged();
}

void SpriteDocument::setSkinProfiles(const QMap<QString, SkinProfile> &profiles)
{
    m_skinProfiles = profiles;
    emit skinProfilesChanged();
}

void SpriteDocument::addSkinProfile(const SkinProfile &profile)
{
    m_skinProfiles[profile.id] = profile;
    emit skinProfilesChanged();
}

void SpriteDocument::removeSkinProfile(const QString &profileId)
{
    if (m_skinProfiles.remove(profileId) > 0) {
        emit skinProfilesChanged();
    }
}

SkinProfile SpriteDocument::skinProfile(const QString &profileId) const
{
    return m_skinProfiles.value(profileId);
}

void SpriteDocument::bakeAnimationVariants(const QStringList &profileIds)
{
    if (m_skinProfiles.isEmpty() || m_animations.isEmpty()) return;

    QStringList targetProfiles = profileIds.isEmpty() ? m_skinProfiles.keys() : profileIds;

    for (const QString &profId : targetProfiles) {
        if (!m_skinProfiles.contains(profId)) continue;
        const SkinProfile &prof = m_skinProfiles.value(profId);

        auto existingAnims = m_animations;
        for (auto it = existingAnims.constBegin(); it != existingAnims.constEnd(); ++it) {
            const QString &animName = it.key();
            const SpriteAnimation &baseAnim = it.value();

            // Avoid re-baking onto already baked variants
            QString suffix = QLatin1Char('_') + (prof.name.isEmpty() ? prof.id : prof.name).toLower();
            if (animName.endsWith(suffix) || animName.contains(suffix)) {
                continue;
            }

            QString variantAnimName = animName + suffix;

            QList<int> variantFrameIndices;
            for (int baseFrameIdx : baseAnim.frameIndices) {
                if (baseFrameIdx < 0 || baseFrameIdx >= m_frames.size()) continue;

                QImage bakedImg = compositeFrame(baseFrameIdx, prof.activeLayerIds);
                SpriteBox bakedBox = (baseFrameIdx < m_boxes.size()) ? m_boxes.at(baseFrameIdx) : SpriteBox(QRect(0, 0, bakedImg.width(), bakedImg.height()));
                bakedBox.index = m_frames.size();

                int newFrameIdx = m_frames.size();
                addFrame(bakedImg, bakedBox);
                variantFrameIndices.append(newFrameIdx);

                if (m_frameCels.contains(baseFrameIdx)) {
                    m_frameCels[newFrameIdx] = m_frameCels.value(baseFrameIdx);
                }
            }

            SpriteAnimation variantAnim = baseAnim;
            variantAnim.name = variantAnimName;
            variantAnim.frameIndices = variantFrameIndices;
            setAnimation(variantAnim);
        }
    }
}

bool SpriteDocument::hasAuxiliaryMap(MaterialMapType type) const
{
    if (type == MaterialMapType::Albedo) {
        return !m_frames.isEmpty();
    }
    if (m_auxiliaryFrames.contains(type) && !m_auxiliaryFrames.value(type).isEmpty()) {
        return true;
    }
    for (const auto &lyr : m_layers) {
        if (MultiAtlasPacker::detectMapType(lyr.name) == type) {
            return true;
        }
    }
    return false;
}

bool SpriteDocument::hasAnyAuxiliaryMaps() const
{
    return hasAuxiliaryMap(MaterialMapType::Normal)
        || hasAuxiliaryMap(MaterialMapType::Emissive)
        || hasAuxiliaryMap(MaterialMapType::Specular);
}

QList<QImage> SpriteDocument::auxiliaryFrames(MaterialMapType type) const
{
    return m_auxiliaryFrames.value(type);
}

void SpriteDocument::setAuxiliaryFrames(MaterialMapType type, const QList<QImage> &frames)
{
    m_auxiliaryFrames[type] = frames;
    emit auxiliaryMapsChanged();
}

QImage SpriteDocument::compositeFrameForMap(int frameIndex, MaterialMapType mapType) const
{
    if (mapType == MaterialMapType::Albedo) {
        return compositeFrame(frameIndex);
    }
    if (frameIndex < 0 || frameIndex >= m_frames.size()) {
        return QImage();
    }

    if (m_auxiliaryFrames.contains(mapType)) {
        const auto &auxList = m_auxiliaryFrames.value(mapType);
        if (frameIndex < auxList.size() && !auxList.at(frameIndex).isNull()) {
            return auxList.at(frameIndex);
        }
    }

    QSize sz = m_frames.at(frameIndex).size();
    if (sz.isEmpty() && frameIndex < m_boxes.size()) {
        sz = m_boxes.at(frameIndex).rect.size();
    }
    if (sz.isEmpty()) sz = QSize(32, 32);

    QImage composite(sz, QImage::Format_ARGB32_Premultiplied);
    composite.fill(MultiAtlasPacker::neutralColor(mapType));

    if (m_layers.isEmpty() || !m_frameCels.contains(frameIndex)) {
        return composite.convertToFormat(QImage::Format_ARGB32);
    }

    QPainter painter(&composite);
    QList<int> sortedLayerIndices;
    sortedLayerIndices.reserve(m_layers.size());
    for (int i = 0; i < m_layers.size(); ++i) {
        sortedLayerIndices.append(i);
    }
    std::stable_sort(sortedLayerIndices.begin(), sortedLayerIndices.end(), [this](int a, int b) {
        return m_layers[a].zOrder < m_layers[b].zOrder;
    });

    const QList<SpriteCel> &cels = m_frameCels.value(frameIndex);
    for (int lIdx : sortedLayerIndices) {
        const SpriteLayer &lyr = m_layers.at(lIdx);
        if (MultiAtlasPacker::detectMapType(lyr.name) != mapType) {
            continue;
        }

        SpriteCel currentCel;
        bool found = false;
        for (const auto &c : cels) {
            if (c.layerIndex == lIdx || (!lyr.id.isEmpty() && c.layerId == lyr.id)) {
                currentCel = c;
                found = true;
                break;
            }
        }
        if (!found || currentCel.isNull()) continue;

        painter.drawImage(QPoint(currentCel.x, currentCel.y), currentCel.image);
    }
    painter.end();

    return composite.convertToFormat(QImage::Format_ARGB32);
}

QList<QImage> SpriteDocument::framesForMap(MaterialMapType mapType) const
{
    if (mapType == MaterialMapType::Albedo) {
        return m_frames;
    }
    if (!hasAuxiliaryMap(mapType)) {
        return QList<QImage>();
    }
    if (m_auxiliaryFrames.contains(mapType) && !m_auxiliaryFrames.value(mapType).isEmpty()) {
        return m_auxiliaryFrames.value(mapType);
    }
    QList<QImage> result;
    result.reserve(m_frames.size());
    for (int i = 0; i < m_frames.size(); ++i) {
        result.append(compositeFrameForMap(i, mapType));
    }
    return result;
}

