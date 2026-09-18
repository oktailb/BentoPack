#include "widgets/pixeleditordialog.h"
#include "widgets/pixelcanvas.h"
#include "model/spritedocument.h"
#include "commands/commands.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QFrame>
#include <QSet>
#include <QPainter>
#include <QShortcut>
#include <algorithm>

PixelEditorDialog::PixelEditorDialog(SpriteDocument *document,
                                     QUndoStack *docUndoStack,
                                     int initialFrameIndex,
                                     QWidget *parent)
    : QDialog(parent)
    , m_document(document)
    , m_docUndoStack(docUndoStack)
    , m_currentFrameIndex(initialFrameIndex)
{
    setWindowTitle(tr("Pixel Editor — SpriteStudio"));
    resize(1000, 680);
    setMinimumSize(800, 500);

    setupUi();

    if (m_document && m_document->frameCount() > 0) {
        if (m_currentFrameIndex < 0 || m_currentFrameIndex >= m_document->frameCount()) {
            m_currentFrameIndex = 0;
        }
        loadFrame(m_currentFrameIndex);
    }

    // Default palette is Sprite Colors
    onPalettePresetChanged(0);
}

void PixelEditorDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // 1. Header Bar (Navigation & Info)
    mainLayout->addWidget(createHeaderBar());

    // 2. Central Area (Toolbar + Canvas + Palette Panel)
    QHBoxLayout *centerLayout = new QHBoxLayout();
    centerLayout->setSpacing(6);

    // Left Toolbar
    centerLayout->addWidget(createToolBar());

    // Canvas inside ScrollArea
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background-color: #1e1e24; border: 1px solid #3a3a44; border-radius: 4px; }"));

    m_canvas = new PixelCanvas(this);
    m_scrollArea->setWidget(m_canvas);
    centerLayout->addWidget(m_scrollArea, 1);

    // Right Palette & Preview Panel
    centerLayout->addWidget(createPalettePanel());

    mainLayout->addLayout(centerLayout, 1);

    // 3. Bottom Bar (Status & Buttons)
    mainLayout->addWidget(createBottomBar());

    // Connect canvas signals
    connect(m_canvas, &PixelCanvas::imageChanged, this, &PixelEditorDialog::onCanvasImageChanged);
    connect(m_canvas, &PixelCanvas::mousePixelMoved, this, &PixelEditorDialog::onCanvasPixelMoved);
    connect(m_canvas, &PixelCanvas::mousePixelLeft, this, &PixelEditorDialog::onCanvasPixelLeft);
    connect(m_canvas, &PixelCanvas::zoomChanged, this, &PixelEditorDialog::onCanvasZoomChanged);

    connect(m_canvas, &PixelCanvas::primaryColorChanged, this, [this](const QColor &col) {
        if (m_primarySwatchBtn) {
            m_primarySwatchBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 2px solid #ffffff; border-radius: 4px;").arg(col.name()));
        }
    });
    connect(m_canvas, &PixelCanvas::secondaryColorChanged, this, [this](const QColor &col) {
        if (m_secondarySwatchBtn) {
            m_secondarySwatchBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 2px solid #888888; border-radius: 4px;").arg(col.name()));
        }
    });

    // Keyboard navigation shortcuts
    QShortcut *prevShortcut = new QShortcut(QKeySequence(Qt::Key_PageUp), this);
    connect(prevShortcut, &QShortcut::activated, this, &PixelEditorDialog::onPreviousFrame);
    QShortcut *nextShortcut = new QShortcut(QKeySequence(Qt::Key_PageDown), this);
    connect(nextShortcut, &QShortcut::activated, this, &PixelEditorDialog::onNextFrame);
}

QWidget* PixelEditorDialog::createHeaderBar()
{
    QWidget *bar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(4, 2, 4, 2);

    m_prevFrameBtn = new QPushButton(tr("◀ Previous Frame"), bar);
    m_prevFrameBtn->setToolTip(tr("Navigate to previous frame (Page Up)"));
    connect(m_prevFrameBtn, &QPushButton::clicked, this, &PixelEditorDialog::onPreviousFrame);
    layout->addWidget(m_prevFrameBtn);

    m_frameInfoLabel = new QLabel(tr("Frame 1 / 1 (32x32 px)"), bar);
    m_frameInfoLabel->setAlignment(Qt::AlignCenter);
    m_frameInfoLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px; color: #e0e0e0;"));
    layout->addWidget(m_frameInfoLabel, 1);

    m_nextFrameBtn = new QPushButton(tr("Next Frame ▶"), bar);
    m_nextFrameBtn->setToolTip(tr("Navigate to next frame (Page Down)"));
    connect(m_nextFrameBtn, &QPushButton::clicked, this, &PixelEditorDialog::onNextFrame);
    layout->addWidget(m_nextFrameBtn);

    return bar;
}

QWidget* PixelEditorDialog::createToolBar()
{
    QWidget *panel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(true);

    auto addToolBtn = [this, layout](const QString &text, const QString &tooltip, PixelTool /*tool*/, int id, bool checked = false) -> QToolButton* {
        QToolButton *btn = new QToolButton(this);
        btn->setText(text);
        btn->setToolTip(tooltip);
        btn->setCheckable(true);
        btn->setChecked(checked);
        btn->setFixedSize(38, 36);
        btn->setStyleSheet(QStringLiteral("QToolButton { font-size: 12px; font-weight: bold; }"));
        m_toolGroup->addButton(btn, id);
        layout->addWidget(btn);
        return btn;
    };

    m_btnPencil = addToolBtn(QStringLiteral("✏"), tr("Pencil (1px continuous Bresenham) [P]"), PixelTool::Pencil, static_cast<int>(PixelTool::Pencil), true);
    m_btnEraser = addToolBtn(QStringLiteral("🧹"), tr("Eraser (1px clear to alpha 0) [E]"), PixelTool::Eraser, static_cast<int>(PixelTool::Eraser));
    m_btnEyedropper = addToolBtn(QStringLiteral("💧"), tr("Eyedropper / Pipette (Alt+Click or [I])"), PixelTool::Eyedropper, static_cast<int>(PixelTool::Eyedropper));
    m_btnBucket = addToolBtn(QStringLiteral("🪣"), tr("Bucket Fill (Flood Fill 4-way) [G]"), PixelTool::BucketFill, static_cast<int>(PixelTool::BucketFill));
    m_btnSelectRect = addToolBtn(QStringLiteral("⬚"), tr("Rectangular Marquee Selection [M]"), PixelTool::SelectRect, static_cast<int>(PixelTool::SelectRect));
    m_btnSelectColor = addToolBtn(QStringLiteral("🪄"), tr("Magic Wand (Color Selection) [W]"), PixelTool::SelectColor, static_cast<int>(PixelTool::SelectColor));

    connect(m_toolGroup, &QButtonGroup::idClicked, this, &PixelEditorDialog::onToolButtonClicked);

    // Separator
    QFrame *line1 = new QFrame(this);
    line1->setFrameShape(QFrame::HLine);
    layout->addWidget(line1);

    // Quick Transform Buttons
    auto addActionBtn = [this, layout](const QString &text, const QString &tooltip, const auto &slot) -> QToolButton* {
        QToolButton *btn = new QToolButton(this);
        btn->setText(text);
        btn->setToolTip(tooltip);
        btn->setFixedSize(38, 32);
        connect(btn, &QToolButton::clicked, this, slot);
        layout->addWidget(btn);
        return btn;
    };

    m_btnFlipH = addActionBtn(QStringLiteral("⇄"), tr("Flip Horizontal"), [this]() { m_canvas->flipHorizontal(); });
    m_btnFlipV = addActionBtn(QStringLiteral("⇅"), tr("Flip Vertical"), [this]() { m_canvas->flipVertical(); });
    m_btnRotate = addActionBtn(QStringLiteral("↻"), tr("Rotate 90° Clockwise"), [this]() { m_canvas->rotate90CW(); });

    // Separator
    QFrame *line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    layout->addWidget(line2);

    // Grid & Zoom controls
    m_btnGrid = new QToolButton(this);
    m_btnGrid->setText(QStringLiteral("#"));
    m_btnGrid->setToolTip(tr("Toggle Pixel Grid"));
    m_btnGrid->setCheckable(true);
    m_btnGrid->setChecked(true);
    m_btnGrid->setFixedSize(38, 32);
    connect(m_btnGrid, &QToolButton::toggled, this, [this](bool checked) {
        m_canvas->setShowGrid(checked);
    });
    layout->addWidget(m_btnGrid);

    m_btnZoomIn = addActionBtn(QStringLiteral("+"), tr("Zoom In"), [this]() { m_canvas->zoomIn(); });
    m_btnZoomOut = addActionBtn(QStringLiteral("-"), tr("Zoom Out"), [this]() { m_canvas->zoomOut(); });
    m_btnFit = addActionBtn(QStringLiteral("⊡"), tr("Fit to View"), [this]() { m_canvas->zoomFit(m_scrollArea->viewport()->size()); });

    layout->addStretch();

    // Undo / Redo
    m_btnUndo = addActionBtn(QStringLiteral("↶"), tr("Undo (Ctrl+Z)"), [this]() { m_canvas->undo(); });
    m_btnRedo = addActionBtn(QStringLiteral("↷"), tr("Redo (Ctrl+Y)"), [this]() { m_canvas->redo(); });

    return panel;
}

QWidget* PixelEditorDialog::createPalettePanel()
{
    QWidget *panel = new QWidget(this);
    panel->setFixedWidth(230);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    // 1. Active Color Box (Primary & Secondary swatches)
    m_colorsGroup = new QGroupBox(tr("Active Colors"), panel);
    QHBoxLayout *colorsLayout = new QHBoxLayout(m_colorsGroup);
    colorsLayout->setContentsMargins(6, 6, 6, 6);

    m_primarySwatchBtn = new QPushButton(m_colorsGroup);
    m_primarySwatchBtn->setFixedSize(36, 36);
    m_primarySwatchBtn->setToolTip(tr("Primary Color (Left Click to change)"));
    m_primarySwatchBtn->setStyleSheet(QStringLiteral("background-color: #000000; border: 2px solid #ffffff; border-radius: 4px;"));
    connect(m_primarySwatchBtn, &QPushButton::clicked, this, &PixelEditorDialog::onPrimarySwatchClicked);
    colorsLayout->addWidget(m_primarySwatchBtn);

    m_swapBtn = new QPushButton(QStringLiteral("⇄"), m_colorsGroup);
    m_swapBtn->setFixedSize(28, 28);
    m_swapBtn->setToolTip(tr("Swap Colors (X)"));
    connect(m_swapBtn, &QPushButton::clicked, this, [this]() { m_canvas->swapColors(); });
    colorsLayout->addWidget(m_swapBtn);

    m_secondarySwatchBtn = new QPushButton(m_colorsGroup);
    m_secondarySwatchBtn->setFixedSize(36, 36);
    m_secondarySwatchBtn->setToolTip(tr("Secondary Color (Left Click to change)"));
    m_secondarySwatchBtn->setStyleSheet(QStringLiteral("background-color: #ffffff; border: 2px solid #888888; border-radius: 4px;"));
    connect(m_secondarySwatchBtn, &QPushButton::clicked, this, &PixelEditorDialog::onSecondarySwatchClicked);
    colorsLayout->addWidget(m_secondarySwatchBtn);

    colorsLayout->addStretch();
    layout->addWidget(m_colorsGroup);

    // 2. Palette Preset Selector
    m_palLabel = new QLabel(tr("Palette:"), panel);
    layout->addWidget(m_palLabel);

    m_paletteCombo = new QComboBox(panel);
    m_paletteCombo->addItem(tr("Sprite Colors (Auto)"), SpriteColors);
    m_paletteCombo->addItem(tr("NES / Famicom (54)"), NES);
    m_paletteCombo->addItem(tr("SNES / Super Famicom (32)"), SNES);
    m_paletteCombo->addItem(tr("Amiga OCS (32)"), Amiga);
    m_paletteCombo->addItem(tr("NEC PC-Engine (32)"), PCEngine);
    m_paletteCombo->addItem(tr("Game Boy DMG (4)"), GameBoy);
    m_paletteCombo->addItem(tr("PICO-8 (16)"), Pico8);
    m_paletteCombo->addItem(tr("Commodore 64 (16)"), Commodore64);
    connect(m_paletteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PixelEditorDialog::onPalettePresetChanged);
    layout->addWidget(m_paletteCombo);

    // 3. Swatches Grid inside ScrollArea
    QScrollArea *swatchScroll = new QScrollArea(panel);
    swatchScroll->setWidgetResizable(true);
    swatchScroll->setFixedHeight(210);
    swatchScroll->setStyleSheet(QStringLiteral("background-color: #25262c; border: 1px solid #3e404b; border-radius: 4px;"));

    m_swatchesContainer = new QWidget(swatchScroll);
    m_swatchesLayout = new QGridLayout(m_swatchesContainer);
    m_swatchesLayout->setContentsMargins(4, 4, 4, 4);
    m_swatchesLayout->setSpacing(3);
    swatchScroll->setWidget(m_swatchesContainer);
    layout->addWidget(swatchScroll);

    // 4. Live Preview (1:1 scale)
    m_prevGroup = new QGroupBox(tr("1:1 Scale Preview"), panel);
    QVBoxLayout *prevLayout = new QVBoxLayout(m_prevGroup);
    prevLayout->setContentsMargins(4, 4, 4, 4);

    m_previewLabel = new QLabel(m_prevGroup);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumHeight(80);
    m_previewLabel->setStyleSheet(QStringLiteral("background-color: #1e1e24; border: 1px dashed #4a4a58; border-radius: 4px;"));
    prevLayout->addWidget(m_previewLabel);
    layout->addWidget(m_prevGroup);

    layout->addStretch();
    return panel;
}

QWidget* PixelEditorDialog::createBottomBar()
{
    QWidget *bar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(4, 2, 4, 2);

    m_coordLabel = new QLabel(tr("X: -- , Y: --"), bar);
    m_coordLabel->setFixedWidth(120);
    layout->addWidget(m_coordLabel);

    m_colorInfoLabel = new QLabel(QStringLiteral(""), bar);
    layout->addWidget(m_colorInfoLabel, 1);

    m_zoomLabel = new QLabel(tr("Zoom: 1600%"), bar);
    m_zoomLabel->setFixedWidth(100);
    layout->addWidget(m_zoomLabel);

    m_cancelBtn = new QPushButton(tr("Cancel"), bar);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(m_cancelBtn);

    m_applyBtn = new QPushButton(tr("Apply"), bar);
    connect(m_applyBtn, &QPushButton::clicked, this, &PixelEditorDialog::onApplyClicked);
    layout->addWidget(m_applyBtn);

    m_okBtn = new QPushButton(tr("OK"), bar);
    m_okBtn->setDefault(true);
    connect(m_okBtn, &QPushButton::clicked, this, &PixelEditorDialog::onOkClicked);
    layout->addWidget(m_okBtn);

    return bar;
}

void PixelEditorDialog::onToolButtonClicked(int id)
{
    m_canvas->setCurrentTool(static_cast<PixelTool>(id));
}

void PixelEditorDialog::onPrimarySwatchClicked()
{
    QColor col = QColorDialog::getColor(m_canvas->primaryColor(), this, tr("Select Primary Color"));
    if (col.isValid()) {
        m_canvas->setPrimaryColor(col);
    }
}

void PixelEditorDialog::onSecondarySwatchClicked()
{
    QColor col = QColorDialog::getColor(m_canvas->secondaryColor(), this, tr("Select Secondary Color"));
    if (col.isValid()) {
        m_canvas->setSecondaryColor(col);
    }
}

void PixelEditorDialog::onCanvasImageChanged()
{
    updateLivePreview();
    if (m_paletteCombo && m_paletteCombo->currentIndex() == 0) {
        populateSpriteColorsPalette();
    }
}

void PixelEditorDialog::onCanvasPixelMoved(int x, int y, const QColor &color)
{
    m_coordLabel->setText(tr("X: %1 , Y: %2").arg(x).arg(y));
    if (color.alpha() == 0) {
        m_colorInfoLabel->setText(tr("Transparent [alpha: 0]"));
    } else {
        m_colorInfoLabel->setText(QStringLiteral("%1  RGBA(%2, %3, %4, %5)")
                                  .arg(color.name().toUpper())
                                  .arg(color.red())
                                  .arg(color.green())
                                  .arg(color.blue())
                                  .arg(color.alpha()));
    }
}

void PixelEditorDialog::onCanvasPixelLeft()
{
    m_coordLabel->setText(tr("X: -- , Y: --"));
    m_colorInfoLabel->clear();
}

void PixelEditorDialog::onCanvasZoomChanged(double zoom)
{
    m_zoomLabel->setText(tr("Zoom: %1%").arg(static_cast<int>(std::round(zoom * 100))));
}

void PixelEditorDialog::updateLivePreview()
{
    if (!m_previewLabel || !m_canvas) return;
    QImage img = m_canvas->image();
    if (img.isNull()) {
        m_previewLabel->clear();
        return;
    }

    // Paint against mini checkerboard
    QPixmap px(img.size());
    px.fill(Qt::transparent);
    QPainter p(&px);
    const int ts = 4;
    for (int y = 0; y < img.height(); y += ts) {
        for (int x = 0; x < img.width(); x += ts) {
            bool alt = ((x / ts) + (y / ts)) % 2 == 0;
            p.fillRect(x, y, ts, ts, alt ? QColor(45, 45, 50) : QColor(60, 60, 65));
        }
    }
    p.drawImage(0, 0, img);
    p.end();

    m_previewLabel->setPixmap(px);
}

void PixelEditorDialog::loadFrame(int index)
{
    if (!m_document || index < 0 || index >= m_document->frameCount()) return;
    m_currentFrameIndex = index;

    QImage frameImg;
    if (m_sessionModifiedFrames.contains(index)) {
        frameImg = m_sessionModifiedFrames[index];
    } else {
        frameImg = m_document->frame(index);
    }

    m_canvas->setImage(frameImg);
    m_canvas->zoomFit(m_scrollArea->viewport()->size());

    m_frameInfoLabel->setText(tr("Frame %1 / %2  (%3x%4 px)")
                              .arg(m_currentFrameIndex + 1)
                              .arg(m_document->frameCount())
                              .arg(frameImg.width())
                              .arg(frameImg.height()));

    updateNavigationButtons();
    updateLivePreview();

    if (m_paletteCombo && m_paletteCombo->currentIndex() == 0) {
        populateSpriteColorsPalette();
    }
}

void PixelEditorDialog::saveCurrentFrameToSession()
{
    if (!m_canvas) return;
    m_sessionModifiedFrames[m_currentFrameIndex] = m_canvas->image();
}

void PixelEditorDialog::onPreviousFrame()
{
    if (m_currentFrameIndex > 0) {
        saveCurrentFrameToSession();
        loadFrame(m_currentFrameIndex - 1);
    }
}

void PixelEditorDialog::onNextFrame()
{
    if (m_document && m_currentFrameIndex < m_document->frameCount() - 1) {
        saveCurrentFrameToSession();
        loadFrame(m_currentFrameIndex + 1);
    }
}

void PixelEditorDialog::updateNavigationButtons()
{
    if (!m_document) return;
    m_prevFrameBtn->setEnabled(m_currentFrameIndex > 0);
    m_nextFrameBtn->setEnabled(m_currentFrameIndex < m_document->frameCount() - 1);
}

void PixelEditorDialog::onPalettePresetChanged(int index)
{
    PalettePreset preset = static_cast<PalettePreset>(m_paletteCombo->itemData(index).toInt());
    if (preset == SpriteColors) {
        populateSpriteColorsPalette();
    } else {
        m_currentPalette = getPresetPalette(preset);
        refreshPaletteSwatches();
    }
}

void PixelEditorDialog::populateSpriteColorsPalette()
{
    if (!m_canvas) return;
    QImage img = m_canvas->image();
    QSet<QRgb> uniqueColors;

    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            QRgb rgb = img.pixel(x, y);
            if (qAlpha(rgb) > 0) {
                uniqueColors.insert(qRgb(qRed(rgb), qGreen(rgb), qBlue(rgb)));
            }
        }
    }

    m_currentPalette = uniqueColors.values().toVector();
    std::sort(m_currentPalette.begin(), m_currentPalette.end(), [](QRgb a, QRgb b) {
        return qGray(a) < qGray(b);
    });

    refreshPaletteSwatches();
}

void PixelEditorDialog::refreshPaletteSwatches()
{
    // Clear existing swatches
    QLayoutItem *child;
    while ((child = m_swatchesLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    const int columns = 6;
    for (int i = 0; i < m_currentPalette.size(); ++i) {
        QRgb rgb = m_currentPalette[i];
        QColor col(rgb);

        QPushButton *btn = new QPushButton(m_swatchesContainer);
        btn->setFixedSize(24, 24);
        btn->setToolTip(QStringLiteral("%1\nRGB(%2, %3, %4)\nLeft-Click: Primary\nRight-Click: Secondary")
                        .arg(col.name().toUpper())
                        .arg(col.red()).arg(col.green()).arg(col.blue()));
        btn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #1a1a20; border-radius: 2px;")
                           .arg(col.name()));

        // Left click sets primary color
        connect(btn, &QPushButton::clicked, this, [this, col]() {
            m_canvas->setPrimaryColor(col);
        });

        // Context menu or right-click handler for secondary color
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QPushButton::customContextMenuRequested, this, [this, col]() {
            m_canvas->setSecondaryColor(col);
        });

        int row = i / columns;
        int colIdx = i % columns;
        m_swatchesLayout->addWidget(btn, row, colIdx);
    }
}

QVector<QRgb> PixelEditorDialog::getPresetPalette(PalettePreset preset)
{
    QVector<QRgb> pal;
    switch (preset) {
    case NES:
        pal = {
            qRgb(124,124,124), qRgb(0,0,252),     qRgb(0,0,188),     qRgb(68,40,188),
            qRgb(148,0,132),   qRgb(168,0,32),     qRgb(168,16,0),    qRgb(136,20,0),
            qRgb(80,48,0),     qRgb(0,120,0),      qRgb(0,104,0),     qRgb(0,88,0),
            qRgb(0,64,88),     qRgb(0,0,0),        qRgb(188,188,188), qRgb(0,120,248),
            qRgb(0,88,248),    qRgb(104,68,252),   qRgb(216,0,204),   qRgb(228,0,88),
            qRgb(248,56,0),    qRgb(228,92,16),    qRgb(172,124,0),   qRgb(0,184,0),
            qRgb(0,168,0),     qRgb(0,168,68),     qRgb(0,136,136),   qRgb(248,248,248),
            qRgb(60,188,252),  qRgb(104,136,252),  qRgb(152,120,248), qRgb(248,120,248),
            qRgb(248,88,152),  qRgb(248,120,88),   qRgb(252,160,68),  qRgb(248,184,0),
            qRgb(184,248,24),  qRgb(88,216,84),    qRgb(88,248,152),  qRgb(0,232,216),
            qRgb(120,120,120), qRgb(252,252,252), qRgb(164,228,252), qRgb(184,184,248),
            qRgb(216,184,248), qRgb(248,184,248), qRgb(248,164,192), qRgb(240,208,176),
            qRgb(252,224,168), qRgb(248,216,120), qRgb(216,248,120), qRgb(184,248,184),
            qRgb(184,248,216), qRgb(0,252,252)
        };
        break;

    case SNES:
        pal = {
            qRgb(0, 0, 0),       qRgb(248, 248, 248), qRgb(184, 184, 184), qRgb(104, 104, 104),
            qRgb(248, 56, 0),    qRgb(216, 0, 0),     qRgb(152, 0, 0),     qRgb(248, 120, 88),
            qRgb(248, 160, 0),   qRgb(248, 224, 0),   qRgb(184, 152, 0),   qRgb(104, 72, 0),
            qRgb(0, 216, 0),     qRgb(0, 144, 0),     qRgb(0, 80, 0),      qRgb(120, 248, 88),
            qRgb(0, 184, 216),   qRgb(0, 104, 184),   qRgb(0, 48, 120),    qRgb(120, 216, 248),
            qRgb(88, 88, 248),   qRgb(40, 40, 184),   qRgb(16, 16, 104),   qRgb(160, 160, 248),
            qRgb(216, 0, 184),   qRgb(144, 0, 120),   qRgb(248, 120, 216), qRgb(248, 184, 152),
            qRgb(216, 136, 88),  qRgb(160, 88, 48),   qRgb(96, 48, 16),    qRgb(48, 48, 48)
        };
        break;

    case Amiga:
        pal = {
            qRgb(0, 85, 170),   qRgb(255, 255, 255), qRgb(0, 0, 0),       qRgb(255, 136, 0),
            qRgb(0, 0, 170),    qRgb(0, 170, 0),     qRgb(0, 170, 170),   qRgb(170, 0, 0),
            qRgb(170, 0, 170),  qRgb(170, 85, 0),    qRgb(170, 170, 170), qRgb(85, 85, 85),
            qRgb(85, 85, 255),  qRgb(85, 255, 85),   qRgb(85, 255, 255),  qRgb(255, 85, 85),
            qRgb(255, 85, 255), qRgb(255, 255, 85), qRgb(238, 68, 68),  qRgb(68, 170, 238),
            qRgb(34, 102, 34),  qRgb(204, 170, 119), qRgb(136, 102, 68), qRgb(68, 51, 34),
            qRgb(221, 221, 221),qRgb(187, 187, 187),qRgb(153, 153, 153),qRgb(102, 102, 102),
            qRgb(51, 51, 51),   qRgb(255, 204, 153), qRgb(204, 119, 85), qRgb(119, 34, 34)
        };
        break;

    case PCEngine:
        pal = {
            qRgb(0, 0, 0),       qRgb(255, 255, 255), qRgb(182, 182, 182), qRgb(109, 109, 109),
            qRgb(255, 36, 36),   qRgb(218, 0, 0),     qRgb(145, 0, 0),     qRgb(255, 145, 145),
            qRgb(255, 109, 0),   qRgb(255, 182, 0),   qRgb(255, 255, 0),   qRgb(182, 145, 0),
            qRgb(36, 218, 36),   qRgb(0, 182, 0),     qRgb(0, 109, 0),     qRgb(145, 255, 145),
            qRgb(36, 218, 255),  qRgb(0, 145, 218),   qRgb(0, 72, 182),    qRgb(145, 218, 255),
            qRgb(72, 72, 255),   qRgb(36, 36, 182),   qRgb(0, 0, 145),     qRgb(182, 182, 255),
            qRgb(218, 36, 218),  qRgb(145, 0, 145),   qRgb(255, 145, 255), qRgb(255, 182, 145),
            qRgb(218, 145, 72),  qRgb(145, 72, 0),    qRgb(109, 36, 0),    qRgb(36, 36, 36)
        };
        break;

    case GameBoy:
        pal = {
            qRgb(15, 56, 15),
            qRgb(48, 98, 48),
            qRgb(139, 172, 15),
            qRgb(155, 188, 15)
        };
        break;

    case Pico8:
        pal = {
            qRgb(0, 0, 0),       qRgb(29, 43, 83),    qRgb(126, 37, 83),  qRgb(0, 135, 81),
            qRgb(171, 82, 54),   qRgb(95, 87, 79),    qRgb(194, 195, 199),qRgb(255, 241, 232),
            qRgb(255, 0, 77),    qRgb(255, 163, 0),   qRgb(255, 236, 39), qRgb(0, 228, 54),
            qRgb(41, 173, 255),  qRgb(131, 118, 156), qRgb(255, 119, 168),qRgb(255, 204, 170)
        };
        break;

    case Commodore64:
        pal = {
            qRgb(0, 0, 0),       qRgb(255, 255, 255), qRgb(136, 0, 0),    qRgb(170, 255, 238),
            qRgb(204, 68, 204),  qRgb(0, 204, 85),    qRgb(0, 0, 170),    qRgb(238, 238, 119),
            qRgb(221, 136, 85),  qRgb(102, 68, 0),    qRgb(255, 119, 119),qRgb(51, 51, 51),
            qRgb(119, 119, 119), qRgb(170, 255, 102), qRgb(0, 136, 255),  qRgb(187, 187, 187)
        };
        break;

    default:
        break;
    }
    return pal;
}

void PixelEditorDialog::onApplyClicked()
{
    saveCurrentFrameToSession();
    if (m_sessionModifiedFrames.isEmpty() || !m_document) return;

    // Filter out unmodified frames
    QMap<int, QImage> actualChanges;
    for (auto it = m_sessionModifiedFrames.constBegin(); it != m_sessionModifiedFrames.constEnd(); ++it) {
        if (m_document->frame(it.key()) != it.value()) {
            actualChanges[it.key()] = it.value();
        }
    }

    if (!actualChanges.isEmpty()) {
        if (m_docUndoStack) {
            m_docUndoStack->push(new EditSpritePixelsCommand(m_document, actualChanges));
        } else {
            EditSpritePixelsCommand cmd(m_document, actualChanges);
            cmd.redo();
        }
    }

    m_sessionModifiedFrames.clear();
}

void PixelEditorDialog::onOkClicked()
{
    onApplyClicked();
    accept();
}

void PixelEditorDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void PixelEditorDialog::retranslateUi()
{
    setWindowTitle(tr("Pixel Editor — SpriteStudio"));

    if (m_prevFrameBtn) {
        m_prevFrameBtn->setText(tr("◀ Previous Frame"));
        m_prevFrameBtn->setToolTip(tr("Navigate to previous frame (Page Up)"));
    }
    if (m_nextFrameBtn) {
        m_nextFrameBtn->setText(tr("Next Frame ▶"));
        m_nextFrameBtn->setToolTip(tr("Navigate to next frame (Page Down)"));
    }

    if (m_document && m_canvas && m_frameInfoLabel) {
        m_frameInfoLabel->setText(tr("Frame %1 / %2  (%3x%4 px)")
                                  .arg(m_currentFrameIndex + 1)
                                  .arg(m_document->frameCount())
                                  .arg(m_canvas->image().width())
                                  .arg(m_canvas->image().height()));
    }

    // Tools
    if (m_btnPencil) m_btnPencil->setToolTip(tr("Pencil (1px continuous Bresenham) [P]"));
    if (m_btnEraser) m_btnEraser->setToolTip(tr("Eraser (1px clear to alpha 0) [E]"));
    if (m_btnEyedropper) m_btnEyedropper->setToolTip(tr("Eyedropper / Pipette (Alt+Click or [I])"));
    if (m_btnBucket) m_btnBucket->setToolTip(tr("Bucket Fill (Flood Fill 4-way) [G]"));
    if (m_btnSelectRect) m_btnSelectRect->setToolTip(tr("Rectangular Marquee Selection [M]"));
    if (m_btnSelectColor) m_btnSelectColor->setToolTip(tr("Magic Wand (Color Selection) [W]"));

    if (m_btnFlipH) m_btnFlipH->setToolTip(tr("Flip Horizontal"));
    if (m_btnFlipV) m_btnFlipV->setToolTip(tr("Flip Vertical"));
    if (m_btnRotate) m_btnRotate->setToolTip(tr("Rotate 90° Clockwise"));
    if (m_btnGrid) m_btnGrid->setToolTip(tr("Toggle Pixel Grid"));
    if (m_btnZoomIn) m_btnZoomIn->setToolTip(tr("Zoom In"));
    if (m_btnZoomOut) m_btnZoomOut->setToolTip(tr("Zoom Out"));
    if (m_btnFit) m_btnFit->setToolTip(tr("Fit to View"));
    if (m_btnUndo) m_btnUndo->setToolTip(tr("Undo (Ctrl+Z)"));
    if (m_btnRedo) m_btnRedo->setToolTip(tr("Redo (Ctrl+Y)"));

    // Palette & Colors
    if (m_colorsGroup) m_colorsGroup->setTitle(tr("Active Colors"));
    if (m_primarySwatchBtn) m_primarySwatchBtn->setToolTip(tr("Primary Color (Left Click to change)"));
    if (m_secondarySwatchBtn) m_secondarySwatchBtn->setToolTip(tr("Secondary Color (Left Click to change)"));
    if (m_swapBtn) m_swapBtn->setToolTip(tr("Swap Colors (X)"));
    if (m_palLabel) m_palLabel->setText(tr("Palette:"));

    if (m_paletteCombo) {
        int curIdx = m_paletteCombo->currentIndex();
        m_paletteCombo->blockSignals(true);
        m_paletteCombo->setItemText(SpriteColors, tr("Sprite Colors (Auto)"));
        m_paletteCombo->setItemText(NES, tr("NES / Famicom (54)"));
        m_paletteCombo->setItemText(SNES, tr("SNES / Super Famicom (32)"));
        m_paletteCombo->setItemText(Amiga, tr("Amiga OCS (32)"));
        m_paletteCombo->setItemText(PCEngine, tr("NEC PC-Engine (32)"));
        m_paletteCombo->setItemText(GameBoy, tr("Game Boy DMG (4)"));
        m_paletteCombo->setItemText(Pico8, tr("PICO-8 (16)"));
        m_paletteCombo->setItemText(Commodore64, tr("Commodore 64 (16)"));
        m_paletteCombo->setCurrentIndex(curIdx);
        m_paletteCombo->blockSignals(false);
    }

    if (m_prevGroup) m_prevGroup->setTitle(tr("1:1 Scale Preview"));

    if (m_zoomLabel && m_canvas) {
        m_zoomLabel->setText(tr("Zoom: %1%").arg(static_cast<int>(std::round(m_canvas->zoom() * 100))));
    }

    // Bottom bar buttons
    if (m_cancelBtn) m_cancelBtn->setText(tr("Cancel"));
    if (m_applyBtn) m_applyBtn->setText(tr("Apply"));
    if (m_okBtn) m_okBtn->setText(tr("OK"));

    updateNavigationButtons();
}

