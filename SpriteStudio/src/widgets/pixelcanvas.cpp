#include "widgets/pixelcanvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QQueue>
#include <QTransform>
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
    PixelCanvasUndoCommand(PixelCanvas *canvas, const QImage &oldImg, const QImage &newImg, const QString &text, QUndoCommand *parent = nullptr)
        : QUndoCommand(text, parent)
        , m_canvas(canvas)
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

    bool isEmpty() const { return m_isEmpty; }

    void undo() override {
        if (!m_canvas || m_isEmpty) return;
        if (m_isFullImage) {
            m_canvas->setImage(m_oldImage);
        } else {
            m_canvas->applyPatch(m_dirtyRect, m_oldPatch);
        }
    }

    void redo() override {
        if (m_firstExecution) {
            m_firstExecution = false;
            return;
        }
        if (!m_canvas || m_isEmpty) return;
        if (m_isFullImage) {
            m_canvas->setImage(m_newImage);
        } else {
            m_canvas->applyPatch(m_dirtyRect, m_newPatch);
        }
    }

private:
    PixelCanvas *m_canvas = nullptr;
    QRect        m_dirtyRect;
    QImage       m_oldPatch;
    QImage       m_newPatch;
    QImage       m_oldImage;
    QImage       m_newImage;
    bool         m_isFullImage = false;
    bool         m_isEmpty = false;
    bool         m_firstExecution = true;
};
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
    deselect();
    updateCanvasSize();
    emit imageChanged();
    update();
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
    if (m_image.isNull() || viewportSize.width() <= 0 || viewportSize.height() <= 0) return;
    double scaleX = static_cast<double>(viewportSize.width() - 32) / std::max(1, m_image.width());
    double scaleY = static_cast<double>(viewportSize.height() - 32) / std::max(1, m_image.height());
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
    update();
}

void PixelCanvas::deselect()
{
    commitFloatingSelection();
    m_selectionRect = QRect();
    m_selectionMask.clear();
    emit selectionStateChanged(false);
    update();
}

void PixelCanvas::clearSelection()
{
    commitFloatingSelection();
    if (m_image.isNull()) return;
    QImage oldImg = m_image;
    bool anyChanged = false;

    if (hasSelection()) {
        for (int y = m_selectionRect.top(); y <= m_selectionRect.bottom(); ++y) {
            for (int x = m_selectionRect.left(); x <= m_selectionRect.right(); ++x) {
                if (isPixelInside(x, y) && isPixelSelected(x, y)) {
                    if (qAlpha(m_image.pixel(x, y)) != 0) {
                        m_image.setPixelColor(x, y, Qt::transparent);
                        anyChanged = true;
                    }
                }
            }
        }
    } else {
        if (!m_image.isNull()) {
            m_image.fill(Qt::transparent);
            anyChanged = true;
        }
    }

    if (anyChanged) {
        pushSnapshot(oldImg, tr("Clear Pixels"));
        emit imageChanged();
        update();
    }
}

void PixelCanvas::copySelection()
{
    commitFloatingSelection();
    if (m_image.isNull()) return;

    if (hasSelection()) {
        QRect r = m_selectionRect.intersected(m_image.rect());
        if (r.isEmpty()) return;
        QImage sub(r.size(), QImage::Format_ARGB32);
        sub.fill(Qt::transparent);
        for (int y = 0; y < r.height(); ++y) {
            for (int x = 0; x < r.width(); ++x) {
                int px = r.x() + x;
                int py = r.y() + y;
                if (isPixelSelected(px, py)) {
                    sub.setPixelColor(x, y, m_image.pixelColor(px, py));
                }
            }
        }
        s_clipboardImage = sub;
    } else {
        s_clipboardImage = m_image.copy();
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

    QImage oldImg = m_image;
    QPainter p(&m_image);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.drawImage(m_floatingPixelPos, m_floatingImage);
    p.end();

    m_hasFloating = false;
    m_floatingImage = QImage();
    pushSnapshot(oldImg, tr("Paste"));
    emit imageChanged();
    update();
}

void PixelCanvas::flipHorizontal()
{
    commitFloatingSelection();
    if (m_image.isNull()) return;
    QImage oldImg = m_image;

    if (hasSelection()) {
        QRect r = m_selectionRect.intersected(m_image.rect());
        for (int y = r.top(); y <= r.bottom(); ++y) {
            for (int x = 0; x < r.width() / 2; ++x) {
                int leftX = r.left() + x;
                int rightX = r.right() - x;
                QColor temp = m_image.pixelColor(leftX, y);
                m_image.setPixelColor(leftX, y, m_image.pixelColor(rightX, y));
                m_image.setPixelColor(rightX, y, temp);
            }
        }
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        m_image = m_image.flipped(Qt::Horizontal);
#else
        m_image = m_image.mirrored(true, false);
#endif
    }

    pushSnapshot(oldImg, tr("Flip Horizontal"));
    emit imageChanged();
    update();
}

void PixelCanvas::flipVertical()
{
    commitFloatingSelection();
    if (m_image.isNull()) return;
    QImage oldImg = m_image;

    if (hasSelection()) {
        QRect r = m_selectionRect.intersected(m_image.rect());
        for (int x = r.left(); x <= r.right(); ++x) {
            for (int y = 0; y < r.height() / 2; ++y) {
                int topY = r.top() + y;
                int botY = r.bottom() - y;
                QColor temp = m_image.pixelColor(x, topY);
                m_image.setPixelColor(x, topY, m_image.pixelColor(x, botY));
                m_image.setPixelColor(x, botY, temp);
            }
        }
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        m_image = m_image.flipped(Qt::Vertical);
#else
        m_image = m_image.mirrored(false, true);
#endif
    }

    pushSnapshot(oldImg, tr("Flip Vertical"));
    emit imageChanged();
    update();
}

void PixelCanvas::rotate90CW()
{
    commitFloatingSelection();
    if (m_image.isNull()) return;
    QImage oldImg = m_image;

    QTransform t;
    t.rotate(90.0);
    m_image = m_image.transformed(t).convertToFormat(QImage::Format_ARGB32);

    deselect();
    updateCanvasSize();
    pushSnapshot(oldImg, tr("Rotate 90°"));
    emit imageChanged();
    update();
}

void PixelCanvas::pushSnapshot(const QImage &oldImage, const QString &text)
{
    auto *cmd = new PixelCanvasUndoCommand(this, oldImage, m_image, text);
    if (cmd->isEmpty()) {
        delete cmd;
        return;
    }
    m_undoStack.push(cmd);
}

void PixelCanvas::applyPatch(const QRect &rect, const QImage &patch)
{
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

void PixelCanvas::updateCanvasSize()
{
    if (m_image.isNull()) {
        setFixedSize(64, 64);
        return;
    }
    int w = std::max(1, static_cast<int>(std::round(m_image.width() * m_zoom)));
    int h = std::max(1, static_cast<int>(std::round(m_image.height() * m_zoom)));
    setFixedSize(w, h);
}

QPoint PixelCanvas::widgetToPixel(const QPoint &widgetPos) const
{
    if (m_zoom <= 0.0) return QPoint(-1, -1);
    int px = static_cast<int>(std::floor(widgetPos.x() / m_zoom));
    int py = static_cast<int>(std::floor(widgetPos.y() / m_zoom));
    return QPoint(px, py);
}

QPoint PixelCanvas::pixelToWidget(const QPoint &pixelPos) const
{
    return QPoint(static_cast<int>(std::floor(pixelPos.x() * m_zoom)),
                  static_cast<int>(std::floor(pixelPos.y() * m_zoom)));
}

QRect PixelCanvas::pixelToWidget(const QRect &pixelRect) const
{
    if (m_zoom <= 0.0 || pixelRect.isEmpty()) return QRect();
    int x1 = static_cast<int>(std::floor(pixelRect.left() * m_zoom));
    int y1 = static_cast<int>(std::floor(pixelRect.top() * m_zoom));
    int x2 = static_cast<int>(std::ceil((pixelRect.right() + 1) * m_zoom));
    int y2 = static_cast<int>(std::ceil((pixelRect.bottom() + 1) * m_zoom));
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

void PixelCanvas::drawBresenhamLine(int x0, int y0, int x1, int y1, const QColor &color)
{
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (isPixelInside(x0, y0) && isPixelSelected(x0, y0)) {
            m_image.setPixelColor(x0, y0, color);
        }
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
}

void PixelCanvas::applyFloodFill(int startX, int startY, const QColor &replacementColor)
{
    if (!isPixelInside(startX, startY) || !isPixelSelected(startX, startY)) return;
    QRgb targetRgb = m_image.pixel(startX, startY);
    QRgb replaceRgb = replacementColor.rgba();
    if (targetRgb == replaceRgb) return;

    QImage oldImg = m_image;
    int w = m_image.width();
    int h = m_image.height();
    QVector<bool> visited(w * h, false);
    QQueue<QPoint> queue;

    queue.enqueue(QPoint(startX, startY));
    visited[startY * w + startX] = true;

    while (!queue.isEmpty()) {
        QPoint pt = queue.dequeue();
        int x = pt.x();
        int y = pt.y();

        m_image.setPixelColor(x, y, replacementColor);

        const int dx[] = {-1, 1, 0, 0};
        const int dy[] = {0, 0, -1, 1};
        for (int i = 0; i < 4; ++i) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int idx = ny * w + nx;
                if (!visited[idx] && isPixelSelected(nx, ny) && m_image.pixel(nx, ny) == targetRgb) {
                    visited[idx] = true;
                    queue.enqueue(QPoint(nx, ny));
                }
            }
        }
    }

    pushSnapshot(oldImg, tr("Flood Fill"));
    emit imageChanged();
    update();
}

void PixelCanvas::applyColorSelection(int targetX, int targetY)
{
    if (!isPixelInside(targetX, targetY)) return;
    commitFloatingSelection();

    QRgb targetRgb = m_image.pixel(targetX, targetY);
    int w = m_image.width();
    int h = m_image.height();
    m_selectionMask.resize(w * h);
    m_selectionMask.fill(false);

    int minX = w, maxX = -1, minY = h, maxY = -1;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (m_image.pixel(x, y) == targetRgb) {
                m_selectionMask[y * w + x] = true;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX >= minX && maxY >= minY) {
        m_selectionRect = QRect(QPoint(minX, minY), QPoint(maxX, maxY));
        emit selectionStateChanged(true);
    } else {
        deselect();
    }
    update();
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
    QRectF r(m_selectionRect.x() * m_zoom,
             m_selectionRect.y() * m_zoom,
             (m_selectionRect.width() + 1) * m_zoom,
             (m_selectionRect.height() + 1) * m_zoom);

    // Subtle outline with dashes
    painter.setPen(QPen(QColor(0, 0, 0, 180), 1.0, Qt::SolidLine));
    painter.drawRect(r);
    painter.setPen(QPen(QColor(255, 255, 255, 220), 1.0, Qt::DashLine));
    painter.drawRect(r);
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

void PixelCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (m_image.isNull()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    // 1. Checkerboard
    drawCheckerboard(painter, rect());

    // 2. Sprite image
    QRect targetRect(0, 0, width(), height());
    painter.drawImage(targetRect, m_image);

    // 3. Floating pasted stamp
    drawFloatingStamp(painter);

    // 4. Pixel grid
    drawPixelGrid(painter, rect());

    // 5. Selection bounds
    drawSelectionBorder(painter);
}

void PixelCanvas::mousePressEvent(QMouseEvent *event)
{
    QPoint pixelPos = widgetToPixel(event->pos());

    if (event->button() == Qt::MiddleButton || (event->modifiers() & Qt::AltModifier && event->button() == Qt::LeftButton && m_tool != PixelTool::Eyedropper)) {
        // Quick eyedropper on Alt+click
        if (event->modifiers() & Qt::AltModifier) {
            if (isPixelInside(pixelPos.x(), pixelPos.y())) {
                QColor picked = m_image.pixelColor(pixelPos.x(), pixelPos.y());
                setPrimaryColor(picked);
            }
            return;
        }
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
    m_strokePreImage = m_image;

    if (m_tool == PixelTool::Pencil || m_tool == PixelTool::Eraser) {
        m_isDrawing = true;
        QColor drawColor;
        if (m_tool == PixelTool::Eraser) {
            drawColor = (m_activeButton == Qt::RightButton) ? m_secondaryColor : Qt::transparent;
        } else {
            drawColor = (m_activeButton == Qt::RightButton) ? m_secondaryColor : m_primaryColor;
        }
        drawBresenhamLine(pixelPos.x(), pixelPos.y(), pixelPos.x(), pixelPos.y(), drawColor);
        emit imageChanged();
        update();
    } else if (m_tool == PixelTool::Eyedropper) {
        if (isPixelInside(pixelPos.x(), pixelPos.y())) {
            QColor picked = m_image.pixelColor(pixelPos.x(), pixelPos.y());
            if (event->button() == Qt::RightButton) {
                setSecondaryColor(picked);
            } else {
                setPrimaryColor(picked);
            }
        }
    } else if (m_tool == PixelTool::BucketFill) {
        QColor fillCol = (event->button() == Qt::RightButton) ? m_secondaryColor : m_primaryColor;
        applyFloodFill(pixelPos.x(), pixelPos.y(), fillCol);
    } else if (m_tool == PixelTool::SelectRect) {
        m_isSelecting = true;
        m_dragStartPixel = pixelPos;
        m_selectionRect = QRect(pixelPos, QSize(1, 1));
        m_selectionMask.clear();
        emit selectionStateChanged(true);
        update();
    } else if (m_tool == PixelTool::SelectColor) {
        applyColorSelection(pixelPos.x(), pixelPos.y());
    }
}

void PixelCanvas::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pixelPos = widgetToPixel(event->pos());

    if (isPixelInside(pixelPos.x(), pixelPos.y())) {
        QColor col = m_image.pixelColor(pixelPos.x(), pixelPos.y());
        emit mousePixelMoved(pixelPos.x(), pixelPos.y(), col);
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
            drawBresenhamLine(m_lastPixelPos.x(), m_lastPixelPos.y(), pixelPos.x(), pixelPos.y(), drawColor);
            m_lastPixelPos = pixelPos;
            emit imageChanged();
            update();
        }
    } else if (m_isSelecting && m_tool == PixelTool::SelectRect) {
        int x1 = std::clamp(std::min(m_dragStartPixel.x(), pixelPos.x()), 0, m_image.width() - 1);
        int y1 = std::clamp(std::min(m_dragStartPixel.y(), pixelPos.y()), 0, m_image.height() - 1);
        int x2 = std::clamp(std::max(m_dragStartPixel.x(), pixelPos.x()), 0, m_image.width() - 1);
        int y2 = std::clamp(std::max(m_dragStartPixel.y(), pixelPos.y()), 0, m_image.height() - 1);
        m_selectionRect = QRect(QPoint(x1, y1), QPoint(x2, y2));
        update();
    }
}

void PixelCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (m_isDraggingFloating) {
        m_isDraggingFloating = false;
        return;
    }

    if (m_isDrawing) {
        m_isDrawing = false;
        pushSnapshot(m_strokePreImage, (m_tool == PixelTool::Eraser) ? tr("Eraser") : tr("Pencil"));
    } else if (m_isSelecting && m_tool == PixelTool::SelectRect) {
        m_isSelecting = false;
        if (!m_selectionRect.isNull() && m_selectionRect.isValid()) {
            int w = m_image.width();
            int h = m_image.height();
            m_selectionMask.resize(w * h);
            m_selectionMask.fill(false);
            for (int y = m_selectionRect.top(); y <= m_selectionRect.bottom(); ++y) {
                for (int x = m_selectionRect.left(); x <= m_selectionRect.right(); ++x) {
                    if (isPixelInside(x, y)) {
                        m_selectionMask[y * w + x] = true;
                    }
                }
            }
        }
        emit selectionStateChanged(hasSelection());
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
    if (event->key() == Qt::Key_X) {
        swapColors();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        clearSelection();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Escape) {
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

void PixelCanvas::leaveEvent(QEvent *event)
{
    emit mousePixelLeft();
    QWidget::leaveEvent(event);
}
