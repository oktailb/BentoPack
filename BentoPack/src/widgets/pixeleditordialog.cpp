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
#include <QSpacerItem>
#include <QtConcurrent/QtConcurrent>
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
    setWindowTitle(tr("Pixel Editor — BentoPack"));
    resize(1000, 680);
    setMinimumSize(800, 500);

    m_recentColors = {
        QColor(0, 0, 0),
        QColor(255, 255, 255),
        QColor(239, 68, 68),
        QColor(249, 115, 22),
        QColor(234, 179, 8),
        QColor(34, 197, 94),
        QColor(59, 130, 246),
        QColor(168, 85, 247)
    };

    setupUi();

    if (m_document && m_document->frameCount() > 0) {
        if (m_currentFrameIndex < 0 || m_currentFrameIndex >= m_document->frameCount()) {
            m_currentFrameIndex = 0;
        }
        loadFrame(m_currentFrameIndex);
    }

    // Default palette is Bento Standard (36)
    onPalettePresetChanged(0);
}

void PixelEditorDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // 1. Header Bar (Navigation & Canvas Controls)
    mainLayout->addWidget(createHeaderBar());

    // 2. Central Area (Toolbar + Canvas + Palette Panel)
    QHBoxLayout *centerLayout = new QHBoxLayout();
    centerLayout->setSpacing(6);

    // Left Toolbar (2-column ergonomic palette)
    centerLayout->addWidget(createToolBar());

    // Canvas inside ScrollArea
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background-color: #14151a; border: 1px solid #2d2e38; border-radius: 6px; }"));

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
            m_primarySwatchBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 2px solid #374151; border-radius: 6px;").arg(col.name()));
        }
        if (m_primaryHexLabel) {
            m_primaryHexLabel->setText(col.name().toUpper());
        }
        if (m_rgbLabel) {
            m_rgbLabel->setText(QStringLiteral("RGB(%1,%2,%3)").arg(col.red()).arg(col.green()).arg(col.blue()));
        }
        addRecentColor(col);
    });
    connect(m_canvas, &PixelCanvas::secondaryColorChanged, this, [this](const QColor &col) {
        if (m_secondarySwatchBtn) {
            m_secondarySwatchBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 2px solid #9ca3af; border-radius: 6px;").arg(col.name()));
        }
    });

    // Ergonomic keyboard shortcuts
    auto addShortcut = [this](const QKeySequence &seq, const auto &slot) {
        QShortcut *sc = new QShortcut(seq, this);
        connect(sc, &QShortcut::activated, this, slot);
    };

    // Tool switching
    addShortcut(QKeySequence(Qt::Key_P), [this]() { m_btnPencil->click(); });
    addShortcut(QKeySequence(Qt::Key_E), [this]() { m_btnEraser->click(); });
    addShortcut(QKeySequence(Qt::Key_I), [this]() { m_btnEyedropper->click(); });
    addShortcut(QKeySequence(Qt::Key_G), [this]() { m_btnBucket->click(); });
    addShortcut(QKeySequence(Qt::Key_M), [this]() { m_btnSelectRect->click(); });
    addShortcut(QKeySequence(Qt::Key_W), [this]() { m_btnSelectColor->click(); });

    // Transforms & Actions
    addShortcut(QKeySequence(Qt::Key_X), [this]() { m_canvas->swapColors(); });
    addShortcut(QKeySequence(Qt::Key_H), [this]() { m_canvas->flipHorizontal(); });
    addShortcut(QKeySequence(Qt::Key_V), [this]() { m_canvas->flipVertical(); });
    addShortcut(QKeySequence(Qt::Key_R), [this]() { m_canvas->rotate90CW(); });
    addShortcut(QKeySequence(Qt::Key_Delete), [this]() { m_canvas->clearSelection(); });
    addShortcut(QKeySequence(Qt::Key_Backspace), [this]() { m_canvas->clearSelection(); });
    addShortcut(QKeySequence(Qt::Key_Escape), [this]() { m_canvas->deselect(); });

    // Zoom & Grid
    addShortcut(QKeySequence(Qt::Key_Plus), [this]() { m_canvas->zoomIn(); });
    addShortcut(QKeySequence(Qt::Key_Equal), [this]() { m_canvas->zoomIn(); });
    addShortcut(QKeySequence(Qt::Key_Minus), [this]() { m_canvas->zoomOut(); });
    addShortcut(QKeySequence(Qt::Key_0), [this]() { m_canvas->zoomFit(m_scrollArea->viewport()->size()); });
    addShortcut(QKeySequence(Qt::Key_F), [this]() { m_canvas->zoomFit(m_scrollArea->viewport()->size()); });

    // Frame navigation
    addShortcut(QKeySequence(Qt::Key_PageUp), [this]() { onPreviousFrame(); });
    addShortcut(QKeySequence(Qt::Key_BracketLeft), [this]() { onPreviousFrame(); });
    addShortcut(QKeySequence(Qt::Key_PageDown), [this]() { onNextFrame(); });
    addShortcut(QKeySequence(Qt::Key_BracketRight), [this]() { onNextFrame(); });

    // Undo / Redo
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z), [this]() { m_canvas->undo(); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y), [this]() { m_canvas->redo(); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), [this]() { m_canvas->redo(); });
}

QWidget* PixelEditorDialog::createHeaderBar()
{
    QWidget *bar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    const QString navBtnStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: #ffffff;"
        "  border: 1px solid #d1d5db;"
        "  border-radius: 6px;"
        "  color: #1f2937;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover { background-color: #f3f4f6; border-color: #9ca3af; }"
        "QPushButton:pressed { background-color: #e5e7eb; }"
        "QPushButton:disabled { color: #9ca3af; background-color: #f9fafb; border-color: #e5e7eb; }"
    );

    m_prevFrameBtn = new QPushButton(QStringLiteral("◀"), bar);
    m_prevFrameBtn->setFixedSize(34, 28);
    m_prevFrameBtn->setStyleSheet(navBtnStyle);
    m_prevFrameBtn->setToolTip(tr("Navigate to previous frame (Page Up)"));
    connect(m_prevFrameBtn, &QPushButton::clicked, this, &PixelEditorDialog::onPreviousFrame);
    layout->addWidget(m_prevFrameBtn);

    m_frameInfoLabel = new QLabel(tr("Frame 1 / 1 (32x32 px)"), bar);
    m_frameInfoLabel->setAlignment(Qt::AlignCenter);
    m_frameInfoLabel->setStyleSheet(QStringLiteral(
        "background-color: #ffffff;"
        "border: 1px solid #d1d5db;"
        "border-radius: 6px;"
        "padding: 4px 14px;"
        "font-weight: bold;"
        "font-size: 12px;"
        "color: #1f2937;"
    ));
    layout->addWidget(m_frameInfoLabel);

    m_nextFrameBtn = new QPushButton(QStringLiteral("▶"), bar);
    m_nextFrameBtn->setFixedSize(34, 28);
    m_nextFrameBtn->setStyleSheet(navBtnStyle);
    m_nextFrameBtn->setToolTip(tr("Navigate to next frame (Page Down)"));
    connect(m_nextFrameBtn, &QPushButton::clicked, this, &PixelEditorDialog::onNextFrame);
    layout->addWidget(m_nextFrameBtn);

    layout->addStretch(1);

    // View & Zoom controls
    const QString viewBtnStyle = QStringLiteral(
        "QToolButton {"
        "  background-color: #ffffff;"
        "  border: 1px solid #d1d5db;"
        "  border-radius: 6px;"
        "  color: #1f2937;"
        "  font-weight: 500;"
        "  font-size: 12px;"
        "}"
        "QToolButton:hover { background-color: #f3f4f6; border-color: #9ca3af; }"
        "QToolButton:checked { background-color: #eff6ff; border: 2px solid #2563eb; color: #2563eb; font-weight: bold; }"
        "QToolButton:pressed { background-color: #e5e7eb; }"
    );

    m_btnGrid = new QToolButton(bar);
    m_btnGrid->setText(QStringLiteral("# Grid"));
    m_btnGrid->setToolTip(tr("Toggle Pixel Grid"));
    m_btnGrid->setCheckable(true);
    m_btnGrid->setChecked(true);
    m_btnGrid->setFixedSize(68, 28);
    m_btnGrid->setStyleSheet(viewBtnStyle);
    connect(m_btnGrid, &QToolButton::toggled, this, [this](bool checked) {
        m_canvas->setShowGrid(checked);
    });
    layout->addWidget(m_btnGrid);

    QFrame *sep = new QFrame(bar);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet(QStringLiteral("color: #d1d5db;"));
    layout->addWidget(sep);

    m_btnZoomOut = new QToolButton(bar);
    m_btnZoomOut->setText(QStringLiteral("−"));
    m_btnZoomOut->setToolTip(tr("Zoom Out"));
    m_btnZoomOut->setFixedSize(28, 28);
    m_btnZoomOut->setStyleSheet(viewBtnStyle);
    connect(m_btnZoomOut, &QToolButton::clicked, this, [this]() { m_canvas->zoomOut(); });
    layout->addWidget(m_btnZoomOut);

    m_zoomLabel = new QLabel(tr("Zoom: 1600%"), bar);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setFixedWidth(92);
    m_zoomLabel->setStyleSheet(QStringLiteral(
        "background-color: #ffffff;"
        "border: 1px solid #d1d5db;"
        "border-radius: 6px;"
        "padding: 4px 6px;"
        "font-weight: bold;"
        "font-size: 11px;"
        "color: #0284c7;"
    ));
    layout->addWidget(m_zoomLabel);

    m_btnZoomIn = new QToolButton(bar);
    m_btnZoomIn->setText(QStringLiteral("+"));
    m_btnZoomIn->setToolTip(tr("Zoom In"));
    m_btnZoomIn->setFixedSize(28, 28);
    m_btnZoomIn->setStyleSheet(viewBtnStyle);
    connect(m_btnZoomIn, &QToolButton::clicked, this, [this]() { m_canvas->zoomIn(); });
    layout->addWidget(m_btnZoomIn);

    m_btnFit = new QToolButton(bar);
    m_btnFit->setText(QStringLiteral("⊡ Fit"));
    m_btnFit->setToolTip(tr("Fit to View"));
    m_btnFit->setFixedSize(56, 28);
    m_btnFit->setStyleSheet(viewBtnStyle);
    connect(m_btnFit, &QToolButton::clicked, this, [this]() { m_canvas->zoomFit(m_scrollArea->viewport()->size()); });
    layout->addWidget(m_btnFit);

    return bar;
}

QWidget* PixelEditorDialog::createToolBar()
{
    QWidget *panel = new QWidget(this);
    panel->setFixedWidth(96);
    QGridLayout *layout = new QGridLayout(panel);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(true);

    const QString toolBtnStyle = QStringLiteral(
        "QToolButton {"
        "  background-color: #ffffff;"
        "  border: 1px solid #d1d5db;"
        "  border-radius: 6px;"
        "  color: #1f2937;"
        "  font-size: 15px;"
        "  font-weight: bold;"
        "}"
        "QToolButton:hover {"
        "  background-color: #f3f4f6;"
        "  border-color: #9ca3af;"
        "}"
        "QToolButton:checked {"
        "  background-color: #eff6ff;"
        "  border: 2px solid #2563eb;"
        "  color: #2563eb;"
        "}"
        "QToolButton:pressed {"
        "  background-color: #e5e7eb;"
        "}"
        "QToolButton:disabled {"
        "  background-color: #f9fafb;"
        "  border-color: #e5e7eb;"
        "  color: #9ca3af;"
        "}"
    );

    auto addToolBtn = [this, layout, &toolBtnStyle](const QString &text, const QString &tooltip, PixelTool /*tool*/, int id, int row, int col, bool checked = false) -> QToolButton* {
        QToolButton *btn = new QToolButton(this);
        btn->setText(text);
        btn->setToolTip(tooltip);
        btn->setCheckable(true);
        btn->setChecked(checked);
        btn->setFixedSize(42, 38);
        btn->setStyleSheet(toolBtnStyle);
        m_toolGroup->addButton(btn, id);
        layout->addWidget(btn, row, col);
        return btn;
    };

    m_btnPencil = addToolBtn(QStringLiteral("✏"), tr("Pencil (1px continuous Bresenham) [P]"), PixelTool::Pencil, static_cast<int>(PixelTool::Pencil), 0, 0, true);
    m_btnEraser = addToolBtn(QStringLiteral("🧹"), tr("Eraser (1px clear to alpha 0) [E]"), PixelTool::Eraser, static_cast<int>(PixelTool::Eraser), 0, 1);
    m_btnEyedropper = addToolBtn(QStringLiteral("💧"), tr("Eyedropper / Pipette (Alt+Click or [I])"), PixelTool::Eyedropper, static_cast<int>(PixelTool::Eyedropper), 1, 0);
    m_btnBucket = addToolBtn(QStringLiteral("🪣"), tr("Bucket Fill (Flood Fill 4-way) [G]"), PixelTool::BucketFill, static_cast<int>(PixelTool::BucketFill), 1, 1);
    m_btnSelectRect = addToolBtn(QStringLiteral("⬚"), tr("Rectangular Marquee Selection [M]"), PixelTool::SelectRect, static_cast<int>(PixelTool::SelectRect), 2, 0);
    m_btnSelectColor = addToolBtn(QStringLiteral("🪄"), tr("Magic Wand (Color Selection) [W]"), PixelTool::SelectColor, static_cast<int>(PixelTool::SelectColor), 2, 1);

    connect(m_toolGroup, &QButtonGroup::idClicked, this, &PixelEditorDialog::onToolButtonClicked);

    // Separator 1
    QFrame *line1 = new QFrame(this);
    line1->setFrameShape(QFrame::HLine);
    line1->setStyleSheet(QStringLiteral("color: #d1d5db;"));
    layout->addWidget(line1, 3, 0, 1, 2);

    auto addActionBtn = [this, layout, &toolBtnStyle](const QString &text, const QString &tooltip, int row, int col, const auto &slot) -> QToolButton* {
        QToolButton *btn = new QToolButton(this);
        btn->setText(text);
        btn->setToolTip(tooltip);
        btn->setFixedSize(42, 34);
        btn->setStyleSheet(toolBtnStyle);
        connect(btn, &QToolButton::clicked, this, slot);
        layout->addWidget(btn, row, col);
        return btn;
    };

    m_btnFlipH = addActionBtn(QStringLiteral("⇄"), tr("Flip Horizontal"), 4, 0, [this]() { m_canvas->flipHorizontal(); });
    m_btnFlipV = addActionBtn(QStringLiteral("⇅"), tr("Flip Vertical"), 4, 1, [this]() { m_canvas->flipVertical(); });
    m_btnRotate = addActionBtn(QStringLiteral("↻"), tr("Rotate 90° Clockwise"), 5, 0, [this]() { m_canvas->rotate90CW(); });
    m_btnClearSel = addActionBtn(QStringLiteral("✕"), tr("Clear Selection / Deselect (Del)"), 5, 1, [this]() {
        if (m_canvas->hasSelection()) {
            m_canvas->clearSelection();
        } else {
            m_canvas->deselect();
        }
    });

    // Separator 2
    QFrame *line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet(QStringLiteral("color: #d1d5db;"));
    layout->addWidget(line2, 6, 0, 1, 2);

    // Undo / Redo
    m_btnUndo = addActionBtn(QStringLiteral("↶"), tr("Undo (Ctrl+Z)"), 7, 0, [this]() { m_canvas->undo(); });
    m_btnRedo = addActionBtn(QStringLiteral("↷"), tr("Redo (Ctrl+Y)"), 7, 1, [this]() { m_canvas->redo(); });

    // Spacer
    layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding), 8, 0, 1, 2);

    return panel;
}

QWidget* PixelEditorDialog::createPalettePanel()
{
    QWidget *panel = new QWidget(this);
    panel->setFixedWidth(244);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(8);

    // 1. Active Color Box & Direct Color Picker
    m_colorsGroup = new QGroupBox(tr("Color Picker"), panel);
    m_colorsGroup->setStyleSheet(QStringLiteral(
        "QGroupBox {"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "  color: #374151;"
        "  border: 1px solid #d1d5db;"
        "  border-radius: 6px;"
        "  margin-top: 8px;"
        "  padding-top: 10px;"
        "  background-color: #ffffff;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 8px;"
        "  padding: 0 4px;"
        "}"
    ));
    QVBoxLayout *colorsMainLayout = new QVBoxLayout(m_colorsGroup);
    colorsMainLayout->setContentsMargins(8, 8, 8, 8);
    colorsMainLayout->setSpacing(6);

    QHBoxLayout *swatchesRow = new QHBoxLayout();
    swatchesRow->setSpacing(8);

    m_primarySwatchBtn = new QPushButton(m_colorsGroup);
    m_primarySwatchBtn->setFixedSize(44, 44);
    m_primarySwatchBtn->setToolTip(tr("Primary Color (Click to open Color Picker)"));
    m_primarySwatchBtn->setStyleSheet(QStringLiteral("background-color: #000000; border: 2px solid #374151; border-radius: 6px;"));
    connect(m_primarySwatchBtn, &QPushButton::clicked, this, &PixelEditorDialog::onPrimarySwatchClicked);
    swatchesRow->addWidget(m_primarySwatchBtn);

    m_swapBtn = new QPushButton(QStringLiteral("⇄"), m_colorsGroup);
    m_swapBtn->setFixedSize(28, 28);
    m_swapBtn->setToolTip(tr("Swap Colors (X)"));
    m_swapBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #f3f4f6;"
        "  border: 1px solid #d1d5db;"
        "  border-radius: 14px;"
        "  color: #374151;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #e5e7eb; border-color: #2563eb; color: #2563eb; }"
    ));
    connect(m_swapBtn, &QPushButton::clicked, this, [this]() { m_canvas->swapColors(); });
    swatchesRow->addWidget(m_swapBtn);

    m_secondarySwatchBtn = new QPushButton(m_colorsGroup);
    m_secondarySwatchBtn->setFixedSize(40, 40);
    m_secondarySwatchBtn->setToolTip(tr("Secondary Color (Click to open Color Picker)"));
    m_secondarySwatchBtn->setStyleSheet(QStringLiteral("background-color: #ffffff; border: 2px solid #9ca3af; border-radius: 6px;"));
    connect(m_secondarySwatchBtn, &QPushButton::clicked, this, &PixelEditorDialog::onSecondarySwatchClicked);
    swatchesRow->addWidget(m_secondarySwatchBtn);

    swatchesRow->addStretch();
    colorsMainLayout->addLayout(swatchesRow);

    QHBoxLayout *hexRow = new QHBoxLayout();
    m_primaryHexLabel = new QLabel(QStringLiteral("#000000"), m_colorsGroup);
    m_primaryHexLabel->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px; font-weight: bold; color: #1f2937;"));
    hexRow->addWidget(m_primaryHexLabel);

    m_rgbLabel = new QLabel(QStringLiteral("RGB(0, 0, 0)"), m_colorsGroup);
    m_rgbLabel->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 10px; color: #6b7280;"));
    hexRow->addWidget(m_rgbLabel);
    hexRow->addStretch();
    colorsMainLayout->addLayout(hexRow);

    // Direct color picker launcher button
    m_btnPickColor = new QPushButton(tr("🎨 Pick Color..."), m_colorsGroup);
    m_btnPickColor->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #f9fafb;"
        "  border: 1px solid #d1d5db;"
        "  border-radius: 6px;"
        "  color: #1f2937;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  padding: 6px 10px;"
        "}"
        "QPushButton:hover { background-color: #eff6ff; border-color: #2563eb; color: #2563eb; }"
        "QPushButton:pressed { background-color: #dbeafe; }"
    ));
    connect(m_btnPickColor, &QPushButton::clicked, this, &PixelEditorDialog::onPickColorClicked);
    colorsMainLayout->addWidget(m_btnPickColor);

    // Recent colors
    m_recentLabel = new QLabel(tr("Recent:"), m_colorsGroup);
    m_recentLabel->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; color: #6b7280; margin-top: 4px;"));
    colorsMainLayout->addWidget(m_recentLabel);

    m_recentContainer = new QWidget(m_colorsGroup);
    m_recentLayout = new QHBoxLayout(m_recentContainer);
    m_recentLayout->setContentsMargins(0, 0, 0, 0);
    m_recentLayout->setSpacing(4);
    refreshRecentSwatches();
    colorsMainLayout->addWidget(m_recentContainer);

    layout->addWidget(m_colorsGroup);

    // 2. Palette Presets & Sample Frame button
    QHBoxLayout *presetHeader = new QHBoxLayout();
    m_palLabel = new QLabel(tr("Preset:"), panel);
    m_palLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px; color: #374151;"));
    presetHeader->addWidget(m_palLabel);

    m_btnSampleFrame = new QPushButton(tr("Sample Frame"), panel);
    m_btnSampleFrame->setToolTip(tr("Extract all unique colors from current sprite frame"));
    m_btnSampleFrame->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #ffffff;"
        "  border: 1px solid #d1d5db;"
        "  border-radius: 4px;"
        "  color: #374151;"
        "  font-size: 10px;"
        "  font-weight: 500;"
        "  padding: 2px 6px;"
        "}"
        "QPushButton:hover { background-color: #f3f4f6; border-color: #2563eb; color: #2563eb; }"
    ));
    connect(m_btnSampleFrame, &QPushButton::clicked, this, &PixelEditorDialog::onSampleFrameColorsClicked);
    presetHeader->addWidget(m_btnSampleFrame);
    layout->addLayout(presetHeader);

    m_paletteCombo = new QComboBox(panel);
    m_paletteCombo->setStyleSheet(QStringLiteral(
        "QComboBox {"
        "  background-color: #ffffff;"
        "  border: 1px solid #d1d5db;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  color: #1f2937;"
        "  font-size: 12px;"
        "  min-height: 24px;"
        "}"
        "QComboBox:hover { border-color: #9ca3af; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #ffffff; color: #1f2937; selection-background-color: #eff6ff; selection-color: #2563eb; }"
    ));
    m_paletteCombo->addItem(tr("Bento Standard (36)"), Standard);
    m_paletteCombo->addItem(tr("NES / Famicom (54)"), NES);
    m_paletteCombo->addItem(tr("SNES / 16-bit (32)"), SNES);
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
    swatchScroll->setFixedHeight(170);
    swatchScroll->setStyleSheet(QStringLiteral("background-color: #ffffff; border: 1px solid #d1d5db; border-radius: 6px;"));

    m_swatchesContainer = new QWidget(swatchScroll);
    m_swatchesLayout = new QGridLayout(m_swatchesContainer);
    m_swatchesLayout->setContentsMargins(4, 4, 4, 4);
    m_swatchesLayout->setSpacing(4);
    swatchScroll->setWidget(m_swatchesContainer);
    layout->addWidget(swatchScroll);

    // 4. Live Preview (1:1 scale)
    m_prevGroup = new QGroupBox(tr("1:1 Scale Preview"), panel);
    m_prevGroup->setStyleSheet(QStringLiteral(
        "QGroupBox {"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "  color: #374151;"
        "  border: 1px solid #d1d5db;"
        "  border-radius: 6px;"
        "  margin-top: 8px;"
        "  padding-top: 10px;"
        "  background-color: #ffffff;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 8px;"
        "  padding: 0 4px;"
        "}"
    ));
    QVBoxLayout *prevLayout = new QVBoxLayout(m_prevGroup);
    prevLayout->setContentsMargins(4, 4, 4, 4);

    m_previewLabel = new QLabel(m_prevGroup);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumHeight(76);
    m_previewLabel->setStyleSheet(QStringLiteral("background-color: #1e1e24; border: 1px solid #cbd5e1; border-radius: 4px;"));
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
    layout->setSpacing(8);

    m_coordLabel = new QLabel(tr("X: -- , Y: --"), bar);
    m_coordLabel->setFixedWidth(100);
    m_coordLabel->setAlignment(Qt::AlignCenter);
    m_coordLabel->setStyleSheet(QStringLiteral(
        "background-color: #ffffff;"
        "border: 1px solid #d1d5db;"
        "border-radius: 4px;"
        "padding: 3px 6px;"
        "font-family: monospace;"
        "font-size: 11px;"
        "color: #374151;"
    ));
    layout->addWidget(m_coordLabel);

    m_hoverColorSwatch = new QLabel(bar);
    m_hoverColorSwatch->setFixedSize(18, 18);
    m_hoverColorSwatch->setStyleSheet(QStringLiteral("border: 1px solid #9ca3af; border-radius: 3px; background-color: transparent;"));
    m_hoverColorSwatch->hide();
    layout->addWidget(m_hoverColorSwatch);

    m_colorInfoLabel = new QLabel(QStringLiteral(""), bar);
    m_colorInfoLabel->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px; color: #374151;"));
    layout->addWidget(m_colorInfoLabel);

    layout->addStretch(1);

    const QString btnSecondaryStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: #ffffff;"
        "  border: 1px solid #d1d5db;"
        "  border-radius: 6px;"
        "  color: #1f2937;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  min-width: 80px;"
        "  min-height: 28px;"
        "}"
        "QPushButton:hover { background-color: #f3f4f6; border-color: #9ca3af; }"
        "QPushButton:pressed { background-color: #e5e7eb; }"
    );

    m_cancelBtn = new QPushButton(tr("Cancel"), bar);
    m_cancelBtn->setStyleSheet(btnSecondaryStyle);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(m_cancelBtn);

    m_applyBtn = new QPushButton(tr("Apply"), bar);
    m_applyBtn->setStyleSheet(btnSecondaryStyle);
    connect(m_applyBtn, &QPushButton::clicked, this, &PixelEditorDialog::onApplyClicked);
    layout->addWidget(m_applyBtn);

    m_okBtn = new QPushButton(tr("OK"), bar);
    m_okBtn->setDefault(true);
    m_okBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #2563eb;"
        "  border: none;"
        "  border-radius: 6px;"
        "  color: #ffffff;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  min-width: 80px;"
        "  min-height: 28px;"
        "}"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }"
    ));
    connect(m_okBtn, &QPushButton::clicked, this, &PixelEditorDialog::onOkClicked);
    layout->addWidget(m_okBtn);

    return bar;
}

void PixelEditorDialog::onToolButtonClicked(int id)
{
    m_canvas->setCurrentTool(static_cast<PixelTool>(id));
}

void PixelEditorDialog::onPickColorClicked()
{
    onPrimarySwatchClicked();
}

void PixelEditorDialog::onPrimarySwatchClicked()
{
    QColor initial = m_canvas ? m_canvas->primaryColor() : Qt::black;
    QColor col = QColorDialog::getColor(initial, this, tr("Select Primary Color"), QColorDialog::ShowAlphaChannel);
    if (col.isValid()) {
        if (m_canvas) {
            m_canvas->setPrimaryColor(col);
        }
        addRecentColor(col);
    }
}

void PixelEditorDialog::onSecondarySwatchClicked()
{
    QColor initial = m_canvas ? m_canvas->secondaryColor() : Qt::white;
    QColor col = QColorDialog::getColor(initial, this, tr("Select Secondary Color"), QColorDialog::ShowAlphaChannel);
    if (col.isValid()) {
        if (m_canvas) {
            m_canvas->setSecondaryColor(col);
        }
        addRecentColor(col);
    }
}

void PixelEditorDialog::onCanvasImageChanged()
{
    updateLivePreview();
}

void PixelEditorDialog::onCanvasPixelMoved(int x, int y, const QColor &color)
{
    m_coordLabel->setText(tr("X: %1 , Y: %2").arg(x).arg(y));
    if (color.alpha() == 0) {
        if (m_hoverColorSwatch) {
            m_hoverColorSwatch->setStyleSheet(QStringLiteral("border: 1px solid #475569; border-radius: 3px; background-color: rgba(255,255,255,0.06);"));
            m_hoverColorSwatch->show();
        }
        m_colorInfoLabel->setText(tr("Transparent [alpha: 0]"));
    } else {
        if (m_hoverColorSwatch) {
            m_hoverColorSwatch->setStyleSheet(QStringLiteral("border: 1px solid #ffffff; border-radius: 3px; background-color: %1;").arg(color.name()));
            m_hoverColorSwatch->show();
        }
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
    if (m_hoverColorSwatch) m_hoverColorSwatch->hide();
    m_colorInfoLabel->clear();
}

void PixelEditorDialog::onCanvasZoomChanged(double zoom)
{
    m_zoomLabel->setText(tr("Zoom: %1%").arg(static_cast<int>(std::round(zoom * 100))));
}

void PixelEditorDialog::updateLivePreview()
{
    if (!m_previewLabel || !m_canvas) return;
    const QImage &img = m_canvas->image();
    if (img.isNull()) {
        m_previewLabel->clear();
        return;
    }

    static const QPixmap s_checker = []() {
        QPixmap pm(16, 16);
        QPainter cp(&pm);
        cp.fillRect(0, 0, 8, 8, QColor(45, 45, 50));
        cp.fillRect(8, 8, 8, 8, QColor(45, 45, 50));
        cp.fillRect(8, 0, 8, 8, QColor(60, 60, 65));
        cp.fillRect(0, 8, 8, 8, QColor(60, 60, 65));
        return pm;
    }();

    QPixmap px(img.size());
    QPainter p(&px);
    p.drawTiledPixmap(px.rect(), s_checker);
    p.drawImage(0, 0, img);
    p.end();

    m_previewLabel->setPixmap(px);
}

void PixelEditorDialog::loadFrame(int index)
{
    if (!m_document || index < 0 || index >= m_document->frameCount()) return;
    m_currentFrameIndex = index;

    const bool hasPoly = (index < m_document->boxes().size() &&
                          m_document->box(index).polygon.size() >= 3);
    const QPolygonF poly = hasPoly ? m_document->box(index).polygon : QPolygonF();

    QImage frameImg;
    if (m_sessionModifiedFrames.contains(index)) {
        frameImg = m_sessionModifiedFrames[index];
    } else {
        if (hasPoly) {
            frameImg = m_document->polygonClippedFrame(index);
        } else {
            frameImg = m_document->frame(index);
        }
    }

    m_canvas->setImage(frameImg);
    m_canvas->setPolygonMesh(poly);
    m_canvas->zoomFit(m_scrollArea->viewport()->size());

    m_frameInfoLabel->setText(tr("Frame %1 / %2  (%3x%4 px)")
                              .arg(m_currentFrameIndex + 1)
                              .arg(m_document->frameCount())
                              .arg(frameImg.width())
                              .arg(frameImg.height()));

    updateNavigationButtons();
    updateLivePreview();
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
    m_currentPalette = getPresetPalette(preset);
    refreshPaletteSwatches();
}

void PixelEditorDialog::onSampleFrameColorsClicked()
{
    if (!m_canvas) return;
    QImage img = m_canvas->image();
    if (img.isNull()) return;

    const bool hasPoly = (m_document && m_currentFrameIndex >= 0 &&
                          m_currentFrameIndex < m_document->boxes().size() &&
                          m_document->box(m_currentFrameIndex).polygon.size() >= 3);
    const QPolygonF poly = hasPoly ? m_document->box(m_currentFrameIndex).polygon : QPolygonF();

    QSet<QRgb> uniqueColors;
    const int w = img.width();
    const int h = img.height();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (hasPoly && !poly.containsPoint(QPointF(x + 0.5, y + 0.5), Qt::OddEvenFill)) {
                continue;
            }
            QRgb rgb = img.pixel(x, y);
            if (qAlpha(rgb) >= 16) {
                uniqueColors.insert(qRgb(qRed(rgb), qGreen(rgb), qBlue(rgb)));
            }
        }
    }

    if (uniqueColors.isEmpty()) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (hasPoly && !poly.containsPoint(QPointF(x + 0.5, y + 0.5), Qt::OddEvenFill)) {
                    continue;
                }
                QRgb rgb = img.pixel(x, y);
                if (qAlpha(rgb) > 0) {
                    uniqueColors.insert(qRgb(qRed(rgb), qGreen(rgb), qBlue(rgb)));
                }
            }
        }
    }

    m_currentPalette = uniqueColors.values().toVector();
    std::sort(m_currentPalette.begin(), m_currentPalette.end(), [](QRgb a, QRgb b) {
        QColor ca(a);
        QColor cb(b);
        bool aNeutral = ca.saturation() < 24;
        bool bNeutral = cb.saturation() < 24;
        if (aNeutral != bNeutral) return aNeutral;
        if (aNeutral) return ca.value() < cb.value();
        if (std::abs(ca.hsvHue() - cb.hsvHue()) > 8) return ca.hsvHue() < cb.hsvHue();
        if (std::abs(ca.saturation() - cb.saturation()) > 15) return ca.saturation() < cb.saturation();
        return ca.value() < cb.value();
    });

    refreshPaletteSwatches();
}

void PixelEditorDialog::addRecentColor(const QColor &color)
{
    if (!color.isValid() || color.alpha() < 16) return;
    QRgb rgb = qRgb(color.red(), color.green(), color.blue());

    for (int i = 0; i < m_recentColors.size(); ++i) {
        if (qRgb(m_recentColors[i].red(), m_recentColors[i].green(), m_recentColors[i].blue()) == rgb) {
            m_recentColors.removeAt(i);
            break;
        }
    }
    m_recentColors.prepend(color);
    while (m_recentColors.size() > 8) {
        m_recentColors.removeLast();
    }
    refreshRecentSwatches();
}

void PixelEditorDialog::refreshRecentSwatches()
{
    if (!m_recentLayout) return;

    QLayoutItem *child;
    while ((child = m_recentLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    for (const QColor &col : m_recentColors) {
        QPushButton *btn = new QPushButton(m_recentContainer);
        btn->setFixedSize(22, 22);
        btn->setToolTip(QStringLiteral("%1\nLeft: Primary, Right: Secondary").arg(col.name().toUpper()));
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  border: 1px solid rgba(0, 0, 0, 0.25);"
            "  border-radius: 4px;"
            "}"
            "QPushButton:hover {"
            "  border: 2px solid #2563eb;"
            "}"
        ).arg(col.name()));

        connect(btn, &QPushButton::clicked, this, [this, col]() {
            if (m_canvas) m_canvas->setPrimaryColor(col);
        });
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QPushButton::customContextMenuRequested, this, [this, col]() {
            if (m_canvas) m_canvas->setSecondaryColor(col);
        });

        m_recentLayout->addWidget(btn);
    }
}

void PixelEditorDialog::refreshPaletteSwatches()
{
    if (m_swatchesContainer) {
        m_swatchesContainer->setUpdatesEnabled(false);
    }

    // Clear existing swatches
    QLayoutItem *child;
    while ((child = m_swatchesLayout->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    const int columns = 7;
    for (int i = 0; i < m_currentPalette.size(); ++i) {
        QRgb rgb = m_currentPalette[i];
        QColor col(rgb);

        QPushButton *btn = new QPushButton(m_swatchesContainer);
        btn->setFixedSize(26, 26);
        btn->setToolTip(QStringLiteral("%1\nRGB(%2, %3, %4)\nLeft-Click: Primary\nRight-Click: Secondary")
                        .arg(col.name().toUpper())
                        .arg(col.red()).arg(col.green()).arg(col.blue()));
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: %1;"
            "  border: 1px solid rgba(0, 0, 0, 0.2);"
            "  border-radius: 4px;"
            "}"
            "QPushButton:hover {"
            "  border: 2px solid #2563eb;"
            "}"
        ).arg(col.name()));

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

    if (m_swatchesContainer) {
        m_swatchesContainer->setUpdatesEnabled(true);
    }
}

QVector<QRgb> PixelEditorDialog::getPresetPalette(PalettePreset preset)
{
    QVector<QRgb> pal;
    switch (preset) {
    case Standard:
        pal = {
            // Row 1: Grayscale / Neutrals
            qRgb(0, 0, 0),       qRgb(43, 43, 43),    qRgb(90, 90, 90),
            qRgb(142, 142, 142), qRgb(196, 196, 196), qRgb(255, 255, 255),
            // Row 2: Skin & Earth Tones
            qRgb(58, 31, 29),    qRgb(107, 62, 46),   qRgb(160, 90, 63),
            qRgb(199, 131, 99),  qRgb(224, 169, 139), qRgb(245, 211, 190),
            // Row 3: Warm Reds & Oranges
            qRgb(128, 12, 12),   qRgb(196, 27, 27),   qRgb(230, 74, 25),
            qRgb(255, 112, 67),  qRgb(245, 124, 0),   qRgb(255, 183, 77),
            // Row 4: Yellows & Greens
            qRgb(251, 192, 45),  qRgb(255, 241, 118), qRgb(104, 159, 56),
            qRgb(139, 195, 74),  qRgb(46, 125, 50),   qRgb(76, 175, 80),
            // Row 5: Cyans & Blues
            qRgb(0, 131, 143),   qRgb(0, 172, 193),   qRgb(2, 119, 189),
            qRgb(3, 169, 244),   qRgb(21, 101, 192),  qRgb(63, 81, 181),
            // Row 6: Purples, Magentas & Pinks
            qRgb(74, 20, 140),   qRgb(123, 31, 162),  qRgb(171, 71, 188),
            qRgb(173, 20, 87),   qRgb(233, 30, 99),   qRgb(244, 143, 177)
        };
        break;

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
    setWindowTitle(tr("Pixel Editor — BentoPack"));

    if (m_prevFrameBtn) {
        m_prevFrameBtn->setText(QStringLiteral("◀"));
        m_prevFrameBtn->setToolTip(tr("Navigate to previous frame (Page Up)"));
    }
    if (m_nextFrameBtn) {
        m_nextFrameBtn->setText(QStringLiteral("▶"));
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
    if (m_btnClearSel) m_btnClearSel->setToolTip(tr("Clear Selection / Deselect (Del)"));
    if (m_btnGrid) {
        m_btnGrid->setText(tr("# Grid"));
        m_btnGrid->setToolTip(tr("Toggle Pixel Grid"));
    }
    if (m_btnZoomIn) m_btnZoomIn->setToolTip(tr("Zoom In"));
    if (m_btnZoomOut) m_btnZoomOut->setToolTip(tr("Zoom Out"));
    if (m_btnFit) {
        m_btnFit->setText(tr("⊡ Fit"));
        m_btnFit->setToolTip(tr("Fit to View"));
    }
    if (m_btnUndo) m_btnUndo->setToolTip(tr("Undo (Ctrl+Z)"));
    if (m_btnRedo) m_btnRedo->setToolTip(tr("Redo (Ctrl+Y)"));

    // Palette & Colors
    if (m_colorsGroup) m_colorsGroup->setTitle(tr("Color Picker"));
    if (m_primarySwatchBtn) m_primarySwatchBtn->setToolTip(tr("Primary Color (Click to open Color Picker)"));
    if (m_secondarySwatchBtn) m_secondarySwatchBtn->setToolTip(tr("Secondary Color (Click to open Color Picker)"));
    if (m_swapBtn) m_swapBtn->setToolTip(tr("Swap Colors (X)"));
    if (m_btnPickColor) m_btnPickColor->setText(tr("🎨 Pick Color..."));
    if (m_recentLabel) m_recentLabel->setText(tr("Recent:"));
    if (m_palLabel) m_palLabel->setText(tr("Preset:"));
    if (m_btnSampleFrame) {
        m_btnSampleFrame->setText(tr("Sample Frame"));
        m_btnSampleFrame->setToolTip(tr("Extract all unique colors from current sprite frame"));
    }

    if (m_paletteCombo) {
        int curIdx = m_paletteCombo->currentIndex();
        m_paletteCombo->blockSignals(true);
        m_paletteCombo->setItemText(Standard, tr("Bento Standard (36)"));
        m_paletteCombo->setItemText(NES, tr("NES / Famicom (54)"));
        m_paletteCombo->setItemText(SNES, tr("SNES / 16-bit (32)"));
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

