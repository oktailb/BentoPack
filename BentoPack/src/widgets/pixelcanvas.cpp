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

#include "widgets/pixelcanvas.h"
#include "geometry/triangulator.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QQueue>
#include <QTransform>
#include <QDateTime>
#include <algorithm>
#include <cmath>

QImage PixelCanvas::s_clipboardImage;

namespace {

static QRect computeDirtyRect(const QImage &img1, const QImage &img2)
{
    if (img1.isNull() || img2.isNull() || img1.size() != img2.size() || img1.format() != img2.format()) {
        int w = std::max(img1.width(), img2.width());
        int h = std::max(img1.height(), img2.height());
        return (w > 0 && h > 0) ? QRect(0, 0, w, h) : QRect();
    }

    const int w = img1.width();
    const int h = img1.height();
    int minX = w, maxX = -1;
    int minY = h, maxY = -1;

    for (int y = 0; y < h; ++y) {
        const QRgb *line1 = reinterpret_cast<const QRgb*>(img1.constScanLine(y));
        const QRgb *line2 = reinterpret_cast<const QRgb*>(img2.constScanLine(y));
        if (std::memcmp(line1, line2, w * sizeof(QRgb)) == 0) {
            continue;
        }
        for (int x = 0; x < w; ++x) {
            if (line1[x] != line2[x]) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return QRect(); // Identical images
    }

    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

class PixelCanvasUndoCommand : public QUndoCommand
{
public:
    PixelCanvasUndoCommand(PixelCanvas *canvas, int layerIndex,
                           const QImage &oldImg, const QImage &newImg,
                           const QPolygonF &oldPoly, const QPolygonF &newPoly,
                           const QString &text,
                           const QList<QPointF> &oldVerts = {},
                           const QList<QPointF> &newVerts = {},
                           const QList<int> &oldTris = {},
                           const QList<int> &newTris = {},
                           QUndoCommand *parent = nullptr)
        : QUndoCommand(text, parent)
        , m_canvas(canvas)
        , m_layerIndex(layerIndex)
        , m_oldPoly(oldPoly)
        , m_newPoly(newPoly)
        , m_oldVerts(oldVerts)
        , m_newVerts(newVerts)
        , m_oldTris(oldTris)
        , m_newTris(newTris)
        , m_firstExecution(true)
    {
        m_dirtyRect = computeDirtyRect(oldImg, newImg);
        if (m_dirtyRect.isEmpty()) {
            m_isEmpty = true;
            return;
        }

        bool dimensionsMatch = (!oldImg.isNull() && !newImg.isNull() && oldImg.size() == newImg.size());
        int totalPixels = dimensionsMatch ? (oldImg.width() * oldImg.height()) : 0;
        int dirtyPixels = m_dirtyRect.width() * m_dirtyRect.height();

        if (!dimensionsMatch || totalPixels <= 256 || dirtyPixels >= totalPixels * 0.85) {
            m_isFullImage = true;
            m_oldImage = oldImg;
            m_newImage = newImg;
        } else {
            m_isFullImage = false;
            // Store only the sub-image patches (lightweight copy, COW on rest)
            m_oldPatch = oldImg.copy(m_dirtyRect);
            m_newPatch = newImg.copy(m_dirtyRect);
        }
    }

    bool isEmpty() const {
        return m_isEmpty && (m_oldPoly == m_newPoly) && (m_oldVerts == m_newVerts) && (m_oldTris == m_newTris);
    }

    void setCustomUndoRedo(const std::function<void()> &undoFunc, const std::function<void()> &redoFunc) {
        m_customUndo = undoFunc;
        m_customRedo = redoFunc;
    }

    void undo() override {
        if (m_customUndo) {
            m_customUndo();
            return;
        }
        QUndoCommand::undo();
        if (!m_canvas) return;
        if (!m_isEmpty) {
            if (m_canvas->hasLayers() && m_layerIndex >= 0 && m_layerIndex < m_canvas->layerCount()) {
                if (m_isFullImage) {
                    m_canvas->setLayerCelImage(m_layerIndex, m_oldImage);
                } else {
                    m_canvas->applyLayerPatch(m_layerIndex, m_dirtyRect, m_oldPatch);
                }
            } else {
                if (m_isFullImage) {
                    m_canvas->setImage(m_oldImage);
                } else {
                    m_canvas->applyPatch(m_dirtyRect, m_oldPatch);
                }
            }
        }
        if (m_oldPoly != m_newPoly || m_oldVerts != m_newVerts) {
            m_canvas->setPolygonMeshData(m_oldPoly, m_oldVerts, m_oldTris);
        }
    }

    void redo() override {
        if (m_firstExecution) {
            m_firstExecution = false;
            return;
        }
        if (m_customRedo) {
            m_customRedo();
            return;
        }
        QUndoCommand::redo();
        if (!m_canvas) return;
        if (!m_isEmpty) {
            if (m_canvas->hasLayers() && m_layerIndex >= 0 && m_layerIndex < m_canvas->layerCount()) {
                if (m_isFullImage) {
                    m_canvas->setLayerCelImage(m_layerIndex, m_newImage);
                } else {
                    m_canvas->applyLayerPatch(m_layerIndex, m_dirtyRect, m_newPatch);
                }
            } else {
                if (m_isFullImage) {
                    m_canvas->setImage(m_newImage);
                } else {
                    m_canvas->applyPatch(m_dirtyRect, m_newPatch);
                }
            }
        }
        if (m_oldPoly != m_newPoly || m_oldVerts != m_newVerts) {
            m_canvas->setPolygonMeshData(m_newPoly, m_newVerts, m_newTris);
        }
    }

private:
    PixelCanvas *m_canvas = nullptr;
    int          m_layerIndex = -1;
    QRect        m_dirtyRect;
    QImage       m_oldPatch;
    QImage       m_newPatch;
    QImage       m_oldImage;
    QImage       m_newImage;
    QPolygonF    m_oldPoly;
    QPolygonF    m_newPoly;
    QList<QPointF> m_oldVerts;
    QList<QPointF> m_newVerts;
    QList<int>   m_oldTris;
    QList<int>   m_newTris;
    std::function<void()> m_customUndo;
    std::function<void()> m_customRedo;
    bool         m_isFullImage = false;
    bool         m_isEmpty = false;
    bool         m_firstExecution = true;
};
}

void PixelCanvas::setCommandCustomUndoRedo(QUndoCommand *cmd,
                                          const std::function<void()> &undoFunc,
                                          const std::function<void()> &redoFunc)
{
    if (auto *pcmd = dynamic_cast<PixelCanvasUndoCommand*>(cmd)) {
        pcmd->setCustomUndoRedo(undoFunc, redoFunc);
    }
}

PixelCanvas::PixelCanvas(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void PixelCanvas::setImage(const QImage &image)
{
    commitFloatingSelection();
    if (image.isNull()) {
        m_image = QImage(32, 32, QImage::Format_ARGB32);
        m_image.fill(Qt::transparent);
    } else {
        m_image = image.convertToFormat(QImage::Format_ARGB32);
    }
    if (!m_layers.isEmpty()) {
        ensureActiveCelAllocated();
        m_layers[m_activeLayerIndex].image = m_image;
        m_layers[m_activeLayerIndex].offset = QPoint(0, 0);
        recomposite();
    }
    deselect();
    updatePolygonMask();
    updateCanvasSize();
    updateOnionSkinComposite();
    emit imageChanged();
    update();
}

void PixelCanvas::ensureActiveCelAllocated()
{
    if (m_layers.isEmpty()) return;
    if (m_activeLayerIndex < 0 || m_activeLayerIndex >= m_layers.size()) {
        m_activeLayerIndex = 0;
    }
    QSize sz;
    for (const auto &l : m_layers) {
        if (!l.image.isNull()) {
            sz = sz.expandedTo(QSize(l.offset.x() + l.image.width(), l.offset.y() + l.image.height()));
        }
    }
    if (sz.isEmpty()) {
        sz = m_image.isNull() ? QSize(32, 32) : m_image.size();
    }
    if (sz.isEmpty()) sz = QSize(32, 32);

    CanvasLayer &cur = m_layers[m_activeLayerIndex];
    if (cur.image.isNull() || cur.image.format() != QImage::Format_ARGB32 || cur.offset != QPoint(0, 0) || cur.image.size() != sz) {
        QImage newImg(sz, QImage::Format_ARGB32);
        newImg.fill(Qt::transparent);
        if (!cur.image.isNull()) {
            QPainter p(&newImg);
            p.drawImage(cur.offset, cur.image);
        }
        cur.image = newImg;
        cur.offset = QPoint(0, 0);
    }
}

CanvasLayer PixelCanvas::activeLayer() const
{
    if (!m_layers.isEmpty() && m_activeLayerIndex >= 0 && m_activeLayerIndex < m_layers.size()) {
        return m_layers.at(m_activeLayerIndex);
    }
    return CanvasLayer();
}

QImage PixelCanvas::activeLayerImage() const
{
    if (!m_layers.isEmpty() && m_activeLayerIndex >= 0 && m_activeLayerIndex < m_layers.size()) {
        return m_layers.at(m_activeLayerIndex).image;
    }
    return m_image;
}

bool PixelCanvas::isLayerLocked(int index) const
{
    if (m_layers.isEmpty()) return false;
    int idx = (index >= 0) ? index : m_activeLayerIndex;
    if (idx >= 0 && idx < m_layers.size()) {
        return m_layers.at(idx).locked;
    }
    return false;
}

void PixelCanvas::setLayers(const QList<CanvasLayer> &layers, int activeIndex)
{
    commitFloatingSelection();
    m_layers = layers;

    // Calculate maximum bounding canvas size across all layers including offsets
    QSize sz;
    for (const auto &l : m_layers) {
        if (!l.image.isNull()) {
            sz = sz.expandedTo(QSize(l.offset.x() + l.image.width(), l.offset.y() + l.image.height()));
        }
    }
    if (sz.isEmpty() && !m_image.isNull()) {
        sz = m_image.size();
    }
    if (sz.isEmpty()) sz = QSize(32, 32);

    // Normalize all layers to uniform canvas size and Format_ARGB32, baking offsets so drawing tools operate correctly
    for (auto &l : m_layers) {
        if (l.image.isNull() || l.image.format() != QImage::Format_ARGB32 || l.offset != QPoint(0, 0) || l.image.size() != sz) {
            QImage normImg(sz, QImage::Format_ARGB32);
            normImg.fill(Qt::transparent);
            if (!l.image.isNull()) {
                QPainter p(&normImg);
                p.drawImage(l.offset, l.image);
            }
            l.image = normImg;
            l.offset = QPoint(0, 0);
        }
    }

    int maxIdx = static_cast<int>(m_layers.size() - 1);
    m_activeLayerIndex = std::clamp(activeIndex, 0, std::max(0, maxIdx));
    ensureActiveCelAllocated();
    recomposite();
    deselect();
    updatePolygonMask();
    updateCanvasSize();
    updateOnionSkinComposite();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerIndex);
    emit imageChanged();
    update();
}

void PixelCanvas::setActiveLayerIndex(int index)
{
    if (m_layers.isEmpty()) return;
    int maxIdx = static_cast<int>(m_layers.size() - 1);
    int clamped = std::clamp(index, 0, maxIdx);
    if (m_activeLayerIndex != clamped) {
        m_activeLayerIndex = clamped;
        ensureActiveCelAllocated();
        emit activeLayerChanged(m_activeLayerIndex);
        update();
    }
}

void PixelCanvas::setLayerVisible(int index, bool visible)
{
    if (index >= 0 && index < m_layers.size()) {
        if (m_layers[index].visible != visible) {
            m_layers[index].visible = visible;
            recomposite();
            emit layersChanged();
        }
    }
}

void PixelCanvas::setLayerLocked(int index, bool locked)
{
    if (index >= 0 && index < m_layers.size()) {
        if (m_layers[index].locked != locked) {
            m_layers[index].locked = locked;
            emit layersChanged();
        }
    }
}

void PixelCanvas::setLayerOpacity(int index, quint8 opacity)
{
    if (index >= 0 && index < m_layers.size()) {
        if (m_layers[index].opacity != opacity) {
            m_layers[index].opacity = opacity;
            recomposite();
            emit layersChanged();
        }
    }
}

void PixelCanvas::setLayerBlendMode(int index, QPainter::CompositionMode mode)
{
    if (index >= 0 && index < m_layers.size()) {
        if (m_layers[index].blendMode != mode) {
            m_layers[index].blendMode = mode;
            recomposite();
            emit layersChanged();
        }
    }
}

void PixelCanvas::setLayerName(int index, const QString &name)
{
    if (index >= 0 && index < m_layers.size()) {
        m_layers[index].name = name;
        emit layersChanged();
    }
}

void PixelCanvas::setLayerCelImage(int index, const QImage &img)
{
    if (index >= 0 && index < m_layers.size()) {
        m_layers[index].image = img;
        recomposite();
    }
}

void PixelCanvas::applyLayerPatch(int index, const QRect &rect, const QImage &patch)
{
    if (index >= 0 && index < m_layers.size() && !rect.isEmpty() && !patch.isNull()) {
        ensureActiveCelAllocated();
        QPainter p(&m_layers[index].image);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(rect.topLeft(), patch);
        p.end();
        recomposite();
    }
}

void PixelCanvas::addLayer(const QString &name)
{
    CanvasLayer layer;
    layer.id = QStringLiteral("layer_%1").arg(QDateTime::currentMSecsSinceEpoch());
    layer.name = name.isEmpty() ? tr("Layer %1").arg(m_layers.size() + 1) : name;
    layer.visible = true;
    layer.locked = false;
    layer.opacity = 255;
    layer.zOrder = m_layers.isEmpty() ? 0 : (m_layers.last().zOrder + 10);
    QSize sz;
    for (const auto &l : m_layers) {
        if (!l.image.isNull()) {
            sz = sz.expandedTo(QSize(l.offset.x() + l.image.width(), l.offset.y() + l.image.height()));
        }
    }
    if (sz.isEmpty()) {
        sz = m_image.isNull() ? QSize(32, 32) : m_image.size();
    }
    if (sz.isEmpty()) sz = QSize(32, 32);
    layer.image = QImage(sz, QImage::Format_ARGB32);
    layer.image.fill(Qt::transparent);
    layer.offset = QPoint(0, 0);

    int insertPos = (m_activeLayerIndex >= 0 && m_activeLayerIndex < m_layers.size())
                    ? m_activeLayerIndex + 1
                    : m_layers.size();
    m_layers.insert(insertPos, layer);
    m_activeLayerIndex = insertPos;

    recomposite();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerIndex);
}

void PixelCanvas::duplicateLayer(int index)
{
    int target = (index >= 0) ? index : m_activeLayerIndex;
    if (target < 0 || target >= m_layers.size()) return;

    CanvasLayer dup = m_layers.at(target);
    dup.id = QStringLiteral("layer_%1").arg(QDateTime::currentMSecsSinceEpoch());
    dup.name = tr("%1 Copy").arg(dup.name);
    dup.image = dup.image.copy();

    m_layers.insert(target + 1, dup);
    m_activeLayerIndex = target + 1;

    recomposite();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerIndex);
}

void PixelCanvas::removeLayer(int index)
{
    if (m_layers.size() <= 1) return;
    int target = (index >= 0) ? index : m_activeLayerIndex;
    if (target < 0 || target >= m_layers.size()) return;

    m_layers.removeAt(target);
    int maxIdx = static_cast<int>(m_layers.size() - 1);
    m_activeLayerIndex = std::clamp(target, 0, maxIdx);

    recomposite();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerIndex);
}

void PixelCanvas::moveLayerUp(int index)
{
    int target = (index >= 0) ? index : m_activeLayerIndex;
    if (target < 0 || target >= m_layers.size() - 1) return;

    m_layers.swapItemsAt(target, target + 1);
    m_activeLayerIndex = target + 1;

    recomposite();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerIndex);
}

void PixelCanvas::moveLayerDown(int index)
{
    int target = (index >= 0) ? index : m_activeLayerIndex;
    if (target <= 0 || target >= m_layers.size()) return;

    m_layers.swapItemsAt(target, target - 1);
    m_activeLayerIndex = target - 1;

    recomposite();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerIndex);
}

void PixelCanvas::mergeLayerDown(int index)
{
    int target = (index >= 0) ? index : m_activeLayerIndex;
    if (target <= 0 || target >= m_layers.size()) return;

    const CanvasLayer &top = m_layers.at(target);
    CanvasLayer &bottom = m_layers[target - 1];

    QSize sz = bottom.image.size().expandedTo(top.image.size());
    if (sz.isEmpty()) {
        sz = m_image.isNull() ? QSize(32, 32) : m_image.size();
    }
    if (bottom.image.isNull() || bottom.image.size() != sz) {
        QImage n(sz, QImage::Format_ARGB32);
        n.fill(Qt::transparent);
        if (!bottom.image.isNull()) {
            QPainter p(&n);
            p.drawImage(0, 0, bottom.image);
        }
        bottom.image = n;
    }

    QPainter p(&bottom.image);
    p.setCompositionMode(top.blendMode);
    p.setOpacity(top.opacity / 255.0);
    p.drawImage(top.offset, top.image);
    p.end();

    m_layers.removeAt(target);
    m_activeLayerIndex = target - 1;

    recomposite();
    emit layersChanged();
    emit activeLayerChanged(m_activeLayerIndex);
}

void PixelCanvas::flattenLayers()
{
    if (m_layers.isEmpty()) return;
    recomposite();

    CanvasLayer flat;
    flat.id = QStringLiteral("flat_0");
    flat.name = tr("Background");
    flat.visible = true;
    flat.locked = false;
    flat.opacity = 255;
    flat.zOrder = 0;
    flat.image = m_image.copy();

    m_layers.clear();
    m_layers.append(flat);
    m_activeLayerIndex = 0;

    recomposite();
    emit layersChanged();
    emit activeLayerChanged(0);
}

void PixelCanvas::recomposite()
{
    if (m_layers.isEmpty()) return;

    QSize sz;
    for (const auto &l : m_layers) {
        if (!l.image.isNull()) {
            sz = sz.expandedTo(QSize(l.offset.x() + l.image.width(), l.offset.y() + l.image.height()));
        }
    }
    if (sz.isEmpty()) {
        sz = m_image.isNull() ? QSize(32, 32) : m_image.size();
    }
    if (sz.isEmpty()) sz = QSize(32, 32);

    QImage composite(sz, QImage::Format_ARGB32_Premultiplied);
    composite.fill(Qt::transparent);

    QPainter painter(&composite);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    // Sort layer indices by zOrder
    QList<int> sortedIndices;
    sortedIndices.reserve(m_layers.size());
    for (int i = 0; i < m_layers.size(); ++i) sortedIndices.append(i);
    std::stable_sort(sortedIndices.begin(), sortedIndices.end(), [this](int a, int b) {
        return m_layers[a].zOrder < m_layers[b].zOrder;
    });

    for (int idx : sortedIndices) {
        const CanvasLayer &l = m_layers.at(idx);
        if (!l.visible || l.image.isNull()) continue;

        painter.setCompositionMode(l.blendMode);
        double op = l.opacity / 255.0;
        painter.setOpacity(std::clamp(op, 0.0, 1.0));
        painter.drawImage(l.offset, l.image);
    }
    painter.end();

    m_image = composite.convertToFormat(QImage::Format_ARGB32);
    emit imageChanged();
    update();
}

void PixelCanvas::setSampleAllLayers(bool sampleAll)
{
    m_sampleAllLayers = sampleAll;
}

void PixelCanvas::setOnionSkinCurrentLayerOnly(bool currentOnly)
{
    if (m_onionSkinCurrentLayerOnly != currentOnly) {
        m_onionSkinCurrentLayerOnly = currentOnly;
        updateOnionSkinComposite();
        update();
    }
}

void PixelCanvas::setOnionSkinEnabled(bool enabled)
{
    if (m_onionSkinEnabled != enabled) {
        m_onionSkinEnabled = enabled;
        updateOnionSkinComposite();
    }
}

void PixelCanvas::setOnionSkinLayers(const QVector<OnionSkinLayer> &layers)
{
    m_onionSkinLayers = layers;
    updateOnionSkinComposite();
}

void PixelCanvas::setOnionSkinOpacity(int percent)
{
    int clamped = std::clamp(percent, 0, 100);
    if (m_onionSkinOpacityPercent != clamped) {
        m_onionSkinOpacityPercent = clamped;
        updateOnionSkinComposite();
    }
}

void PixelCanvas::setOnionSkinEffect(OnionSkinEffect effect)
{
    if (m_onionSkinEffect != effect) {
        m_onionSkinEffect = effect;
        updateOnionSkinComposite();
    }
}

QImage PixelCanvas::processOnionSkinLayer(const QImage &src, int relativeOffset, int opacityPercent, OnionSkinEffect effect, const QSize &targetSize)
{
    if (src.isNull() || targetSize.isEmpty() || opacityPercent <= 0) {
        return QImage();
    }

    QImage srcArgb = (src.format() == QImage::Format_ARGB32 || src.format() == QImage::Format_ARGB32_Premultiplied)
                     ? src : src.convertToFormat(QImage::Format_ARGB32);

    QSize actualSize = targetSize.isEmpty() ? srcArgb.size() : targetSize;
    QImage out(actualSize, QImage::Format_ARGB32);
    out.fill(Qt::transparent);

    int dist = std::max(1, std::abs(relativeOffset));
    double falloff = 1.0;
    if (dist == 2) {
        falloff = 0.65;
    } else if (dist >= 3) {
        falloff = 0.40;
    }

    double effOpacity = std::clamp((opacityPercent / 100.0) * falloff, 0.0, 1.0);
    if (effOpacity <= 0.001) {
        return out;
    }

    int copyW = std::min(actualSize.width(), srcArgb.width());
    int copyH = std::min(actualSize.height(), srcArgb.height());

    switch (effect) {
    case OnionSkinEffect::TintedBlueRed: {
        // Past (offset < 0): Electric Sky Blue (0, 160, 255); Future: Vivid Red (255, 60, 60)
        QRgb tint = (relativeOffset < 0) ? qRgb(0, 160, 255) : qRgb(255, 60, 60);
        int tintR = qRed(tint);
        int tintG = qGreen(tint);
        int tintB = qBlue(tint);

        for (int y = 0; y < copyH; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb*>(srcArgb.constScanLine(y));
            QRgb *dstLine = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < copyW; ++x) {
                int srcA = qAlpha(srcLine[x]);
                if (srcA == 0) continue;
                int lum = qGray(srcLine[x]);
                int r = std::clamp((tintR * (lum + 128)) / 384, 0, 255);
                int g = std::clamp((tintG * (lum + 128)) / 384, 0, 255);
                int b = std::clamp((tintB * (lum + 128)) / 384, 0, 255);
                int a = std::clamp(static_cast<int>(std::round(srcA * effOpacity)), 0, 255);
                dstLine[x] = qRgba(r, g, b, a);
            }
        }
        break;
    }

    case OnionSkinEffect::EdgeDetection: {
        // Border / Contour 1px only
        QRgb tint = (relativeOffset < 0) ? qRgb(0, 220, 255) : qRgb(255, 80, 80);
        int tintR = qRed(tint);
        int tintG = qGreen(tint);
        int tintB = qBlue(tint);

        for (int y = 0; y < copyH; ++y) {
            QRgb *dstLine = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < copyW; ++x) {
                int srcA = qAlpha(srcArgb.pixel(x, y));
                if (srcA <= 20) continue;

                // 4-connected boundary check
                bool isBorder = (x == 0 || x == srcArgb.width() - 1 || y == 0 || y == srcArgb.height() - 1);
                if (!isBorder) {
                    if (qAlpha(srcArgb.pixel(x - 1, y)) <= 20 ||
                        qAlpha(srcArgb.pixel(x + 1, y)) <= 20 ||
                        qAlpha(srcArgb.pixel(x, y - 1)) <= 20 ||
                        qAlpha(srcArgb.pixel(x, y + 1)) <= 20) {
                        isBorder = true;
                    }
                }

                if (isBorder) {
                    int a = std::clamp(static_cast<int>(std::round(255 * effOpacity)), 0, 255);
                    dstLine[x] = qRgba(tintR, tintG, tintB, a);
                }
            }
        }
        break;
    }

    case OnionSkinEffect::ChannelR: {
        for (int y = 0; y < copyH; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb*>(srcArgb.constScanLine(y));
            QRgb *dstLine = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < copyW; ++x) {
                int srcA = qAlpha(srcLine[x]);
                if (srcA == 0) continue;
                int r = qRed(srcLine[x]);
                int a = std::clamp(static_cast<int>(std::round(srcA * effOpacity)), 0, 255);
                dstLine[x] = qRgba(r, 0, 0, a);
            }
        }
        break;
    }

    case OnionSkinEffect::ChannelG: {
        for (int y = 0; y < copyH; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb*>(srcArgb.constScanLine(y));
            QRgb *dstLine = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < copyW; ++x) {
                int srcA = qAlpha(srcLine[x]);
                if (srcA == 0) continue;
                int g = qGreen(srcLine[x]);
                int a = std::clamp(static_cast<int>(std::round(srcA * effOpacity)), 0, 255);
                dstLine[x] = qRgba(0, g, 0, a);
            }
        }
        break;
    }

    case OnionSkinEffect::ChannelB: {
        for (int y = 0; y < copyH; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb*>(srcArgb.constScanLine(y));
            QRgb *dstLine = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < copyW; ++x) {
                int srcA = qAlpha(srcLine[x]);
                if (srcA == 0) continue;
                int b = qBlue(srcLine[x]);
                int a = std::clamp(static_cast<int>(std::round(srcA * effOpacity)), 0, 255);
                dstLine[x] = qRgba(0, 0, b, a);
            }
        }
        break;
    }

    case OnionSkinEffect::Silhouette: {
        for (int y = 0; y < copyH; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb*>(srcArgb.constScanLine(y));
            QRgb *dstLine = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < copyW; ++x) {
                int srcA = qAlpha(srcLine[x]);
                if (srcA == 0) continue;
                int a = std::clamp(static_cast<int>(std::round(srcA * effOpacity * 0.85)), 0, 255);
                dstLine[x] = qRgba(225, 230, 240, a);
            }
        }
        break;
    }

    case OnionSkinEffect::TrueColor: {
        for (int y = 0; y < copyH; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb*>(srcArgb.constScanLine(y));
            QRgb *dstLine = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < copyW; ++x) {
                int srcA = qAlpha(srcLine[x]);
                if (srcA == 0) continue;
                int a = std::clamp(static_cast<int>(std::round(srcA * effOpacity)), 0, 255);
                dstLine[x] = qRgba(qRed(srcLine[x]), qGreen(srcLine[x]), qBlue(srcLine[x]), a);
            }
        }
        break;
    }
    }

    return out;
}

void PixelCanvas::updateOnionSkinComposite()
{
    if (!m_onionSkinEnabled || m_image.isNull() || m_onionSkinLayers.isEmpty() || m_onionSkinOpacityPercent <= 0) {
        m_onionSkinComposite = QImage();
        update();
        return;
    }

    QImage composite(canvasSize(), QImage::Format_ARGB32);
    composite.fill(Qt::transparent);

    // Sort layers: draw furthest first (dist=3, then 2, then 1 so closer layers overlay nicely)
    QVector<OnionSkinLayer> sorted = m_onionSkinLayers;
    std::sort(sorted.begin(), sorted.end(), [](const OnionSkinLayer &a, const OnionSkinLayer &b) {
        return std::abs(a.relativeOffset) > std::abs(b.relativeOffset);
    });

    QPainter painter(&composite);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (const auto &layer : sorted) {
        if (layer.image.isNull() || layer.relativeOffset == 0) continue;
        QImage processed = processOnionSkinLayer(layer.image, layer.relativeOffset, m_onionSkinOpacityPercent, m_onionSkinEffect, layer.image.size());
        if (!processed.isNull()) {
            painter.drawImage(layer.alignmentOffset, processed);
        }
    }
    painter.end();

    m_onionSkinComposite = composite;
    update();
}

void PixelCanvas::drawOnionSkins(QPainter &painter)
{
    if (!m_onionSkinEnabled || m_onionSkinComposite.isNull()) return;
    QRect targetRect(0, 0, width(), height());
    painter.drawImage(targetRect, m_onionSkinComposite);
}

QImage PixelCanvas::image() const
{
    if (m_hasFloating) {
        QImage copy = m_image;
        QPainter p(&copy);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.drawImage(m_floatingPixelPos, m_floatingImage);
        p.end();
        return copy;
    }
    return m_image;
}

void PixelCanvas::setCurrentTool(PixelTool tool)
{
    commitFloatingSelection();
    m_tool = tool;
    if (m_tool == PixelTool::PolygonEdit) {
        if (m_meshVertices.isEmpty() && m_polygonMesh.size() >= 3) {
            m_meshVertices = m_polygonMesh.toList();
            m_meshTriangles = BentoPackGeometry::Triangulator::triangulate(m_polygonMesh);
        }
    } else if (m_tool == PixelTool::PivotEdit) {
        m_showPivot = true;
    }
    update();
}

void PixelCanvas::setPrimaryColor(const QColor &color)
{
    m_primaryColor = color;
    emit primaryColorChanged(m_primaryColor);
}

void PixelCanvas::setSecondaryColor(const QColor &color)
{
    m_secondaryColor = color;
    emit secondaryColorChanged(m_secondaryColor);
}

void PixelCanvas::swapColors()
{
    std::swap(m_primaryColor, m_secondaryColor);
    emit primaryColorChanged(m_primaryColor);
    emit secondaryColorChanged(m_secondaryColor);
}

void PixelCanvas::setZoom(double zoom)
{
    double clamped = std::clamp(zoom, 1.0, 64.0);
    if (std::abs(m_zoom - clamped) > 0.001) {
        m_zoom = clamped;
        updateCanvasSize();
        emit zoomChanged(m_zoom);
        update();
    }
}

void PixelCanvas::zoomIn()
{
    if (m_zoom < 4.0) setZoom(m_zoom + 1.0);
    else if (m_zoom < 16.0) setZoom(m_zoom + 2.0);
    else setZoom(m_zoom + 4.0);
}

void PixelCanvas::zoomOut()
{
    if (m_zoom <= 4.0) setZoom(std::max(1.0, m_zoom - 1.0));
    else if (m_zoom <= 16.0) setZoom(m_zoom - 2.0);
    else setZoom(m_zoom - 4.0);
}

void PixelCanvas::zoomFit(const QSize &viewportSize)
{
    QSize sz = canvasSize();
    if (sz.isEmpty() || viewportSize.width() <= 0 || viewportSize.height() <= 0) return;
    double scaleX = static_cast<double>(viewportSize.width() - 32) / std::max(1, sz.width());
    double scaleY = static_cast<double>(viewportSize.height() - 32) / std::max(1, sz.height());
    double best = std::floor(std::min(scaleX, scaleY));
    setZoom(std::clamp(best, 1.0, 32.0));
}

void PixelCanvas::setShowGrid(bool show)
{
    if (m_showGrid != show) {
        m_showGrid = show;
        update();
    }
}

bool PixelCanvas::hasSelection() const
{
    return !m_selectionRect.isNull() && m_selectionRect.isValid();
}

void PixelCanvas::selectAll()
{
    commitFloatingSelection();
    if (m_image.isNull()) return;
    m_selectionRect = m_image.rect();
    m_selectionMask.fill(true, m_image.width() * m_image.height());
    emit selectionStateChanged(true);
    emit selectionGeometryChanged(m_selectionRect, m_image.width() * m_image.height());
    update();
}

void PixelCanvas::deselect()
{
    commitFloatingSelection();
    m_selectionRect = QRect();
    m_selectionMask.clear();
    emit selectionStateChanged(false);
    emit selectionGeometryChanged(QRect(), 0);
    update();
}

void PixelCanvas::invertSelection()
{
    if (m_image.isNull()) return;
    commitFloatingSelection();

    int w = m_image.width();
    int h = m_image.height();
    if (m_selectionMask.size() != w * h) {
        m_selectionMask.resize(w * h);
        if (!hasSelection()) {
            m_selectionMask.fill(false);
        } else {
            m_selectionMask.fill(false);
            for (int y = m_selectionRect.top(); y <= m_selectionRect.bottom(); ++y) {
                for (int x = m_selectionRect.left(); x <= m_selectionRect.right(); ++x) {
                    if (isPixelInside(x, y)) {
                        m_selectionMask[y * w + x] = true;
                    }
                }
            }
        }
    }

    int minX = w, maxX = -1, minY = h, maxY = -1;
    int pixelCount = 0;
    for (int i = 0; i < m_selectionMask.size(); ++i) {
        m_selectionMask[i] = !m_selectionMask[i];
        if (m_selectionMask[i]) {
            pixelCount++;
            int x = i % w;
            int y = i / w;
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }

    if (pixelCount > 0 && maxX >= minX && maxY >= minY) {
        m_selectionRect = QRect(QPoint(minX, minY), QPoint(maxX, maxY));
        emit selectionStateChanged(true);
        emit selectionGeometryChanged(m_selectionRect, pixelCount);
    } else {
        deselect();
    }
    update();
}

void PixelCanvas::clearSelection()
{
    if (isLayerLocked()) {
        emit layerLockedAttempted();
        return;
    }
    commitFloatingSelection();
    if (m_image.isNull()) return;
    if (hasLayers()) ensureActiveCelAllocated();
    QImage oldImg = hasLayers() ? activeLayerImage() : m_image;
    bool anyChanged = false;

    if (hasSelection()) {
        for (int y = m_selectionRect.top(); y <= m_selectionRect.bottom(); ++y) {
            for (int x = m_selectionRect.left(); x <= m_selectionRect.right(); ++x) {
                if (isPixelEditable(x, y)) {
                    QColor curCol = hasLayers() ? m_layers[m_activeLayerIndex].image.pixelColor(x, y) : m_image.pixelColor(x, y);
                    if (curCol.alpha() != 0) {
                        if (hasLayers()) {
                            m_layers[m_activeLayerIndex].image.setPixelColor(x, y, Qt::transparent);
                        } else {
                            m_image.setPixelColor(x, y, Qt::transparent);
                        }
                        anyChanged = true;
                    }
                }
            }
        }
    } else {
        for (int y = 0; y < m_image.height(); ++y) {
            for (int x = 0; x < m_image.width(); ++x) {
                if (isPixelEditable(x, y)) {
                    QColor curCol = hasLayers() ? m_layers[m_activeLayerIndex].image.pixelColor(x, y) : m_image.pixelColor(x, y);
                    if (curCol.alpha() != 0) {
                        if (hasLayers()) {
                            m_layers[m_activeLayerIndex].image.setPixelColor(x, y, Qt::transparent);
                        } else {
                            m_image.setPixelColor(x, y, Qt::transparent);
                        }
                        anyChanged = true;
                    }
                }
            }
        }
    }

    if (anyChanged) {
        if (hasLayers()) recomposite();
        m_lastActionData = CanvasActionData();
        m_lastActionData.action = CanvasAction::Clear;
        m_lastActionData.hasSelection = hasSelection();
        m_lastActionData.selectionRect = m_selectionRect;
        pushSnapshot(oldImg, tr("Clear Pixels"), QPolygonF(), CanvasAction::Clear);
        emit imageChanged();
        update();
    }
}

void PixelCanvas::copySelection()
{
    commitFloatingSelection();
    if (m_image.isNull()) return;

    QImage srcImg = (hasLayers() && !m_sampleAllLayers) ? activeLayerImage() : m_image;
    if (srcImg.isNull()) return;

    if (hasSelection()) {
        QRect r = m_selectionRect.intersected(srcImg.rect());
        if (r.isEmpty()) return;
        QImage sub(r.size(), QImage::Format_ARGB32);
        sub.fill(Qt::transparent);
        for (int y = 0; y < r.height(); ++y) {
            for (int x = 0; x < r.width(); ++x) {
                int px = r.x() + x;
                int py = r.y() + y;
                if (isPixelSelected(px, py)) {
                    sub.setPixelColor(x, y, srcImg.pixelColor(px, py));
                }
            }
        }
        s_clipboardImage = sub;
    } else {
        s_clipboardImage = srcImg.copy();
    }
}

void PixelCanvas::cutSelection()
{
    copySelection();
    clearSelection();
}

void PixelCanvas::pasteClipboard()
{
    if (s_clipboardImage.isNull()) return;
    commitFloatingSelection();

    m_hasFloating = true;
    m_floatingImage = s_clipboardImage;
    // Center floating image in view
    int fx = std::max(0, (m_image.width() - m_floatingImage.width()) / 2);
    int fy = std::max(0, (m_image.height() - m_floatingImage.height()) / 2);
    m_floatingPixelPos = QPoint(fx, fy);
    deselect();
    update();
}

void PixelCanvas::commitFloatingSelection()
{
    if (!m_hasFloating) return;

    if (isLayerLocked()) {
        emit layerLockedAttempted();
        m_hasFloating = false;
        m_floatingImage = QImage();
        update();
        return;
    }

    if (hasLayers()) ensureActiveCelAllocated();
    QImage oldImg = hasLayers() ? activeLayerImage() : m_image;

    if (!m_allowEditingOutsidePolygon && hasPolygonMesh()) {
        int fw = m_floatingImage.width();
        int fh = m_floatingImage.height();
        for (int y = 0; y < fh; ++y) {
            for (int x = 0; x < fw; ++x) {
                int tx = m_floatingPixelPos.x() + x;
                int ty = m_floatingPixelPos.y() + y;
                if (isPixelEditable(tx, ty)) {
                    QColor srcCol = m_floatingImage.pixelColor(x, y);
                    if (srcCol.alpha() > 0) {
                        if (srcCol.alpha() == 255) {
                            if (hasLayers()) {
                                m_layers[m_activeLayerIndex].image.setPixelColor(tx, ty, srcCol);
                            } else {
                                m_image.setPixelColor(tx, ty, srcCol);
                            }
                        } else {
                            QColor dstCol = hasLayers() ? m_layers[m_activeLayerIndex].image.pixelColor(tx, ty) : m_image.pixelColor(tx, ty);
                            int a = srcCol.alpha();
                            int invA = 255 - a;
                            int outA = a + (dstCol.alpha() * invA) / 255;
                            if (outA > 0) {
                                int r = (srcCol.red() * a + dstCol.red() * dstCol.alpha() * invA / 255) / outA;
                                int g = (srcCol.green() * a + dstCol.green() * dstCol.alpha() * invA / 255) / outA;
                                int b = (srcCol.blue() * a + dstCol.blue() * dstCol.alpha() * invA / 255) / outA;
                                if (hasLayers()) {
                                    m_layers[m_activeLayerIndex].image.setPixelColor(tx, ty, QColor(r, g, b, outA));
                                } else {
                                    m_image.setPixelColor(tx, ty, QColor(r, g, b, outA));
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        if (hasLayers()) {
            QPainter p(&m_layers[m_activeLayerIndex].image);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.drawImage(m_floatingPixelPos, m_floatingImage);
            p.end();
        } else {
            QPainter p(&m_image);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.drawImage(m_floatingPixelPos, m_floatingImage);
            p.end();
        }
    }

    if (hasLayers()) recomposite();

    m_lastActionData = CanvasActionData();
    m_lastActionData.action = CanvasAction::Paste;
    m_lastActionData.hasSelection = hasSelection();
    m_lastActionData.selectionRect = m_selectionRect;
    m_lastActionData.pos = m_floatingPixelPos;
    m_hasFloating = false;
    m_floatingImage = QImage();
    pushSnapshot(oldImg, tr("Paste"), QPolygonF(), CanvasAction::Paste);
    emit imageChanged();
    update();
}

void PixelCanvas::flipHorizontal()
{
    if (isLayerLocked()) {
        emit layerLockedAttempted();
        return;
    }
    commitFloatingSelection();
    if (m_image.isNull()) return;
    if (hasLayers()) ensureActiveCelAllocated();
    QImage oldImg = hasLayers() ? activeLayerImage() : m_image;
    QPolygonF oldPoly = m_polygonMesh;

    if (hasSelection()) {
        QRect r = m_selectionRect.intersected(m_image.rect());
        for (int y = r.top(); y <= r.bottom(); ++y) {
            for (int x = 0; x < r.width() / 2; ++x) {
                int leftX = r.left() + x;
                int rightX = r.right() - x;
                if (hasLayers()) {
                    QColor temp = m_layers[m_activeLayerIndex].image.pixelColor(leftX, y);
                    m_layers[m_activeLayerIndex].image.setPixelColor(leftX, y, m_layers[m_activeLayerIndex].image.pixelColor(rightX, y));
                    m_layers[m_activeLayerIndex].image.setPixelColor(rightX, y, temp);
                } else {
                    QColor temp = m_image.pixelColor(leftX, y);
                    m_image.setPixelColor(leftX, y, m_image.pixelColor(rightX, y));
                    m_image.setPixelColor(rightX, y, temp);
                }
            }
        }
        if (!m_allowEditingOutsidePolygon && hasPolygonMesh() && !m_polygonMask.isEmpty()) {
            for (int y = r.top(); y <= r.bottom(); ++y) {
                for (int x = r.left(); x <= r.right(); ++x) {
                    if (!isPixelInsidePolygon(x, y)) {
                        if (hasLayers()) {
                            m_layers[m_activeLayerIndex].image.setPixelColor(x, y, oldImg.pixelColor(x, y));
                        } else {
                            m_image.setPixelColor(x, y, oldImg.pixelColor(x, y));
                        }
                    }
                }
            }
        }
    } else {
        if (hasLayers()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            m_layers[m_activeLayerIndex].image = m_layers[m_activeLayerIndex].image.flipped(Qt::Horizontal);
#else
            m_layers[m_activeLayerIndex].image = m_layers[m_activeLayerIndex].image.mirrored(true, false);
#endif
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            m_image = m_image.flipped(Qt::Horizontal);
#else
            m_image = m_image.mirrored(true, false);
#endif
        }
        if (hasPolygonMesh()) {
            QPolygonF flippedPoly;
            flippedPoly.reserve(m_polygonMesh.size());
            double w = m_image.width();
            for (const QPointF &pt : m_polygonMesh) {
                flippedPoly.append(QPointF(w - pt.x(), pt.y()));
            }
            m_polygonMesh = flippedPoly;
            updatePolygonMask();
            emit polygonMeshChanged(m_polygonMesh);
        }
    }

    if (hasLayers()) recomposite();

    m_lastActionData = CanvasActionData();
    m_lastActionData.action = CanvasAction::FlipHorizontal;
    m_lastActionData.hasSelection = hasSelection();
    m_lastActionData.selectionRect = m_selectionRect;
    pushSnapshot(oldImg, tr("Flip Horizontal"), oldPoly, CanvasAction::FlipHorizontal);
    emit imageChanged();
    update();
}

void PixelCanvas::flipVertical()
{
    if (isLayerLocked()) {
        emit layerLockedAttempted();
        return;
    }
    commitFloatingSelection();
    if (m_image.isNull()) return;
    if (hasLayers()) ensureActiveCelAllocated();
    QImage oldImg = hasLayers() ? activeLayerImage() : m_image;
    QPolygonF oldPoly = m_polygonMesh;

    if (hasSelection()) {
        QRect r = m_selectionRect.intersected(m_image.rect());
        for (int x = r.left(); x <= r.right(); ++x) {
            for (int y = 0; y < r.height() / 2; ++y) {
                int topY = r.top() + y;
                int botY = r.bottom() - y;
                if (hasLayers()) {
                    QColor temp = m_layers[m_activeLayerIndex].image.pixelColor(x, topY);
                    m_layers[m_activeLayerIndex].image.setPixelColor(x, topY, m_layers[m_activeLayerIndex].image.pixelColor(x, botY));
                    m_layers[m_activeLayerIndex].image.setPixelColor(x, botY, temp);
                } else {
                    QColor temp = m_image.pixelColor(x, topY);
                    m_image.setPixelColor(x, topY, m_image.pixelColor(x, botY));
                    m_image.setPixelColor(x, botY, temp);
                }
            }
        }
        if (!m_allowEditingOutsidePolygon && hasPolygonMesh() && !m_polygonMask.isEmpty()) {
            for (int y = r.top(); y <= r.bottom(); ++y) {
                for (int x = r.left(); x <= r.right(); ++x) {
                    if (!isPixelInsidePolygon(x, y)) {
                        if (hasLayers()) {
                            m_layers[m_activeLayerIndex].image.setPixelColor(x, y, oldImg.pixelColor(x, y));
                        } else {
                            m_image.setPixelColor(x, y, oldImg.pixelColor(x, y));
                        }
                    }
                }
            }
        }
    } else {
        if (hasLayers()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            m_layers[m_activeLayerIndex].image = m_layers[m_activeLayerIndex].image.flipped(Qt::Vertical);
#else
            m_layers[m_activeLayerIndex].image = m_layers[m_activeLayerIndex].image.mirrored(false, true);
#endif
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            m_image = m_image.flipped(Qt::Vertical);
#else
            m_image = m_image.mirrored(false, true);
#endif
        }
        if (hasPolygonMesh()) {
            QPolygonF flippedPoly;
            flippedPoly.reserve(m_polygonMesh.size());
            double h = m_image.height();
            for (const QPointF &pt : m_polygonMesh) {
                flippedPoly.append(QPointF(pt.x(), h - pt.y()));
            }
            m_polygonMesh = flippedPoly;
            updatePolygonMask();
            emit polygonMeshChanged(m_polygonMesh);
        }
    }

    if (hasLayers()) recomposite();

    m_lastActionData = CanvasActionData();
    m_lastActionData.action = CanvasAction::FlipVertical;
    m_lastActionData.hasSelection = hasSelection();
    m_lastActionData.selectionRect = m_selectionRect;
    pushSnapshot(oldImg, tr("Flip Vertical"), oldPoly, CanvasAction::FlipVertical);
    emit imageChanged();
    update();
}

void PixelCanvas::rotate90CW()
{
    if (isLayerLocked()) {
        emit layerLockedAttempted();
        return;
    }
    commitFloatingSelection();
    if (m_image.isNull()) return;
    if (hasLayers()) ensureActiveCelAllocated();
    QImage oldImg = hasLayers() ? activeLayerImage() : m_image;
    QPolygonF oldPoly = m_polygonMesh;
    int oldH = m_image.height();

    QTransform t;
    t.rotate(90.0);
    if (hasLayers()) {
        for (int i = 0; i < m_layers.size(); ++i) {
            if (!m_layers[i].image.isNull()) {
                m_layers[i].image = m_layers[i].image.transformed(t).convertToFormat(QImage::Format_ARGB32);
            }
        }
        recomposite();
    } else {
        m_image = m_image.transformed(t).convertToFormat(QImage::Format_ARGB32);
    }

    if (hasPolygonMesh()) {
        QPolygonF rotatedPoly;
        rotatedPoly.reserve(m_polygonMesh.size());
        for (const QPointF &pt : m_polygonMesh) {
            rotatedPoly.append(QPointF(static_cast<double>(oldH) - pt.y(), pt.x()));
        }
        m_polygonMesh = rotatedPoly;
        updatePolygonMask();
        emit polygonMeshChanged(m_polygonMesh);
    }

    deselect();
    if (m_canvasSize.isValid() && !m_canvasSize.isEmpty()) {
        m_canvasSize = m_canvasSize.expandedTo(m_image.size());
    }
    updateCanvasSize();
    m_lastActionData = CanvasActionData();
    m_lastActionData.action = CanvasAction::Rotate90CW;
    m_lastActionData.hasSelection = hasSelection();
    m_lastActionData.selectionRect = m_selectionRect;
    pushSnapshot(oldImg, tr("Rotate 90°"), oldPoly, CanvasAction::Rotate90CW);
    emit imageChanged();
    update();
}

void PixelCanvas::pushSnapshot(const QImage &oldImage, const QString &text, const QPolygonF &oldPolygon, CanvasAction action)
{
    QImage curImg = hasLayers() ? activeLayerImage() : m_image;
    if (action == CanvasAction::FloodFill && (m_lastActionData.action != CanvasAction::FloodFill || !m_lastActionData.color.isValid())) {
        m_lastActionData = CanvasActionData();
        m_lastActionData.action = CanvasAction::FloodFill;
        m_lastActionData.hasSelection = hasSelection();
        m_lastActionData.selectionRect = m_selectionRect;
        for (int y = 0; y < curImg.height(); ++y) {
            for (int x = 0; x < curImg.width(); ++x) {
                if (oldImage.isNull() || curImg.pixel(x, y) != oldImage.pixel(x, y)) {
                    m_lastActionData.pos = QPoint(x, y);
                    m_lastActionData.color = QColor(curImg.pixel(x, y));
                    break;
                }
            }
            if (m_lastActionData.color.isValid()) break;
        }
    }
    QPolygonF oldP = oldPolygon.isEmpty() ? m_polygonMesh : oldPolygon;
    int layerIdx = hasLayers() ? m_activeLayerIndex : -1;
    auto *cmd = new PixelCanvasUndoCommand(this, layerIdx, oldImage, curImg, oldP, m_polygonMesh, text);
    if (cmd->isEmpty()) {
        delete cmd;
        return;
    }
    emit modificationPushed(oldImage, curImg, oldP, m_polygonMesh, action, cmd);
    m_undoStack.push(cmd);
}

void PixelCanvas::pushMeshSnapshot(const QString &text, const QPolygonF &oldPoly, const QList<QPointF> &oldVerts, const QList<int> &oldTris)
{
    QImage curImg = hasLayers() ? activeLayerImage() : m_image;
    auto *cmd = new PixelCanvasUndoCommand(this, hasLayers() ? m_activeLayerIndex : -1,
                                           curImg, curImg,
                                           oldPoly, m_polygonMesh,
                                           text,
                                           oldVerts, m_meshVertices,
                                           oldTris, m_meshTriangles);
    if (cmd->isEmpty()) {
        delete cmd;
        return;
    }
    m_undoStack.push(cmd);
}

void PixelCanvas::applyPatch(const QRect &rect, const QImage &patch)
{
    if (hasLayers()) {
        applyLayerPatch(m_activeLayerIndex, rect, patch);
        return;
    }
    if (m_image.isNull() || rect.isEmpty() || patch.isNull()) return;

    QPainter p(&m_image);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(rect.topLeft(), patch);
    p.end();

    emit imageChanged();

    QRect widgetDirty = pixelToWidget(rect);
    widgetDirty.adjust(-2, -2, 2, 2);
    update(widgetDirty);
}

QSize PixelCanvas::canvasSize() const
{
    if (m_canvasSize.isValid() && !m_canvasSize.isEmpty()) {
        return m_canvasSize;
    }
    return m_image.isNull() ? QSize(32, 32) : m_image.size();
}

void PixelCanvas::setCanvasEnvelope(const QSize &canvasSize, const QPoint &imageOffset, const QPoint &pivotPos)
{
    m_canvasSize = canvasSize;
    m_imageOffset = imageOffset;
    m_pivotPos = pivotPos;
    updateCanvasSize();
    updateOnionSkinComposite();
    update();
}

QPoint PixelCanvas::framePivot() const
{
    return m_pivotPos - m_imageOffset;
}

void PixelCanvas::setFramePivot(const QPoint &pivot)
{
    m_pivotPos = pivot + m_imageOffset;
    update();
}

void PixelCanvas::setShowPivot(bool show)
{
    if (m_showPivot != show) {
        m_showPivot = show;
        update();
    }
}

void PixelCanvas::setShowPivotGuidelines(bool show)
{
    if (m_showPivotGuidelines != show) {
        m_showPivotGuidelines = show;
        update();
    }
}

void PixelCanvas::updateCanvasSize()
{
    QSize sz = canvasSize();
    if (sz.isEmpty()) {
        setFixedSize(64, 64);
        return;
    }
    int w = std::max(1, static_cast<int>(std::round(sz.width() * m_zoom)));
    int h = std::max(1, static_cast<int>(std::round(sz.height() * m_zoom)));
    setFixedSize(w, h);
}

QPoint PixelCanvas::widgetToPixel(const QPoint &widgetPos) const
{
    if (m_zoom <= 0.0) return QPoint(-1, -1);
    int px = static_cast<int>(std::floor(widgetPos.x() / m_zoom)) - m_imageOffset.x();
    int py = static_cast<int>(std::floor(widgetPos.y() / m_zoom)) - m_imageOffset.y();
    return QPoint(px, py);
}

QPoint PixelCanvas::pixelToWidget(const QPoint &pixelPos) const
{
    return QPoint(static_cast<int>(std::floor((pixelPos.x() + m_imageOffset.x()) * m_zoom)),
                  static_cast<int>(std::floor((pixelPos.y() + m_imageOffset.y()) * m_zoom)));
}

QRect PixelCanvas::pixelToWidget(const QRect &pixelRect) const
{
    if (m_zoom <= 0.0 || pixelRect.isEmpty()) return QRect();
    int x1 = static_cast<int>(std::floor((pixelRect.left() + m_imageOffset.x()) * m_zoom));
    int y1 = static_cast<int>(std::floor((pixelRect.top() + m_imageOffset.y()) * m_zoom));
    int x2 = static_cast<int>(std::ceil((pixelRect.right() + 1 + m_imageOffset.x()) * m_zoom));
    int y2 = static_cast<int>(std::ceil((pixelRect.bottom() + 1 + m_imageOffset.y()) * m_zoom));
    return QRect(x1, y1, x2 - x1, y2 - y1);
}

bool PixelCanvas::isPixelInside(int x, int y) const
{
    return x >= 0 && x < m_image.width() && y >= 0 && y < m_image.height();
}

bool PixelCanvas::isPixelSelected(int x, int y) const
{
    if (!hasSelection()) return true;
    if (m_selectionMask.isEmpty()) {
        return m_selectionRect.contains(x, y);
    }
    int idx = y * m_image.width() + x;
    return (idx >= 0 && idx < m_selectionMask.size()) ? m_selectionMask[idx] : false;
}

bool PixelCanvas::isPixelInsidePolygon(int x, int y) const
{
    if (!hasPolygonMesh()) return true;
    if (x < 0 || x >= m_image.width() || y < 0 || y >= m_image.height()) return false;
    if (m_polygonMask.isEmpty()) {
        return m_polygonMesh.containsPoint(QPointF(x + 0.5, y + 0.5), Qt::OddEvenFill);
    }
    int idx = y * m_image.width() + x;
    return (idx >= 0 && idx < m_polygonMask.size()) ? m_polygonMask[idx] : false;
}

bool PixelCanvas::isPixelEditable(int x, int y) const
{
    if (!isPixelInside(x, y)) return false;
    if (isLayerLocked()) return false;
    if (!isPixelSelected(x, y)) return false;
    if (!m_allowEditingOutsidePolygon && hasPolygonMesh()) {
        if (!isPixelInsidePolygon(x, y)) return false;
    }
    return true;
}

void PixelCanvas::drawPixelBrush(int centerX, int centerY, const QColor &color, int brushSize, int opacity)
{
    BrushShape activeShape = (color == Qt::transparent) ? m_eraserShape : m_brushShape;

    if (brushSize <= 1) {
        if (!isPixelEditable(centerX, centerY)) return;
        QColor effectiveColor = color;
        if (color == Qt::transparent) {
            if (opacity >= 100) {
                if (hasLayers()) {
                    if (m_eraseAllLayers) {
                        for (auto &l : m_layers) {
                            if (l.visible && !l.locked) l.image.setPixelColor(centerX, centerY, Qt::transparent);
                        }
                    } else {
                        m_layers[m_activeLayerIndex].image.setPixelColor(centerX, centerY, Qt::transparent);
                    }
                } else {
                    m_image.setPixelColor(centerX, centerY, Qt::transparent);
                }
            } else {
                if (hasLayers()) {
                    if (m_eraseAllLayers) {
                        for (auto &l : m_layers) {
                            if (l.visible && !l.locked) {
                                QColor cur = l.image.pixelColor(centerX, centerY);
                                cur.setAlpha((cur.alpha() * (100 - opacity)) / 100);
                                l.image.setPixelColor(centerX, centerY, cur);
                            }
                        }
                    } else {
                        QColor cur = m_layers[m_activeLayerIndex].image.pixelColor(centerX, centerY);
                        cur.setAlpha((cur.alpha() * (100 - opacity)) / 100);
                        m_layers[m_activeLayerIndex].image.setPixelColor(centerX, centerY, cur);
                    }
                } else {
                    QColor cur = m_image.pixelColor(centerX, centerY);
                    cur.setAlpha((cur.alpha() * (100 - opacity)) / 100);
                    m_image.setPixelColor(centerX, centerY, cur);
                }
            }
        } else {
            if (opacity < 100) {
                effectiveColor.setAlpha((color.alpha() * opacity) / 100);
                QPainter p(hasLayers() ? &m_layers[m_activeLayerIndex].image : &m_image);
                p.setCompositionMode(QPainter::CompositionMode_SourceOver);
                p.fillRect(centerX, centerY, 1, 1, effectiveColor);
            } else {
                if (hasLayers()) m_layers[m_activeLayerIndex].image.setPixelColor(centerX, centerY, color);
                else m_image.setPixelColor(centerX, centerY, color);
            }
        }
        return;
    }

    int half = brushSize / 2;
    int xStart = centerX - half;
    int yStart = centerY - half;
    int xEnd = xStart + brushSize;
    int yEnd = yStart + brushSize;
    double radiusSq = (brushSize / 2.0) * (brushSize / 2.0);

    for (int py = yStart; py < yEnd; ++py) {
        for (int px = xStart; px < xEnd; ++px) {
            if (activeShape == BrushShape::Circle) {
                double dx = (px - centerX);
                double dy = (py - centerY);
                if (dx * dx + dy * dy > radiusSq) continue;
            }
            if (!isPixelEditable(px, py)) continue;

            if (color == Qt::transparent) {
                if (opacity >= 100) {
                    if (hasLayers()) {
                        if (m_eraseAllLayers) {
                            for (auto &l : m_layers) {
                                if (l.visible && !l.locked) l.image.setPixelColor(px, py, Qt::transparent);
                            }
                        } else {
                            m_layers[m_activeLayerIndex].image.setPixelColor(px, py, Qt::transparent);
                        }
                    } else {
                        m_image.setPixelColor(px, py, Qt::transparent);
                    }
                } else {
                    if (hasLayers()) {
                        if (m_eraseAllLayers) {
                            for (auto &l : m_layers) {
                                if (l.visible && !l.locked) {
                                    QColor cur = l.image.pixelColor(px, py);
                                    cur.setAlpha((cur.alpha() * (100 - opacity)) / 100);
                                    l.image.setPixelColor(px, py, cur);
                                }
                            }
                        } else {
                            QColor cur = m_layers[m_activeLayerIndex].image.pixelColor(px, py);
                            cur.setAlpha((cur.alpha() * (100 - opacity)) / 100);
                            m_layers[m_activeLayerIndex].image.setPixelColor(px, py, cur);
                        }
                    } else {
                        QColor cur = m_image.pixelColor(px, py);
                        cur.setAlpha((cur.alpha() * (100 - opacity)) / 100);
                        m_image.setPixelColor(px, py, cur);
                    }
                }
            } else {
                if (opacity < 100) {
                    QColor effectiveColor = color;
                    effectiveColor.setAlpha((color.alpha() * opacity) / 100);
                    QPainter p(hasLayers() ? &m_layers[m_activeLayerIndex].image : &m_image);
                    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
                    p.fillRect(px, py, 1, 1, effectiveColor);
                } else {
                    if (hasLayers()) m_layers[m_activeLayerIndex].image.setPixelColor(px, py, color);
                    else m_image.setPixelColor(px, py, color);
                }
            }
        }
    }
}

void PixelCanvas::drawBresenhamLine(int x0, int y0, int x1, int y1, const QColor &color)
{
    if (isLayerLocked()) {
        emit layerLockedAttempted();
        return;
    }

    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    if (hasLayers()) {
        ensureActiveCelAllocated();
    }

    int bSize = (m_tool == PixelTool::Eraser) ? m_eraserSize : m_brushSize;
    int bOp = (m_tool == PixelTool::Eraser) ? m_eraserOpacity : m_brushOpacity;

    while (true) {
        drawPixelBrush(x0, y0, color, bSize, bOp);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }

    if (hasLayers()) {
        recomposite();
    }
}

void PixelCanvas::applyFloodFill(int startX, int startY, const QColor &replacementColor)
{
    if (isLayerLocked()) {
        emit layerLockedAttempted();
        return;
    }
    if (!isPixelEditable(startX, startY)) return;

    if (hasLayers()) {
        ensureActiveCelAllocated();
    }

    QImage sourceImage = (hasLayers() && !m_sampleAllLayers) ? activeLayerImage() : m_image;
    QRgb targetRgb = sourceImage.pixel(startX, startY);
    QColor effectiveColor = replacementColor;
    if (m_bucketOpacity < 100) {
        effectiveColor.setAlpha((replacementColor.alpha() * m_bucketOpacity) / 100);
    }
    QRgb replaceRgb = effectiveColor.rgba();
    if (targetRgb == replaceRgb && m_bucketThreshold == 0) return;

    QImage oldImg = hasLayers() ? activeLayerImage() : m_image;
    int w = m_image.width();
    int h = m_image.height();

    auto matchColor = [this, targetRgb](QRgb c) -> bool {
        int a1 = qAlpha(targetRgb);
        int a2 = qAlpha(c);
        if (a1 == 0 && a2 == 0) return m_bucketFillTransparent;
        if ((a1 == 0) != (a2 == 0)) {
            if (!m_bucketFillTransparent) return false;
            if (m_bucketThreshold == 0) return false;
        }
        if (m_bucketThreshold == 0) return targetRgb == c;
        int dr = qRed(targetRgb) - qRed(c);
        int dg = qGreen(targetRgb) - qGreen(c);
        int db = qBlue(targetRgb) - qBlue(c);
        int da = a1 - a2;
        return std::sqrt(dr * dr + dg * dg + db * db + da * da) <= (m_bucketThreshold * 2.0);
    };

    auto setPixel = [this, effectiveColor](int x, int y) {
        if (m_bucketOpacity >= 100) {
            if (hasLayers()) m_layers[m_activeLayerIndex].image.setPixelColor(x, y, effectiveColor);
            else m_image.setPixelColor(x, y, effectiveColor);
        } else {
            QPainter p(hasLayers() ? &m_layers[m_activeLayerIndex].image : &m_image);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.fillRect(x, y, 1, 1, effectiveColor);
        }
    };

    if (m_bucketMode == BucketFillMode::WholeSelection && hasSelection()) {
        for (int y = m_selectionRect.top(); y <= m_selectionRect.bottom(); ++y) {
            for (int x = m_selectionRect.left(); x <= m_selectionRect.right(); ++x) {
                if (isPixelEditable(x, y)) {
                    setPixel(x, y);
                }
            }
        }
    } else if (m_bucketMode == BucketFillMode::GlobalReplace) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (isPixelEditable(x, y) && matchColor(sourceImage.pixel(x, y))) {
                    setPixel(x, y);
                }
            }
        }
    } else {
        // SimilarColors: Flood fill BFS
        QVector<bool> visited(w * h, false);
        QQueue<QPoint> queue;

        queue.enqueue(QPoint(startX, startY));
        visited[startY * w + startX] = true;

        const int dx4[] = {-1, 1, 0, 0};
        const int dy4[] = {0, 0, -1, 1};
        const int dx8[] = {-1, 1, 0, 0, -1, 1, -1, 1};
        const int dy8[] = {0, 0, -1, 1, -1, -1, 1, 1};
        const int *dx = m_bucketDiagonal ? dx8 : dx4;
        const int *dy = m_bucketDiagonal ? dy8 : dy4;
        const int nDirs = m_bucketDiagonal ? 8 : 4;

        while (!queue.isEmpty()) {
            QPoint pt = queue.dequeue();
            int x = pt.x();
            int y = pt.y();

            setPixel(x, y);

            for (int i = 0; i < nDirs; ++i) {
                int nx = x + dx[i];
                int ny = y + dy[i];
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                    int idx = ny * w + nx;
                    if (!visited[idx] && isPixelEditable(nx, ny) && matchColor(sourceImage.pixel(nx, ny))) {
                        visited[idx] = true;
                        queue.enqueue(QPoint(nx, ny));
                    }
                }
            }
        }
    }

    if (hasLayers()) {
        recomposite();
    }

    m_lastActionData = CanvasActionData();
    m_lastActionData.action = CanvasAction::FloodFill;
    m_lastActionData.pos = QPoint(startX, startY);
    m_lastActionData.color = replacementColor;
    m_lastActionData.hasSelection = hasSelection();
    m_lastActionData.selectionRect = m_selectionRect;
    pushSnapshot(oldImg, tr("Flood Fill"), QPolygonF(), CanvasAction::FloodFill);
    emit imageChanged();
    update();
}

void PixelCanvas::applyColorSelection(int targetX, int targetY)
{
    if (!isPixelInside(targetX, targetY)) return;
    commitFloatingSelection();

    QImage source = (hasLayers() && !m_sampleAllLayers) ? activeLayerImage() : m_image;
    QRgb targetRgb = source.pixel(targetX, targetY);
    int w = source.width();
    int h = source.height();

    QVector<bool> newMask(w * h, false);

    auto matches = [this, targetRgb](QRgb c) -> bool {
        int a1 = qAlpha(targetRgb);
        int a2 = qAlpha(c);
        if (a1 == 0 && a2 == 0) return m_magicWandSelectTransparent;
        if ((a1 == 0) != (a2 == 0)) {
            if (!m_magicWandSelectTransparent) return false;
            if (m_magicWandThreshold == 0) return false;
        }
        if (m_magicWandThreshold == 0) return targetRgb == c;
        int dr = qRed(targetRgb) - qRed(c);
        int dg = qGreen(targetRgb) - qGreen(c);
        int db = qBlue(targetRgb) - qBlue(c);
        int da = a1 - a2;
        return std::sqrt(dr * dr + dg * dg + db * db + da * da) <= (m_magicWandThreshold * 2.0);
    };

    if (m_magicWandContiguous) {
        if (matches(source.pixel(targetX, targetY))) {
            QQueue<QPoint> queue;
            queue.enqueue(QPoint(targetX, targetY));
            newMask[targetY * w + targetX] = true;

            const int dx4[] = {-1, 1, 0, 0};
            const int dy4[] = {0, 0, -1, 1};
            const int dx8[] = {-1, 1, 0, 0, -1, 1, -1, 1};
            const int dy8[] = {0, 0, -1, 1, -1, -1, 1, 1};
            const int *dx = m_magicWandDiagonal ? dx8 : dx4;
            const int *dy = m_magicWandDiagonal ? dy8 : dy4;
            const int nDirs = m_magicWandDiagonal ? 8 : 4;

            while (!queue.isEmpty()) {
                QPoint pt = queue.dequeue();
                for (int i = 0; i < nDirs; ++i) {
                    int nx = pt.x() + dx[i];
                    int ny = pt.y() + dy[i];
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        int idx = ny * w + nx;
                        if (!newMask[idx] && matches(source.pixel(nx, ny))) {
                            newMask[idx] = true;
                            queue.enqueue(QPoint(nx, ny));
                        }
                    }
                }
            }
        }
    } else {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (matches(source.pixel(x, y))) {
                    newMask[y * w + x] = true;
                }
            }
        }
    }

    SelectionMode mode = effectiveSelectionMode();
    ensureSelectionMaskValid();

    if (mode == SelectionMode::Replace || m_selectionMask.size() != w * h) {
        m_selectionMask = newMask;
    } else if (mode == SelectionMode::Add) {
        for (int i = 0; i < w * h; ++i) {
            m_selectionMask[i] = m_selectionMask[i] || newMask[i];
        }
    } else if (mode == SelectionMode::Subtract) {
        for (int i = 0; i < w * h; ++i) {
            m_selectionMask[i] = m_selectionMask[i] && !newMask[i];
        }
    } else if (mode == SelectionMode::Intersect) {
        for (int i = 0; i < w * h; ++i) {
            m_selectionMask[i] = m_selectionMask[i] && newMask[i];
        }
    }

    int minX = w, maxX = -1, minY = h, maxY = -1;
    int pixelCount = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (m_selectionMask[y * w + x]) {
                pixelCount++;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (pixelCount > 0 && maxX >= minX && maxY >= minY) {
        m_selectionRect = QRect(QPoint(minX, minY), QPoint(maxX, maxY));
        emit selectionStateChanged(true);
        emit selectionGeometryChanged(m_selectionRect, pixelCount);
    } else {
        deselect();
    }
    update();
}

SelectionMode PixelCanvas::effectiveSelectionMode() const
{
    SelectionMode mode = m_selectionMode;
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    if (mods.testFlag(Qt::ControlModifier) && mods.testFlag(Qt::ShiftModifier)) {
        mode = SelectionMode::Intersect;
    } else if (mods.testFlag(Qt::ShiftModifier)) {
        mode = SelectionMode::Add;
    } else if (mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::AltModifier)) {
        mode = SelectionMode::Subtract;
    }
    return mode;
}

void PixelCanvas::ensureSelectionMaskValid()
{
    if (m_image.isNull()) return;
    int w = m_image.width();
    int h = m_image.height();
    if (m_selectionMask.size() != w * h) {
        m_selectionMask.resize(w * h);
        if (hasSelection()) {
            m_selectionMask.fill(false);
            for (int y = m_selectionRect.top(); y <= m_selectionRect.bottom(); ++y) {
                for (int x = m_selectionRect.left(); x <= m_selectionRect.right(); ++x) {
                    if (isPixelInside(x, y)) {
                        m_selectionMask[y * w + x] = true;
                    }
                }
            }
        } else {
            m_selectionMask.fill(false);
        }
    }
}

void PixelCanvas::updateSelectionPreview()
{
    if (m_image.isNull()) return;
    int w = m_image.width();
    int h = m_image.height();
    int totalPixels = w * h;

    if (m_selectionMask.size() != totalPixels) {
        m_selectionMask.resize(totalPixels);
    }

    SelectionMode mode = effectiveSelectionMode();

    if (mode == SelectionMode::Replace) {
        m_selectionMask.fill(false);
        if (!m_dragSelectionRect.isNull() && m_dragSelectionRect.isValid()) {
            int top = std::clamp(m_dragSelectionRect.top(), 0, h - 1);
            int bottom = std::clamp(m_dragSelectionRect.bottom(), 0, h - 1);
            int left = std::clamp(m_dragSelectionRect.left(), 0, w - 1);
            int right = std::clamp(m_dragSelectionRect.right(), 0, w - 1);
            for (int y = top; y <= bottom; ++y) {
                for (int x = left; x <= right; ++x) {
                    m_selectionMask[y * w + x] = true;
                }
            }
        }
    } else {
        if (m_preDragSelectionMask.size() == totalPixels) {
            m_selectionMask = m_preDragSelectionMask;
        } else {
            m_selectionMask.fill(false);
        }

        if (!m_dragSelectionRect.isNull() && m_dragSelectionRect.isValid()) {
            int top = std::clamp(m_dragSelectionRect.top(), 0, h - 1);
            int bottom = std::clamp(m_dragSelectionRect.bottom(), 0, h - 1);
            int left = std::clamp(m_dragSelectionRect.left(), 0, w - 1);
            int right = std::clamp(m_dragSelectionRect.right(), 0, w - 1);

            if (mode == SelectionMode::Add) {
                for (int y = top; y <= bottom; ++y) {
                    for (int x = left; x <= right; ++x) {
                        m_selectionMask[y * w + x] = true;
                    }
                }
            } else if (mode == SelectionMode::Subtract) {
                for (int y = top; y <= bottom; ++y) {
                    for (int x = left; x <= right; ++x) {
                        m_selectionMask[y * w + x] = false;
                    }
                }
            } else if (mode == SelectionMode::Intersect) {
                m_selectionMask.fill(false);
                if (m_preDragSelectionMask.size() == totalPixels) {
                    for (int y = top; y <= bottom; ++y) {
                        for (int x = left; x <= right; ++x) {
                            if (m_preDragSelectionMask[y * w + x]) {
                                m_selectionMask[y * w + x] = true;
                            }
                        }
                    }
                }
            }
        }
    }

    // Recompute bounding box & pixel count
    int minX = w, maxX = -1, minY = h, maxY = -1;
    int pixelCount = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (m_selectionMask[y * w + x]) {
                pixelCount++;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (pixelCount > 0 && maxX >= minX && maxY >= minY) {
        m_selectionRect = QRect(QPoint(minX, minY), QPoint(maxX, maxY));
        emit selectionStateChanged(true);
        emit selectionGeometryChanged(m_selectionRect, pixelCount);
    } else {
        m_selectionRect = QRect();
        emit selectionStateChanged(false);
        emit selectionGeometryChanged(QRect(), 0);
    }
}


void PixelCanvas::drawCheckerboard(QPainter &painter, const QRect &rect)
{
    const int tileSize = 8;
    QColor c1(40, 42, 48);
    QColor c2(54, 57, 65);

    int startX = (rect.left() / tileSize) * tileSize;
    int startY = (rect.top() / tileSize) * tileSize;

    for (int y = startY; y < rect.bottom(); y += tileSize) {
        for (int x = startX; x < rect.right(); x += tileSize) {
            bool alt = ((x / tileSize) + (y / tileSize)) % 2 == 0;
            painter.fillRect(x, y, tileSize, tileSize, alt ? c1 : c2);
        }
    }
}

void PixelCanvas::drawPixelGrid(QPainter &painter, const QRect &rect)
{
    Q_UNUSED(rect);
    if (!m_showGrid || m_zoom < 4.0 || m_image.isNull()) return;

    painter.save();
    painter.setPen(QColor(180, 180, 180, 45));

    int w = m_image.width();
    int h = m_image.height();

    for (int x = 0; x <= w; ++x) {
        int vx = static_cast<int>(std::round(x * m_zoom));
        painter.drawLine(vx, 0, vx, static_cast<int>(std::round(h * m_zoom)));
    }
    for (int y = 0; y <= h; ++y) {
        int vy = static_cast<int>(std::round(y * m_zoom));
        painter.drawLine(0, vy, static_cast<int>(std::round(w * m_zoom)), vy);
    }

    painter.restore();
}

void PixelCanvas::drawSelectionBorder(QPainter &painter)
{
    if (!hasSelection() || m_image.isNull()) return;

    painter.save();
    int w = m_image.width();
    int h = m_image.height();

    // 1. If we have a selection mask, draw pixel highlight overlay
    if (!m_selectionMask.isEmpty() && m_selectionMask.size() == w * h) {
        QColor selTint(56, 189, 248, 45); // Soft translucent cyan highlight
        for (int y = m_selectionRect.top(); y <= m_selectionRect.bottom(); ++y) {
            for (int x = m_selectionRect.left(); x <= m_selectionRect.right(); ++x) {
                if (m_selectionMask[y * w + x]) {
                    painter.fillRect(QRectF(x * m_zoom, y * m_zoom, m_zoom, m_zoom), selTint);
                }
            }
        }

        // 2. Extract contour boundary edges where selected meets unselected
        QVector<QLineF> contourEdges;
        contourEdges.reserve(m_selectionRect.width() * m_selectionRect.height() * 2);

        for (int y = m_selectionRect.top(); y <= m_selectionRect.bottom(); ++y) {
            for (int x = m_selectionRect.left(); x <= m_selectionRect.right(); ++x) {
                if (!m_selectionMask[y * w + x]) continue;

                // Top edge
                if (y == 0 || !m_selectionMask[(y - 1) * w + x]) {
                    contourEdges.append(QLineF(x * m_zoom, y * m_zoom, (x + 1) * m_zoom, y * m_zoom));
                }
                // Bottom edge
                if (y == h - 1 || !m_selectionMask[(y + 1) * w + x]) {
                    contourEdges.append(QLineF(x * m_zoom, (y + 1) * m_zoom, (x + 1) * m_zoom, (y + 1) * m_zoom));
                }
                // Left edge
                if (x == 0 || !m_selectionMask[y * w + (x - 1)]) {
                    contourEdges.append(QLineF(x * m_zoom, y * m_zoom, x * m_zoom, (y + 1) * m_zoom));
                }
                // Right edge
                if (x == w - 1 || !m_selectionMask[y * w + (x + 1)]) {
                    contourEdges.append(QLineF((x + 1) * m_zoom, y * m_zoom, (x + 1) * m_zoom, (y + 1) * m_zoom));
                }
            }
        }

        // Solid dark background line
        QPen darkPen(QColor(0, 0, 0, 220), 1.5, Qt::SolidLine, Qt::SquareCap);
        painter.setPen(darkPen);
        painter.drawLines(contourEdges);

        // Dashed bright marching ants foreground
        QPen dashPen(QColor(255, 255, 255, 240), 1.5, Qt::DashLine, Qt::SquareCap);
        dashPen.setDashPattern({4, 4});
        painter.setPen(dashPen);
        painter.drawLines(contourEdges);
    } else if (hasSelection()) {
        // Fallback for simple bounding rectangle
        QRectF r(m_selectionRect.x() * m_zoom,
                 m_selectionRect.y() * m_zoom,
                 (m_selectionRect.width() + 1) * m_zoom,
                 (m_selectionRect.height() + 1) * m_zoom);

        painter.setPen(QPen(QColor(0, 0, 0, 200), 1.5, Qt::SolidLine));
        painter.drawRect(r);
        QPen dashPen(QColor(255, 255, 255, 240), 1.5, Qt::DashLine);
        dashPen.setDashPattern({4, 4});
        painter.setPen(dashPen);
        painter.drawRect(r);
    }

    painter.restore();
}

void PixelCanvas::drawFloatingStamp(QPainter &painter)
{
    if (!m_hasFloating || m_floatingImage.isNull()) return;

    painter.save();
    QRectF r(m_floatingPixelPos.x() * m_zoom,
             m_floatingPixelPos.y() * m_zoom,
             m_floatingImage.width() * m_zoom,
             m_floatingImage.height() * m_zoom);

    painter.drawImage(r, m_floatingImage);

    // Floating border highlight
    painter.setPen(QPen(QColor(0, 180, 255, 220), 1.5, Qt::DashLine));
    painter.drawRect(r);
    painter.restore();
}

void PixelCanvas::setPolygonMesh(const QPolygonF &polygon)
{
    m_polygonMesh = polygon;
    if (polygon.size() >= 3) {
        m_meshVertices = polygon.toList();
        m_meshTriangles = BentoPackGeometry::Triangulator::triangulate(polygon);
    } else {
        m_meshVertices.clear();
        m_meshTriangles.clear();
    }
    m_selectedVertexIndex = -1;
    m_hoveredVertexIndex = -1;
    m_hoveredEdgeIndex = -1;
    updatePolygonMask();
    update();
    emit polygonMeshChanged(m_polygonMesh);
    emit meshDataChanged(m_polygonMesh, m_meshVertices, m_meshTriangles);
}

void PixelCanvas::setPolygonMeshData(const QPolygonF &polygon, const QList<QPointF> &vertices, const QList<int> &triangles)
{
    m_polygonMesh = polygon;
    m_meshVertices = vertices.isEmpty() ? polygon.toList() : vertices;
    if (m_meshVertexWeights.size() != m_meshVertices.size()) {
        m_meshVertexWeights.resize(m_meshVertices.size(), 0.0f);
    }
    if (!triangles.isEmpty()) {
        m_meshTriangles = triangles;
    } else if (polygon.size() >= 3) {
        if (m_meshVertices.size() > polygon.size()) {
            m_meshTriangles = BentoPackGeometry::Triangulator::triangulateCDT(polygon, m_meshVertices);
        } else {
            m_meshTriangles = BentoPackGeometry::Triangulator::triangulate(polygon);
        }
    } else {
        m_meshTriangles.clear();
    }
    m_selectedVertexIndex = -1;
    m_hoveredVertexIndex = -1;
    m_hoveredEdgeIndex = -1;
    updatePolygonMask();
    update();
    emit polygonMeshChanged(m_polygonMesh);
    emit meshDataChanged(m_polygonMesh, m_meshVertices, m_meshTriangles);
}

void PixelCanvas::setPolygonEditMode(PolygonEditMode mode)
{
    if (m_polygonEditMode != mode) {
        m_polygonEditMode = mode;
        update();
    }
}

void PixelCanvas::setSelectedVertexIndex(int index)
{
    if (m_selectedVertexIndex != index) {
        m_selectedVertexIndex = index;
        bool isInterior = (index >= m_polygonMesh.size());
        emit selectedVertexChanged(index, isInterior);
        update();
    }
}

void PixelCanvas::retriangulateMesh()
{
    if (m_polygonMesh.size() < 3) {
        m_meshTriangles.clear();
        updatePolygonMask();
        update();
        emit polygonMeshChanged(m_polygonMesh);
        emit meshDataChanged(m_polygonMesh, m_meshVertices, m_meshTriangles);
        return;
    }
    if (m_meshVertices.size() > m_polygonMesh.size()) {
        m_meshTriangles = BentoPackGeometry::Triangulator::triangulateCDT(m_polygonMesh, m_meshVertices);
    } else {
        m_meshTriangles = BentoPackGeometry::Triangulator::triangulate(m_polygonMesh);
    }
    if (m_meshVertexWeights.size() != m_meshVertices.size()) {
        m_meshVertexWeights.resize(m_meshVertices.size(), 0.0f);
    }
    updatePolygonMask();
    update();
    emit polygonMeshChanged(m_polygonMesh);
    emit meshDataChanged(m_polygonMesh, m_meshVertices, m_meshTriangles);
}

void PixelCanvas::deleteSelectedVertex()
{
    if (m_selectedVertexIndex < 0 || m_selectedVertexIndex >= m_meshVertices.size()) return;

    QPolygonF oldPoly = m_polygonMesh;
    QList<QPointF> oldVerts = m_meshVertices;
    QList<int> oldTris = m_meshTriangles;

    int idx = m_selectedVertexIndex;
    if (idx < m_polygonMesh.size()) {
        // Exterior vertex
        if (m_polygonMesh.size() <= 3) {
            return;
        }
        m_polygonMesh.removeAt(idx);
        m_meshVertices.removeAt(idx);
    } else {
        // Interior vertex
        m_meshVertices.removeAt(idx);
    }
    if (idx >= 0 && idx < m_meshVertexWeights.size()) {
        m_meshVertexWeights.removeAt(idx);
    }

    m_selectedVertexIndex = -1;
    retriangulateMesh();
    pushMeshSnapshot(tr("Delete Mesh Vertex"), oldPoly, oldVerts, oldTris);
    emit selectedVertexChanged(-1, false);
}

void PixelCanvas::applyCutLine(const QPointF &startPt, const QPointF &endPt)
{
    if (m_polygonMesh.size() < 3) return;

    double len = std::hypot(endPt.x() - startPt.x(), endPt.y() - startPt.y());
    if (len < 1.5) return;

    QPolygonF oldPoly = m_polygonMesh;
    QList<QPointF> oldVerts = m_meshVertices;
    QList<int> oldTris = m_meshTriangles;

    auto lineIntersection = [](const QPointF &p1, const QPointF &p2,
                               const QPointF &p3, const QPointF &p4,
                               double &tSeg, double &uSeg) -> bool {
        double d = (p1.x() - p2.x()) * (p3.y() - p4.y()) - (p1.y() - p2.y()) * (p3.x() - p4.x());
        if (std::abs(d) < 1e-9) return false;
        double t = ((p1.x() - p3.x()) * (p3.y() - p4.y()) - (p1.y() - p3.y()) * (p3.x() - p4.x())) / d;
        double u = -((p1.x() - p2.x()) * (p1.y() - p3.y()) - (p1.y() - p2.y()) * (p1.x() - p3.x())) / d;
        if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
            tSeg = t;
            uSeg = u;
            return true;
        }
        return false;
    };

    struct CandidatePoint {
        double t;
        QPointF pt;
    };
    QList<CandidatePoint> candidates;

    // 1. Endpoints if inside polygon
    if (m_polygonMesh.containsPoint(startPt, Qt::OddEvenFill)) {
        candidates.append({0.0, startPt});
    }
    if (m_polygonMesh.containsPoint(endPt, Qt::OddEvenFill)) {
        candidates.append({1.0, endPt});
    }

    // 2. Intersections with boundary edges
    const int numBoundary = m_polygonMesh.size();
    for (int b = 0; b < numBoundary; ++b) {
        const QPointF &bp1 = m_polygonMesh[b];
        const QPointF &bp2 = m_polygonMesh[(b + 1) % numBoundary];
        double tCut, uBound;
        if (lineIntersection(startPt, endPt, bp1, bp2, tCut, uBound)) {
            QPointF pInt(startPt.x() + tCut * (endPt.x() - startPt.x()),
                         startPt.y() + tCut * (endPt.y() - startPt.y()));
            candidates.append({tCut, pInt});
        }
    }

    // 3. Intersections with all current mesh triangle edges
    for (int i = 0; i + 2 < m_meshTriangles.size(); i += 3) {
        int idxs[3] = {m_meshTriangles[i], m_meshTriangles[i + 1], m_meshTriangles[i + 2]};
        for (int e = 0; e < 3; ++e) {
            int v0 = idxs[e];
            int v1 = idxs[(e + 1) % 3];
            if (v0 < 0 || v0 >= m_meshVertices.size() || v1 < 0 || v1 >= m_meshVertices.size()) continue;
            const QPointF &ep1 = m_meshVertices[v0];
            const QPointF &ep2 = m_meshVertices[v1];
            double tCut, uEdge;
            if (lineIntersection(startPt, endPt, ep1, ep2, tCut, uEdge)) {
                QPointF pInt(startPt.x() + tCut * (endPt.x() - startPt.x()),
                             startPt.y() + tCut * (endPt.y() - startPt.y()));
                if (m_polygonMesh.containsPoint(pInt, Qt::OddEvenFill)) {
                    candidates.append({tCut, pInt});
                }
            }
        }
    }

    // 4. Regular intermediate samples along the cut line (every 8px)
    int steps = std::max(1, static_cast<int>(std::round(len / 8.0)));
    for (int s = 1; s < steps; ++s) {
        double tStep = static_cast<double>(s) / static_cast<double>(steps);
        QPointF pStep(startPt.x() + tStep * (endPt.x() - startPt.x()),
                      startPt.y() + tStep * (endPt.y() - startPt.y()));
        if (m_polygonMesh.containsPoint(pStep, Qt::OddEvenFill)) {
            candidates.append({tStep, pStep});
        }
    }

    if (candidates.isEmpty()) return;

    // Sort candidates by t along cut line
    std::sort(candidates.begin(), candidates.end(), [](const CandidatePoint &a, const CandidatePoint &b) {
        return a.t < b.t;
    });

    // Deduplicate against existing vertices and among candidates
    QList<QPointF> pointsToAdd;
    for (const auto &cand : candidates) {
        bool tooClose = false;
        for (const QPointF &v : m_meshVertices) {
            if (std::hypot(v.x() - cand.pt.x(), v.y() - cand.pt.y()) < 2.0) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose) {
            for (const QPointF &added : pointsToAdd) {
                if (std::hypot(added.x() - cand.pt.x(), added.y() - cand.pt.y()) < 2.0) {
                    tooClose = true;
                    break;
                }
            }
        }
        if (!tooClose) {
            pointsToAdd.append(cand.pt);
        }
    }

    if (pointsToAdd.isEmpty()) return;

    for (const QPointF &pt : pointsToAdd) {
        m_meshVertices.append(pt);
    }

    m_selectedVertexIndex = m_meshVertices.size() - 1;
    retriangulateMesh();
    pushMeshSnapshot(tr("Mesh Cut Line"), oldPoly, oldVerts, oldTris);
    emit selectedVertexChanged(m_selectedVertexIndex, true);
}

int PixelCanvas::findVertexAt(const QPoint &widgetPos, double hitRadius) const
{
    for (int i = m_meshVertices.size() - 1; i >= 0; --i) {
        const QPointF &pt = m_meshVertices[i];
        QPointF widgetPt((pt.x() + m_imageOffset.x()) * m_zoom, (pt.y() + m_imageOffset.y()) * m_zoom);
        double dist = std::hypot(widgetPos.x() - widgetPt.x(), widgetPos.y() - widgetPt.y());
        if (dist <= hitRadius) {
            return i;
        }
    }
    return -1;
}

int PixelCanvas::findBoundaryEdgeAt(const QPoint &widgetPos, double hitRadius) const
{
    int n = m_polygonMesh.size();
    if (n < 3) return -1;
    for (int i = 0; i < n; ++i) {
        const QPointF &p1 = m_polygonMesh[i];
        const QPointF &p2 = m_polygonMesh[(i + 1) % n];
        QPointF w1((p1.x() + m_imageOffset.x()) * m_zoom, (p1.y() + m_imageOffset.y()) * m_zoom);
        QPointF w2((p2.x() + m_imageOffset.x()) * m_zoom, (p2.y() + m_imageOffset.y()) * m_zoom);

        double l2 = (w2.x() - w1.x()) * (w2.x() - w1.x()) + (w2.y() - w1.y()) * (w2.y() - w1.y());
        if (l2 < 1e-6) continue;
        double t = std::clamp(((widgetPos.x() - w1.x()) * (w2.x() - w1.x()) + (widgetPos.y() - w1.y()) * (w2.y() - w1.y())) / l2, 0.0, 1.0);
        QPointF proj(w1.x() + t * (w2.x() - w1.x()), w1.y() + t * (w2.y() - w1.y()));
        double dist = std::hypot(widgetPos.x() - proj.x(), widgetPos.y() - proj.y());
        if (dist <= hitRadius) {
            return i;
        }
    }
    return -1;
}

void PixelCanvas::setAllowEditingOutsidePolygon(bool allow)
{
    if (m_allowEditingOutsidePolygon != allow) {
        m_allowEditingOutsidePolygon = allow;
        update();
    }
}

void PixelCanvas::updatePolygonMask()
{
    m_polygonMask.clear();
    if (m_polygonMesh.size() < 3 || m_image.isNull()) return;

    int w = m_image.width();
    int h = m_image.height();
    if (w <= 0 || h <= 0) return;

    QImage mask(w, h, QImage::Format_Alpha8);
    mask.fill(0);
    {
        QPainter mp(&mask);
        mp.setRenderHint(QPainter::Antialiasing, false);
        mp.setBrush(Qt::white);
        mp.setPen(QPen(Qt::white, 1.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        mp.drawPolygon(m_polygonMesh);
    }

    m_polygonMask.resize(w * h);
    for (int y = 0; y < h; ++y) {
        const uchar *scan = mask.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            m_polygonMask[y * w + x] = (scan[x] > 0);
        }
    }
}

void PixelCanvas::drawPolygonMesh(QPainter &painter)
{
    if (m_polygonMesh.size() < 3 || m_image.isNull()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const bool isWeightPaintTool = (m_tool == PixelTool::WeightPaint);
    const bool showHeatmap = m_showWeightHeatmap || isWeightPaintTool;

    // 1. Draw CDT wireframe triangles (with optional thermal heatmap)
    if (!m_meshTriangles.isEmpty()) {
        QPen triPen(QColor(0, 210, 255, 130), 1.0, Qt::DashLine);
        painter.setPen(triPen);
        painter.setBrush(Qt::NoBrush);
        for (int i = 0; i + 2 < m_meshTriangles.size(); i += 3) {
            int i0 = m_meshTriangles[i];
            int i1 = m_meshTriangles[i + 1];
            int i2 = m_meshTriangles[i + 2];
            if (i0 >= 0 && i0 < m_meshVertices.size() &&
                i1 >= 0 && i1 < m_meshVertices.size() &&
                i2 >= 0 && i2 < m_meshVertices.size()) {
                QPolygonF triWidget;
                triWidget << QPointF(m_meshVertices[i0].x() * m_zoom, m_meshVertices[i0].y() * m_zoom);
                triWidget << QPointF(m_meshVertices[i1].x() * m_zoom, m_meshVertices[i1].y() * m_zoom);
                triWidget << QPointF(m_meshVertices[i2].x() * m_zoom, m_meshVertices[i2].y() * m_zoom);

                if (showHeatmap) {
                    float w0 = (i0 < m_meshVertexWeights.size()) ? m_meshVertexWeights[i0] : 0.0f;
                    float w1 = (i1 < m_meshVertexWeights.size()) ? m_meshVertexWeights[i1] : 0.0f;
                    float w2 = (i2 < m_meshVertexWeights.size()) ? m_meshVertexWeights[i2] : 0.0f;
                    float avgW = (w0 + w1 + w2) / 3.0f;
                    QColor faceCol = weightToColor(avgW);
                    faceCol.setAlpha(isWeightPaintTool ? 110 : 75);
                    painter.setBrush(QBrush(faceCol));
                } else {
                    painter.setBrush(Qt::NoBrush);
                }
                painter.drawPolygon(triWidget);
            }
        }
    }

    // 2. Draw outer polygon contour
    QPolygonF widgetPoly;
    for (const QPointF &pt : m_polygonMesh) {
        widgetPoly << QPointF(pt.x() * m_zoom, pt.y() * m_zoom);
    }
    QPen pen(QColor(0, 230, 130, 230), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(QBrush(QColor(0, 230, 130, 20)));
    painter.drawPolygon(widgetPoly);

    // 3. Highlight hovered edge if present
    if (m_hoveredEdgeIndex >= 0 && m_hoveredEdgeIndex < m_polygonMesh.size()) {
        int next = (m_hoveredEdgeIndex + 1) % m_polygonMesh.size();
        QPointF p1(m_polygonMesh[m_hoveredEdgeIndex].x() * m_zoom, m_polygonMesh[m_hoveredEdgeIndex].y() * m_zoom);
        QPointF p2(m_polygonMesh[next].x() * m_zoom, m_polygonMesh[next].y() * m_zoom);
        painter.setPen(QPen(QColor(255, 220, 0, 230), 3.0, Qt::SolidLine));
        painter.drawLine(p1, p2);
    }

    // 4. If PolygonEdit tool is active, draw vertex handles
    if (m_tool == PixelTool::PolygonEdit) {
        const int numBoundary = m_polygonMesh.size();

        for (int i = 0; i < m_meshVertices.size(); ++i) {
            const QPointF &pt = m_meshVertices[i];
            QPointF wpt(pt.x() * m_zoom, pt.y() * m_zoom);
            bool isInterior = (i >= numBoundary);
            bool isSelected = (i == m_selectedVertexIndex);
            bool isHovered = (i == m_hoveredVertexIndex);

            if (!isInterior) {
                // Exterior boundary vertex: Square handle
                double size = isSelected ? 10.0 : (isHovered ? 9.0 : 7.0);
                QRectF rect(wpt.x() - size * 0.5, wpt.y() - size * 0.5, size, size);

                QColor fillColor = isSelected ? QColor(255, 170, 0) : (isHovered ? QColor(255, 235, 59) : QColor(240, 255, 240));
                QColor borderColor = isSelected ? QColor(255, 255, 255) : QColor(0, 120, 60);

                painter.setPen(QPen(borderColor, isSelected ? 2.0 : 1.2));
                painter.setBrush(QBrush(fillColor));
                painter.drawRect(rect);
            } else {
                // Interior Steiner / contrast vertex: Circular handle
                double r = isSelected ? 6.0 : (isHovered ? 5.5 : 4.0);

                QColor fillColor = isSelected ? QColor(255, 120, 0) : (isHovered ? QColor(255, 235, 59) : QColor(0, 229, 255));
                QColor borderColor = isSelected ? QColor(255, 255, 255) : QColor(0, 50, 120);

                painter.setPen(QPen(borderColor, isSelected ? 2.0 : 1.2));
                painter.setBrush(QBrush(fillColor));
                painter.drawEllipse(wpt, r, r);
            }

            if (isSelected) {
                // Draw glowing halo around selected point
                painter.setPen(QPen(QColor(255, 200, 0, 180), 1.0, Qt::DashLine));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(wpt, 11.0, 11.0);
            }
        }
    } else if (showHeatmap || isWeightPaintTool) {
        // Draw colored vertex weight dots
        for (int i = 0; i < m_meshVertices.size(); ++i) {
            const QPointF &pt = m_meshVertices[i];
            QPointF wpt(pt.x() * m_zoom, pt.y() * m_zoom);
            float w = (i < m_meshVertexWeights.size()) ? m_meshVertexWeights[i] : 0.0f;
            QColor dotCol = weightToColor(w);

            double r = isWeightPaintTool ? 4.5 : 3.5;
            painter.setPen(QPen(Qt::white, 1.2));
            painter.setBrush(QBrush(dotCol));
            painter.drawEllipse(wpt, r, r);
        }

        // Draw brush radius circle around mouse cursor if WeightPaint is active
        if (isWeightPaintTool && m_lastPixelPos.x() >= 0) {
            QPointF cursorWpt(m_lastPixelPos.x() * m_zoom, m_lastPixelPos.y() * m_zoom);
            double radWidget = m_weightBrushRadius * m_zoom;
            painter.setPen(QPen(QColor(255, 255, 255, 200), 1.5, Qt::DashLine));
            QColor brushFill = weightToColor(m_paintWeightValue);
            brushFill.setAlpha(40);
            painter.setBrush(QBrush(brushFill));
            painter.drawEllipse(cursorWpt, radWidget, radWidget);
        }
    }

    // 5. Draw laser / cut line preview if actively cutting
    if (m_isCuttingLine && m_tool == PixelTool::PolygonEdit) {
        QPointF wStart(m_cutLineStart.x() * m_zoom, m_cutLineStart.y() * m_zoom);
        QPointF wEnd(m_cutLineEnd.x() * m_zoom, m_cutLineEnd.y() * m_zoom);

        // Glow halo
        painter.setPen(QPen(QColor(255, 61, 0, 90), 5.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(wStart, wEnd);

        // Sharp laser line
        painter.setPen(QPen(QColor(255, 61, 0, 240), 2.0, Qt::DashLine, Qt::RoundCap));
        painter.drawLine(wStart, wEnd);

        // Endpoints
        painter.setPen(QPen(Qt::white, 1.5));
        painter.setBrush(QBrush(QColor(255, 61, 0)));
        painter.drawEllipse(wStart, 4.0, 4.0);
        painter.drawEllipse(wEnd, 4.0, 4.0);
    }

    painter.restore();
}

void PixelCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (m_image.isNull()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    // 1. Checkerboard
    drawCheckerboard(painter, rect());

    // 1b. Onion Skin layers (rendered directly under the active sprite)
    drawOnionSkins(painter);

    // 2. Active frame sprite image and local overlays
    painter.save();
    painter.translate(m_imageOffset.x() * m_zoom, m_imageOffset.y() * m_zoom);

    QRect targetRect(0, 0,
                     static_cast<int>(std::round(m_image.width() * m_zoom)),
                     static_cast<int>(std::round(m_image.height() * m_zoom)));
    painter.drawImage(targetRect, m_image);

    // If canvas envelope is larger than active frame, draw subtle frame boundary
    if (m_canvasSize.isValid() && !m_canvasSize.isEmpty() && m_canvasSize != m_image.size()) {
        painter.setPen(QPen(QColor(100, 116, 139, 140), 1.0, Qt::DashLine));
        painter.drawRect(targetRect.adjusted(0, 0, -1, -1));
    }

    // 3. Floating pasted stamp
    drawFloatingStamp(painter);

    // 4. Pixel grid
    drawPixelGrid(painter, rect());

    // 5. Polygon mesh contour
    drawPolygonMesh(painter);

    // 6. Selection bounds
    drawSelectionBorder(painter);

    // 6b. Sockets / Attachment Pins (M12)
    drawSockets(painter);

    painter.restore();

    // 7. Pivot Anchor Marker
    if (m_showPivot) {
        drawPivotMarker(painter);
    }
}

void PixelCanvas::drawPivotMarker(QPainter &painter)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    double cx = m_pivotPos.x() * m_zoom;
    double cy = m_pivotPos.y() * m_zoom;

    // 1. Guidelines (ground horizontal line & vertical axis)
    if (m_showPivotGuidelines || m_tool == PixelTool::PivotEdit) {
        // Horizontal ground dashed line
        QPen groundPen(QColor(255, 60, 60, 190), 1.0, Qt::DashLine);
        groundPen.setCosmetic(true);
        painter.setPen(groundPen);
        painter.drawLine(QPointF(0, cy), QPointF(width(), cy));

        // Vertical axis dashed line
        QPen vertPen(QColor(60, 160, 255, 180), 1.0, Qt::DashLine);
        vertPen.setCosmetic(true);
        painter.setPen(vertPen);
        painter.drawLine(QPointF(cx, 0), QPointF(cx, height()));
    }

    const double radius = 6.0;
    const double arm = 11.0;

    // 2. High contrast dark shadow / halo
    QPen darkPen(QColor(15, 23, 42, 210), 3.0, Qt::SolidLine, Qt::RoundCap);
    darkPen.setCosmetic(true);
    painter.setPen(darkPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(cx, cy), radius, radius);
    painter.drawLine(QPointF(cx - arm, cy), QPointF(cx - 3.0, cy));
    painter.drawLine(QPointF(cx + 3.0, cy), QPointF(cx + arm, cy));
    painter.drawLine(QPointF(cx, cy - arm), QPointF(cx, cy - 3.0));
    painter.drawLine(QPointF(cx, cy + 3.0), QPointF(cx, cy + arm));

    // 3. Neon core (golden yellow if active/dragging, cyan otherwise)
    QColor reticleColor = m_isDraggingPivot ? QColor(255, 220, 40) : (m_tool == PixelTool::PivotEdit ? QColor(0, 230, 255) : QColor(56, 189, 248));
    QPen corePen(reticleColor, 1.5, Qt::SolidLine, Qt::RoundCap);
    corePen.setCosmetic(true);
    painter.setPen(corePen);
    painter.drawEllipse(QPointF(cx, cy), radius, radius);
    painter.drawLine(QPointF(cx - (arm - 1.0), cy), QPointF(cx - 3.0, cy));
    painter.drawLine(QPointF(cx + 3.0, cy), QPointF(cx + (arm - 1.0), cy));
    painter.drawLine(QPointF(cx, cy - (arm - 1.0)), QPointF(cx, cy - 3.0));
    painter.drawLine(QPointF(cx, cy + 3.0), QPointF(cx, cy + (arm - 1.0)));

    // 4. Center dot
    painter.setPen(Qt::NoPen);
    painter.setBrush(reticleColor);
    painter.drawEllipse(QPointF(cx, cy), 2.0, 2.0);

    painter.restore();
}

void PixelCanvas::mousePressEvent(QMouseEvent *event)
{
    // Pan mode: Middle button or Space + Left button
    if (event->button() == Qt::MiddleButton || (m_isSpacePressed && event->button() == Qt::LeftButton)) {
        m_isPanning = true;
        m_lastPanGlobalPos = event->globalPosition().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QPoint pixelPos = widgetToPixel(event->pos());

    if (event->modifiers() & Qt::AltModifier && event->button() == Qt::LeftButton && m_tool != PixelTool::Eyedropper) {
        // Quick eyedropper on Alt+click
        if (isPixelInside(pixelPos.x(), pixelPos.y())) {
            QColor picked = m_image.pixelColor(pixelPos.x(), pixelPos.y());
            setPrimaryColor(picked);
        }
        return;
    }

    // Check if clicking inside floating stamp
    if (m_hasFloating) {
        QRect floatRect(m_floatingPixelPos, m_floatingImage.size());
        if (floatRect.contains(pixelPos)) {
            m_isDraggingFloating = true;
            m_dragStartPixel = pixelPos - m_floatingPixelPos;
            return;
        } else {
            commitFloatingSelection();
        }
    }

    m_activeButton = event->button();
    m_lastPixelPos = pixelPos;
    m_strokePreImage = hasLayers() ? activeLayerImage() : m_image;

    if (m_tool == PixelTool::Pencil || m_tool == PixelTool::Eraser) {
        if (isLayerLocked()) {
            emit layerLockedAttempted();
            return;
        }
        m_isDrawing = true;
        m_currentStroke.clear();
        QColor drawColor;
        if (m_tool == PixelTool::Eraser) {
            drawColor = (m_activeButton == Qt::RightButton) ? m_secondaryColor : Qt::transparent;
        } else {
            drawColor = (m_activeButton == Qt::RightButton) ? m_secondaryColor : m_primaryColor;
        }
        m_currentStroke.append({pixelPos, pixelPos, drawColor});
        drawBresenhamLine(pixelPos.x(), pixelPos.y(), pixelPos.x(), pixelPos.y(), drawColor);
        emit imageChanged();
        update();
    } else if (m_tool == PixelTool::Eyedropper) {
        if (isPixelInside(pixelPos.x(), pixelPos.y())) {
            QColor picked;
            if (hasLayers() && !m_sampleAllLayers) {
                picked = activeLayerImage().pixelColor(pixelPos.x(), pixelPos.y());
            } else {
                picked = m_image.pixelColor(pixelPos.x(), pixelPos.y());
            }
            bool setSec = (event->button() == Qt::RightButton) || m_eyedropperTargetSecondary;
            if (setSec) {
                setSecondaryColor(picked);
            } else {
                setPrimaryColor(picked);
            }
        }
    } else if (m_tool == PixelTool::BucketFill) {
        if (isLayerLocked()) {
            emit layerLockedAttempted();
            return;
        }
        QColor fillCol = (event->button() == Qt::RightButton) ? m_secondaryColor : m_primaryColor;
        applyFloodFill(pixelPos.x(), pixelPos.y(), fillCol);
    } else if (m_tool == PixelTool::SelectRect) {
        if (event->button() != Qt::LeftButton) return;
        if (m_image.isNull()) return;

        commitFloatingSelection();
        ensureSelectionMaskValid();

        int clampedX = std::clamp(pixelPos.x(), 0, m_image.width() - 1);
        int clampedY = std::clamp(pixelPos.y(), 0, m_image.height() - 1);
        QPoint startPt(clampedX, clampedY);

        m_isSelecting = true;
        m_dragStartPixel = startPt;
        m_dragSelectionRect = QRect(startPt, QSize(1, 1));

        SelectionMode mode = effectiveSelectionMode();
        if (mode == SelectionMode::Replace) {
            m_preDragSelectionMask.fill(false, m_image.width() * m_image.height());
            m_preDragSelectionRect = QRect();
        } else {
            m_preDragSelectionMask = m_selectionMask;
            m_preDragSelectionRect = m_selectionRect;
        }

        updateSelectionPreview();
        update();
    } else if (m_tool == PixelTool::SelectColor) {
        applyColorSelection(pixelPos.x(), pixelPos.y());
    } else if (m_tool == PixelTool::PolygonEdit) {
        if (m_polygonMesh.size() < 3) return;

        int hitV = findVertexAt(event->pos(), 8.0);
        int hitE = findBoundaryEdgeAt(event->pos(), 6.0);

        if (event->button() == Qt::RightButton || m_polygonEditMode == PolygonEditMode::DeleteVertex) {
            if (hitV >= 0) {
                m_selectedVertexIndex = hitV;
                deleteSelectedVertex();
            }
            return;
        }

        if (event->button() == Qt::LeftButton) {
            if (m_polygonEditMode == PolygonEditMode::CutLine) {
                m_isCuttingLine = true;
                m_cutLineStart = QPointF(event->pos().x() / m_zoom - m_imageOffset.x(),
                                         event->pos().y() / m_zoom - m_imageOffset.y());
                m_cutLineEnd = m_cutLineStart;
                update();
                return;
            }

            if (m_polygonEditMode == PolygonEditMode::AddInterior) {
                QPointF newP(event->pos().x() / m_zoom - m_imageOffset.x(),
                             event->pos().y() / m_zoom - m_imageOffset.y());
                if (m_polygonMesh.containsPoint(newP, Qt::OddEvenFill)) {
                    QPolygonF oldPoly = m_polygonMesh;
                    QList<QPointF> oldVerts = m_meshVertices;
                    QList<int> oldTris = m_meshTriangles;

                    m_meshVertices.append(newP);
                    m_selectedVertexIndex = m_meshVertices.size() - 1;
                    retriangulateMesh();
                    pushMeshSnapshot(tr("Add Interior Mesh Vertex"), oldPoly, oldVerts, oldTris);
                    emit selectedVertexChanged(m_selectedVertexIndex, true);
                }
                return;
            }

            if (m_polygonEditMode == PolygonEditMode::AddExterior || (hitV < 0 && hitE >= 0 && (event->modifiers() & Qt::ShiftModifier))) {
                if (hitE >= 0 && hitE < m_polygonMesh.size()) {
                    QPolygonF oldPoly = m_polygonMesh;
                    QList<QPointF> oldVerts = m_meshVertices;
                    QList<int> oldTris = m_meshTriangles;

                    const QPointF &p1 = m_polygonMesh[hitE];
                    const QPointF &p2 = m_polygonMesh[(hitE + 1) % m_polygonMesh.size()];
                    QPointF w1((p1.x() + m_imageOffset.x()) * m_zoom, (p1.y() + m_imageOffset.y()) * m_zoom);
                    QPointF w2((p2.x() + m_imageOffset.x()) * m_zoom, (p2.y() + m_imageOffset.y()) * m_zoom);
                    double l2 = (w2.x() - w1.x()) * (w2.x() - w1.x()) + (w2.y() - w1.y()) * (w2.y() - w1.y());
                    double t = (l2 > 1e-6) ? std::clamp(((event->pos().x() - w1.x()) * (w2.x() - w1.x()) + (event->pos().y() - w1.y()) * (w2.y() - w1.y())) / l2, 0.05, 0.95) : 0.5;
                    QPointF insertPt(p1.x() + t * (p2.x() - p1.x()), p1.y() + t * (p2.y() - p1.y()));

                    m_polygonMesh.insert(hitE + 1, insertPt);
                    m_meshVertices.insert(hitE + 1, insertPt);
                    m_selectedVertexIndex = hitE + 1;
                    retriangulateMesh();
                    pushMeshSnapshot(tr("Add Boundary Mesh Vertex"), oldPoly, oldVerts, oldTris);
                    emit selectedVertexChanged(m_selectedVertexIndex, false);
                }
                return;
            }

            // Select or Move Mode
            if (hitV >= 0) {
                m_selectedVertexIndex = hitV;
                m_isDraggingVertex = true;
                m_dragVertexOriginalPos = m_meshVertices[hitV];
                m_dragPrePolygon = m_polygonMesh;
                m_dragPreVertices = m_meshVertices;
                m_dragPreTriangles = m_meshTriangles;
                bool isInterior = (hitV >= m_polygonMesh.size());
                emit selectedVertexChanged(hitV, isInterior);
                update();
                return;
            } else {
                m_selectedVertexIndex = -1;
                emit selectedVertexChanged(-1, false);
                update();
            }
        }
        return;
    } else if (m_tool == PixelTool::PivotEdit) {
        if (event->button() == Qt::LeftButton) {
            m_isDraggingPivot = true;
            QPoint frameP = widgetToPixel(event->pos());
            setFramePivot(frameP);
            emit pivotChanged(frameP);
            update();
            event->accept();
            return;
        }
    } else if (m_tool == PixelTool::WeightPaint) {
        QPointF pixelPosF(pixelPos.x(), pixelPos.y());
        m_prePaintWeights = m_meshVertexWeights;
        applyWeightBrush(pixelPosF, event->button() == Qt::RightButton);
    } else if (m_tool == PixelTool::SocketEdit) {
        QString hitSocket = findSocketAt(event->pos(), 14.0);
        if (!hitSocket.isEmpty()) {
            m_selectedSocketName = hitSocket;
            emit socketSelected(hitSocket);
            m_isDraggingSocket = (event->button() == Qt::LeftButton);
            update();
        } else if (event->button() == Qt::LeftButton && !m_selectedSocketName.isEmpty()) {
            m_sockets[m_selectedSocketName] = QPointF(pixelPos.x(), pixelPos.y());
            emit socketsChanged(m_sockets);
            update();
        }
    }
}

void PixelCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        QPoint currentGlobal = event->globalPosition().toPoint();
        QPoint delta = currentGlobal - m_lastPanGlobalPos;
        m_lastPanGlobalPos = currentGlobal;
        emit panRequested(delta.x(), delta.y());
        event->accept();
        return;
    }

    QPoint pixelPos = widgetToPixel(event->pos());

    if (isPixelInside(pixelPos.x(), pixelPos.y())) {
        QColor col = m_image.pixelColor(pixelPos.x(), pixelPos.y());
        emit mousePixelMoved(pixelPos.x(), pixelPos.y(), col);
    } else {
        emit mousePixelLeft();
    }

    if (m_isDraggingFloating && m_hasFloating) {
        m_floatingPixelPos = pixelPos - m_dragStartPixel;
        update();
        return;
    }

    if (m_isDrawing && (m_tool == PixelTool::Pencil || m_tool == PixelTool::Eraser)) {
        if (pixelPos != m_lastPixelPos) {
            QColor drawColor;
            if (m_tool == PixelTool::Eraser) {
                drawColor = (m_activeButton == Qt::RightButton) ? m_secondaryColor : Qt::transparent;
            } else {
                drawColor = (m_activeButton == Qt::RightButton) ? m_secondaryColor : m_primaryColor;
            }
            m_currentStroke.append({m_lastPixelPos, pixelPos, drawColor});
            drawBresenhamLine(m_lastPixelPos.x(), m_lastPixelPos.y(), pixelPos.x(), pixelPos.y(), drawColor);
            m_lastPixelPos = pixelPos;
            emit imageChanged();
            update();
        }
    } else if (m_isSelecting && m_tool == PixelTool::SelectRect) {
        if (m_image.isNull()) return;
        int x1 = std::clamp(std::min(m_dragStartPixel.x(), pixelPos.x()), 0, m_image.width() - 1);
        int y1 = std::clamp(std::min(m_dragStartPixel.y(), pixelPos.y()), 0, m_image.height() - 1);
        int x2 = std::clamp(std::max(m_dragStartPixel.x(), pixelPos.x()), 0, m_image.width() - 1);
        int y2 = std::clamp(std::max(m_dragStartPixel.y(), pixelPos.y()), 0, m_image.height() - 1);
        if (m_selectRectFixedSquare) {
            int dx = std::abs(pixelPos.x() - m_dragStartPixel.x());
            int dy = std::abs(pixelPos.y() - m_dragStartPixel.y());
            int side = std::max(dx, dy);
            int sx = (pixelPos.x() >= m_dragStartPixel.x()) ? 1 : -1;
            int sy = (pixelPos.y() >= m_dragStartPixel.y()) ? 1 : -1;
            int targetX = std::clamp(m_dragStartPixel.x() + sx * side, 0, m_image.width() - 1);
            int targetY = std::clamp(m_dragStartPixel.y() + sy * side, 0, m_image.height() - 1);
            x1 = std::min(m_dragStartPixel.x(), targetX);
            x2 = std::max(m_dragStartPixel.x(), targetX);
            y1 = std::min(m_dragStartPixel.y(), targetY);
            y2 = std::max(m_dragStartPixel.y(), targetY);
        }
        m_dragSelectionRect = QRect(QPoint(x1, y1), QPoint(x2, y2));
        updateSelectionPreview();
        update();
    } else if (m_tool == PixelTool::PolygonEdit) {
        if (m_isCuttingLine) {
            m_cutLineEnd = QPointF(event->pos().x() / m_zoom - m_imageOffset.x(),
                                   event->pos().y() / m_zoom - m_imageOffset.y());
            update();
            return;
        }
        if (m_isDraggingVertex && m_selectedVertexIndex >= 0 && m_selectedVertexIndex < m_meshVertices.size()) {
            QPointF newP(event->pos().x() / m_zoom - m_imageOffset.x(),
                         event->pos().y() / m_zoom - m_imageOffset.y());
            int maxW = m_image.width();
            int maxH = m_image.height();
            newP.setX(std::clamp(newP.x(), 0.0, static_cast<double>(maxW)));
            newP.setY(std::clamp(newP.y(), 0.0, static_cast<double>(maxH)));

            int numBoundary = m_polygonMesh.size();
            if (m_selectedVertexIndex < numBoundary) {
                m_polygonMesh[m_selectedVertexIndex] = newP;
                m_meshVertices[m_selectedVertexIndex] = newP;
            } else {
                if (m_polygonMesh.containsPoint(newP, Qt::OddEvenFill)) {
                    m_meshVertices[m_selectedVertexIndex] = newP;
                }
            }
            retriangulateMesh();
        } else {
            int prevH = m_hoveredVertexIndex;
            int prevE = m_hoveredEdgeIndex;
            m_hoveredVertexIndex = findVertexAt(event->pos(), 8.0);
            m_hoveredEdgeIndex = (m_hoveredVertexIndex < 0) ? findBoundaryEdgeAt(event->pos(), 6.0) : -1;
            if (m_hoveredVertexIndex != prevH || m_hoveredEdgeIndex != prevE) {
                update();
            }
        }
    } else if (m_tool == PixelTool::PivotEdit) {
        if (m_isDraggingPivot) {
            QPoint frameP = widgetToPixel(event->pos());
            setFramePivot(frameP);
            emit pivotChanged(frameP);
            update();
            event->accept();
            return;
        } else {
            QPoint widgetPivotPos = pixelToWidget(framePivot());
            double dist = QLineF(event->pos(), widgetPivotPos).length();
            if (dist <= 12.0) {
                setCursor(Qt::SizeAllCursor);
            } else {
                setCursor(Qt::CrossCursor);
            }
        }
    } else if (m_tool == PixelTool::WeightPaint) {
        m_lastPixelPos = pixelPos;
        setCursor(Qt::CrossCursor);
        if (event->buttons() & (Qt::LeftButton | Qt::RightButton)) {
            QPointF pixelPosF(pixelPos.x(), pixelPos.y());
            applyWeightBrush(pixelPosF, event->buttons() & Qt::RightButton);
        } else {
            update();
        }
    } else if (m_tool == PixelTool::SocketEdit) {
        if (m_isDraggingSocket && !m_selectedSocketName.isEmpty()) {
            m_sockets[m_selectedSocketName] = QPointF(pixelPos.x(), pixelPos.y());
            emit socketsChanged(m_sockets);
            update();
        } else {
            QString hit = findSocketAt(event->pos(), 14.0);
            if (!hit.isEmpty()) {
                setCursor(Qt::SizeAllCursor);
            } else {
                setCursor(Qt::CrossCursor);
            }
        }
    }
}

void PixelCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isPanning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_isPanning = false;
        if (m_isSpacePressed) {
            setCursor(Qt::OpenHandCursor);
        } else {
            unsetCursor();
        }
        event->accept();
        return;
    }

    if (m_isDraggingFloating) {
        m_isDraggingFloating = false;
        return;
    }

    if (m_isDrawing) {
        m_isDrawing = false;
        m_lastActionData = CanvasActionData();
        m_lastActionData.action = (m_tool == PixelTool::Eraser) ? CanvasAction::Eraser : CanvasAction::Pencil;
        m_lastActionData.hasSelection = hasSelection();
        m_lastActionData.selectionRect = m_selectionRect;
        m_lastActionData.stroke = m_currentStroke;
        pushSnapshot(m_strokePreImage, (m_tool == PixelTool::Eraser) ? tr("Eraser") : tr("Pencil"),
                     QPolygonF(), (m_tool == PixelTool::Eraser) ? CanvasAction::Eraser : CanvasAction::Pencil);
        emit strokeFinished();
    } else if (m_isSelecting && m_tool == PixelTool::SelectRect) {
        if (event->button() != Qt::LeftButton) return;
        m_isSelecting = false;
        updateSelectionPreview();
        m_dragSelectionRect = QRect();
        m_preDragSelectionMask.clear();
        m_preDragSelectionRect = QRect();

        if (hasSelection()) {
            emit selectionStateChanged(true);
        } else {
            deselect();
        }
        update();
    } else if (m_tool == PixelTool::PolygonEdit) {
        if (m_isCuttingLine) {
            m_isCuttingLine = false;
            applyCutLine(m_cutLineStart, m_cutLineEnd);
            update();
            m_activeButton = Qt::NoButton;
            return;
        }
        if (m_isDraggingVertex) {
            m_isDraggingVertex = false;
            if (m_dragPreVertices != m_meshVertices) {
                pushMeshSnapshot(tr("Move Mesh Vertex"), m_dragPrePolygon, m_dragPreVertices, m_dragPreTriangles);
            }
        }
    } else if (m_tool == PixelTool::PivotEdit) {
        if (m_isDraggingPivot && event->button() == Qt::LeftButton) {
            m_isDraggingPivot = false;
            emit pivotCommitRequested(framePivot());
            update();
            event->accept();
            m_activeButton = Qt::NoButton;
            return;
        }
    } else if (m_tool == PixelTool::WeightPaint) {
        if (m_meshVertexWeights != m_prePaintWeights) {
            emit vertexWeightsChanged(m_meshVertexWeights);
        }
    } else if (m_tool == PixelTool::SocketEdit) {
        if (m_isDraggingSocket) {
            m_isDraggingSocket = false;
            emit socketsChanged(m_sockets);
        }
    }
    m_activeButton = Qt::NoButton;
}

void PixelCanvas::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0) {
        zoomIn();
    } else if (event->angleDelta().y() < 0) {
        zoomOut();
    }
    event->accept();
}

void PixelCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_isSpacePressed = true;
        if (!m_isPanning) {
            setCursor(Qt::OpenHandCursor);
        }
        event->accept();
        return;
    }

    if (m_isSelecting && m_tool == PixelTool::SelectRect) {
        if (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control || event->key() == Qt::Key_Alt) {
            updateSelectionPreview();
            update();
        }
    }

    if (event->key() == Qt::Key_X) {
        swapColors();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (m_tool == PixelTool::PolygonEdit && m_selectedVertexIndex >= 0) {
            deleteSelectedVertex();
            event->accept();
            return;
        }
        clearSelection();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Escape) {
        if (m_isSelecting && m_tool == PixelTool::SelectRect) {
            m_isSelecting = false;
            m_dragSelectionRect = QRect();
            m_selectionMask = m_preDragSelectionMask;
            m_selectionRect = m_preDragSelectionRect;
            m_preDragSelectionMask.clear();
            m_preDragSelectionRect = QRect();
            if (hasSelection()) {
                int count = 0;
                for (bool b : m_selectionMask) if (b) count++;
                emit selectionStateChanged(true);
                emit selectionGeometryChanged(m_selectionRect, count);
            } else {
                deselect();
            }
            update();
            event->accept();
            return;
        }
        if (m_hasFloating) {
            commitFloatingSelection();
        } else {
            deselect();
        }
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_hasFloating) {
            commitFloatingSelection();
            event->accept();
            return;
        }
    }

    // Ctrl shortcuts
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Z) {
            undo();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_Y || (event->key() == Qt::Key_Z && (event->modifiers() & Qt::ShiftModifier))) {
            redo();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_C) {
            copySelection();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_X) {
            cutSelection();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_V) {
            pasteClipboard();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_A) {
            selectAll();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_D) {
            deselect();
            event->accept();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void PixelCanvas::keyReleaseEvent(QKeyEvent *event)
{
    if (m_isSelecting && m_tool == PixelTool::SelectRect) {
        if (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control || event->key() == Qt::Key_Alt) {
            updateSelectionPreview();
            update();
        }
    }
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_isSpacePressed = false;
        if (!m_isPanning) {
            unsetCursor();
        }
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void PixelCanvas::leaveEvent(QEvent *event)
{
    if (!m_isPanning && !m_isSpacePressed) {
        unsetCursor();
    }
    emit mousePixelLeft();
    QWidget::leaveEvent(event);
}

// =========================================================================
// Vertex Weight Painting (M12)
// =========================================================================

void PixelCanvas::setMeshVertexWeights(const QList<float> &weights)
{
    m_meshVertexWeights = weights;
    if (m_meshVertices.size() > m_meshVertexWeights.size()) {
        m_meshVertexWeights.resize(m_meshVertices.size(), 0.0f);
    }
    update();
}

void PixelCanvas::setPaintWeightValue(float val)
{
    m_paintWeightValue = std::clamp(val, 0.0f, 1.0f);
    update();
}

void PixelCanvas::setWeightBrushRadius(int r)
{
    m_weightBrushRadius = std::max(1, r);
    update();
}

void PixelCanvas::setShowWeightHeatmap(bool show)
{
    m_showWeightHeatmap = show;
    update();
}

void PixelCanvas::setAllVertexWeights(float val)
{
    val = std::clamp(val, 0.0f, 1.0f);
    if (m_meshVertices.isEmpty()) return;

    m_meshVertexWeights.fill(val, m_meshVertices.size());
    update();
    emit vertexWeightsChanged(m_meshVertexWeights);
}

void PixelCanvas::applyLinearGradientWeights(bool horizontal)
{
    if (m_meshVertices.size() < 2) return;

    double minCoord = std::numeric_limits<double>::max();
    double maxCoord = -std::numeric_limits<double>::max();
    for (const QPointF &v : m_meshVertices) {
        double c = horizontal ? v.x() : v.y();
        minCoord = std::min(minCoord, c);
        maxCoord = std::max(maxCoord, c);
    }
    double range = maxCoord - minCoord;
    if (range <= 0.0001) range = 1.0;

    m_meshVertexWeights.resize(m_meshVertices.size());
    for (int i = 0; i < m_meshVertices.size(); ++i) {
        const QPointF &v = m_meshVertices[i];
        double c = horizontal ? v.x() : v.y();
        float w = 0.0f;
        if (horizontal) {
            w = static_cast<float>((c - minCoord) / range);
        } else {
            // Top (min Y) = 1.0 (moving tip), Bottom (max Y) = 0.0 (fixed ground root)
            w = static_cast<float>((maxCoord - c) / range);
        }
        m_meshVertexWeights[i] = std::clamp(w, 0.0f, 1.0f);
    }
    update();
    emit vertexWeightsChanged(m_meshVertexWeights);
}

QColor PixelCanvas::weightToColor(float w)
{
    w = std::clamp(w, 0.0f, 1.0f);
    // 5-stop thermal heatmap gradient:
    // 0.00: Blue   rgb(0, 80, 255)
    // 0.25: Cyan   rgb(0, 220, 255)
    // 0.50: Green  rgb(50, 220, 50)
    // 0.75: Yellow rgb(255, 220, 0)
    // 1.00: Red    rgb(255, 30, 0)
    if (w <= 0.25f) {
        float t = w / 0.25f;
        return QColor(0, static_cast<int>(80 + t * (220 - 80)), 255);
    } else if (w <= 0.50f) {
        float t = (w - 0.25f) / 0.25f;
        return QColor(static_cast<int>(t * 50), 220, static_cast<int>(255 - t * (255 - 50)));
    } else if (w <= 0.75f) {
        float t = (w - 0.50f) / 0.25f;
        return QColor(static_cast<int>(50 + t * (255 - 50)), 220, static_cast<int>(50 - t * 50));
    } else {
        float t = (w - 0.75f) / 0.25f;
        return QColor(255, static_cast<int>(220 - t * (220 - 30)), 0);
    }
}

void PixelCanvas::applyWeightBrush(const QPointF &pos, bool eraseMode)
{
    if (m_meshVertices.isEmpty()) return;

    if (m_meshVertexWeights.size() != m_meshVertices.size()) {
        m_meshVertexWeights.resize(m_meshVertices.size(), 0.0f);
    }

    const float targetW = eraseMode ? 0.0f : m_paintWeightValue;
    const double radius = std::max(1.0, static_cast<double>(m_weightBrushRadius));

    bool changed = false;
    for (int i = 0; i < m_meshVertices.size(); ++i) {
        const QPointF &v = m_meshVertices[i];
        double dist = std::hypot(v.x() - pos.x(), v.y() - pos.y());
        if (dist <= radius) {
            double factor = 1.0 - (dist / radius);
            float strength = static_cast<float>(std::clamp(factor * 0.45, 0.05, 1.0));
            float oldW = m_meshVertexWeights[i];
            float newW = (1.0f - strength) * oldW + strength * targetW;
            newW = std::clamp(newW, 0.0f, 1.0f);
            if (std::abs(newW - oldW) > 0.001f) {
                m_meshVertexWeights[i] = newW;
                changed = true;
            }
        }
    }

    if (changed) {
        update();
        emit vertexWeightsChanged(m_meshVertexWeights);
    }
}

// =========================================================================
// Sockets / Attachment Pins (M12)
// =========================================================================

void PixelCanvas::setSockets(const QMap<QString, QPointF> &sockets)
{
    m_sockets = sockets;
    if (!m_sockets.contains(m_selectedSocketName)) {
        m_selectedSocketName = m_sockets.isEmpty() ? QString() : m_sockets.firstKey();
    }
    update();
}

void PixelCanvas::setSelectedSocket(const QString &name)
{
    if (m_selectedSocketName != name) {
        m_selectedSocketName = name;
        update();
        emit socketSelected(name);
    }
}

void PixelCanvas::addOrUpdateSocket(const QString &name, const QPointF &pos)
{
    QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) return;
    m_sockets[cleanName] = pos;
    m_selectedSocketName = cleanName;
    update();
    emit socketsChanged(m_sockets);
    emit socketSelected(m_selectedSocketName);
}

void PixelCanvas::removeSocket(const QString &name)
{
    if (m_sockets.remove(name) > 0) {
        if (m_selectedSocketName == name) {
            m_selectedSocketName = m_sockets.isEmpty() ? QString() : m_sockets.firstKey();
            emit socketSelected(m_selectedSocketName);
        }
        update();
        emit socketsChanged(m_sockets);
    }
}

void PixelCanvas::renameSocket(const QString &oldName, const QString &newName)
{
    QString cleanNew = newName.trimmed();
    if (oldName == cleanNew || cleanNew.isEmpty() || !m_sockets.contains(oldName)) return;
    QPointF pos = m_sockets.take(oldName);
    m_sockets[cleanNew] = pos;
    if (m_selectedSocketName == oldName) {
        m_selectedSocketName = cleanNew;
    }
    update();
    emit socketsChanged(m_sockets);
    emit socketSelected(m_selectedSocketName);
}

QString PixelCanvas::findSocketAt(const QPoint &widgetPos, double hitRadius) const
{
    double bestDist = hitRadius;
    QString bestName;
    for (auto it = m_sockets.constBegin(); it != m_sockets.constEnd(); ++it) {
        QPointF wpt((it.value().x() + m_imageOffset.x()) * m_zoom,
                    (it.value().y() + m_imageOffset.y()) * m_zoom);
        double dist = std::hypot(widgetPos.x() - wpt.x(), widgetPos.y() - wpt.y());
        if (dist <= bestDist) {
            bestDist = dist;
            bestName = it.key();
        }
    }
    return bestName;
}

void PixelCanvas::drawSockets(QPainter &painter)
{
    if (m_sockets.isEmpty() && m_tool != PixelTool::SocketEdit) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont font = painter.font();
    font.setPixelSize(10);
    font.setBold(true);
    painter.setFont(font);

    for (auto it = m_sockets.constBegin(); it != m_sockets.constEnd(); ++it) {
        const QString &name = it.key();
        const QPointF &pt = it.value();
        QPointF wpt(pt.x() * m_zoom, pt.y() * m_zoom);
        bool isSelected = (name == m_selectedSocketName);

        // Highlight ring if selected
        if (isSelected) {
            painter.setPen(QPen(QColor(255, 215, 0, 220), 2.0, Qt::DashLine));
            painter.setBrush(QBrush(QColor(255, 215, 0, 45)));
            painter.drawEllipse(wpt, 13.0, 13.0);
        }

        // Crosshair pin
        QColor pinColor = isSelected ? QColor(255, 215, 0) : QColor(255, 50, 100);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.setBrush(QBrush(pinColor));
        painter.drawEllipse(wpt, 6.0, 6.0);

        painter.setPen(QPen(Qt::white, 1.0));
        painter.drawLine(wpt.x() - 8.0, wpt.y(), wpt.x() + 8.0, wpt.y());
        painter.drawLine(wpt.x(), wpt.y() - 8.0, wpt.x(), wpt.y() + 8.0);

        // Text badge pill
        QFontMetrics fm(font);
        int textW = fm.horizontalAdvance(name);
        int textH = fm.height();
        QRectF badgeRect(wpt.x() + 10.0, wpt.y() - textH * 0.5 - 2.0, textW + 8.0, textH + 4.0);

        painter.setPen(QPen(isSelected ? QColor(255, 215, 0) : QColor(80, 80, 90), 1.0));
        painter.setBrush(QBrush(QColor(20, 22, 28, 220)));
        painter.drawRoundedRect(badgeRect, 3.0, 3.0);

        painter.setPen(Qt::white);
        painter.drawText(badgeRect, Qt::AlignCenter, name);
    }

    painter.restore();
}
