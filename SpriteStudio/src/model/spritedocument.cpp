#include "model/spritedocument.h"
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

QPixmap SpriteDocument::frame(int index) const
{
    if (index >= 0 && index < m_frames.size()) {
        return m_frames.at(index);
    }
    return QPixmap();
}

void SpriteDocument::setFrames(const QList<QPixmap> &frames, const QList<SpriteBox> &boxes)
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

void SpriteDocument::addFrame(const QPixmap &pixmap, const SpriteBox &box)
{
    if (!pixmap.isNull()) {
        m_frames.append(pixmap);
        m_boxes.append(box);
        if (pixmap.width() > m_maxFrameWidth) m_maxFrameWidth = pixmap.width();
        if (pixmap.height() > m_maxFrameHeight) m_maxFrameHeight = pixmap.height();
        emit framesChanged();
    }
}

void SpriteDocument::insertFrame(int index, const QPixmap &pixmap, const SpriteBox &box)
{
    if (index < 0 || index > m_frames.size() || pixmap.isNull()) return;

    m_frames.insert(index, pixmap);
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

    recalculateMaxFrameDimensions();
    emit framesChanged();
    emit animationsChanged();
}

void SpriteDocument::replaceFrame(int index, const QPixmap &pixmap, const SpriteBox &box)
{
    if (index < 0 || index >= m_frames.size()) return;

    m_frames[index] = pixmap;
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

    QList<QPixmap> reorderedFrames;
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
    QPixmap mergedPixmap;

    if (!m_atlas.isNull() && unitedRect.isValid()) {
        mergedPixmap = QPixmap::fromImage(m_atlas).copy(unitedRect);
    } else {
        // Fallback: draw both pixmaps side by side or overlay
        QSize combinedSize = m_frames[targetIndex].size().expandedTo(m_frames[sourceIndex].size());
        QImage composite(combinedSize, QImage::Format_ARGB32_Premultiplied);
        composite.fill(Qt::transparent);
        QPainter p(&composite);
        p.drawPixmap(0, 0, m_frames[targetIndex]);
        p.drawPixmap(0, 0, m_frames[sourceIndex]);
        p.end();
        mergedPixmap = QPixmap::fromImage(composite);
    }

    SpriteBox newBox;
    newBox.rect = unitedRect;
    newBox.selected = srcBox.selected || tgtBox.selected;
    newBox.index = tgtBox.index;

    m_boxes[targetIndex] = newBox;
    m_frames[targetIndex] = mergedPixmap;

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
        m_frames[index] = QPixmap::fromImage(m_atlas.copy(clampedRect));
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

    QPixmap framePixmap = QPixmap::fromImage(m_atlas.copy(clampedRect));
    m_frames.append(framePixmap);
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
    for (const QPixmap &pix : m_frames) {
        if (pix.width() > m_maxFrameWidth) m_maxFrameWidth = pix.width();
        if (pix.height() > m_maxFrameHeight) m_maxFrameHeight = pix.height();
    }
}
