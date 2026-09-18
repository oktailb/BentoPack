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

class SpriteDocument;
class PixelCanvas;

/**
 * @brief Surgical pixel-by-pixel sprite editor dialog.
 *
 * Provides continuous 1px drawing, eraser, flood fill, color/area selections,
 * clipboard, live preview, frame navigation, and dynamic/retro palettes.
 */
class PixelEditorDialog : public QDialog
{
    Q_OBJECT

public:
    enum PalettePreset {
        SpriteColors = 0,
        NES,
        SNES,
        Amiga,
        PCEngine,
        GameBoy,
        Pico8,
        Commodore64
    };

    explicit PixelEditorDialog(SpriteDocument *document,
                               QUndoStack *docUndoStack,
                               int initialFrameIndex = 0,
                               QWidget *parent = nullptr);
    ~PixelEditorDialog() override = default;

    static QVector<QRgb> getPresetPalette(PalettePreset preset);

private slots:
    void onPreviousFrame();
    void onNextFrame();
    void onToolButtonClicked(int id);
    void onPalettePresetChanged(int index);
    void onPrimarySwatchClicked();
    void onSecondarySwatchClicked();
    void onCanvasImageChanged();
    void onCanvasPixelMoved(int x, int y, const QColor &color);
    void onCanvasPixelLeft();
    void onCanvasZoomChanged(double zoom);
    void onApplyClicked();
    void onOkClicked();

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();
    void updateNavigationButtons();
    void loadFrame(int index);
    void saveCurrentFrameToSession();
    void refreshPaletteSwatches();
    void populateSpriteColorsPalette();
    void updateLivePreview();

    QWidget* createToolBar();
    QWidget* createPalettePanel();
    QWidget* createHeaderBar();
    QWidget* createBottomBar();

private:
    SpriteDocument*         m_document = nullptr;
    QUndoStack*             m_docUndoStack = nullptr;
    int                     m_currentFrameIndex = 0;
    QMap<int, QImage>       m_sessionModifiedFrames;

    // UI Widgets
    PixelCanvas*            m_canvas = nullptr;
    QScrollArea*            m_scrollArea = nullptr;
    QLabel*                 m_frameInfoLabel = nullptr;
    QPushButton*            m_prevFrameBtn = nullptr;
    QPushButton*            m_nextFrameBtn = nullptr;

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
    QToolButton*            m_btnZoomIn = nullptr;
    QToolButton*            m_btnZoomOut = nullptr;
    QToolButton*            m_btnFit = nullptr;
    QToolButton*            m_btnUndo = nullptr;
    QToolButton*            m_btnRedo = nullptr;

    // Palette & Colors
    QGroupBox*              m_colorsGroup = nullptr;
    QPushButton*            m_primarySwatchBtn = nullptr;
    QPushButton*            m_secondarySwatchBtn = nullptr;
    QPushButton*            m_swapBtn = nullptr;
    QLabel*                 m_palLabel = nullptr;
    QComboBox*              m_paletteCombo = nullptr;
    QWidget*                m_swatchesContainer = nullptr;
    QGridLayout*            m_swatchesLayout = nullptr;
    QVector<QRgb>           m_currentPalette;

    // Live Preview
    QGroupBox*              m_prevGroup = nullptr;
    QLabel*                 m_previewLabel = nullptr;

    // Status & Buttons
    QLabel*                 m_coordLabel = nullptr;
    QLabel*                 m_colorInfoLabel = nullptr;
    QLabel*                 m_zoomLabel = nullptr;
    QPushButton*            m_cancelBtn = nullptr;
    QPushButton*            m_applyBtn = nullptr;
    QPushButton*            m_okBtn = nullptr;
};


#endif // PIXELEDITORDIALOG_H
