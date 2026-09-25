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

#ifndef ATLASBOXITEM_H
#define ATLASBOXITEM_H

#include <QGraphicsObject>
#include <QRectF>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPolygonF>
#include <QSet>
#include "bentopackwidgets_export.h"

/**
 * @brief Interactive QGraphicsObject representing a sprite bounding box on the atlas.
 *
 * Provides 8 resize handles when selected, drag-to-move, context menu, and emits
 * signals for selection and geometry modifications with undo/redo integration.
 */
class BENTOPACK_WIDGETS_EXPORT AtlasBoxItem : public QGraphicsObject
{
    Q_OBJECT

public:
    enum Handle {
        None,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left,
        Pivot,
        Move,
        Vertex
    };

    explicit AtlasBoxItem(int index, const QRect &rect, const QRect &atlasBounds, QGraphicsItem *parent = nullptr);
    ~AtlasBoxItem() override = default;

    int index() const { return m_index; }
    void setIndex(int idx);

    QRect boxRect() const { return m_rect.toRect(); }
    void setBoxRect(const QRect &rect);

    QPoint boxPivot() const { return m_pivot; }
    void setBoxPivot(const QPoint &pivot, bool custom = true);
    bool hasCustomPivot() const { return m_hasCustomPivot; }

    bool isSelectedBox() const { return m_selected; }
    void setSelectedBox(bool sel);

    void setAtlasBounds(const QRect &bounds) { m_atlasBounds = bounds; }

    // M8: 2D Polygon Mesh & Wireframe display
    void setPolygonMesh(const QPolygonF &poly, const QList<int> &triangles, bool hasMesh);
    bool hasPolygonMesh() const { return m_hasPolygonMesh; }
    QPolygonF polygon() const { return m_polygon; }
    QList<int> triangles() const { return m_triangles; }

    void setShowPolygonMesh(bool show);
    bool showPolygonMesh() const { return m_showPolygonMesh; }

    bool hasSelectedVertices() const { return !m_selectedVertices.isEmpty(); }
    QSet<int> selectedVertices() const { return m_selectedVertices; }
    void selectVertex(int index, bool multiSelect = false);
    void clearVertexSelection();
    bool deleteSelectedVertices();
    bool nudgeSelectedVertices(int dx, int dy);

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    // Geometry helpers
    double currentHandleSize() const;

signals:
    void boxSelected(int index, bool selected, Qt::KeyboardModifiers modifiers);
    void boxGeometryChanged(int index, const QRect &newRect, const QRect &oldRect);
    void boxPivotChanged(int index, const QPoint &newPivot, const QPoint &oldPivot);
    void boxContextMenuRequested(int index, const QPoint &screenPos);
    void boxInteractiveMoved(int index, const QPoint &delta);
    void boxInteractiveMoveFinished(int index, const QPoint &totalDelta);
    void boxPolygonMeshChanged(int index, const QPolygonF &newPoly, const QList<int> &newTris,
                               const QPolygonF &oldPoly, const QList<int> &oldTris);

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    Handle handleAt(const QPointF &pos, double handleSize) const;
    int vertexAt(const QPointF &pos, double grabRadius) const;
    int edgeAt(const QPointF &pos, double maxDist, QPointF *projectedPoint = nullptr) const;
    QRectF getHandleRect(Handle handle, double handleSize) const;
    void updateCursor(Handle handle);

    int      m_index = 0;
    QRectF   m_rect;
    QRect    m_atlasBounds;
    bool     m_selected = false;
    bool     m_hovered = false;

    QPoint   m_pivot = QPoint(0, 0);
    bool     m_hasCustomPivot = false;
    QPoint   m_initialPivot;

    Handle   m_activeHandle = None;
    QPointF  m_pressScenePos;
    QRectF   m_initialRect;
    bool     m_hasMoved = false;

    // Polygon mesh data and vertex interaction
    QPolygonF  m_polygon;
    QList<int> m_triangles;
    bool       m_hasPolygonMesh = false;
    bool       m_showPolygonMesh = true;

    int        m_activeVertexIndex = -1;
    int        m_hoveredVertexIndex = -1;
    QSet<int>  m_selectedVertices;
    QPolygonF  m_initialPolygon;
    QList<int> m_initialTriangles;
};

#endif // ATLASBOXITEM_H
