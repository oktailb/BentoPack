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

#ifndef ATLASVIEWCONTROLLER_H
#define ATLASVIEWCONTROLLER_H

#include <QObject>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QList>
#include <QRect>
#include <QPointF>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QGuiApplication>
#include "spritestudiocore_export.h"

class SpriteDocument;
class QUndoStack;
class AtlasBoxItem;

/**
 * @brief Controller managing the Atlas QGraphicsView, zoom/pan, tools, and AtlasBoxItem interactions.
 */
class SPRITESTUDIO_CORE_EXPORT AtlasViewController : public QObject
{
    Q_OBJECT

public:
    enum SliceToolMode {
        ToolSelect,
        ToolAddSlice
    };
    Q_ENUM(SliceToolMode)

    explicit AtlasViewController(QGraphicsView *view,
                                 SpriteDocument *document,
                                 QUndoStack *undoStack = nullptr,
                                 QObject *parent = nullptr);
    ~AtlasViewController() override;

    // Tool mode
    SliceToolMode toolMode() const { return m_sliceToolMode; }
    void setToolMode(SliceToolMode mode);

    // Zoom and Pan
    double zoomFactor() const { return m_zoomFactor; }
    void setZoomFactor(double factor);
    void zoomIn(double step = -1.0);
    void zoomOut(double step = -1.0);
    void zoomAt(const QPointF &viewportPos, double factor);
    void fitInView();
    void adjustZoomToWindow();

    // Scene & Atlas Image
    void setAtlasImage(const QImage &image);
    void patchAtlasRegion(const QRect &rect, const QImage &patch);
    void clearAtlas();
    QGraphicsScene* scene() const { return m_scene; }
    QGraphicsPixmapItem* atlasPixmapItem() const { return m_atlasPixmapItem; }

    // Box items
    void syncAtlasBoxes();
    void clearAtlasBoxes();
    const QList<AtlasBoxItem*>& boxItems() const { return m_boxItems; }
    int boxCount() const { return m_boxItems.size(); }
    void ensureBoxVisible(int index);

    // M8: Polygon Mesh Display
    bool showPolygonMeshes() const { return m_showPolygonMeshes; }
    void setShowPolygonMeshes(bool show);

    // Selection
    QList<int> selectedBoxIndices() const;
    void setSelectedBoxIndices(const QList<int> &indices);
    void clearSelection();
    void selectAll();
    void invertSelection();
    void startMarqueeSelection(const QPointF &scenePos, Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers());
    void updateMarqueeSelection(const QPointF &scenePos);
    void endMarqueeSelection();

    // Slice Commands
    void trimSelectedSlice(int alphaThreshold = -1);
    void mergeSelectedSlices();
    void deleteSelectedSlices();
    void eraseSelectedSlicesPixels();
    void nudgeSelectedBoxes(int dx, int dy);
    void moveSelectedBoxes(int dx, int dy);
    bool deleteSelectedMeshVertices();
    bool nudgeSelectedMeshVertices(int dx, int dy);

    // Context Menu & Views
    void fitSelectedFramesInView(int padding = -1);

signals:
    void selectionChanged(const QList<int> &selectedIndices);
    void zoomChanged(double zoomFactor);
    void toolModeChanged(SliceToolMode mode);
    void boxContextMenuRequested(int index, const QPoint &screenPos);
    void atlasContextMenuRequested(const QPoint &pos);
    void statusMessage(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onBoxItemSelected(int index, bool selected, Qt::KeyboardModifiers modifiers);
    void onBoxItemGeometryChanged(int index, const QRect &newRect, const QRect &oldRect);
    void onBoxContextMenu(int index, const QPoint &screenPos);
    void onBoxItemInteractiveMoved(int index, const QPoint &delta);
    void onBoxItemInteractiveMoveFinished(int index, const QPoint &totalDelta);

private:
    QList<int> findFramesInSelectionRect(const QRectF &rect);
    void updateBoxSelectionVisuals(const QList<int> &selectedIndices);

    QGraphicsView       *m_view = nullptr;
    QGraphicsScene      *m_scene = nullptr;
    SpriteDocument      *m_document = nullptr;
    QUndoStack          *m_undoStack = nullptr;

    QGraphicsPixmapItem *m_atlasPixmapItem = nullptr;
    QList<AtlasBoxItem*> m_boxItems;

    SliceToolMode        m_sliceToolMode = ToolSelect;
    double               m_zoomFactor = 1.0;

    // Pan state
    bool                 m_isPanning = false;
    QPoint               m_panStartPos;

    // New slice drawing state
    bool                 m_isDrawingNewSlice = false;
    QPointF              m_newSliceStart;
    QGraphicsRectItem   *m_newSlicePreviewItem = nullptr;

    // Marquee selection state
    bool                 m_isSelecting = false;
    QPointF              m_selectionStartPoint;
    QGraphicsRectItem   *m_selectionRectItem = nullptr;
    QList<int>           m_dragBaseSelection;
    QList<int>           m_dragCurrentSelection;
    Qt::KeyboardModifiers m_selectionModifiers = Qt::NoModifier;

    bool                 m_showPolygonMeshes = true;
    QRect                m_lastPatchedRegion;
};

#endif // ATLASVIEWCONTROLLER_H
