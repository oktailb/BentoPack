/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#include "include/controller/atlasviewcontroller.h"
#include "include/atlasboxitem.h"
#include "include/model/spritedocument.h"
#include "include/commands/commands.h"
#include "include/commands/meshcommands.h"
#include "include/config/appconfig.h"
#include <QUndoStack>
#include <QScrollBar>
#include <QContextMenuEvent>
#include <algorithm>
#include <cmath>

AtlasViewController::AtlasViewController(QGraphicsView *view,
                                         SpriteDocument *document,
                                         QUndoStack *undoStack,
                                         QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_scene(new QGraphicsScene(this))
    , m_document(document)
    , m_undoStack(undoStack)
{
    if (m_view) {
        m_view->setScene(m_scene);
        m_view->setRenderHint(QPainter::Antialiasing, true);
        m_view->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
        m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
        m_view->viewport()->installEventFilter(this);
    }

    if (m_document) {
        if (!m_document->atlas().isNull()) {
            setAtlasImage(m_document->atlas());
        }
        connect(m_document, &SpriteDocument::atlasRegionChanged, this, [this](const QRect &rect) {
            if (m_atlasPixmapItem && m_scene->sceneRect() == m_document->atlas().rect()) {
                patchAtlasRegion(rect, m_document->atlas().copy(rect));
                m_lastPatchedRegion = rect;
            }
        });
        connect(m_document, &SpriteDocument::atlasChanged, this, [this]() {
            if (!m_lastPatchedRegion.isEmpty()) {
                m_lastPatchedRegion = QRect();
                return;
            }
            setAtlasImage(m_document->atlas());
        });
        connect(m_document, &SpriteDocument::framesChanged, this, &AtlasViewController::syncAtlasBoxes);
        connect(m_document, &SpriteDocument::frameUpdated, this, [this](int index) {
            if (index >= 0 && index < m_boxItems.size() && m_boxItems[index]) {
                const SpriteBox &box = m_document->box(index);
                m_boxItems[index]->setBoxRect(box.rect);
                m_boxItems[index]->setBoxPivot(box.effectivePivot(), box.hasCustomPivot);
                m_boxItems[index]->setPolygonMesh(box.polygon, box.triangles, box.hasPolygonMesh);
            }
        });
        connect(m_document, &SpriteDocument::boxPivotChanged, this, [this](int index, const QPoint &pivot) {
            if (index >= 0 && index < m_boxItems.size() && m_boxItems[index]) {
                m_boxItems[index]->setBoxPivot(pivot, m_document->boxHasCustomPivot(index));
            }
        });
    }
}

AtlasViewController::~AtlasViewController()
{
    clearAtlas();
}

void AtlasViewController::setToolMode(SliceToolMode mode)
{
    m_sliceToolMode = mode;
    if (m_view) {
        m_view->setCursor(mode == ToolAddSlice ? Qt::CrossCursor : Qt::ArrowCursor);
    }
    emit toolModeChanged(mode);
}

void AtlasViewController::setZoomFactor(double factor)
{
    const AtlasConfig &cfg = AppConfig::instance().atlas();
    double clamped = std::clamp(factor, cfg.zoomMin, cfg.zoomMax);
    if (m_zoomFactor == clamped) return;

    double scaleDelta = clamped / m_zoomFactor;
    m_zoomFactor = clamped;

    if (m_view) {
        m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        m_view->scale(scaleDelta, scaleDelta);
    }

    emit zoomChanged(clamped);
}

void AtlasViewController::zoomAt(const QPointF &viewportPos, double factor)
{
    if (!m_view || factor <= 0.0) return;

    const AtlasConfig &cfg = AppConfig::instance().atlas();
    double targetZoom = std::clamp(m_zoomFactor * factor, cfg.zoomMin, cfg.zoomMax);
    double scaleDelta = targetZoom / m_zoomFactor;
    if (qFuzzyCompare(scaleDelta, 1.0)) return;

    QPointF scenePointBefore = m_view->mapToScene(viewportPos.toPoint());

    m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
    m_view->scale(scaleDelta, scaleDelta);
    m_zoomFactor = targetZoom;

    QPointF newViewportPoint = m_view->mapFromScene(scenePointBefore);
    QPointF deltaViewport = newViewportPoint - viewportPos;

    m_view->horizontalScrollBar()->setValue(m_view->horizontalScrollBar()->value() + qRound(deltaViewport.x()));
    m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->value() + qRound(deltaViewport.y()));

    emit zoomChanged(m_zoomFactor);
}

void AtlasViewController::zoomIn(double step)
{
    if (step <= 0.0) step = AppConfig::instance().atlas().zoomStep;
    setZoomFactor(m_zoomFactor * step);
}

void AtlasViewController::zoomOut(double step)
{
    if (step <= 0.0) step = AppConfig::instance().atlas().zoomStep;
    setZoomFactor(m_zoomFactor / step);
}

void AtlasViewController::fitInView()
{
    if (m_view && m_scene && !m_scene->sceneRect().isEmpty()) {
        m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
        m_zoomFactor = m_view->transform().m11();
        emit zoomChanged(m_zoomFactor);
    }
}

void AtlasViewController::adjustZoomToWindow()
{
    fitInView();
}

void AtlasViewController::setAtlasImage(const QImage &image)
{
    if (image.isNull()) {
        clearAtlas();
        return;
    }

    if (m_atlasPixmapItem && m_scene->sceneRect() == image.rect()) {
        m_atlasPixmapItem->setPixmap(QPixmap::fromImage(image));
        syncAtlasBoxes();
        return;
    }

    clearAtlas();
    m_atlasPixmapItem = m_scene->addPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(image.rect());

    syncAtlasBoxes();
    fitInView();
}

void AtlasViewController::patchAtlasRegion(const QRect &rect, const QImage &patch)
{
    if (!m_atlasPixmapItem || rect.isEmpty() || patch.isNull()) return;

    QPixmap currentPix = m_atlasPixmapItem->pixmap();
    QPainter p(&currentPix);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(rect.topLeft(), patch);
    p.end();

    m_atlasPixmapItem->setPixmap(currentPix);
    m_scene->update(rect);
}

void AtlasViewController::clearAtlas()
{
    clearAtlasBoxes();
    if (m_newSlicePreviewItem) {
        if (m_newSlicePreviewItem->scene()) {
            m_newSlicePreviewItem->scene()->removeItem(m_newSlicePreviewItem);
        }
        delete m_newSlicePreviewItem;
        m_newSlicePreviewItem = nullptr;
    }
    if (m_selectionRectItem) {
        if (m_selectionRectItem->scene()) {
            m_selectionRectItem->scene()->removeItem(m_selectionRectItem);
        }
        delete m_selectionRectItem;
        m_selectionRectItem = nullptr;
    }
    m_scene->clear();
    m_atlasPixmapItem = nullptr;
}

void AtlasViewController::syncAtlasBoxes()
{
    clearAtlasBoxes();
    if (!m_document || m_document->isEmpty()) return;

    QRect atlasBounds = m_document->atlas().rect();
    int count = m_document->frameCount();

    for (int i = 0; i < count; ++i) {
        const SpriteBox &sb = m_document->box(i);
        AtlasBoxItem *boxItem = new AtlasBoxItem(i, sb.rect, atlasBounds);
        boxItem->setBoxPivot(sb.effectivePivot(), sb.hasCustomPivot);
        boxItem->setPolygonMesh(sb.polygon, sb.triangles, sb.hasPolygonMesh);
        boxItem->setShowPolygonMesh(m_showPolygonMeshes);
        m_scene->addItem(boxItem);
        m_boxItems.append(boxItem);

        connect(boxItem, &AtlasBoxItem::boxSelected,
                this, &AtlasViewController::onBoxItemSelected);
        connect(boxItem, &AtlasBoxItem::boxGeometryChanged,
                this, &AtlasViewController::onBoxItemGeometryChanged);
        connect(boxItem, &AtlasBoxItem::boxPivotChanged, this, [this](int index, const QPoint &newPivot, const QPoint &oldPivot) {
            if (m_undoStack) {
                m_undoStack->push(new ChangePivotCommand(m_document, index, oldPivot, newPivot, true));
            } else if (m_document) {
                m_document->setBoxPivot(index, newPivot, true);
            }
        });
        connect(boxItem, &AtlasBoxItem::boxContextMenuRequested,
                this, &AtlasViewController::onBoxContextMenu);
        connect(boxItem, &AtlasBoxItem::boxInteractiveMoved,
                this, &AtlasViewController::onBoxItemInteractiveMoved);
        connect(boxItem, &AtlasBoxItem::boxInteractiveMoveFinished,
                this, &AtlasViewController::onBoxItemInteractiveMoveFinished);
        connect(boxItem, &AtlasBoxItem::boxPolygonMeshChanged, this, [this](int index, const QPolygonF &newPoly, const QList<int> &newTris, const QPolygonF &oldPoly, const QList<int> &oldTris) {
            Q_UNUSED(oldPoly);
            Q_UNUSED(oldTris);
            using namespace SpriteStudioCommands;
            if (m_undoStack) {
                MeshState st;
                st.index = index;
                st.hasPolygonMesh = (!newPoly.isEmpty() && !newTris.isEmpty());
                st.polygon = newPoly;
                st.vertices = newPoly.toList();
                st.triangles = newTris;
                m_undoStack->push(new SetPolygonMeshCommand(m_document, {st}));
            } else if (m_document) {
                SpriteBox b = m_document->box(index);
                b.hasPolygonMesh = (!newPoly.isEmpty() && !newTris.isEmpty());
                b.polygon = newPoly;
                b.vertices = newPoly.toList();
                b.triangles = newTris;
                m_document->setBox(index, b);
            }
        });
    }

    updateBoxSelectionVisuals(m_document->selectedFrameIndices());
}

void AtlasViewController::setShowPolygonMeshes(bool show)
{
    m_showPolygonMeshes = show;
    for (AtlasBoxItem *item : m_boxItems) {
        if (item) {
            item->setShowPolygonMesh(show);
        }
    }
}

void AtlasViewController::clearAtlasBoxes()
{
    for (AtlasBoxItem *item : m_boxItems) {
        if (item) {
            if (item->scene()) {
                item->scene()->removeItem(item);
            }
            delete item;
        }
    }
    m_boxItems.clear();
}

QList<int> AtlasViewController::selectedBoxIndices() const
{
    return m_document ? m_document->selectedFrameIndices() : QList<int>();
}

void AtlasViewController::setSelectedBoxIndices(const QList<int> &indices)
{
    if (m_document) {
        if (m_document->selectedFrameIndices() == indices) {
            return;
        }
        m_document->setSelectedFrameIndices(indices);
    }
    updateBoxSelectionVisuals(indices);
    emit selectionChanged(indices);
}

void AtlasViewController::clearSelection()
{
    setSelectedBoxIndices(QList<int>());
}

void AtlasViewController::selectAll()
{
    if (!m_document) return;
    QList<int> allIndices;
    for (int i = 0; i < m_document->frameCount(); ++i) {
        allIndices.append(i);
    }
    setSelectedBoxIndices(allIndices);
}

void AtlasViewController::invertSelection()
{
    if (!m_document) return;
    QList<int> current = m_document->selectedFrameIndices();
    QList<int> inverted;
    for (int i = 0; i < m_document->frameCount(); ++i) {
        if (!current.contains(i)) {
            inverted.append(i);
        }
    }
    setSelectedBoxIndices(inverted);
}

void AtlasViewController::updateBoxSelectionVisuals(const QList<int> &selectedIndices)
{
    for (int i = 0; i < m_boxItems.size(); ++i) {
        if (m_boxItems[i]) {
            m_boxItems[i]->setSelectedBox(selectedIndices.contains(i));
        }
    }
}

void AtlasViewController::trimSelectedSlice(int alphaThreshold)
{
    if (!m_document) return;
    if (alphaThreshold < 0) {
        alphaThreshold = AppConfig::instance().atlas().defaultAlphaThreshold;
    }
    QList<int> selected = selectedBoxIndices();
    if (selected.isEmpty()) return;

    if (selected.size() == 1) {
        int idx = selected.first();
        if (idx >= 0 && idx < m_document->frameCount()) {
            QRect oldRect = m_document->box(idx).rect;
            QRect trimmed = m_document->computeTrimmedRect(idx, alphaThreshold);
            if (trimmed.isValid() && trimmed != oldRect) {
                if (m_undoStack) {
                    m_undoStack->push(new ChangeBoxRectCommand(m_document, idx, oldRect, trimmed));
                } else {
                    m_document->updateBoxRect(idx, trimmed);
                }
            }
        }
    } else {
        if (m_undoStack) m_undoStack->beginMacro(tr("KEY_CMD_TRIM_SLICES").arg(selected.size()));
        for (int idx : selected) {
            if (idx >= 0 && idx < m_document->frameCount()) {
                QRect oldRect = m_document->box(idx).rect;
                QRect trimmed = m_document->computeTrimmedRect(idx, alphaThreshold);
                if (trimmed.isValid() && trimmed != oldRect) {
                    if (m_undoStack) {
                        m_undoStack->push(new ChangeBoxRectCommand(m_document, idx, oldRect, trimmed));
                    } else {
                        m_document->updateBoxRect(idx, trimmed);
                    }
                }
            }
        }
        if (m_undoStack) m_undoStack->endMacro();
    }
}

void AtlasViewController::mergeSelectedSlices()
{
    if (!m_document) return;
    QList<int> selected = selectedBoxIndices();
    if (selected.size() < 2) return;

    std::sort(selected.begin(), selected.end());
    int target = selected.first();

    if (m_undoStack) {
        m_undoStack->beginMacro(tr("KEY_CMD_MERGE_SLICES").arg(selected.size()));
        for (int i = selected.size() - 1; i >= 1; --i) {
            int src = selected.at(i);
            m_undoStack->push(new MergeFramesCommand(m_document, src, target));
        }
        m_undoStack->endMacro();
    } else {
        for (int i = selected.size() - 1; i >= 1; --i) {
            m_document->mergeFrames(selected.at(i), target);
        }
    }
}

void AtlasViewController::deleteSelectedSlices()
{
    if (!m_document) return;
    QList<int> selected = selectedBoxIndices();
    if (selected.isEmpty()) return;

    if (m_undoStack) {
        m_undoStack->push(new DeleteFramesCommand(m_document, selected));
    } else {
        m_document->removeFrames(selected);
    }
}

void AtlasViewController::eraseSelectedSlicesPixels()
{
    if (!m_document) return;
    QList<int> selected = selectedBoxIndices();
    if (selected.isEmpty()) return;

    if (m_undoStack) {
        m_undoStack->push(new EraseAtlasPixelsCommand(m_document, selected));
    } else {
        EraseAtlasPixelsCommand cmd(m_document, selected);
        cmd.redo();
    }
}

void AtlasViewController::nudgeSelectedBoxes(int dx, int dy)
{
    moveSelectedBoxes(dx, dy);
}

void AtlasViewController::moveSelectedBoxes(int dx, int dy)
{
    if (!m_document || (dx == 0 && dy == 0)) return;
    QList<int> selected = selectedBoxIndices();
    if (selected.isEmpty()) return;

    QRect atlasBounds = m_document->atlas().rect();

    // Determine max allowable delta that keeps all selected boxes inside atlas bounds
    int minDx = -999999, maxDx = 999999;
    int minDy = -999999, maxDy = 999999;

    if (!atlasBounds.isEmpty()) {
        for (int idx : selected) {
            if (idx >= 0 && idx < m_document->frameCount()) {
                QRect r = m_document->box(idx).rect;
                minDx = std::max(minDx, atlasBounds.left() - r.left());
                maxDx = std::min(maxDx, atlasBounds.right() - r.right());
                minDy = std::max(minDy, atlasBounds.top() - r.top());
                maxDy = std::min(maxDy, atlasBounds.bottom() - r.bottom());
            }
        }
    }

    int clampedDx = std::clamp(dx, minDx, maxDx);
    int clampedDy = std::clamp(dy, minDy, maxDy);

    if (clampedDx == 0 && clampedDy == 0) return;

    if (selected.size() == 1) {
        int idx = selected.first();
        QRect oldRect = m_document->box(idx).rect;
        QRect newRect = oldRect.translated(clampedDx, clampedDy);
        if (m_undoStack) {
            m_undoStack->push(new ChangeBoxRectCommand(m_document, idx, oldRect, newRect));
        } else {
            m_document->updateBoxRect(idx, newRect);
        }
    } else {
        if (m_undoStack) {
            m_undoStack->beginMacro(tr("KEY_CMD_MOVE_SLICES").arg(selected.size()));
        }
        for (int idx : selected) {
            if (idx >= 0 && idx < m_document->frameCount()) {
                QRect oldRect = m_document->box(idx).rect;
                QRect newRect = oldRect.translated(clampedDx, clampedDy);
                if (m_undoStack) {
                    m_undoStack->push(new ChangeBoxRectCommand(m_document, idx, oldRect, newRect));
                } else {
                    m_document->updateBoxRect(idx, newRect);
                }
            }
        }
        if (m_undoStack) {
            m_undoStack->endMacro();
        }
    }
}

bool AtlasViewController::deleteSelectedMeshVertices()
{
    for (AtlasBoxItem *item : m_boxItems) {
        if (item && item->hasSelectedVertices()) {
            return item->deleteSelectedVertices();
        }
    }
    return false;
}

bool AtlasViewController::nudgeSelectedMeshVertices(int dx, int dy)
{
    for (AtlasBoxItem *item : m_boxItems) {
        if (item && item->hasSelectedVertices()) {
            return item->nudgeSelectedVertices(dx, dy);
        }
    }
    return false;
}

void AtlasViewController::fitSelectedFramesInView(int padding)
{
    if (!m_view || !m_document) return;
    if (padding < 0) {
        padding = AppConfig::instance().atlas().fitViewPadding;
    }
    QList<int> selected = selectedBoxIndices();
    if (selected.isEmpty()) {
        fitInView();
        return;
    }

    QRect combinedRect;
    for (int idx : selected) {
        if (idx >= 0 && idx < m_document->frameCount()) {
            combinedRect = combinedRect.united(m_document->box(idx).rect);
        }
    }

    if (combinedRect.isValid()) {
        QRectF paddedRect = combinedRect.adjusted(-padding, -padding, padding, padding);
        m_view->fitInView(paddedRect, Qt::KeepAspectRatio);
        m_zoomFactor = m_view->transform().m11();
        emit zoomChanged(m_zoomFactor);
    }
}

void AtlasViewController::onBoxItemSelected(int index, bool /*selected*/, Qt::KeyboardModifiers modifiers)
{
    int totalCount = m_document ? m_document->frameCount() : 0;
    if (index < 0 || index >= totalCount) return;

    QList<int> currentSel = selectedBoxIndices();

    if (modifiers & Qt::ControlModifier) {
        if (currentSel.contains(index)) {
            currentSel.removeAll(index);
        } else {
            currentSel.append(index);
        }
    } else if (modifiers & Qt::ShiftModifier) {
        int last = currentSel.isEmpty() ? 0 : currentSel.last();
        if (last <= index) {
            for (int i = last; i <= index; ++i) {
                if (!currentSel.contains(i)) {
                    currentSel.append(i);
                }
            }
        } else {
            for (int i = last; i >= index; --i) {
                if (!currentSel.contains(i)) {
                    currentSel.append(i);
                }
            }
        }
    } else {
        currentSel.clear();
        currentSel.append(index);
    }

    setSelectedBoxIndices(currentSel);
}

void AtlasViewController::onBoxItemGeometryChanged(int index, const QRect &newRect, const QRect &oldRect)
{
    if (!m_document || index < 0 || index >= m_document->frameCount()) return;

    if (m_undoStack) {
        m_undoStack->push(new ChangeBoxRectCommand(m_document, index, oldRect, newRect));
    } else {
        m_document->updateBoxRect(index, newRect);
    }
}

void AtlasViewController::onBoxContextMenu(int index, const QPoint &screenPos)
{
    QList<int> currentSel = selectedBoxIndices();
    if (!currentSel.contains(index)) {
        setSelectedBoxIndices({index});
    }

    emit boxContextMenuRequested(index, screenPos);
}

void AtlasViewController::onBoxItemInteractiveMoved(int index, const QPoint &delta)
{
    if (!m_document || m_document->isEmpty()) return;

    QList<int> selected = selectedBoxIndices();
    if (!selected.contains(index)) {
        selected = { index };
    }

    QRect atlasBounds = m_document->atlas().rect();

    int minDx = -999999, maxDx = 999999;
    int minDy = -999999, maxDy = 999999;

    if (!atlasBounds.isEmpty()) {
        for (int idx : selected) {
            if (idx >= 0 && idx < m_document->frameCount()) {
                QRect r = m_document->box(idx).rect;
                minDx = std::max(minDx, atlasBounds.left() - r.left());
                maxDx = std::min(maxDx, atlasBounds.right() - r.right());
                minDy = std::max(minDy, atlasBounds.top() - r.top());
                maxDy = std::min(maxDy, atlasBounds.bottom() - r.bottom());
            }
        }
    }

    int clampedDx = std::clamp(delta.x(), minDx, maxDx);
    int clampedDy = std::clamp(delta.y(), minDy, maxDy);

    for (int idx : selected) {
        if (idx >= 0 && idx < m_boxItems.size() && m_boxItems[idx]) {
            QRect orig = m_document->box(idx).rect;
            m_boxItems[idx]->setBoxRect(orig.translated(clampedDx, clampedDy));
        }
    }
}

void AtlasViewController::onBoxItemInteractiveMoveFinished(int index, const QPoint &totalDelta)
{
    if (!m_document || m_document->isEmpty()) return;

    QList<int> selected = selectedBoxIndices();
    if (!selected.contains(index)) {
        selected = { index };
    }

    // Reset visual positions of items back to document rects first
    for (int idx : selected) {
        if (idx >= 0 && idx < m_boxItems.size() && m_boxItems[idx]) {
            m_boxItems[idx]->setBoxRect(m_document->box(idx).rect);
        }
    }

    // Commit movement for the selection
    moveSelectedBoxes(totalDelta.x(), totalDelta.y());
}

void AtlasViewController::startMarqueeSelection(const QPointF &scenePos, Qt::KeyboardModifiers modifiers)
{
    if (!m_document || m_document->isEmpty()) return;

    m_selectionModifiers = modifiers;
    if (m_selectionModifiers & (Qt::ControlModifier | Qt::ShiftModifier)) {
        m_dragBaseSelection = selectedBoxIndices();
    } else {
        m_dragBaseSelection.clear();
    }
    m_dragCurrentSelection = m_dragBaseSelection;

    m_isSelecting = true;
    m_selectionStartPoint = scenePos;

    if (!m_selectionRectItem) {
        m_selectionRectItem = new QGraphicsRectItem();
        QColor marqueeCol = AppConfig::instance().visuals().marqueeColor;
        QPen pen(marqueeCol, 1, Qt::DashLine);
        pen.setCosmetic(true);
        m_selectionRectItem->setPen(pen);
        QColor fill = marqueeCol;
        fill.setAlpha(40);
        m_selectionRectItem->setBrush(QBrush(fill));
        m_selectionRectItem->setZValue(100.0);
        m_scene->addItem(m_selectionRectItem);
    } else if (m_selectionRectItem->scene() != m_scene) {
        m_scene->addItem(m_selectionRectItem);
    }

    m_selectionRectItem->setRect(QRectF(scenePos, QSizeF(1, 1)));
    m_selectionRectItem->setVisible(true);

    updateMarqueeSelection(scenePos);
}

void AtlasViewController::updateMarqueeSelection(const QPointF &scenePos)
{
    if (!m_isSelecting || !m_selectionRectItem) return;

    QRectF rect = QRectF(m_selectionStartPoint, scenePos).normalized();
    m_selectionRectItem->setRect(rect);

    QList<int> framesInRect = findFramesInSelectionRect(rect);
    QList<int> newSelection;

    if (m_selectionModifiers & Qt::ControlModifier) {
        newSelection = m_dragBaseSelection;
        for (int idx : framesInRect) {
            if (!newSelection.contains(idx)) newSelection.append(idx);
        }
    } else if (m_selectionModifiers & Qt::ShiftModifier) {
        newSelection = m_dragBaseSelection;
        for (int idx : framesInRect) {
            newSelection.removeAll(idx);
        }
    } else {
        newSelection = framesInRect;
    }

    if (newSelection != m_dragCurrentSelection) {
        m_dragCurrentSelection = newSelection;
        updateBoxSelectionVisuals(newSelection);
    }
}

void AtlasViewController::endMarqueeSelection()
{
    if (!m_isSelecting) return;
    m_isSelecting = false;
    if (m_selectionRectItem) {
        m_selectionRectItem->setVisible(false);
    }

    setSelectedBoxIndices(m_dragCurrentSelection);
}

QList<int> AtlasViewController::findFramesInSelectionRect(const QRectF &rect)
{
    QList<int> result;
    if (!m_document) return result;

    for (int i = 0; i < m_document->frameCount(); ++i) {
        QRect boxRect = m_document->box(i).rect;
        if (rect.intersects(boxRect)) {
            result.append(i);
        }
    }
    return result;
}

bool AtlasViewController::eventFilter(QObject *watched, QEvent *event)
{
    if (m_view && watched == m_view->viewport()) {
        // --- Wheel Zoom ---
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            const AtlasConfig &cfg = AppConfig::instance().atlas();
            double step = cfg.zoomStep;
            if (wheelEvent->angleDelta().y() > 0) {
                zoomAt(wheelEvent->position(), step);
            } else if (wheelEvent->angleDelta().y() < 0) {
                zoomAt(wheelEvent->position(), 1.0 / step);
            }
            return true;
        }

        // --- Context Menu (Atlas Background) ---
        if (event->type() == QEvent::ContextMenu) {
            QContextMenuEvent *cme = static_cast<QContextMenuEvent*>(event);
            QGraphicsItem *item = m_view->itemAt(cme->pos());
            AtlasBoxItem *boxItem = dynamic_cast<AtlasBoxItem*>(item);
            if (!boxItem && item && item->parentItem()) {
                boxItem = dynamic_cast<AtlasBoxItem*>(item->parentItem());
            }
            if (!boxItem) {
                emit atlasContextMenuRequested(cme->pos());
                return true;
            }
            // If it is a box item, let standard QGraphicsView delivery handle it so AtlasBoxItem::contextMenuEvent gets called
            return false;
        }

        // Mouse events
        if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseMove ||
            event->type() == QEvent::MouseButtonRelease) {

            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

            // 1. Middle Button Pan
            if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::MiddleButton) {
                m_isPanning = true;
                m_panStartPos = mouseEvent->pos();
                m_view->setCursor(Qt::ClosedHandCursor);
                return true;
            } else if (event->type() == QEvent::MouseMove && m_isPanning) {
                QPoint delta = mouseEvent->pos() - m_panStartPos;
                m_panStartPos = mouseEvent->pos();
                m_view->horizontalScrollBar()->setValue(m_view->horizontalScrollBar()->value() - delta.x());
                m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->value() - delta.y());
                return true;
            } else if (event->type() == QEvent::MouseButtonRelease && mouseEvent->button() == Qt::MiddleButton) {
                if (m_isPanning) {
                    m_isPanning = false;
                    m_view->setCursor(m_sliceToolMode == ToolAddSlice ? Qt::CrossCursor : Qt::ArrowCursor);
                    return true;
                }
            }

            // 2. ToolAddSlice Mode
            if (m_sliceToolMode == ToolAddSlice) {
                if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
                    QPointF scenePos = m_view->mapToScene(mouseEvent->pos());
                    QRect atlasBounds = (m_document && !m_document->atlas().isNull()) ? m_document->atlas().rect() : QRect();
                    if (!atlasBounds.isEmpty()) {
                        scenePos.setX(std::clamp<double>(scenePos.x(), atlasBounds.left(), atlasBounds.right()));
                        scenePos.setY(std::clamp<double>(scenePos.y(), atlasBounds.top(), atlasBounds.bottom()));
                    }
                    m_isDrawingNewSlice = true;
                    m_newSliceStart = scenePos;

                    if (!m_newSlicePreviewItem) {
                        m_newSlicePreviewItem = new QGraphicsRectItem();
                        QColor previewCol = AppConfig::instance().visuals().newSlicePreviewColor;
                        QPen pen(previewCol, 2, Qt::DashLine);
                        pen.setCosmetic(true);
                        m_newSlicePreviewItem->setPen(pen);
                        QColor fill = previewCol;
                        fill.setAlpha(40);
                        m_newSlicePreviewItem->setBrush(QBrush(fill));
                        m_newSlicePreviewItem->setZValue(50.0);
                        m_scene->addItem(m_newSlicePreviewItem);
                    } else if (m_newSlicePreviewItem->scene() != m_scene) {
                        m_scene->addItem(m_newSlicePreviewItem);
                    }
                    m_newSlicePreviewItem->setRect(QRectF(scenePos, QSizeF(1, 1)));
                    m_newSlicePreviewItem->setVisible(true);
                    return true;
                } else if (event->type() == QEvent::MouseMove && m_isDrawingNewSlice) {
                    QPointF scenePos = m_view->mapToScene(mouseEvent->pos());
                    QRect atlasBounds = (m_document && !m_document->atlas().isNull()) ? m_document->atlas().rect() : QRect();
                    if (!atlasBounds.isEmpty()) {
                        scenePos.setX(std::clamp<double>(scenePos.x(), atlasBounds.left(), atlasBounds.right()));
                        scenePos.setY(std::clamp<double>(scenePos.y(), atlasBounds.top(), atlasBounds.bottom()));
                    }
                    if (m_newSlicePreviewItem) {
                        m_newSlicePreviewItem->setRect(QRectF(m_newSliceStart, scenePos).normalized());
                    }
                    return true;
                } else if (event->type() == QEvent::MouseButtonRelease && mouseEvent->button() == Qt::LeftButton) {
                    if (m_isDrawingNewSlice) {
                        m_isDrawingNewSlice = false;
                        if (m_newSlicePreviewItem) {
                            m_newSlicePreviewItem->setVisible(false);
                        }
                        QPointF scenePos = m_view->mapToScene(mouseEvent->pos());
                        QRect atlasBounds = (m_document && !m_document->atlas().isNull()) ? m_document->atlas().rect() : QRect();
                        if (!atlasBounds.isEmpty()) {
                            scenePos.setX(std::clamp<double>(scenePos.x(), atlasBounds.left(), atlasBounds.right()));
                            scenePos.setY(std::clamp<double>(scenePos.y(), atlasBounds.top(), atlasBounds.bottom()));
                        }
                        QRect sliceRect = QRectF(m_newSliceStart, scenePos).normalized().toRect();
                        int minSlice = AppConfig::instance().atlas().minSliceSize;
                        if (sliceRect.width() >= minSlice && sliceRect.height() >= minSlice) {
                            if (m_undoStack) {
                                m_undoStack->push(new AddSliceCommand(m_document, sliceRect));
                            } else {
                                m_document->addSlice(sliceRect);
                            }
                            int newIndex = m_document->frameCount() - 1;
                            if (newIndex >= 0) {
                                setSelectedBoxIndices({newIndex});
                            }
                            // Auto-switch to ToolSelect for immediate manipulation UNLESS Shift is held
                            if (!(mouseEvent->modifiers() & Qt::ShiftModifier)) {
                                setToolMode(ToolSelect);
                            }
                        }
                        return true;
                    }
                }
            }

            // 3. ToolSelect Mode
            if (m_sliceToolMode == ToolSelect) {
                if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
                    QGraphicsItem *item = m_view->itemAt(mouseEvent->pos());
                    AtlasBoxItem *boxItem = dynamic_cast<AtlasBoxItem*>(item);
                    if (!boxItem && item && item->parentItem()) {
                        boxItem = dynamic_cast<AtlasBoxItem*>(item->parentItem());
                    }

                    if (boxItem) {
                        // Handled by AtlasBoxItem / QGraphicsScene
                        return false;
                    }

                    // Background click -> Marquee selection
                    if (m_document && !m_document->isEmpty()) {
                        QPointF scenePos = m_view->mapToScene(mouseEvent->pos());
                        startMarqueeSelection(scenePos, mouseEvent->modifiers());
                        return true;
                    }
                } else if (event->type() == QEvent::MouseMove) {
                    if (m_isSelecting) {
                        QPointF scenePos = m_view->mapToScene(mouseEvent->pos());
                        updateMarqueeSelection(scenePos);
                        return true;
                    }
                } else if (event->type() == QEvent::MouseButtonRelease && mouseEvent->button() == Qt::LeftButton) {
                    if (m_isSelecting) {
                        endMarqueeSelection();
                        return true;
                    }
                }
            }
        }
    }

    return QObject::eventFilter(watched, event);
}
