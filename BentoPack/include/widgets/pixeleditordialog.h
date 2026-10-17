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

#ifndef PIXELEDITORDIALOG_H
#define PIXELEDITORDIALOG_H

#include <QDialog>
#include <QMap>
#include <QVector>
#include <QColor>
#include <QScrollArea>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QGridLayout>
#include <QUndoStack>
#include <QGroupBox>
#include <QEvent>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QRadioButton>
#include <QMainWindow>
#include <QDockWidget>
#include <QProgressBar>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <atomic>
#include "image/colorpalettepresets.h"
#include "widgets/colorpickerwidget.h"
#include "widgets/layerstackwidget.h"
#include "model/spritedocument.h"
#include "geometry/triangulator.h"

class PixelCanvas;
class FilterPlugin;
class QMenu;
enum class CanvasAction;
#include "bentopackwidgets_export.h"

/**
 * @brief Surgical pixel-by-pixel sprite editor dialog.
 *
 * Provides continuous 1px drawing, eraser, flood fill, color/area selections,
 * clipboard, live preview, frame navigation, and dynamic/retro palettes.
 */
class BENTOPACK_WIDGETS_EXPORT PixelEditorDialog : public QDialog
{
    Q_OBJECT

public:
    enum PalettePreset {
        Standard = ColorPalettePresets::Standard,
        SpriteColors = ColorPalettePresets::Standard // Compatible alias
    };

    explicit PixelEditorDialog(SpriteDocument *document,
                               QUndoStack *docUndoStack,
                               int initialFrameIndex = 0,
                               QWidget *parent = nullptr);
    ~PixelEditorDialog() override;

    static QVector<QRgb> getPresetPalette(PalettePreset preset = Standard);
    static QVector<QRgb> getPaletteById(const QString &id);

    QString activeAnimationName() const { return m_activeAnimName; }
    QList<int> activeSequence() const { return m_activeSequence; }
    int currentFrameIndex() const { return m_currentFrameIndex; }
    QComboBox* animationCombo() const { return m_animCombo; }
    PixelCanvas* canvas() const { return m_canvas; }
    LayerStackWidget* layerStackWidget() const { return m_layerStackWidget; }
    QTabWidget* rightTabWidget() const { return m_rightTabWidget; }
    QStackedWidget* contextualStack() const { return m_contextualStack; }
    QButtonGroup* toolButtonGroup() const { return m_toolGroup; }
    QPoint visualPivotPos() const;
    QCheckBox* allowOutsidePolygonCheckBox() const { return m_allowOutsidePolyCheck; }
    bool isEditingOutsidePolygonAllowed() const;
    QCheckBox* applyToAllFramesCheckBox() const { return m_applyToAllFramesCheck; }
    QCheckBox* meshApplyAllFramesCheckBox() const { return m_meshApplyAllFramesCheck; }
    bool isApplyToAllFramesEnabled() const { return m_applyToAllFramesCheck && m_applyToAllFramesCheck->isChecked(); }
    void setApplyToAllFrames(bool enabled);
    QToolButton* filtersButton() const { return m_btnFilters; }
    void applyFilterToSession(FilterPlugin *filter);
    bool hasAtlasCollision() const;
    QLabel* collisionAlertLabel() const { return m_lblCollisionWarning; }
    QPushButton* repackButton() const { return m_btnRepackAtlas; }
    QPushButton* applyButton() const { return m_applyBtn; }
    QPolygonF sessionModifiedPolygon(int frameIndex) const;
    QMap<int, QImage> sessionModifiedFrames() const { return m_sessionModifiedFrames; }
    QMap<int, QPolygonF> sessionModifiedPolygons() const { return m_sessionModifiedPolygons; }
    QMap<int, QList<QPointF>> sessionModifiedVertices() const { return m_sessionModifiedVertices; }
    QMap<int, QList<int>> sessionModifiedTriangles() const { return m_sessionModifiedTriangles; }
    struct CdtBatchWorkItem {
        int index = -1;
        QImage image;
        QPolygonF polygon;
        BentoPackGeometry::SmartMeshParams params;
    };

    struct CdtFrameResult {
        int index = -1;
        QPolygonF outerPolygon;
        QList<QPointF> vertices;
        QList<int> triangles;
        bool success = false;
    };

    struct CdtBatchJobResult {
        QList<CdtFrameResult> results;
        int totalFrames = 0;
        bool cancelled = false;
    };

    QPushButton* generateSmartMeshButton() const { return m_btnGenerateSmartMesh; }
    QPushButton* resetMeshToOutlineButton() const { return m_btnResetToOutline; }
    QProgressBar* cdtProgressBar() const { return m_cdtProgressBar; }
    QLabel* cdtFeedbackLabel() const { return m_lblCdtFeedback; }
    QPushButton* cancelCdtMeshButton() const { return m_btnCancelCdtMesh; }
    bool isCdtProcessing() const { return m_isCdtProcessing; }
    void waitForCdtMeshFinished();
    void cancelCdtMeshCalculation();

    bool applyChanges();
    bool openAtlasPackingDialog(bool nonInteractive = false);
    bool performAtlasRepack(const QMap<int, QImage> &modifiedFrames, const QMap<int, QPolygonF> &modifiedPolygons);

    QMainWindow* dockMainWindow() const { return m_dockMainWindow; }
    QDockWidget* toolsDock() const { return m_dockTools; }
    QDockWidget* optionsDock() const { return m_dockOptions; }
    QToolButton* panelsButton() const { return m_btnPanels; }
    void resetDefaultLayout();
    void saveLayoutState();
    void reject() override;

public slots:
    void onPreviousFrame();
    void onNextFrame();
    void loadFrame(int index);
    void onGenerateSmartMeshRequested(bool async = true);
    void onResetMeshToOutlineRequested();
    void updateToolIcons();

private slots:
    void onToolButtonClicked(int id);
    void onPalettePresetChanged(int index);
    void onPrimarySwatchClicked();
    void onSecondarySwatchClicked();
    void onCanvasImageChanged();
    void onCanvasPixelMoved(int x, int y, const QColor &color);
    void onCanvasPixelLeft();
    void onCanvasZoomChanged(double zoom);
    void onPickColorClicked();
    void onSampleFrameColorsClicked();
    void onApplyClicked();
    void onOkClicked();
    void onOnionSkinToggled(bool enabled);
    void onOnionSkinPastChanged(int val);
    void onOnionSkinFutureChanged(int val);
    void onOnionSkinOpacityChanged(int val);
    void onOnionSkinEffectChanged(int index);
    void onAnimationFilterChanged(int index);
    void onAllowOutsidePolygonToggled(bool checked);
    void onCanvasModificationPushed(const QImage &oldImage, const QImage &newImage,
                                    const QPolygonF &oldPolygon, const QPolygonF &newPolygon,
                                    CanvasAction action, QUndoCommand *parentCommand);

    // Multi-Layer Slots (M18)
    void onLayerVisibilityChanged(int index, bool visible);
    void onLayerLockChanged(int index, bool locked);
    void onLayerOpacityChanged(int index, quint8 opacity);
    void onLayerBlendModeChanged(int index, QPainter::CompositionMode mode);
    void onLayerNameChanged(int index, const QString &name);
    void onAddLayerRequested();
    void onDuplicateLayerRequested();
    void onRemoveLayerRequested();
    void onMoveLayerUpRequested();
    void onMoveLayerDownRequested();
    void onMergeLayerDownRequested();
    void onFlattenLayersRequested();
    void onSampleAllLayersChanged(bool enabled);
    void onOnionSkinCurrentLayerOnlyChanged(bool enabled);
    void onCanvasLayersChanged();
    void onCanvasActiveLayerChanged(int index);

    // Smart Mesh & CDT Slots (M19)
    void onMeshEditModeChanged(int id);
    void onDeleteSelectedVertexRequested();
    void onCanvasMeshDataChanged(const QPolygonF &poly, const QList<QPointF> &verts, const QList<int> &tris);
    void onCanvasSelectedVertexChanged(int index, bool isInterior);
    void updateMeshStatsUI();
    void onCdtBatchFinished();

public:
    void restoreFrameBackup(int frameIndex, const QImage &img, const QPolygonF &poly,
                            const QList<QPointF> &verts = {}, const QList<int> &tris = {});
    void onMultiFrameUndoRedoDone();

protected:
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateNavigationButtons();
    void populateAnimationCombo();
    void saveCurrentFrameToSession();
    bool checkAtlasPolygonCollision(QString *outDetails = nullptr, QList<int> *outCollidingIndices = nullptr) const;
    void updateCollisionWarningUI();
    void refreshPaletteSwatches();
    void refreshRecentSwatches();
    void addRecentColor(const QColor &color);
    void updateLivePreview();
    void updateOnionSkinLayers();
    void populateFiltersMenu(QMenu *menu);
    void computeAnimationEnvelope(QSize &outSize, QPoint &outPivot, QPoint &outFrameOffset) const;
    void syncSessionLayersFromCanvas();
    void setCdtProcessingState(bool processing);

    QWidget* createToolBar();
    QWidget* createPalettePanel();
    QWidget* createHeaderBar();
    QWidget* createBottomBar();

    QWidget* createPencilOptionsPage();
    QWidget* createEraserOptionsPage();
    QWidget* createSelectionOptionsPage();
    QWidget* createEyedropperOptionsPage();
    QWidget* createMeshOptionsPage();
    QWidget* createBucketOptionsPage();
    QWidget* createMagicWandOptionsPage();

private:
    SpriteDocument*         m_document = nullptr;
    QUndoStack*             m_docUndoStack = nullptr;
    int                     m_currentFrameIndex = 0;
    QMap<int, QImage>       m_sessionModifiedFrames;
    QMap<int, QPolygonF>    m_sessionModifiedPolygons;
    QMap<int, QList<QPointF>> m_sessionModifiedVertices;
    QMap<int, QList<int>>     m_sessionModifiedTriangles;
    QMap<int, QList<SpriteCel>> m_sessionModifiedCels;
    QList<SpriteLayer>      m_sessionLayers;
    bool                    m_sessionLayersModified = false;

    // Animation Scoping
    QLabel*                 m_animLabel = nullptr;
    QComboBox*              m_animCombo = nullptr;
    QString                 m_activeAnimName;
    QList<int>              m_activeSequence;

    // UI Widgets & Docking
    QMainWindow*            m_dockMainWindow = nullptr;
    QDockWidget*            m_dockTools = nullptr;
    QDockWidget*            m_dockOptions = nullptr;
    QToolButton*            m_btnPanels = nullptr;
    PixelCanvas*            m_canvas = nullptr;
    QScrollArea*            m_scrollArea = nullptr;
    QLabel*                 m_frameInfoLabel = nullptr;
    QPushButton*            m_prevFrameBtn = nullptr;
    QPushButton*            m_nextFrameBtn = nullptr;
    bool                    m_initialZoomDone = false;
    bool                    m_firstShowFitDone = false;

    // Tools
    QButtonGroup*           m_toolGroup = nullptr;
    QToolButton*            m_btnPencil = nullptr;
    QToolButton*            m_btnEraser = nullptr;
    QToolButton*            m_btnEyedropper = nullptr;
    QToolButton*            m_btnBucket = nullptr;
    QToolButton*            m_btnSelectRect = nullptr;
    QToolButton*            m_btnSelectColor = nullptr;
    QToolButton*            m_btnToolPolygon = nullptr;
    QToolButton*            m_btnFlipH = nullptr;
    QToolButton*            m_btnFlipV = nullptr;
    QToolButton*            m_btnRotate = nullptr;
    QToolButton*            m_btnGrid = nullptr;
    QToolButton*            m_btnShowPivot = nullptr;
    QCheckBox*              m_allowOutsidePolyCheck = nullptr;
    QCheckBox*              m_applyToAllFramesCheck = nullptr;
    QToolButton*            m_btnFilters = nullptr;
    QToolButton*            m_btnZoomIn = nullptr;
    QToolButton*            m_btnZoomOut = nullptr;
    QToolButton*            m_btnFit = nullptr;
    QToolButton*            m_btnUndo = nullptr;
    QToolButton*            m_btnRedo = nullptr;
    QToolButton*            m_btnClearSel = nullptr;

    // Contextual Panel (Ergonomic Tool-Aware Suite, Left Panel)
    QStackedWidget*         m_contextualStack = nullptr;
    QWidget*                m_pencilOptionsPage = nullptr;
    QWidget*                m_eraserOptionsPage = nullptr;
    QWidget*                m_selectionOptionsPage = nullptr;
    QWidget*                m_eyedropperOptionsPage = nullptr;
    QWidget*                m_meshOptionsPage = nullptr;
    QWidget*                m_bucketOptionsPage = nullptr;
    QWidget*                m_magicWandOptionsPage = nullptr;
    QWidget*                m_colorOptionsPage = nullptr;

    // Contextual: Pencil
    QGroupBox*              m_pencilGroup = nullptr;
    QSlider*                m_pencilSizeSlider = nullptr;
    QSpinBox*               m_pencilSizeSpin = nullptr;
    QSlider*                m_pencilOpacitySlider = nullptr;
    QSpinBox*               m_pencilOpacitySpin = nullptr;
    QComboBox*              m_pencilShapeCombo = nullptr;
    QCheckBox*              m_pencilHardEdgeCheck = nullptr;
    QLabel*                 m_pencilTipLabel = nullptr;

    // Contextual: Eraser
    QGroupBox*              m_eraserGroup = nullptr;
    QSlider*                m_eraserSizeSlider = nullptr;
    QSpinBox*               m_eraserSizeSpin = nullptr;
    QSlider*                m_eraserOpacitySlider = nullptr;
    QSpinBox*               m_eraserOpacitySpin = nullptr;
    QComboBox*              m_eraserShapeCombo = nullptr;
    QCheckBox*              m_eraserHardEdgeCheck = nullptr;
    QCheckBox*              m_eraserApplyAllFramesCheck = nullptr;
    QCheckBox*              m_eraserAllLayersCheck = nullptr;
    QLabel*                 m_eraserTipLabel = nullptr;

    // Contextual: Selection (Rect)
    QGroupBox*              m_selGroup = nullptr;
    QButtonGroup*           m_selModeGroup = nullptr;
    QToolButton*            m_btnSelModeReplace = nullptr;
    QToolButton*            m_btnSelModeAdd = nullptr;
    QToolButton*            m_btnSelModeSubtract = nullptr;
    QToolButton*            m_btnSelModeIntersect = nullptr;
    QCheckBox*              m_selFixedSquareCheck = nullptr;
    QLabel*                 m_selPosLabel = nullptr;
    QLabel*                 m_selSizeLabel = nullptr;
    QLabel*                 m_selCountLabel = nullptr;
    QPushButton*            m_btnSelectAll = nullptr;
    QPushButton*            m_btnDeselect = nullptr;
    QPushButton*            m_btnInvertSel = nullptr;
    QPushButton*            m_btnClearSelection = nullptr;
    QPushButton*            m_btnCopySel = nullptr;
    QPushButton*            m_btnCutSel = nullptr;
    QPushButton*            m_btnPasteSel = nullptr;
    QLabel*                 m_selTipLabel = nullptr;

    // Contextual: Eyedropper
    QGroupBox*              m_eyeGroup = nullptr;
    QRadioButton*           m_radioSampleActiveLayer = nullptr;
    QRadioButton*           m_radioSampleAllLayers = nullptr;
    QButtonGroup*           m_eyeTargetGroup = nullptr;
    QRadioButton*           m_radioEyeTargetPrimary = nullptr;
    QRadioButton*           m_radioEyeTargetSecondary = nullptr;
    QLabel*                 m_eyeColorPreviewSwatch = nullptr;
    QLabel*                 m_eyeColorHexLabel = nullptr;
    QLabel*                 m_eyeTipLabel = nullptr;

    // Contextual: Smart Mesh CDT (M19)
    QLabel*                 m_meshNoticeLabel = nullptr;
    QWidget*                m_meshControlsContainer = nullptr;
    QGroupBox*              m_meshModeGroup_box = nullptr;
    QButtonGroup*           m_meshModeGroup = nullptr;
    QRadioButton*           m_radioMeshSelectMove = nullptr;
    QRadioButton*           m_radioMeshAddInterior = nullptr;
    QRadioButton*           m_radioMeshAddExterior = nullptr;
    QRadioButton*           m_radioMeshDelete = nullptr;
    QRadioButton*           m_radioMeshCutLine = nullptr;
    QPushButton*            m_btnDeleteSelectedVertex = nullptr;
    QGroupBox*              m_cdtGroup = nullptr;
    QLabel*                 m_lblContrastModeTitle = nullptr;
    QComboBox*              m_comboContrastMode = nullptr;
    QLabel*                 m_lblSteinerDensityTitle = nullptr;
    QSlider*                m_sliderSteinerDensity = nullptr;
    QLabel*                 m_lblSteinerDensityVal = nullptr;
    QLabel*                 m_lblMinAngleTitle = nullptr;
    QSlider*                m_sliderMinAngle = nullptr;
    QLabel*                 m_lblMinAngleVal = nullptr;
    QLabel*                 m_lblContrastSensitivityTitle = nullptr;
    QSlider*                m_sliderContrastSensitivity = nullptr;
    QLabel*                 m_lblContrastSensitivityVal = nullptr;
    QPushButton*            m_btnGenerateSmartMesh = nullptr;
    QPushButton*            m_btnResetToOutline = nullptr;
    QCheckBox*              m_meshApplyAllFramesCheck = nullptr;
    QProgressBar*           m_cdtProgressBar = nullptr;
    QLabel*                 m_lblCdtFeedback = nullptr;
    QPushButton*            m_btnCancelCdtMesh = nullptr;
    QFutureWatcher<CdtBatchJobResult> m_cdtBatchWatcher;
    std::atomic<bool>       m_cancelCdtMesh{false};
    bool                    m_isCdtProcessing = false;
    QGroupBox*              m_meshStatsGroup = nullptr;
    QLabel*                 m_lblMeshBoundaryTitle = nullptr;
    QLabel*                 m_lblMeshBoundaryVerts = nullptr;
    QLabel*                 m_lblMeshInteriorTitle = nullptr;
    QLabel*                 m_lblMeshInteriorVerts = nullptr;
    QLabel*                 m_lblMeshTrianglesTitle = nullptr;
    QLabel*                 m_lblMeshTriangles = nullptr;
    QLabel*                 m_lblMeshOverdrawTitle = nullptr;
    QLabel*                 m_lblMeshOverdrawSavings = nullptr;

    // Contextual: Bucket Fill
    QGroupBox*              m_bucketGroup = nullptr;
    QButtonGroup*           m_bucketModeGroup = nullptr;
    QRadioButton*           m_radioBucketSimilar = nullptr;
    QRadioButton*           m_radioBucketSelection = nullptr;
    QRadioButton*           m_radioBucketGlobal = nullptr;
    QSlider*                m_bucketThresholdSlider = nullptr;
    QSpinBox*               m_bucketThresholdSpin = nullptr;
    QSlider*                m_bucketOpacitySlider = nullptr;
    QSpinBox*               m_bucketOpacitySpin = nullptr;
    QCheckBox*              m_bucketDiagonalCheck = nullptr;
    QCheckBox*              m_bucketTransparentCheck = nullptr;
    QCheckBox*              m_bucketSampleMergedCheck = nullptr;
    QLabel*                 m_bucketTipLabel = nullptr;

    // Contextual: Magic Wand (Fuzzy Select)
    QGroupBox*              m_wandGroup = nullptr;
    QButtonGroup*           m_wandModeGroup = nullptr;
    QToolButton*            m_btnWandModeReplace = nullptr;
    QToolButton*            m_btnWandModeAdd = nullptr;
    QToolButton*            m_btnWandModeSubtract = nullptr;
    QToolButton*            m_btnWandModeIntersect = nullptr;
    QCheckBox*              m_wandContiguousCheck = nullptr;
    QSlider*                m_wandThresholdSlider = nullptr;
    QSpinBox*               m_wandThresholdSpin = nullptr;
    QCheckBox*              m_wandDiagonalCheck = nullptr;
    QCheckBox*              m_wandTransparentCheck = nullptr;
    QCheckBox*              m_wandSampleMergedCheck = nullptr;
    QPushButton*            m_btnWandDeselect = nullptr;
    QPushButton*            m_btnWandInvert = nullptr;
    QPushButton*            m_btnWandClear = nullptr;
    QPushButton*            m_btnWandCopy = nullptr;
    QPushButton*            m_btnWandCut = nullptr;
    QLabel*                 m_wandTipLabel = nullptr;

    // Palette & Colors
    QGroupBox*              m_colorsGroup = nullptr;
    ColorPickerWidget*      m_colorPickerWidget = nullptr;
    QPushButton*            m_primarySwatchBtn = nullptr;
    QPushButton*            m_secondarySwatchBtn = nullptr;
    QPushButton*            m_swapBtn = nullptr;
    QPushButton*            m_btnPickColor = nullptr;
    QLabel*                 m_primaryHexLabel = nullptr;
    QLabel*                 m_rgbLabel = nullptr;
    QLabel*                 m_recentLabel = nullptr;
    QWidget*                m_recentContainer = nullptr;
    QHBoxLayout*            m_recentLayout = nullptr;
    QVector<QColor>         m_recentColors;

    QLabel*                 m_palLabel = nullptr;
    QComboBox*              m_paletteCombo = nullptr;
    QPushButton*            m_btnSampleFrame = nullptr;
    QWidget*                m_swatchesContainer = nullptr;
    QGridLayout*            m_swatchesLayout = nullptr;
    QVector<QRgb>           m_currentPalette;

    // Right Column Tabs (Ergonomic separation of Tool Options vs Layers)
    QTabWidget*             m_rightTabWidget = nullptr;
    QWidget*                m_toolTab = nullptr;
    QWidget*                m_layersTab = nullptr;

    // Layer Stack (M18)
    QGroupBox*              m_layerStackGroup = nullptr;
    LayerStackWidget*       m_layerStackWidget = nullptr;

    // Live Preview
    QGroupBox*              m_prevGroup = nullptr;
    QLabel*                 m_previewLabel = nullptr;

    // Onion Skinning
    QGroupBox*              m_onionSkinGroup = nullptr;
    QCheckBox*              m_onionSkinCheck = nullptr;
    QLabel*                 m_lblPastTitle = nullptr;
    QSlider*                m_sliderPastFrames = nullptr;
    QLabel*                 m_lblPastFrames = nullptr;
    QLabel*                 m_lblFutureTitle = nullptr;
    QSlider*                m_sliderFutureFrames = nullptr;
    QLabel*                 m_lblFutureFrames = nullptr;
    QLabel*                 m_lblOpacityTitle = nullptr;
    QSlider*                m_sliderOpacity = nullptr;
    QLabel*                 m_lblOpacity = nullptr;
    QLabel*                 m_lblEffect = nullptr;
    QComboBox*              m_comboEffect = nullptr;

    // Status & Buttons
    QLabel*                 m_coordLabel = nullptr;
    QLabel*                 m_hoverColorSwatch = nullptr;
    QLabel*                 m_colorInfoLabel = nullptr;
    QLabel*                 m_lblCollisionWarning = nullptr;
    QPushButton*            m_btnRepackAtlas = nullptr;
    QLabel*                 m_zoomLabel = nullptr;
    QPushButton*            m_cancelBtn = nullptr;
    QPushButton*            m_applyBtn = nullptr;
    QPushButton*            m_okBtn = nullptr;
};


#endif // PIXELEDITORDIALOG_H
