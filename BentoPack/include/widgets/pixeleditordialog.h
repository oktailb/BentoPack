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
#include <QCheckBox>
#include "image/colorpalettepresets.h"
#include "widgets/colorpickerwidget.h"

class SpriteDocument;
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
        SpriteColors = ColorPalettePresets::Standard, // Compatible alias
        GameBoy = ColorPalettePresets::GameBoyDMG,
        GameBoyPocket = ColorPalettePresets::GameBoyPocket,
        NES = ColorPalettePresets::NES,
        SNES = ColorPalettePresets::SNES,
        Pico8 = ColorPalettePresets::Pico8,
        Commodore64 = ColorPalettePresets::Commodore64,
        Amiga = ColorPalettePresets::Amiga,
        PCEngine = ColorPalettePresets::PCEngine,
        CGAMode1 = ColorPalettePresets::CGAMode1,
        CGAMode2 = ColorPalettePresets::CGAMode2,
        Endesga32 = ColorPalettePresets::Endesga32
    };

    explicit PixelEditorDialog(SpriteDocument *document,
                               QUndoStack *docUndoStack,
                               int initialFrameIndex = 0,
                               QWidget *parent = nullptr);
    ~PixelEditorDialog() override;

    static QVector<QRgb> getPresetPalette(PalettePreset preset);

    QString activeAnimationName() const { return m_activeAnimName; }
    QList<int> activeSequence() const { return m_activeSequence; }
    int currentFrameIndex() const { return m_currentFrameIndex; }
    QComboBox* animationCombo() const { return m_animCombo; }
    PixelCanvas* canvas() const { return m_canvas; }
    QPoint visualPivotPos() const;
    QCheckBox* allowOutsidePolygonCheckBox() const { return m_allowOutsidePolyCheck; }
    bool isEditingOutsidePolygonAllowed() const;
    QCheckBox* applyToAllFramesCheckBox() const { return m_applyToAllFramesCheck; }
    bool isApplyToAllFramesEnabled() const { return m_applyToAllFramesCheck && m_applyToAllFramesCheck->isChecked(); }
    void setApplyToAllFrames(bool enabled);
    QToolButton* filtersButton() const { return m_btnFilters; }
    void applyFilterToSession(FilterPlugin *filter);
    bool hasAtlasCollision() const;
    QLabel* collisionAlertLabel() const { return m_lblCollisionWarning; }
    QPushButton* repackButton() const { return m_btnRepackAtlas; }
    QPolygonF sessionModifiedPolygon(int frameIndex) const;
    QMap<int, QImage> sessionModifiedFrames() const { return m_sessionModifiedFrames; }
    QMap<int, QPolygonF> sessionModifiedPolygons() const { return m_sessionModifiedPolygons; }
    bool openAtlasPackingDialog(bool nonInteractive = false);
    bool performAtlasRepack(const QMap<int, QImage> &modifiedFrames, const QMap<int, QPolygonF> &modifiedPolygons);

public slots:
    void onPreviousFrame();
    void onNextFrame();
    void loadFrame(int index);

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

public:
    void restoreFrameBackup(int frameIndex, const QImage &img, const QPolygonF &poly);
    void onMultiFrameUndoRedoDone();

protected:
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateNavigationButtons();
    void populateAnimationCombo();
    void saveCurrentFrameToSession();
    bool applyChanges();
    bool checkAtlasPolygonCollision(QString *outDetails = nullptr, QList<int> *outCollidingIndices = nullptr) const;
    void updateCollisionWarningUI();
    void refreshPaletteSwatches();
    void refreshRecentSwatches();
    void addRecentColor(const QColor &color);
    void updateLivePreview();
    void updateOnionSkinLayers();
    void populateFiltersMenu(QMenu *menu);
    void computeAnimationEnvelope(QSize &outSize, QPoint &outPivot, QPoint &outFrameOffset) const;

    QWidget* createToolBar();
    QWidget* createPalettePanel();
    QWidget* createHeaderBar();
    QWidget* createBottomBar();

private:
    SpriteDocument*         m_document = nullptr;
    QUndoStack*             m_docUndoStack = nullptr;
    int                     m_currentFrameIndex = 0;
    QMap<int, QImage>       m_sessionModifiedFrames;
    QMap<int, QPolygonF>    m_sessionModifiedPolygons;

    // Animation Scoping
    QLabel*                 m_animLabel = nullptr;
    QComboBox*              m_animCombo = nullptr;
    QString                 m_activeAnimName;
    QList<int>              m_activeSequence;

    // UI Widgets
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
