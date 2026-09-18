#include "atlasboxitem.h"
#include "config/appconfig.h"
#include "geometry/triangulator.h"
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>
#include <algorithm>
#include <cmath>

AtlasBoxItem::AtlasBoxItem(int index, const QRect &rect, const QRect &atlasBounds, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_index(index)
    , m_rect(rect)
    , m_atlasBounds(atlasBounds)
    , m_pivot(rect.width() / 2, rect.height())
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setZValue(10.0); // Draw on top of the atlas pixmap
}

void AtlasBoxItem::setIndex(int idx)
{
    if (m_index != idx) {
        m_index = idx;
        update();
    }
}

void AtlasBoxItem::setBoxPivot(const QPoint &pivot, bool custom)
{
    if (m_pivot != pivot || m_hasCustomPivot != custom) {
        prepareGeometryChange();
        m_pivot = pivot;
        m_hasCustomPivot = custom;
        update();
    }
}

void AtlasBoxItem::setBoxRect(const QRect &rect)
{
    QRectF newRect(rect);
    if (m_rect != newRect) {
        prepareGeometryChange();
        m_rect = newRect;
        if (!m_hasCustomPivot) {
            m_pivot = QPoint(rect.width() / 2, rect.height());
        }
        update();
    }
}

void AtlasBoxItem::setSelectedBox(bool sel)
{
    if (m_selected != sel) {
        m_selected = sel;
        if (!m_selected) {
            m_selectedVertices.clear();
        }
        update();
    }
}

void AtlasBoxItem::selectVertex(int index, bool multiSelect)
{
    if (index >= 0 && index < m_polygon.size()) {
        if (!multiSelect) {
            m_selectedVertices.clear();
        }
        m_selectedVertices.insert(index);
        update();
    }
}

void AtlasBoxItem::clearVertexSelection()
{
    if (!m_selectedVertices.isEmpty()) {
        m_selectedVertices.clear();
        update();
    }
}

bool AtlasBoxItem::deleteSelectedVertices()
{
    if (!m_selected || !m_hasPolygonMesh || m_selectedVertices.isEmpty()) {
        return false;
    }

    if (m_polygon.size() - m_selectedVertices.size() < 3) {
        return false;
    }

    QPolygonF oldPoly = m_polygon;
    QList<int> oldTris = m_triangles;

    QPolygonF newPoly;
    for (int i = 0; i < m_polygon.size(); ++i) {
        if (!m_selectedVertices.contains(i)) {
            newPoly.append(m_polygon[i]);
        }
    }

    prepareGeometryChange();
    m_polygon = newPoly;
    m_triangles = SpriteStudioGeometry::Triangulator::triangulate(m_polygon);
    m_selectedVertices.clear();

    emit boxPolygonMeshChanged(m_index, m_polygon, m_triangles, oldPoly, oldTris);
    update();
    return true;
}

bool AtlasBoxItem::nudgeSelectedVertices(int dx, int dy)
{
    if (!m_selected || !m_hasPolygonMesh || m_selectedVertices.isEmpty()) {
        return false;
    }

    QPolygonF oldPoly = m_polygon;
    QList<int> oldTris = m_triangles;

    prepareGeometryChange();
    for (int idx : m_selectedVertices) {
        if (idx >= 0 && idx < m_polygon.size()) {
            QPointF pt = m_polygon[idx];
            pt.rx() = std::clamp(pt.x() + dx, 0.0, static_cast<double>(m_rect.width()));
            pt.ry() = std::clamp(pt.y() + dy, 0.0, static_cast<double>(m_rect.height()));
            m_polygon[idx] = pt;
        }
    }

    m_triangles = SpriteStudioGeometry::Triangulator::triangulate(m_polygon);
    emit boxPolygonMeshChanged(m_index, m_polygon, m_triangles, oldPoly, oldTris);
    update();
    return true;
}

void AtlasBoxItem::setPolygonMesh(const QPolygonF &poly, const QList<int> &triangles, bool hasMesh)
{
    m_polygon = poly;
    m_triangles = triangles;
    m_hasPolygonMesh = hasMesh;
    update();
}

void AtlasBoxItem::setShowPolygonMesh(bool show)
{
    if (m_showPolygonMesh != show) {
        m_showPolygonMesh = show;
        update();
    }
}

double AtlasBoxItem::currentHandleSize() const
{
    double scale = 1.0;
    if (scene() && !scene()->views().isEmpty()) {
        scale = scene()->views().first()->transform().m11();
    }
    const double baseScreenSize = AppConfig::instance().visuals().handleSize;
    double sceneSize = (scale > 0.0) ? (baseScreenSize / scale) : baseScreenSize;

    // Ensure handle never occupies more than 35% of the box dimension on micro-sprites
    if (m_rect.width() > 0 && m_rect.height() > 0) {
        double maxByBox = std::min(m_rect.width(), m_rect.height()) * 0.35;
        if (maxByBox > 0.0) {
            sceneSize = std::min(sceneSize, maxByBox);
        }
    }
    return std::max(sceneSize, 0.1);
}

QRectF AtlasBoxItem::boundingRect() const
{
    double hs = currentHandleSize();
    double margin = std::max(hs / 2.0 + 2.0, AppConfig::instance().visuals().handleMargin);
    QRectF base = m_rect.adjusted(-margin, -margin, margin, margin);
    if (m_selected) {
        QPointF p = m_rect.topLeft() + m_pivot;
        base = base.united(QRectF(p.x() - 15.0, p.y() - 15.0, 30.0, 30.0));
    }
    return base;
}

QPainterPath AtlasBoxItem::shape() const
{
    QPainterPath path;
    if (m_hasPolygonMesh && m_showPolygonMesh && !m_polygon.isEmpty()) {
        path.addPolygon(m_polygon.translated(m_rect.topLeft()));
    } else {
        path.addRect(m_rect);
    }
    if (m_selected) {
        const double handleSize = currentHandleSize();
        for (int h = TopLeft; h <= Left; ++h) {
            path.addRect(getHandleRect(static_cast<Handle>(h), handleSize));
        }
        QPointF p = m_rect.topLeft() + m_pivot;
        path.addEllipse(p, 8.0, 8.0);
    }
    return path;
}

QRectF AtlasBoxItem::getHandleRect(Handle handle, double handleSize) const
{
    const double half = handleSize / 2.0;
    double x = 0.0;
    double y = 0.0;

    switch (handle) {
    case TopLeft:
        x = m_rect.left();
        y = m_rect.top();
        break;
    case Top:
        x = m_rect.center().x();
        y = m_rect.top();
        break;
    case TopRight:
        x = m_rect.right();
        y = m_rect.top();
        break;
    case Right:
        x = m_rect.right();
        y = m_rect.center().y();
        break;
    case BottomRight:
        x = m_rect.right();
        y = m_rect.bottom();
        break;
    case Bottom:
        x = m_rect.center().x();
        y = m_rect.bottom();
        break;
    case BottomLeft:
        x = m_rect.left();
        y = m_rect.bottom();
        break;
    case Left:
        x = m_rect.left();
        y = m_rect.center().y();
        break;
    default:
        return QRectF();
    }

    return QRectF(x - half, y - half, handleSize, handleSize);
}

int AtlasBoxItem::vertexAt(const QPointF &pos, double grabRadius) const
{
    if (!m_hasPolygonMesh || !m_showPolygonMesh || m_polygon.isEmpty()) {
        return -1;
    }

    QPointF origin = m_rect.topLeft();
    for (int i = 0; i < m_polygon.size(); ++i) {
        QPointF vPos = origin + m_polygon[i];
        if (QLineF(pos, vPos).length() <= grabRadius) {
            return i;
        }
    }
    return -1;
}

int AtlasBoxItem::edgeAt(const QPointF &pos, double maxDist, QPointF *projectedPoint) const
{
    if (!m_hasPolygonMesh || !m_showPolygonMesh || m_polygon.size() < 3) {
        return -1;
    }

    QPointF localPos = pos - m_rect.topLeft();
    int bestEdge = -1;
    double bestDistSq = maxDist * maxDist;
    QPointF bestProj;

    const int n = m_polygon.size();
    for (int i = 0; i < n; ++i) {
        const QPointF &p1 = m_polygon[i];
        const QPointF &p2 = m_polygon[(i + 1) % n];

        QPointF seg = p2 - p1;
        double lenSq = seg.x() * seg.x() + seg.y() * seg.y();
        if (lenSq <= 1e-6) continue;

        double t = ((localPos.x() - p1.x()) * seg.x() + (localPos.y() - p1.y()) * seg.y()) / lenSq;
        // Check if projection falls within edge segment (with a small margin so we don't duplicate existing vertices)
        if (t > 0.05 && t < 0.95) {
            QPointF proj = p1 + t * seg;
            double dx = localPos.x() - proj.x();
            double dy = localPos.y() - proj.y();
            double distSq = dx * dx + dy * dy;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestEdge = i;
                bestProj = proj;
            }
        }
    }

    if (bestEdge >= 0 && projectedPoint) {
        *projectedPoint = bestProj;
    }
    return bestEdge;
}

AtlasBoxItem::Handle AtlasBoxItem::handleAt(const QPointF &pos, double handleSize) const
{
    if (m_selected) {
        // 1. Test polygon vertices first if mesh is visible
        if (m_hasPolygonMesh && m_showPolygonMesh) {
            double scale = 1.0;
            if (scene() && !scene()->views().isEmpty()) {
                scale = scene()->views().first()->transform().m11();
            }
            double vRadius = (scale > 0.0) ? (7.0 / scale) : 7.0;
            vRadius = std::max(vRadius, 4.0);
            int vIdx = vertexAt(pos, vRadius);
            if (vIdx >= 0) {
                return Vertex;
            }
        }

        // 2. Test pivot
        QPointF pivotPos = m_rect.topLeft() + m_pivot;
        double grabRadius = std::max(handleSize * 1.2, 8.0);
        if (QLineF(pos, pivotPos).length() <= grabRadius) {
            return Pivot;
        }

        // 3. Test corners
        const Handle corners[] = { TopLeft, TopRight, BottomRight, BottomLeft };
        for (Handle h : corners) {
            if (getHandleRect(h, handleSize).contains(pos)) {
                return h;
            }
        }
        // 4. Then test edges
        const Handle edges[] = { Top, Right, Bottom, Left };
        for (Handle h : edges) {
            if (getHandleRect(h, handleSize).contains(pos)) {
                return h;
            }
        }
    }

    if (m_hasPolygonMesh && m_showPolygonMesh && !m_polygon.isEmpty()) {
        if (m_polygon.containsPoint(pos - m_rect.topLeft(), Qt::OddEvenFill)) {
            return Move;
        }
    } else if (m_rect.contains(pos)) {
        return Move;
    }

    return None;
}

void AtlasBoxItem::updateCursor(Handle handle)
{
    switch (handle) {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Top:
    case Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case Left:
    case Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case Pivot:
    case Vertex:
        setCursor(Qt::CrossCursor);
        break;
    case Move:
        setCursor(m_selected ? Qt::SizeAllCursor : Qt::PointingHandCursor);
        break;
    default:
        unsetCursor();
        break;
    }
}

void AtlasBoxItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = true;
    double handleSize = currentHandleSize();
    Handle h = handleAt(event->pos(), handleSize);
    updateCursor(h);

    int oldHovered = m_hoveredVertexIndex;
    if (h == Vertex) {
        double scale = 1.0;
        if (scene() && !scene()->views().isEmpty()) {
            scale = scene()->views().first()->transform().m11();
        }
        double vRadius = (scale > 0.0) ? (7.0 / scale) : 7.0;
        vRadius = std::max(vRadius, 4.0);
        m_hoveredVertexIndex = vertexAt(event->pos(), vRadius);
    } else {
        m_hoveredVertexIndex = -1;
    }

    if (oldHovered != m_hoveredVertexIndex) {
        update();
    }
}

void AtlasBoxItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    m_hovered = false;
    m_hoveredVertexIndex = -1;
    unsetCursor();
    update();
}

void AtlasBoxItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        double handleSize = currentHandleSize();
        m_activeHandle = handleAt(event->pos(), handleSize);
        m_pressScenePos = event->scenePos();
        m_initialRect = m_rect;
        m_initialPivot = m_pivot;
        m_initialPolygon = m_polygon;
        m_initialTriangles = m_triangles;
        m_hasMoved = false;

        if (m_activeHandle == Vertex) {
            double scale = 1.0;
            if (scene() && !scene()->views().isEmpty()) {
                scale = scene()->views().first()->transform().m11();
            }
            double vRadius = (scale > 0.0) ? (7.0 / scale) : 7.0;
            vRadius = std::max(vRadius, 4.0);
            m_activeVertexIndex = vertexAt(event->pos(), vRadius);

            if (m_activeVertexIndex >= 0) {
                if (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) {
                    if (m_selectedVertices.contains(m_activeVertexIndex)) {
                        m_selectedVertices.remove(m_activeVertexIndex);
                    } else {
                        m_selectedVertices.insert(m_activeVertexIndex);
                    }
                } else {
                    if (!m_selectedVertices.contains(m_activeVertexIndex)) {
                        m_selectedVertices.clear();
                        m_selectedVertices.insert(m_activeVertexIndex);
                    }
                }
            }
            update();
        } else {
            m_activeVertexIndex = -1;
            if (!m_selectedVertices.isEmpty()) {
                m_selectedVertices.clear();
                update();
            }
        }

        emit boxSelected(m_index, true, event->modifiers());
        event->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

void AtlasBoxItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeHandle == None) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    if (m_activeHandle == Vertex) {
        if (m_selectedVertices.isEmpty() && m_activeVertexIndex >= 0) {
            m_selectedVertices.insert(m_activeVertexIndex);
        }
        if (!m_selectedVertices.isEmpty()) {
            QPointF sceneDelta = event->scenePos() - m_pressScenePos;
            prepareGeometryChange();
            for (int idx : m_selectedVertices) {
                if (idx >= 0 && idx < m_initialPolygon.size()) {
                    QPointF newPt = m_initialPolygon[idx] + sceneDelta;
                    newPt.setX(std::clamp(newPt.x(), 0.0, static_cast<double>(m_rect.width())));
                    newPt.setY(std::clamp(newPt.y(), 0.0, static_cast<double>(m_rect.height())));
                    m_polygon[idx] = newPt;
                }
            }
            m_triangles = SpriteStudioGeometry::Triangulator::triangulate(m_polygon);
            m_hasMoved = true;
            update();
            event->accept();
            return;
        }
    }

    if (m_activeHandle == Pivot) {
        QPointF delta = event->scenePos() - m_pressScenePos;
        QPoint newPivot(static_cast<int>(std::round(m_initialPivot.x() + delta.x())),
                        static_cast<int>(std::round(m_initialPivot.y() + delta.y())));
        prepareGeometryChange();
        m_pivot = newPivot;
        m_hasCustomPivot = true;
        m_hasMoved = true;
        update();
        event->accept();
        return;
    }

    QPointF delta = event->scenePos() - m_pressScenePos;
    QRectF newRect = m_initialRect;
    const double minSize = static_cast<double>(AppConfig::instance().atlas().minSliceSize);

    switch (m_activeHandle) {
    case Move: {
        newRect.translate(delta.x(), delta.y());
        // Clamp to atlas bounds if valid
        if (!m_atlasBounds.isEmpty()) {
            if (newRect.left() < m_atlasBounds.left()) {
                newRect.moveLeft(m_atlasBounds.left());
            }
            if (newRect.right() > m_atlasBounds.right()) {
                newRect.moveRight(m_atlasBounds.right());
            }
            if (newRect.top() < m_atlasBounds.top()) {
                newRect.moveTop(m_atlasBounds.top());
            }
            if (newRect.bottom() > m_atlasBounds.bottom()) {
                newRect.moveBottom(m_atlasBounds.bottom());
            }
        }
        prepareGeometryChange();
        m_rect = newRect;
        m_hasMoved = true;
        update();

        QPoint intDelta(static_cast<int>(std::round(m_rect.left() - m_initialRect.left())),
                        static_cast<int>(std::round(m_rect.top() - m_initialRect.top())));
        emit boxInteractiveMoved(m_index, intDelta);
        event->accept();
        return;
    }
    case TopLeft: {
        double newLeft = std::min(m_initialRect.right() - minSize, m_initialRect.left() + delta.x());
        double newTop = std::min(m_initialRect.bottom() - minSize, m_initialRect.top() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newLeft = std::max<double>(m_atlasBounds.left(), newLeft);
            newTop = std::max<double>(m_atlasBounds.top(), newTop);
        }
        newRect.setLeft(newLeft);
        newRect.setTop(newTop);
        break;
    }
    case Top: {
        double newTop = std::min(m_initialRect.bottom() - minSize, m_initialRect.top() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newTop = std::max<double>(m_atlasBounds.top(), newTop);
        }
        newRect.setTop(newTop);
        break;
    }
    case TopRight: {
        double newRight = std::max(m_initialRect.left() + minSize, m_initialRect.right() + delta.x());
        double newTop = std::min(m_initialRect.bottom() - minSize, m_initialRect.top() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newRight = std::min<double>(m_atlasBounds.right(), newRight);
            newTop = std::max<double>(m_atlasBounds.top(), newTop);
        }
        newRect.setRight(newRight);
        newRect.setTop(newTop);
        break;
    }
    case Right: {
        double newRight = std::max(m_initialRect.left() + minSize, m_initialRect.right() + delta.x());
        if (!m_atlasBounds.isEmpty()) {
            newRight = std::min<double>(m_atlasBounds.right(), newRight);
        }
        newRect.setRight(newRight);
        break;
    }
    case BottomRight: {
        double newRight = std::max(m_initialRect.left() + minSize, m_initialRect.right() + delta.x());
        double newBottom = std::max(m_initialRect.top() + minSize, m_initialRect.bottom() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newRight = std::min<double>(m_atlasBounds.right(), newRight);
            newBottom = std::min<double>(m_atlasBounds.bottom(), newBottom);
        }
        newRect.setRight(newRight);
        newRect.setBottom(newBottom);
        break;
    }
    case Bottom: {
        double newBottom = std::max(m_initialRect.top() + minSize, m_initialRect.bottom() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newBottom = std::min<double>(m_atlasBounds.bottom(), newBottom);
        }
        newRect.setBottom(newBottom);
        break;
    }
    case BottomLeft: {
        double newLeft = std::min(m_initialRect.right() - minSize, m_initialRect.left() + delta.x());
        double newBottom = std::max(m_initialRect.top() + minSize, m_initialRect.bottom() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newLeft = std::max<double>(m_atlasBounds.left(), newLeft);
            newBottom = std::min<double>(m_atlasBounds.bottom(), newBottom);
        }
        newRect.setLeft(newLeft);
        newRect.setBottom(newBottom);
        break;
    }
    case Left: {
        double newLeft = std::min(m_initialRect.right() - minSize, m_initialRect.left() + delta.x());
        if (!m_atlasBounds.isEmpty()) {
            newLeft = std::max<double>(m_atlasBounds.left(), newLeft);
        }
        newRect.setLeft(newLeft);
        break;
    }
    default:
        break;
    }

    // Clamp resize to atlas bounds if available
    if (!m_atlasBounds.isEmpty()) {
        if (newRect.left() < m_atlasBounds.left()) newRect.setLeft(m_atlasBounds.left());
        if (newRect.right() > m_atlasBounds.right()) newRect.setRight(m_atlasBounds.right());
        if (newRect.top() < m_atlasBounds.top()) newRect.setTop(m_atlasBounds.top());
        if (newRect.bottom() > m_atlasBounds.bottom()) newRect.setBottom(m_atlasBounds.bottom());
    }

    newRect = newRect.normalized();
    if (newRect.width() >= minSize && newRect.height() >= minSize) {
        prepareGeometryChange();
        m_rect = newRect;
        m_hasMoved = true;
        update();
    }

    event->accept();
}

void AtlasBoxItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_activeHandle != None) {
        if (m_activeHandle == Vertex) {
            if (m_hasMoved && !m_selectedVertices.isEmpty()) {
                emit boxPolygonMeshChanged(m_index, m_polygon, m_triangles, m_initialPolygon, m_initialTriangles);
            }
            m_activeHandle = None;
            m_activeVertexIndex = -1;
            m_hasMoved = false;
            update();
            event->accept();
            return;
        }

        if (m_activeHandle == Pivot) {
            if (m_hasMoved) {
                if (m_pivot != m_initialPivot) {
                    emit boxPivotChanged(m_index, m_pivot, m_initialPivot);
                }
            }
            m_activeHandle = None;
            m_hasMoved = false;
            event->accept();
            return;
        }

        if (m_activeHandle == Move) {
            if (m_hasMoved) {
                QPoint intTotalDelta(static_cast<int>(std::round(m_rect.left() - m_initialRect.left())),
                                     static_cast<int>(std::round(m_rect.top() - m_initialRect.top())));
                emit boxInteractiveMoveFinished(m_index, intTotalDelta);
            }
            m_activeHandle = None;
            m_hasMoved = false;
            event->accept();
            return;
        }

        if (m_hasMoved) {
            QRect oldRect = m_initialRect.toRect();
            QRect newRect = m_rect.toRect();
            if (oldRect != newRect) {
                emit boxGeometryChanged(m_index, newRect, oldRect);
            }
        }
        m_activeHandle = None;
        m_hasMoved = false;
        event->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void AtlasBoxItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selected && m_hasPolygonMesh && m_showPolygonMesh) {
        double scale = 1.0;
        if (scene() && !scene()->views().isEmpty()) {
            scale = scene()->views().first()->transform().m11();
        }
        double grabRadius = (scale > 0.0) ? (7.0 / scale) : 7.0;
        grabRadius = std::max(grabRadius, 4.0);

        QPointF newVertexLocal;
        int edgeIdx = edgeAt(event->pos(), grabRadius, &newVertexLocal);
        if (edgeIdx >= 0) {
            QPolygonF oldPoly = m_polygon;
            QList<int> oldTris = m_triangles;

            prepareGeometryChange();
            int insertIdx = edgeIdx + 1;
            m_polygon.insert(insertIdx, newVertexLocal);
            m_triangles = SpriteStudioGeometry::Triangulator::triangulate(m_polygon);

            m_selectedVertices.clear();
            m_selectedVertices.insert(insertIdx);

            emit boxPolygonMeshChanged(m_index, m_polygon, m_triangles, oldPoly, oldTris);
            update();
            event->accept();
            return;
        }
    }
    QGraphicsObject::mouseDoubleClickEvent(event);
}

void AtlasBoxItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    emit boxContextMenuRequested(m_index, event->screenPos());
    event->accept();
}

void AtlasBoxItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    const VisualConfig &vis = AppConfig::instance().visuals();

    // 1. Fill
    if (m_selected) {
        painter->fillRect(m_rect, vis.selectedBoxFillColor);
    } else if (m_hovered) {
        painter->fillRect(m_rect, vis.hoveredBoxFillColor);
    }

    // 2. Outline (cosmetic)
    QPen borderPen;
    borderPen.setCosmetic(true);
    if (m_selected) {
        borderPen.setColor(vis.selectedBoxColor);
        borderPen.setWidth(2);
        borderPen.setStyle(Qt::SolidLine);
    } else {
        borderPen.setColor(vis.unselectedBoxColor);
        borderPen.setWidth(1);
        borderPen.setStyle(Qt::SolidLine);
    }
    painter->setPen(borderPen);
    painter->drawRect(m_rect);

    // 3. Index Badge in top-left corner
    QString labelText = QString::number(m_index + 1);
    QFont badgeFont("Arial", 8, QFont::Bold);
    QFontMetrics fm(badgeFont);
    int textW = fm.horizontalAdvance(labelText);
    int textH = fm.height();
    double screenBadgeW = textW + 6.0;
    double screenBadgeH = textH + 2.0;

    double scale = 1.0;
    if (scene() && !scene()->views().isEmpty()) {
        scale = scene()->views().first()->transform().m11();
    }

    painter->save();
    painter->translate(m_rect.left(), m_rect.top());
    if (scale > 0.0) {
        painter->scale(1.0 / scale, 1.0 / scale);
    }

    QRectF badgeRect(0, 0, screenBadgeW, screenBadgeH);
    painter->fillRect(badgeRect, m_selected ? vis.selectedBoxColor : QColor(0, 50, 90, 200));

    painter->setFont(badgeFont);
    painter->setPen(m_selected ? Qt::black : Qt::white);
    painter->drawText(badgeRect, Qt::AlignCenter, labelText);
    painter->restore();

    // 3.5. 2D Polygon Mesh Wireframe & Contour
    if (m_showPolygonMesh && m_hasPolygonMesh && !m_polygon.isEmpty()) {
        painter->save();
        painter->translate(m_rect.topLeft());
        painter->setRenderHint(QPainter::Antialiasing, true);

        // Triangles wireframe
        if (!m_triangles.isEmpty() && m_triangles.size() % 3 == 0) {
            QPen triPen(QColor(0, 220, 255, 120), 1.0, Qt::DashLine);
            triPen.setCosmetic(true);
            painter->setPen(triPen);
            painter->setBrush(Qt::NoBrush);

            const int triCount = m_triangles.size() / 3;
            for (int t = 0; t < triCount; ++t) {
                int i0 = m_triangles[t * 3];
                int i1 = m_triangles[t * 3 + 1];
                int i2 = m_triangles[t * 3 + 2];
                if (i0 < m_polygon.size() && i1 < m_polygon.size() && i2 < m_polygon.size()) {
                    const QPointF &p0 = m_polygon[i0];
                    const QPointF &p1 = m_polygon[i1];
                    const QPointF &p2 = m_polygon[i2];
                    painter->drawLine(p0, p1);
                    painter->drawLine(p1, p2);
                    painter->drawLine(p2, p0);
                }
            }
        }

        // Outer polygon boundary
        QPen contourPen(QColor(0, 255, 128), 1.5, Qt::SolidLine);
        contourPen.setCosmetic(true);
        painter->setPen(contourPen);
        painter->drawPolygon(m_polygon);

        // Vertices control points when selected
        if (m_selected) {
            QPen defaultPen(Qt::black, 1.0);
            defaultPen.setCosmetic(true);
            QBrush defaultBrush(QColor(255, 230, 40)); // Yellow

            QPen selectedPen(Qt::white, 1.5);
            selectedPen.setCosmetic(true);
            QBrush selectedBrush(QColor(0, 229, 255)); // Bright cyan

            QPen hoveredPen(Qt::white, 1.2);
            hoveredPen.setCosmetic(true);
            QBrush hoveredBrush(QColor(128, 255, 255)); // Light cyan

            for (int i = 0; i < m_polygon.size(); ++i) {
                const QPointF &pt = m_polygon[i];
                if (m_selectedVertices.contains(i)) {
                    // Glowing outer ring for selected vertices
                    QPen ringPen(QColor(0, 229, 255, 180), 2.5);
                    ringPen.setCosmetic(true);
                    painter->setPen(ringPen);
                    painter->setBrush(Qt::NoBrush);
                    painter->drawEllipse(pt, 5.5, 5.5);

                    painter->setPen(selectedPen);
                    painter->setBrush(selectedBrush);
                    painter->drawEllipse(pt, 4.0, 4.0);
                } else if (i == m_hoveredVertexIndex) {
                    painter->setPen(hoveredPen);
                    painter->setBrush(hoveredBrush);
                    painter->drawEllipse(pt, 3.5, 3.5);
                } else {
                    painter->setPen(defaultPen);
                    painter->setBrush(defaultBrush);
                    painter->drawEllipse(pt, 2.5, 2.5);
                }
            }
        }

        painter->restore();
    }

    // 4. Resize Handles (only when selected)
    if (m_selected) {
        double handleSize = currentHandleSize();
        QPen handlePen(QColor(40, 40, 40));
        handlePen.setCosmetic(true);
        handlePen.setWidth(1);
        painter->setPen(handlePen);
        painter->setBrush(QColor(255, 255, 255));

        for (int h = TopLeft; h <= Left; ++h) {
            QRectF hr = getHandleRect(static_cast<Handle>(h), handleSize);
            painter->drawRect(hr);
        }

        // 5. Pivot Reticle (only when selected)
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QPointF p = m_rect.topLeft() + m_pivot;

        // Draw dark shadow/halo for high contrast against any background
        QPen shadowPen(QColor(0, 0, 0, 200), 3.0);
        shadowPen.setCosmetic(true);
        painter->setPen(shadowPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(p, 6.0, 6.0);
        painter->drawLine(QPointF(p.x() - 11.0, p.y()), QPointF(p.x() - 3.0, p.y()));
        painter->drawLine(QPointF(p.x() + 3.0, p.y()), QPointF(p.x() + 11.0, p.y()));
        painter->drawLine(QPointF(p.x(), p.y() - 11.0), QPointF(p.x(), p.y() - 3.0));
        painter->drawLine(QPointF(p.x(), p.y() + 3.0), QPointF(p.x(), p.y() + 11.0));

        // Draw bright neon reticle (cyan, or golden yellow if active/hovered)
        bool pivotActive = (m_activeHandle == Pivot);
        QColor reticleColor = pivotActive ? QColor(255, 220, 40) : QColor(0, 230, 255);

        QPen reticlePen(reticleColor, 1.5);
        reticlePen.setCosmetic(true);
        painter->setPen(reticlePen);
        painter->drawEllipse(p, 6.0, 6.0);
        painter->drawLine(QPointF(p.x() - 10.0, p.y()), QPointF(p.x() - 3.0, p.y()));
        painter->drawLine(QPointF(p.x() + 3.0, p.y()), QPointF(p.x() + 10.0, p.y()));
        painter->drawLine(QPointF(p.x(), p.y() - 10.0), QPointF(p.x(), p.y() - 3.0));
        painter->drawLine(QPointF(p.x(), p.y() + 3.0), QPointF(p.x(), p.y() + 10.0));

        // Center dot
        painter->setBrush(reticleColor);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(p, 2.0, 2.0);

        painter->restore();
    }

    painter->restore();
}
