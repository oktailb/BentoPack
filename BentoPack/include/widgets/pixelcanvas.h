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

#ifndef PIXELCANVAS_H
#define PIXELCANVAS_H

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <QUndoStack>
#include <QPainter>
#include <functional>

enum class PixelTool {
    Pencil,
    Eraser,
    Eyedropper,
    BucketFill,
    SelectRect,
    SelectColor,
    PolygonEdit
};

enum class PolygonEditMode {
    SelectOrMove = 0,
    AddInterior = 1,
    AddExterior = 2,
    DeleteVertex = 3,
    CutLine = 4
};

enum class OnionSkinEffect {
    TintedBlueRed = 0,   ///< Past: Blue/Cyan, Future: Red/Coral
    EdgeDetection,       ///< 1px contour / border detection only
    ChannelR,            ///< Red channel monochrome
    ChannelG,            ///< Green channel monochrome
    ChannelB,            ///< Blue channel monochrome
    Silhouette,          ///< Neutral flat silhouette
    TrueColor            ///< Original colors with translucency
};

enum class CanvasAction {
    Generic,
    Pencil,
    Eraser,
    FloodFill,
    Clear,
    Paste,
    FlipHorizontal,
    FlipVertical,
    Rotate90CW
};

enum class SelectionMode {
    Replace = 0,
    Add = 1,
    Subtract = 2,
    Intersect = 3
};

enum class BucketFillMode {
    SimilarColors = 0,
    WholeSelection = 1,
    GlobalReplace = 2
};

enum class BrushShape {
    Square = 0,
    Circle = 1
};

struct StrokeSegment {
    QPoint p1;
    QPoint p2;
    QColor color;
};

struct CanvasActionData {
    CanvasAction action = CanvasAction::Generic;
    QPoint pos;                 ///< Seed/start point for FloodFill
    QColor color;               ///< Tool/replacement color
    QRect selectionRect;
    bool hasSelection = false;
    QVector<StrokeSegment> stroke;
};

struct OnionSkinLayer {
    QImage image;
    int relativeOffset = 0; ///< Relative offset: e.g. -3, -2, -1, +1, +2, +3
    QPoint alignmentOffset = QPoint(0, 0); ///< Alignment offset based on animation pivot points (currentPivot - layerPivot)
};

/**
 * @brief Structure representing a visual layer and its active frame cel inside the Pixel Canvas (M18).
 */
struct CanvasLayer {
    QString                     id;
    QString                     name;
    bool                        visible = true;
    bool                        locked = false;
    quint8                      opacity = 255;
    int                         zOrder = 0;
    QPainter::CompositionMode   blendMode = QPainter::CompositionMode_SourceOver;
    QImage                      image; ///< Current frame cel image
    QPoint                      offset = QPoint(0, 0);

    bool operator==(const CanvasLayer &other) const {
        return id == other.id && name == other.name && visible == other.visible
               && locked == other.locked && opacity == other.opacity
               && zOrder == other.zOrder && blendMode == other.blendMode
               && offset == other.offset && image == other.image;
    }
    bool operator!=(const CanvasLayer &other) const { return !(*this == other); }
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

    // Brush & Eraser Options
    int brushSize() const { return m_brushSize; }
    void setBrushSize(int size) { m_brushSize = std::clamp(size, 1, 32); }
    int brushOpacity() const { return m_brushOpacity; }
    void setBrushOpacity(int opacity) { m_brushOpacity = std::clamp(opacity, 1, 100); }
    BrushShape brushShape() const { return m_brushShape; }
    void setBrushShape(BrushShape shape) { m_brushShape = shape; }
    bool brushHardEdge() const { return m_brushHardEdge; }
    void setBrushHardEdge(bool hard) { m_brushHardEdge = hard; }

    int eraserSize() const { return m_eraserSize; }
    void setEraserSize(int size) { m_eraserSize = std::clamp(size, 1, 32); }
    int eraserOpacity() const { return m_eraserOpacity; }
    void setEraserOpacity(int opacity) { m_eraserOpacity = std::clamp(opacity, 1, 100); }
    BrushShape eraserShape() const { return m_eraserShape; }
    void setEraserShape(BrushShape shape) { m_eraserShape = shape; }
    bool eraserHardEdge() const { return m_eraserHardEdge; }
    void setEraserHardEdge(bool hard) { m_eraserHardEdge = hard; }
    bool eraseAllLayers() const { return m_eraseAllLayers; }
    void setEraseAllLayers(bool all) { m_eraseAllLayers = all; }

    // Eyedropper Options
    bool eyedropperTargetSecondary() const { return m_eyedropperTargetSecondary; }
    void setEyedropperTargetSecondary(bool sec) { m_eyedropperTargetSecondary = sec; }

    // Bucket Fill Options
    BucketFillMode bucketMode() const { return m_bucketMode; }
    void setBucketMode(BucketFillMode mode) { m_bucketMode = mode; }
    int bucketThreshold() const { return m_bucketThreshold; }
    void setBucketThreshold(int thresh) { m_bucketThreshold = std::clamp(thresh, 0, 255); }
    bool bucketDiagonal() const { return m_bucketDiagonal; }
    void setBucketDiagonal(bool diag) { m_bucketDiagonal = diag; }
    bool bucketFillTransparent() const { return m_bucketFillTransparent; }
    void setBucketFillTransparent(bool fill) { m_bucketFillTransparent = fill; }
    int bucketOpacity() const { return m_bucketOpacity; }
    void setBucketOpacity(int opacity) { m_bucketOpacity = std::clamp(opacity, 1, 100); }

    // Selection & Transformations
    bool hasSelection() const;
    QRect selectionRect() const { return m_selectionRect; }
    bool isPixelSelected(int x, int y) const;
    void selectAll();
    void deselect();
    void clearSelection(); // Erases pixels in selection (Suppr)
    void invertSelection();

    SelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(SelectionMode mode) { m_selectionMode = mode; }

    bool selectRectFixedSquare() const { return m_selectRectFixedSquare; }
    void setSelectRectFixedSquare(bool fixed) { m_selectRectFixedSquare = fixed; }

    // Magic Wand (Fuzzy Select) Options
    bool magicWandContiguous() const { return m_magicWandContiguous; }
    void setMagicWandContiguous(bool cont) { m_magicWandContiguous = cont; }
    bool magicWandDiagonal() const { return m_magicWandDiagonal; }
    void setMagicWandDiagonal(bool diag) { m_magicWandDiagonal = diag; }
    int magicWandThreshold() const { return m_magicWandThreshold; }
    void setMagicWandThreshold(int thresh) { m_magicWandThreshold = std::clamp(thresh, 0, 255); }
    bool magicWandSelectTransparent() const { return m_magicWandSelectTransparent; }
    void setMagicWandSelectTransparent(bool sel) { m_magicWandSelectTransparent = sel; }

    // Zoom & Grid
    double zoom() const { return m_zoom; }
    void setZoom(double zoom);
    void zoomIn();
    void zoomOut();
    void zoomFit(const QSize &viewportSize);

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool show);

    void copySelection();
    void cutSelection();
    void pasteClipboard();
    void commitFloatingSelection();

    void flipHorizontal();
    void flipVertical();
    void rotate90CW();
    void applyFloodFill(int startX, int startY, const QColor &replacementColor);

    CanvasActionData lastActionData() const { return m_lastActionData; }

    // Polygon Mesh & Restriction (M8 / M19 Smart Mesh CDT)
    void setPolygonMesh(const QPolygonF &polygon);
    void setPolygonMeshData(const QPolygonF &polygon, const QList<QPointF> &vertices, const QList<int> &triangles);
    QPolygonF polygonMesh() const { return m_polygonMesh; }
    QList<QPointF> meshVertices() const { return m_meshVertices; }
    QList<int> meshTriangles() const { return m_meshTriangles; }
    bool hasPolygonMesh() const { return m_polygonMesh.size() >= 3; }

    PolygonEditMode polygonEditMode() const { return m_polygonEditMode; }
    void setPolygonEditMode(PolygonEditMode mode);
    int selectedVertexIndex() const { return m_selectedVertexIndex; }
    void setSelectedVertexIndex(int index);
    void deleteSelectedVertex();
    void applyCutLine(const QPointF &startPt, const QPointF &endPt);
    void retriangulateMesh();
    void pushMeshSnapshot(const QString &text, const QPolygonF &oldPoly, const QList<QPointF> &oldVerts, const QList<int> &oldTris);

    bool allowEditingOutsidePolygon() const { return m_allowEditingOutsidePolygon; }
    void setAllowEditingOutsidePolygon(bool allow);

    bool isPixelInsidePolygon(int x, int y) const;
    bool isPixelEditable(int x, int y) const;

    // Canvas Envelope & Pivot
    void setCanvasEnvelope(const QSize &canvasSize, const QPoint &imageOffset, const QPoint &pivotPos);
    QSize canvasSize() const;
    QPoint imageOffset() const { return m_imageOffset; }
    QPoint pivotPos() const { return m_pivotPos; }
    bool showPivot() const { return m_showPivot; }
    void setShowPivot(bool show);

    // Onion Skinning
    bool isOnionSkinEnabled() const { return m_onionSkinEnabled; }
    void setOnionSkinEnabled(bool enabled);

    void setOnionSkinLayers(const QVector<OnionSkinLayer> &layers);
    QVector<OnionSkinLayer> onionSkinLayers() const { return m_onionSkinLayers; }

    int onionSkinOpacity() const { return m_onionSkinOpacityPercent; }
    void setOnionSkinOpacity(int percent); // 0 to 100

    OnionSkinEffect onionSkinEffect() const { return m_onionSkinEffect; }
    void setOnionSkinEffect(OnionSkinEffect effect);

    QImage onionSkinComposite() const { return m_onionSkinComposite; }

    static QImage processOnionSkinLayer(const QImage &src, int relativeOffset, int opacityPercent, OnionSkinEffect effect, const QSize &targetSize);

    // Multi-Layer Support (M18)
    bool hasLayers() const { return !m_layers.isEmpty(); }
    int layerCount() const { return m_layers.size(); }
    QList<CanvasLayer> layers() const { return m_layers; }
    void setLayers(const QList<CanvasLayer> &layers, int activeIndex = 0);
    int activeLayerIndex() const { return m_activeLayerIndex; }
    void setActiveLayerIndex(int index);
    CanvasLayer activeLayer() const;
    QImage activeLayerImage() const;
    bool isLayerLocked(int index = -1) const;

    void setLayerVisible(int index, bool visible);
    void setLayerLocked(int index, bool locked);
    void setLayerOpacity(int index, quint8 opacity);
    void setLayerBlendMode(int index, QPainter::CompositionMode mode);
    void setLayerName(int index, const QString &name);
    void setLayerCelImage(int index, const QImage &img);
    void applyLayerPatch(int index, const QRect &rect, const QImage &patch);

    void addLayer(const QString &name = QString());
    void duplicateLayer(int index = -1);
    void removeLayer(int index = -1);
    void moveLayerUp(int index = -1);
    void moveLayerDown(int index = -1);
    void mergeLayerDown(int index = -1);
    void flattenLayers();

    void recomposite();

    // Multi-layer sampling & onion skinning (M18)
    bool sampleAllLayers() const { return m_sampleAllLayers; }
    void setSampleAllLayers(bool sampleAll);

    bool onionSkinCurrentLayerOnly() const { return m_onionSkinCurrentLayerOnly; }
    void setOnionSkinCurrentLayerOnly(bool currentOnly);

    friend class PixelCanvasUndoCommand;

    // Undo / Redo
    bool canUndo() const { return m_undoStack.canUndo(); }
    bool canRedo() const { return m_undoStack.canRedo(); }
    void undo() { commitFloatingSelection(); m_undoStack.undo(); }
    void redo() { commitFloatingSelection(); m_undoStack.redo(); }
    QUndoStack* undoStack() { return &m_undoStack; }
    void applyPatch(const QRect &rect, const QImage &patch);
    void pushSnapshot(const QImage &oldImage, const QString &text, const QPolygonF &oldPolygon = QPolygonF(), CanvasAction action = CanvasAction::Generic);
    void setCommandCustomUndoRedo(QUndoCommand *cmd,
                                  const std::function<void()> &undoFunc,
                                  const std::function<void()> &redoFunc);

    bool isDrawing() const { return m_isDrawing; }

signals:
    void imageChanged();
    void polygonMeshChanged(const QPolygonF &polygon);
    void meshDataChanged(const QPolygonF &polygon, const QList<QPointF> &vertices, const QList<int> &triangles);
    void selectedVertexChanged(int index, bool isInterior);
    void modificationPushed(const QImage &oldImage, const QImage &newImage,
                            const QPolygonF &oldPolygon, const QPolygonF &newPolygon,
                            CanvasAction action, QUndoCommand *parentCommand);
    void strokeFinished();
    void primaryColorChanged(const QColor &color);
    void secondaryColorChanged(const QColor &color);
    void mousePixelMoved(int x, int y, const QColor &color);
    void mousePixelLeft();
    void selectionStateChanged(bool hasSelection);
    void selectionGeometryChanged(const QRect &rect, int pixelCount);
    void zoomChanged(double zoom);
    void panRequested(int dx, int dy);
    void layersChanged();
    void activeLayerChanged(int index);
    void layerLockedAttempted();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // Helpers
    QPoint widgetToPixel(const QPoint &widgetPos) const;
    QPoint pixelToWidget(const QPoint &pixelPos) const;
    QRect pixelToWidget(const QRect &pixelRect) const;
    bool isPixelInside(int x, int y) const;

    void drawPixelBrush(int centerX, int centerY, const QColor &color, int brushSize, int opacity);
    void drawBresenhamLine(int x0, int y0, int x1, int y1, const QColor &color);
    void applyColorSelection(int targetX, int targetY);
    SelectionMode effectiveSelectionMode() const;
    void ensureSelectionMaskValid();
    void updateSelectionPreview();

    void drawCheckerboard(QPainter &painter, const QRect &rect);
    void drawPixelGrid(QPainter &painter, const QRect &rect);
    void drawSelectionBorder(QPainter &painter);
    void drawFloatingStamp(QPainter &painter);
    void drawPolygonMesh(QPainter &painter);
    void drawOnionSkins(QPainter &painter);
    void drawPivotMarker(QPainter &painter);

    void updateCanvasSize();
    void updateOnionSkinComposite();

private:
    QImage          m_image;
    QSize           m_canvasSize;
    QPoint          m_imageOffset = QPoint(0, 0);
    QPoint          m_pivotPos = QPoint(0, 0);
    bool            m_showPivot = true;
    double          m_zoom = 16.0;
    bool            m_showGrid = true;
    PixelTool       m_tool = PixelTool::Pencil;
    QColor          m_primaryColor = Qt::black;
    QColor          m_secondaryColor = Qt::transparent;

    // Brush & Eraser Settings
    int             m_brushSize = 1;
    int             m_brushOpacity = 100;
    BrushShape      m_brushShape = BrushShape::Square;
    bool            m_brushHardEdge = true;
    int             m_eraserSize = 1;
    int             m_eraserOpacity = 100;
    BrushShape      m_eraserShape = BrushShape::Square;
    bool            m_eraserHardEdge = true;
    bool            m_eraseAllLayers = false;
    bool            m_eyedropperTargetSecondary = false;

    // Bucket Fill Settings
    BucketFillMode  m_bucketMode = BucketFillMode::SimilarColors;
    int             m_bucketThreshold = 0;
    bool            m_bucketDiagonal = false;
    bool            m_bucketFillTransparent = true;
    int             m_bucketOpacity = 100;

    // Selection & Magic Wand Settings
    SelectionMode   m_selectionMode = SelectionMode::Replace;
    bool            m_selectRectFixedSquare = false;
    bool            m_magicWandContiguous = true;
    bool            m_magicWandDiagonal = false;
    int             m_magicWandThreshold = 0;
    bool            m_magicWandSelectTransparent = true;

    // Onion Skinning
    bool                    m_onionSkinEnabled = true;
    int                     m_onionSkinOpacityPercent = 50;
    OnionSkinEffect         m_onionSkinEffect = OnionSkinEffect::TintedBlueRed;
    QVector<OnionSkinLayer> m_onionSkinLayers;
    QImage                  m_onionSkinComposite;

    // Polygon Mesh (M8 / M19 Smart Mesh CDT)
    QPolygonF       m_polygonMesh;
    QList<QPointF>  m_meshVertices;
    QList<int>      m_meshTriangles;
    PolygonEditMode m_polygonEditMode = PolygonEditMode::SelectOrMove;
    int             m_selectedVertexIndex = -1;
    int             m_hoveredVertexIndex = -1;
    int             m_hoveredEdgeIndex = -1;
    bool            m_isDraggingVertex = false;
    QPointF         m_dragVertexOriginalPos;
    QPolygonF       m_dragPrePolygon;
    QList<QPointF>  m_dragPreVertices;
    QList<int>      m_dragPreTriangles;
    bool            m_allowEditingOutsidePolygon = true;
    bool            m_isCuttingLine = false;
    QPointF         m_cutLineStart;
    QPointF         m_cutLineEnd;
    QVector<bool>   m_polygonMask;
    void updatePolygonMask();
    int findVertexAt(const QPoint &widgetPos, double hitRadius = 8.0) const;
    int findBoundaryEdgeAt(const QPoint &widgetPos, double hitRadius = 6.0) const;

    // Interaction state
    bool            m_isDrawing = false;
    bool            m_isSelecting = false;
    bool            m_isDraggingFloating = false;
    bool            m_isPanning = false;
    bool            m_isSpacePressed = false;
    QPoint          m_lastPixelPos = QPoint(-1, -1);
    QPoint          m_dragStartPixel = QPoint(-1, -1);
    QPoint          m_lastPanGlobalPos;
    Qt::MouseButton m_activeButton = Qt::NoButton;
    QImage          m_strokePreImage;
    CanvasActionData m_lastActionData;
    QVector<StrokeSegment> m_currentStroke;

    // Selection
    QRect           m_selectionRect;
    QVector<bool>   m_selectionMask; // size width * height
    QRect           m_dragSelectionRect;
    QVector<bool>   m_preDragSelectionMask;
    QRect           m_preDragSelectionRect;

    // Clipboard & Floating Stamp
    static QImage   s_clipboardImage;
    bool            m_hasFloating = false;
    QImage          m_floatingImage;
    QPoint          m_floatingPixelPos = QPoint(0, 0);

    // Multi-Layer Support (M18)
    void ensureActiveCelAllocated();
    QList<CanvasLayer>  m_layers;
    int                 m_activeLayerIndex = 0;
    bool                m_sampleAllLayers = false;
    bool                m_onionSkinCurrentLayerOnly = false;

    // Local Undo Stack
    QUndoStack      m_undoStack;
};

#endif // PIXELCANVAS_H
