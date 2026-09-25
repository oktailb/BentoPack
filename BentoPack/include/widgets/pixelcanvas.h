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

#ifndef PIXELCANVAS_H
#define PIXELCANVAS_H

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <QUndoStack>

enum class PixelTool {
    Pencil,
    Eraser,
    Eyedropper,
    BucketFill,
    SelectRect,
    SelectColor
};

#include "bentopackwidgets_export.h"

/**
 * @brief High-precision pixel-art canvas widget with zoom, grid, Bresenham drawing,
 * selections, clipboard, and local undo/redo.
 */
class BENTOPACK_WIDGETS_EXPORT PixelCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit PixelCanvas(QWidget *parent = nullptr);
    ~PixelCanvas() override = default;

    // Image data
    void setImage(const QImage &image);
    QImage image() const;
    QSize imageSize() const { return m_image.size(); }

    // Tools & Colors
    PixelTool currentTool() const { return m_tool; }
    void setCurrentTool(PixelTool tool);

    QColor primaryColor() const { return m_primaryColor; }
    void setPrimaryColor(const QColor &color);

    QColor secondaryColor() const { return m_secondaryColor; }
    void setSecondaryColor(const QColor &color);

    void swapColors();

    // Zoom & Grid
    double zoom() const { return m_zoom; }
    void setZoom(double zoom);
    void zoomIn();
    void zoomOut();
    void zoomFit(const QSize &viewportSize);

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool show);

    // Selection & Transformations
    bool hasSelection() const;
    QRect selectionRect() const { return m_selectionRect; }
    void selectAll();
    void deselect();
    void clearSelection(); // Erases pixels in selection (Suppr)

    void copySelection();
    void cutSelection();
    void pasteClipboard();
    void commitFloatingSelection();

    void flipHorizontal();
    void flipVertical();
    void rotate90CW();

    // Undo / Redo
    bool canUndo() const { return m_undoStack.canUndo(); }
    bool canRedo() const { return m_undoStack.canRedo(); }
    void undo() { commitFloatingSelection(); m_undoStack.undo(); }
    void redo() { commitFloatingSelection(); m_undoStack.redo(); }
    QUndoStack* undoStack() { return &m_undoStack; }
    void applyPatch(const QRect &rect, const QImage &patch);
    void pushSnapshot(const QImage &oldImage, const QString &text);

signals:
    void imageChanged();
    void primaryColorChanged(const QColor &color);
    void secondaryColorChanged(const QColor &color);
    void mousePixelMoved(int x, int y, const QColor &color);
    void mousePixelLeft();
    void selectionStateChanged(bool hasSelection);
    void zoomChanged(double zoom);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // Helpers
    QPoint widgetToPixel(const QPoint &widgetPos) const;
    QPoint pixelToWidget(const QPoint &pixelPos) const;
    QRect pixelToWidget(const QRect &pixelRect) const;
    bool isPixelInside(int x, int y) const;
    bool isPixelSelected(int x, int y) const;

    void drawBresenhamLine(int x0, int y0, int x1, int y1, const QColor &color);
    void applyFloodFill(int startX, int startY, const QColor &replacementColor);
    void applyColorSelection(int targetX, int targetY);

    void drawCheckerboard(QPainter &painter, const QRect &rect);
    void drawPixelGrid(QPainter &painter, const QRect &rect);
    void drawSelectionBorder(QPainter &painter);
    void drawFloatingStamp(QPainter &painter);

    void updateCanvasSize();

private:
    QImage          m_image;
    double          m_zoom = 16.0;
    bool            m_showGrid = true;
    PixelTool       m_tool = PixelTool::Pencil;
    QColor          m_primaryColor = Qt::black;
    QColor          m_secondaryColor = Qt::transparent;

    // Interaction state
    bool            m_isDrawing = false;
    bool            m_isSelecting = false;
    bool            m_isDraggingFloating = false;
    bool            m_isPanning = false;
    QPoint          m_lastPixelPos = QPoint(-1, -1);
    QPoint          m_dragStartPixel = QPoint(-1, -1);
    QPoint          m_lastPanMousePos;
    Qt::MouseButton m_activeButton = Qt::NoButton;
    QImage          m_strokePreImage;

    // Selection
    QRect           m_selectionRect;
    QVector<bool>   m_selectionMask; // size width * height

    // Clipboard & Floating Stamp
    static QImage   s_clipboardImage;
    bool            m_hasFloating = false;
    QImage          m_floatingImage;
    QPoint          m_floatingPixelPos = QPoint(0, 0);

    // Local Undo Stack
    QUndoStack      m_undoStack;
};

#endif // PIXELCANVAS_H
