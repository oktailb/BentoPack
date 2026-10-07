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
#include "commands/filtercommands.h"
#include "packer/atlaspacker.h"
#include "geometry/triangulator.h"
#include "filters/filterregistry.h"
#include "filters/filterplugin.h"
#include "widgets/filterdialogbase.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QMenu>
#include <QMessageBox>
#include <QGroupBox>
#include <QFrame>
#include <QSet>
#include <QPainter>
#include <QShortcut>
#include <QKeyEvent>
#include <QSpacerItem>
#include <QScrollBar>
#include <QtConcurrent/QtConcurrent>
#include <QPointer>
#include <QQueue>
#include <algorithm>

namespace {

struct FrameBackup {
    int frameIndex = 0;
    QImage oldImage;
    QImage newImage;
    QPolygonF oldPoly;
    QPolygonF newPoly;
};

static void drawBresenhamOnTarget(QImage &img, int x0, int y0, int x1, int y1, const QColor &color,
                                  bool canEditOutside, const QPolygonF &targetPoly,
                                  bool hasSelection, const QRect &targetSelRect)
{
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int w = img.width();
    int h = img.height();

    while (true) {
        if (x0 >= 0 && x0 < w && y0 >= 0 && y0 < h) {
            bool editable = true;
            if (hasSelection && !targetSelRect.contains(x0, y0)) {
                editable = false;
            }
            if (editable && !canEditOutside && targetPoly.size() >= 3) {
                if (!targetPoly.containsPoint(QPointF(x0 + 0.5, y0 + 0.5), Qt::OddEvenFill)) {
                    editable = false;
                }
            }
            if (editable) {
                img.setPixelColor(x0, y0, color);
            }
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

static void floodFillOnTarget(QImage &img, int startX, int startY, const QColor &replacementColor,
                              bool canEditOutside, const QPolygonF &targetPoly,
                              bool hasSelection, const QRect &targetSelRect)
{
    int w = img.width();
    int h = img.height();
    if (startX < 0 || startX >= w || startY < 0 || startY >= h) return;

    auto isTargetPixelEditable = [&](int tx, int ty) -> bool {
        if (tx < 0 || tx >= w || ty < 0 || ty >= h) return false;
        if (hasSelection && !targetSelRect.contains(tx, ty)) return false;
        if (!canEditOutside && targetPoly.size() >= 3) {
            if (!targetPoly.containsPoint(QPointF(tx + 0.5, ty + 0.5), Qt::OddEvenFill)) {
                return false;
            }
        }
        return true;
    };

    if (!isTargetPixelEditable(startX, startY)) return;

    QRgb targetLocalRgb = img.pixel(startX, startY);
    QRgb replaceRgb = replacementColor.rgba();
    if (targetLocalRgb == replaceRgb) return;

    QVector<bool> visited(w * h, false);
    QQueue<QPoint> queue;

    queue.enqueue(QPoint(startX, startY));
    visited[startY * w + startX] = true;

    while (!queue.isEmpty()) {
        QPoint pt = queue.dequeue();
        int x = pt.x();
        int y = pt.y();

        img.setPixelColor(x, y, replacementColor);

        const int dx[] = {-1, 1, 0, 0};
        const int dy[] = {0, 0, -1, 1};
        for (int i = 0; i < 4; ++i) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int idx = ny * w + nx;
                if (!visited[idx] && isTargetPixelEditable(nx, ny) && img.pixel(nx, ny) == targetLocalRgb) {
                    visited[idx] = true;
                    queue.enqueue(QPoint(nx, ny));
                }
            }
        }
    }
}

} // namespace

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
    resize(1080, 720);
    setMinimumSize(850, 540);

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

    populateAnimationCombo();

    if (m_document && m_document->frameCount() > 0) {
        if (m_currentFrameIndex < 0 || m_currentFrameIndex >= m_document->frameCount()) {
            m_currentFrameIndex = 0;
        }
        loadFrame(m_currentFrameIndex);
    }

    // Default palette is Bento Standard (36)
    onPalettePresetChanged(0);
    updateOnionSkinLayers();
}

PixelEditorDialog::~PixelEditorDialog()
{
    if (m_canvas && m_canvas->undoStack()) {
        m_canvas->undoStack()->disconnect(this);
    }
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
    connect(m_canvas, &PixelCanvas::polygonMeshChanged, this, [this](const QPolygonF &newMesh) {
        m_sessionModifiedPolygons[m_currentFrameIndex] = newMesh;
        updateCollisionWarningUI();
    });
    connect(m_canvas, &PixelCanvas::mousePixelMoved, this, &PixelEditorDialog::onCanvasPixelMoved);
    connect(m_canvas, &PixelCanvas::mousePixelLeft, this, &PixelEditorDialog::onCanvasPixelLeft);
    connect(m_canvas, &PixelCanvas::zoomChanged, this, &PixelEditorDialog::onCanvasZoomChanged);
    connect(m_canvas, &PixelCanvas::panRequested, this, [this](int dx, int dy) {
        if (m_scrollArea) {
            m_scrollArea->horizontalScrollBar()->setValue(m_scrollArea->horizontalScrollBar()->value() - dx);
            m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->value() - dy);
        }
    });

    connect(m_canvas, &PixelCanvas::modificationPushed, this, &PixelEditorDialog::onCanvasModificationPushed);

    if (m_canvas->undoStack()) {
        QPointer<QToolButton> btnUndo = m_btnUndo;
        QPointer<QToolButton> btnRedo = m_btnRedo;
        connect(m_canvas->undoStack(), &QUndoStack::canUndoChanged, this, [btnUndo](bool can) {
            if (btnUndo) btnUndo->setEnabled(can);
        });
        connect(m_canvas->undoStack(), &QUndoStack::canRedoChanged, this, [btnRedo](bool can) {
            if (btnRedo) btnRedo->setEnabled(can);
        });
        if (m_btnUndo) m_btnUndo->setEnabled(m_canvas->undoStack()->canUndo());
        if (m_btnRedo) m_btnRedo->setEnabled(m_canvas->undoStack()->canRedo());
    }

    connect(m_canvas, &PixelCanvas::primaryColorChanged, this, [this](const QColor &col) {
        if (m_primarySwatchBtn) {
            m_primarySwatchBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 2px solid palette(window-text); border-radius: 6px;").arg(col.name()));
        }
        if (m_primaryHexLabel) {
            m_primaryHexLabel->setText(col.name().toUpper());
        }
        if (m_rgbLabel) {
            m_rgbLabel->setText(QStringLiteral("RGB(%1,%2,%3)").arg(col.red()).arg(col.green()).arg(col.blue()));
        }
        if (m_colorPickerWidget) {
            m_colorPickerWidget->setColor(col);
        }
        addRecentColor(col);
    });
    connect(m_canvas, &PixelCanvas::secondaryColorChanged, this, [this](const QColor &col) {
        if (m_secondarySwatchBtn) {
            m_secondarySwatchBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 2px solid palette(mid); border-radius: 6px;").arg(col.name()));
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

    retranslateUi();
}

QWidget* PixelEditorDialog::createHeaderBar()
{
    QWidget *bar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    const QString navBtnStyle = QStringLiteral(
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover { background-color: palette(alternate-base); border-color: palette(highlight); }"
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
        "border: 1px solid palette(mid);"
        "border-radius: 6px;"
        "padding: 4px 14px;"
        "font-weight: bold;"
        "font-size: 12px;"
    ));
    layout->addWidget(m_frameInfoLabel);

    m_nextFrameBtn = new QPushButton(QStringLiteral("▶"), bar);
    m_nextFrameBtn->setFixedSize(34, 28);
    m_nextFrameBtn->setStyleSheet(navBtnStyle);
    m_nextFrameBtn->setToolTip(tr("Navigate to next frame (Page Down)"));
    connect(m_nextFrameBtn, &QPushButton::clicked, this, &PixelEditorDialog::onNextFrame);
    layout->addWidget(m_nextFrameBtn);

    // Animation Selector Combobox
    QFrame *animSep = new QFrame(bar);
    animSep->setFrameShape(QFrame::VLine);
    layout->addWidget(animSep);

    m_animLabel = new QLabel(tr("Animation:"), bar);
    m_animLabel->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 11px;"));
    layout->addWidget(m_animLabel);

    m_animCombo = new QComboBox(bar);
    m_animCombo->setMinimumWidth(160);
    m_animCombo->setMaximumWidth(240);
    connect(m_animCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PixelEditorDialog::onAnimationFilterChanged);
    layout->addWidget(m_animCombo);

    layout->addStretch(1);

    // View & Zoom controls
    const QString viewBtnStyle = QStringLiteral(
        "QToolButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  font-weight: 500;"
        "  font-size: 12px;"
        "}"
        "QToolButton:hover { background-color: palette(alternate-base); border-color: palette(highlight); }"
        "QToolButton:checked { background-color: palette(highlight); color: palette(highlighted-text); border: 2px solid palette(highlight); font-weight: bold; }"
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

    m_btnShowPivot = new QToolButton(bar);
    m_btnShowPivot->setText(QStringLiteral("⌖ ") + tr("Pivot"));
    m_btnShowPivot->setToolTip(tr("Show / Hide Pivot Anchor Marker"));
    m_btnShowPivot->setCheckable(true);
    m_btnShowPivot->setChecked(true);
    m_btnShowPivot->setFixedSize(68, 28);
    m_btnShowPivot->setStyleSheet(viewBtnStyle);
    connect(m_btnShowPivot, &QToolButton::toggled, this, [this](bool checked) {
        if (m_canvas) m_canvas->setShowPivot(checked);
    });
    layout->addWidget(m_btnShowPivot);

    m_allowOutsidePolyCheck = new QCheckBox(tr("Edit outside polygon"), bar);
    m_allowOutsidePolyCheck->setToolTip(tr("Allow editing pixels outside polygon boundaries (Default: off when polygon exists)"));
    m_allowOutsidePolyCheck->setFixedHeight(28);
    connect(m_allowOutsidePolyCheck, &QCheckBox::toggled, this, &PixelEditorDialog::onAllowOutsidePolygonToggled);
    layout->addWidget(m_allowOutsidePolyCheck);

    m_applyToAllFramesCheck = new QCheckBox(tr("Apply to all frames"), bar);
    m_applyToAllFramesCheck->setObjectName(QStringLiteral("applyToAllFramesCheck"));
    m_applyToAllFramesCheck->setToolTip(tr("Apply edits (drawing, flip, fill, etc.) to all frames aligned by pivot"));
    m_applyToAllFramesCheck->setChecked(false);
    m_applyToAllFramesCheck->setFixedHeight(28);
    layout->addWidget(m_applyToAllFramesCheck);

    m_btnFilters = new QToolButton(bar);
    m_btnFilters->setObjectName(QStringLiteral("filtersButton"));
    m_btnFilters->setText(tr("✨ Filters ▾"));
    m_btnFilters->setToolTip(tr("Apply image and color filters (Despill, Outline, Rescale, Palette...)"));
    m_btnFilters->setPopupMode(QToolButton::InstantPopup);
    m_btnFilters->setFixedHeight(28);
    m_btnFilters->setStyleSheet(viewBtnStyle);

    QMenu *filtersMenu = new QMenu(m_btnFilters);
    populateFiltersMenu(filtersMenu);
    m_btnFilters->setMenu(filtersMenu);
    layout->addWidget(m_btnFilters);

    QFrame *sep = new QFrame(bar);
    sep->setFrameShape(QFrame::VLine);
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
        "border: 1px solid palette(mid);"
        "border-radius: 6px;"
        "padding: 4px 6px;"
        "font-weight: bold;"
        "font-size: 11px;"
        "font-family: monospace;"
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
    m_btnFit->setText(tr("⊡ Fit"));
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
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  font-size: 15px;"
        "  font-weight: bold;"
        "}"
        "QToolButton:hover {"
        "  background-color: palette(alternate-base);"
        "  border-color: palette(highlight);"
        "}"
        "QToolButton:checked {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "  border: 2px solid palette(highlight);"
        "}"
        "QToolButton:disabled {"
        "  color: palette(placeholder-text);"
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
    layout->addWidget(line2, 6, 0, 1, 2);

    // Undo / Redo
    m_btnUndo = addActionBtn(QStringLiteral("↶"), tr("Undo (Ctrl+Z)"), 7, 0, [this]() { m_canvas->undo(); });
    m_btnRedo = addActionBtn(QStringLiteral("↷"), tr("Redo (Ctrl+Y)"), 7, 1, [this]() { m_canvas->redo(); });
    if (m_btnUndo) m_btnUndo->setEnabled(false);
    if (m_btnRedo) m_btnRedo->setEnabled(false);

    // Spacer
    layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding), 8, 0, 1, 2);

    return panel;
}

QWidget* PixelEditorDialog::createPalettePanel()
{
    QScrollArea *scrollPanel = new QScrollArea(this);
    scrollPanel->setFixedWidth(318);
    scrollPanel->setWidgetResizable(true);
    scrollPanel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollPanel->setStyleSheet(QStringLiteral("QScrollArea { background-color: transparent; border: none; }"));

    QWidget *panel = new QWidget(scrollPanel);
    panel->setFixedWidth(304);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 8, 6, 8);
    layout->setSpacing(10);

    const QString groupBoxStyle = QStringLiteral(
        "QGroupBox {"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  margin-top: 14px;"
        "  padding-top: 6px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 8px;"
        "  top: 0px;"
        "  padding: 0 4px;"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "}"
    );

    // 1. Active Color Box & Pro Color Picker Suite
    m_colorsGroup = new QGroupBox(tr("Color Studio && Harmonies"), panel);
    m_colorsGroup->setStyleSheet(groupBoxStyle);
    QVBoxLayout *colorsMainLayout = new QVBoxLayout(m_colorsGroup);
    colorsMainLayout->setContentsMargins(6, 8, 6, 8);
    colorsMainLayout->setSpacing(6);

    QHBoxLayout *swatchesRow = new QHBoxLayout();
    swatchesRow->setSpacing(6);

    m_primarySwatchBtn = new QPushButton(m_colorsGroup);
    m_primarySwatchBtn->setFixedSize(38, 38);
    m_primarySwatchBtn->setToolTip(tr("Primary Color (Click to open Pro Color Picker)"));
    m_primarySwatchBtn->setStyleSheet(QStringLiteral("background-color: #000000; border: 2px solid palette(window-text); border-radius: 6px;"));
    connect(m_primarySwatchBtn, &QPushButton::clicked, this, &PixelEditorDialog::onPrimarySwatchClicked);
    swatchesRow->addWidget(m_primarySwatchBtn);

    m_swapBtn = new QPushButton(QStringLiteral("⇄"), m_colorsGroup);
    m_swapBtn->setFixedSize(26, 26);
    m_swapBtn->setToolTip(tr("Swap Colors (X)"));
    m_swapBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 13px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: palette(alternate-base); border-color: palette(highlight); }"
    ));
    connect(m_swapBtn, &QPushButton::clicked, this, [this]() { m_canvas->swapColors(); });
    swatchesRow->addWidget(m_swapBtn);

    m_secondarySwatchBtn = new QPushButton(m_colorsGroup);
    m_secondarySwatchBtn->setFixedSize(34, 34);
    m_secondarySwatchBtn->setToolTip(tr("Secondary Color (Click to open Pro Color Picker)"));
    m_secondarySwatchBtn->setStyleSheet(QStringLiteral("background-color: #ffffff; border: 2px solid palette(mid); border-radius: 6px;"));
    connect(m_secondarySwatchBtn, &QPushButton::clicked, this, &PixelEditorDialog::onSecondarySwatchClicked);
    swatchesRow->addWidget(m_secondarySwatchBtn);

    QVBoxLayout *hexCol = new QVBoxLayout();
    hexCol->setSpacing(1);
    m_primaryHexLabel = new QLabel(QStringLiteral("#000000"), m_colorsGroup);
    m_primaryHexLabel->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px; font-weight: bold;"));
    hexCol->addWidget(m_primaryHexLabel);

    m_rgbLabel = new QLabel(QStringLiteral("RGB(0, 0, 0)"), m_colorsGroup);
    m_rgbLabel->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 10px;"));
    hexCol->addWidget(m_rgbLabel);
    swatchesRow->addLayout(hexCol);

    swatchesRow->addStretch();

    m_btnPickColor = new QPushButton(tr("⛶ Pop-out..."), m_colorsGroup);
    m_btnPickColor->setToolTip(tr("Open Full Pro Color Picker Dialog"));
    m_btnPickColor->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  font-size: 10px;"
        "  font-weight: 600;"
        "  padding: 3px 6px;"
        "}"
        "QPushButton:hover { background-color: palette(alternate-base); border-color: palette(highlight); }"
    ));
    connect(m_btnPickColor, &QPushButton::clicked, this, &PixelEditorDialog::onPickColorClicked);
    swatchesRow->addWidget(m_btnPickColor);

    colorsMainLayout->addLayout(swatchesRow);

    // Embedded Interactive Pro Color Picker (Wheel + Harmonies, 2D Map, Sliders)
    m_colorPickerWidget = new ColorPickerWidget(m_colorsGroup);
    QColor initialCol = m_canvas ? m_canvas->primaryColor() : Qt::black;
    m_colorPickerWidget->setColor(initialCol);
    m_colorPickerWidget->setOldColor(initialCol);
    connect(m_colorPickerWidget, &ColorPickerWidget::colorChanged, this, [this](const QColor &col) {
        if (m_canvas) {
            m_canvas->setPrimaryColor(col);
        }
    });
    colorsMainLayout->addWidget(m_colorPickerWidget);

    // Recent colors
    m_recentLabel = new QLabel(tr("Recent:"), m_colorsGroup);
    m_recentLabel->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold; margin-top: 2px;"));
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
    m_palLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px;"));
    presetHeader->addWidget(m_palLabel);

    m_btnSampleFrame = new QPushButton(tr("Sample Frame"), panel);
    m_btnSampleFrame->setToolTip(tr("Extract all unique colors from current sprite frame"));
    m_btnSampleFrame->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  font-size: 10px;"
        "  font-weight: 500;"
        "  padding: 2px 6px;"
        "}"
        "QPushButton:hover { background-color: palette(alternate-base); border-color: palette(highlight); }"
    ));
    connect(m_btnSampleFrame, &QPushButton::clicked, this, &PixelEditorDialog::onSampleFrameColorsClicked);
    presetHeader->addWidget(m_btnSampleFrame);
    layout->addLayout(presetHeader);

    m_paletteCombo = new QComboBox(panel);
    m_paletteCombo->addItem(tr("Bento Standard (36)"), Standard);
    m_paletteCombo->addItem(tr("Game Boy DMG (4 Greens)"), GameBoy);
    m_paletteCombo->addItem(tr("Game Boy Pocket (4 Grays)"), GameBoyPocket);
    m_paletteCombo->addItem(tr("NES / Famicom (54)"), NES);
    m_paletteCombo->addItem(tr("SNES / 16-bit (32)"), SNES);
    m_paletteCombo->addItem(tr("PICO-8 (16)"), Pico8);
    m_paletteCombo->addItem(tr("Commodore 64 (16)"), Commodore64);
    m_paletteCombo->addItem(tr("Amiga OCS (32)"), Amiga);
    m_paletteCombo->addItem(tr("NEC PC-Engine (32)"), PCEngine);
    m_paletteCombo->addItem(tr("CGA Mode 1 (4)"), CGAMode1);
    m_paletteCombo->addItem(tr("CGA Mode 2 (4)"), CGAMode2);
    m_paletteCombo->addItem(tr("Endesga 32 (32)"), Endesga32);
    connect(m_paletteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PixelEditorDialog::onPalettePresetChanged);
    layout->addWidget(m_paletteCombo);

    // 3. Swatches Grid inside ScrollArea
    QScrollArea *swatchScroll = new QScrollArea(panel);
    swatchScroll->setWidgetResizable(true);
    swatchScroll->setFixedHeight(110);
    swatchScroll->setStyleSheet(QStringLiteral("background-color: palette(base); border: 1px solid palette(mid); border-radius: 6px;"));

    m_swatchesContainer = new QWidget(swatchScroll);
    m_swatchesLayout = new QGridLayout(m_swatchesContainer);
    m_swatchesLayout->setContentsMargins(4, 4, 4, 4);
    m_swatchesLayout->setSpacing(4);
    swatchScroll->setWidget(m_swatchesContainer);
    layout->addWidget(swatchScroll);

    // 4. Onion Skinning Suite
    m_onionSkinGroup = new QGroupBox(tr("Onion Skinning"), panel);
    m_onionSkinGroup->setStyleSheet(groupBoxStyle);
    QVBoxLayout *osLayout = new QVBoxLayout(m_onionSkinGroup);
    osLayout->setContentsMargins(8, 8, 8, 8);
    osLayout->setSpacing(6);

    m_onionSkinCheck = new QCheckBox(tr("Enable Onion Skin"), m_onionSkinGroup);
    m_onionSkinCheck->setChecked(true);
    m_onionSkinCheck->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: 600;"));
    connect(m_onionSkinCheck, &QCheckBox::toggled, this, &PixelEditorDialog::onOnionSkinToggled);
    osLayout->addWidget(m_onionSkinCheck);

    // Side-by-side Past and Future sliders
    QHBoxLayout *slidersRow = new QHBoxLayout();
    slidersRow->setSpacing(8);

    // Left column: Past frames (-3 to 0)
    QVBoxLayout *pastCol = new QVBoxLayout();
    pastCol->setSpacing(2);
    QHBoxLayout *pastHeaderLayout = new QHBoxLayout();
    m_lblPastTitle = new QLabel(tr("Past:"), m_onionSkinGroup);
    m_lblPastTitle->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: 600;"));
    pastHeaderLayout->addWidget(m_lblPastTitle);
    pastHeaderLayout->addStretch();
    m_lblPastFrames = new QLabel(tr("-1"), m_onionSkinGroup);
    m_lblPastFrames->setStyleSheet(QStringLiteral(
        "border: 1px solid palette(mid); border-radius: 3px; padding: 1px 6px; font-size: 10px; font-weight: bold;"
    ));
    pastHeaderLayout->addWidget(m_lblPastFrames);
    pastCol->addLayout(pastHeaderLayout);

    m_sliderPastFrames = new QSlider(Qt::Horizontal, m_onionSkinGroup);
    m_sliderPastFrames->setRange(-3, 0);
    m_sliderPastFrames->setValue(-1);
    m_sliderPastFrames->setTickPosition(QSlider::TicksBelow);
    m_sliderPastFrames->setTickInterval(1);
    m_sliderPastFrames->setToolTip(tr("Past frames to display (-3 to 0)"));
    connect(m_sliderPastFrames, &QSlider::valueChanged, this, &PixelEditorDialog::onOnionSkinPastChanged);
    pastCol->addWidget(m_sliderPastFrames);
    slidersRow->addLayout(pastCol, 1);

    // Right column: Future frames (0 to +3)
    QVBoxLayout *futureCol = new QVBoxLayout();
    futureCol->setSpacing(2);
    QHBoxLayout *futureHeaderLayout = new QHBoxLayout();
    m_lblFutureTitle = new QLabel(tr("Future:"), m_onionSkinGroup);
    m_lblFutureTitle->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: 600;"));
    futureHeaderLayout->addWidget(m_lblFutureTitle);
    futureHeaderLayout->addStretch();
    m_lblFutureFrames = new QLabel(tr("0"), m_onionSkinGroup);
    m_lblFutureFrames->setStyleSheet(QStringLiteral(
        "border: 1px solid palette(mid); border-radius: 3px; padding: 1px 6px; font-size: 10px; font-weight: bold;"
    ));
    futureHeaderLayout->addWidget(m_lblFutureFrames);
    futureCol->addLayout(futureHeaderLayout);

    m_sliderFutureFrames = new QSlider(Qt::Horizontal, m_onionSkinGroup);
    m_sliderFutureFrames->setRange(0, 3);
    m_sliderFutureFrames->setValue(0);
    m_sliderFutureFrames->setTickPosition(QSlider::TicksBelow);
    m_sliderFutureFrames->setTickInterval(1);
    m_sliderFutureFrames->setToolTip(tr("Future frames to display (0 to +3)"));
    connect(m_sliderFutureFrames, &QSlider::valueChanged, this, &PixelEditorDialog::onOnionSkinFutureChanged);
    futureCol->addWidget(m_sliderFutureFrames);
    slidersRow->addLayout(futureCol, 1);

    osLayout->addLayout(slidersRow);

    // Opacity / Intensity slider row directly below
    QHBoxLayout *opHeaderLayout = new QHBoxLayout();
    m_lblOpacityTitle = new QLabel(tr("Effect Intensity:"), m_onionSkinGroup);
    m_lblOpacityTitle->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: 600;"));
    opHeaderLayout->addWidget(m_lblOpacityTitle);
    opHeaderLayout->addStretch();
    m_lblOpacity = new QLabel(tr("50%"), m_onionSkinGroup);
    m_lblOpacity->setStyleSheet(QStringLiteral(
        "border: 1px solid palette(mid); border-radius: 3px; padding: 1px 6px; font-size: 10px; font-weight: bold;"
    ));
    opHeaderLayout->addWidget(m_lblOpacity);
    osLayout->addLayout(opHeaderLayout);

    m_sliderOpacity = new QSlider(Qt::Horizontal, m_onionSkinGroup);
    m_sliderOpacity->setRange(0, 100);
    m_sliderOpacity->setValue(50);
    m_sliderOpacity->setToolTip(tr("Global opacity intensity with distance falloff (0% to 100%)"));
    connect(m_sliderOpacity, &QSlider::valueChanged, this, &PixelEditorDialog::onOnionSkinOpacityChanged);
    osLayout->addWidget(m_sliderOpacity);

    // Effect combo
    m_lblEffect = new QLabel(tr("Effect mode:"), m_onionSkinGroup);
    m_lblEffect->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: 600;"));
    osLayout->addWidget(m_lblEffect);

    m_comboEffect = new QComboBox(m_onionSkinGroup);
    m_comboEffect->addItem(tr("Tinted (Blue/Red)"), static_cast<int>(OnionSkinEffect::TintedBlueRed));
    m_comboEffect->addItem(tr("Border Detection (Edge)"), static_cast<int>(OnionSkinEffect::EdgeDetection));
    m_comboEffect->addItem(tr("Red Channel (R)"), static_cast<int>(OnionSkinEffect::ChannelR));
    m_comboEffect->addItem(tr("Green Channel (G)"), static_cast<int>(OnionSkinEffect::ChannelG));
    m_comboEffect->addItem(tr("Blue Channel (B)"), static_cast<int>(OnionSkinEffect::ChannelB));
    m_comboEffect->addItem(tr("Monochrome Silhouette"), static_cast<int>(OnionSkinEffect::Silhouette));
    m_comboEffect->addItem(tr("True Color (Ghost)"), static_cast<int>(OnionSkinEffect::TrueColor));
    connect(m_comboEffect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PixelEditorDialog::onOnionSkinEffectChanged);
    osLayout->addWidget(m_comboEffect);

    layout->addWidget(m_onionSkinGroup);

    // 5. Live Preview (1:1 scale)
    m_prevGroup = new QGroupBox(tr("1:1 Scale Preview"), panel);
    m_prevGroup->setStyleSheet(groupBoxStyle);
    QVBoxLayout *prevLayout = new QVBoxLayout(m_prevGroup);
    prevLayout->setContentsMargins(4, 4, 4, 4);

    m_previewLabel = new QLabel(m_prevGroup);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumHeight(76);
    m_previewLabel->setStyleSheet(QStringLiteral("border: 1px solid palette(mid); border-radius: 4px;"));
    prevLayout->addWidget(m_previewLabel);
    layout->addWidget(m_prevGroup);

    layout->addStretch();

    scrollPanel->setWidget(panel);
    return scrollPanel;
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
        "border: 1px solid palette(mid);"
        "border-radius: 4px;"
        "padding: 3px 6px;"
        "font-family: monospace;"
        "font-size: 11px;"
    ));
    layout->addWidget(m_coordLabel);

    m_hoverColorSwatch = new QLabel(bar);
    m_hoverColorSwatch->setFixedSize(18, 18);
    m_hoverColorSwatch->setStyleSheet(QStringLiteral("border: 1px solid palette(mid); border-radius: 3px; background-color: transparent;"));
    m_hoverColorSwatch->hide();
    layout->addWidget(m_hoverColorSwatch);

    m_colorInfoLabel = new QLabel(QStringLiteral(""), bar);
    m_colorInfoLabel->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px;"));
    layout->addWidget(m_colorInfoLabel);

    m_lblCollisionWarning = new QLabel(bar);
    m_lblCollisionWarning->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: #fef3c7;"
        "  color: #92400e;"
        "  border: 1px solid #f59e0b;"
        "  border-radius: 4px;"
        "  padding: 3px 8px;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "}"
    ));
    m_lblCollisionWarning->hide();
    layout->addWidget(m_lblCollisionWarning);

    m_btnRepackAtlas = new QPushButton(tr("Repack Atlas..."), bar);
    m_btnRepackAtlas->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #f59e0b;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 3px 10px;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #d97706; }"
    ));
    m_btnRepackAtlas->setToolTip(tr("Open interactive Atlas Packing dialog to resolve sprite collisions"));
    m_btnRepackAtlas->hide();
    connect(m_btnRepackAtlas, &QPushButton::clicked, this, [this]() {
        openAtlasPackingDialog(false);
    });
    layout->addWidget(m_btnRepackAtlas);

    layout->addStretch(1);

    const QString btnSecondaryStyle = QStringLiteral(
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  min-width: 80px;"
        "  min-height: 28px;"
        "}"
        "QPushButton:hover { background-color: palette(alternate-base); border-color: palette(highlight); }"
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
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "  border: 1px solid palette(highlight);"
        "  border-radius: 6px;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  min-width: 80px;"
        "  min-height: 28px;"
        "}"
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
    QColor col = ProColorPickerDialog::getColor(initial, this, tr("Select Primary Color"));
    if (col.isValid()) {
        if (m_canvas) {
            m_canvas->setPrimaryColor(col);
        }
        if (m_colorPickerWidget) {
            m_colorPickerWidget->setColor(col);
        }
        addRecentColor(col);
    }
}

void PixelEditorDialog::onSecondarySwatchClicked()
{
    QColor initial = m_canvas ? m_canvas->secondaryColor() : Qt::white;
    QColor col = ProColorPickerDialog::getColor(initial, this, tr("Select Secondary Color"));
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
    updateCollisionWarningUI();
}

void PixelEditorDialog::onCanvasPixelMoved(int x, int y, const QColor &color)
{
    m_coordLabel->setText(tr("X: %1 , Y: %2").arg(x).arg(y));
    if (color.alpha() == 0) {
        if (m_hoverColorSwatch) {
            m_hoverColorSwatch->setStyleSheet(QStringLiteral("border: 1px solid palette(mid); border-radius: 3px; background-color: transparent;"));
            m_hoverColorSwatch->show();
        }
        m_colorInfoLabel->setText(tr("Transparent [alpha: 0]"));
    } else {
        if (m_hoverColorSwatch) {
            m_hoverColorSwatch->setStyleSheet(QStringLiteral("border: 1px solid palette(window-text); border-radius: 3px; background-color: %1;").arg(color.name()));
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

QPoint PixelEditorDialog::visualPivotPos() const
{
    if (!m_canvas || !m_scrollArea || !m_scrollArea->viewport()) return QPoint();
    QPoint widgetPivot(static_cast<int>(std::round(m_canvas->pivotPos().x() * m_canvas->zoom())),
                       static_cast<int>(std::round(m_canvas->pivotPos().y() * m_canvas->zoom())));
    return m_canvas->mapTo(m_scrollArea->viewport(), widgetPivot);
}

bool PixelEditorDialog::isEditingOutsidePolygonAllowed() const
{
    return m_canvas ? m_canvas->allowEditingOutsidePolygon() : true;
}

void PixelEditorDialog::computeAnimationEnvelope(QSize &outSize, QPoint &outPivot, QPoint &outFrameOffset) const
{
    if (!m_document || m_document->frameCount() == 0) {
        outSize = (m_canvas && !m_canvas->image().isNull()) ? m_canvas->image().size() : QSize(32, 32);
        outPivot = QPoint(outSize.width() / 2, outSize.height() / 2);
        outFrameOffset = QPoint(0, 0);
        return;
    }

    QList<int> seq = m_activeSequence;
    if (seq.isEmpty()) {
        seq.reserve(m_document->frameCount());
        for (int i = 0; i < m_document->frameCount(); ++i) {
            seq.append(i);
        }
    }

    int minX = 0;
    int maxX = 0;
    int minY = 0;
    int maxY = 0;
    bool first = true;

    for (int idx : seq) {
        if (idx < 0 || idx >= m_document->frameCount()) continue;

        QSize sz;
        if (m_sessionModifiedFrames.contains(idx)) {
            sz = m_sessionModifiedFrames[idx].size();
        } else {
            sz = m_document->frame(idx).size();
        }
        if (sz.isEmpty()) sz = QSize(32, 32);

        QPoint piv;
        if (idx < m_document->boxes().size()) {
            piv = m_document->box(idx).effectivePivot();
        } else {
            piv = QPoint(sz.width() / 2, sz.height() / 2);
        }

        int left = -piv.x();
        int right = sz.width() - piv.x();
        int top = -piv.y();
        int bottom = sz.height() - piv.y();

        if (first) {
            minX = left;
            maxX = right;
            minY = top;
            maxY = bottom;
            first = false;
        } else {
            minX = std::min(minX, left);
            maxX = std::max(maxX, right);
            minY = std::min(minY, top);
            maxY = std::max(maxY, bottom);
        }
    }

    if (first) {
        outSize = QSize(32, 32);
        outPivot = QPoint(16, 16);
        outFrameOffset = QPoint(0, 0);
        return;
    }

    outSize = QSize(std::max(1, maxX - minX), std::max(1, maxY - minY));
    outPivot = QPoint(-minX, -minY);

    QPoint curPivot(0, 0);
    if (m_currentFrameIndex >= 0 && m_currentFrameIndex < m_document->boxes().size()) {
        curPivot = m_document->box(m_currentFrameIndex).effectivePivot();
    } else if (m_currentFrameIndex >= 0 && m_currentFrameIndex < m_document->frameCount()) {
        QSize curSz = m_document->frame(m_currentFrameIndex).size();
        curPivot = QPoint(curSz.width() / 2, curSz.height() / 2);
    }
    outFrameOffset = outPivot - curPivot;
}

void PixelEditorDialog::loadFrame(int index)
{
    if (!m_document || index < 0 || index >= m_document->frameCount()) return;
    m_currentFrameIndex = index;

    QPolygonF poly;
    bool hasPoly = false;
    if (m_sessionModifiedPolygons.contains(index)) {
        poly = m_sessionModifiedPolygons[index];
        hasPoly = (poly.size() >= 3);
    } else if (index < m_document->boxes().size() && m_document->box(index).polygon.size() >= 3) {
        poly = m_document->box(index).polygon;
        hasPoly = true;
    }

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

    QSize envSize;
    QPoint envPivot;
    QPoint curFrameOffset;
    computeAnimationEnvelope(envSize, envPivot, curFrameOffset);

    m_canvas->setImage(frameImg);
    m_canvas->setPolygonMesh(poly);
    m_canvas->setCanvasEnvelope(envSize, curFrameOffset, envPivot);

    if (m_allowOutsidePolyCheck) {
        m_allowOutsidePolyCheck->blockSignals(true);
        if (hasPoly) {
            m_allowOutsidePolyCheck->setEnabled(true);
            m_allowOutsidePolyCheck->setChecked(false);
            m_allowOutsidePolyCheck->setToolTip(tr("Allow editing pixels outside polygon boundaries (Unchecked: editing outside polygon is disabled)"));
            m_canvas->setAllowEditingOutsidePolygon(false);
        } else {
            m_allowOutsidePolyCheck->setEnabled(false);
            m_allowOutsidePolyCheck->setChecked(false);
            m_allowOutsidePolyCheck->setToolTip(tr("No polygon mesh defined for this frame"));
            m_canvas->setAllowEditingOutsidePolygon(true);
        }
        m_allowOutsidePolyCheck->blockSignals(false);
    }

    if (!m_initialZoomDone) {
        if (m_scrollArea && m_scrollArea->viewport()->width() > 50 && m_scrollArea->viewport()->height() > 50) {
            m_canvas->zoomFit(m_scrollArea->viewport()->size());
        }
        m_initialZoomDone = true;
    }
    updateOnionSkinLayers();

    if (!m_activeSequence.isEmpty() && !m_activeAnimName.isEmpty()) {
        int seqIdx = m_activeSequence.indexOf(m_currentFrameIndex);
        m_frameInfoLabel->setText(tr("%1: Frame %2 / %3  [Global #%4] (%5x%6 px)")
                                  .arg(m_activeAnimName)
                                  .arg(seqIdx >= 0 ? seqIdx + 1 : 1)
                                  .arg(m_activeSequence.size())
                                  .arg(m_currentFrameIndex + 1)
                                  .arg(frameImg.width())
                                  .arg(frameImg.height()));
    } else {
        m_frameInfoLabel->setText(tr("Frame %1 / %2  (%3x%4 px)")
                                  .arg(m_currentFrameIndex + 1)
                                  .arg(m_document->frameCount())
                                  .arg(frameImg.width())
                                  .arg(frameImg.height()));
    }

    updateNavigationButtons();
    updateLivePreview();
    updateCollisionWarningUI();
}

void PixelEditorDialog::populateAnimationCombo()
{
    if (!m_animCombo || !m_document) return;

    m_animCombo->blockSignals(true);
    m_animCombo->clear();

    // 1. "All Frames" option
    m_animCombo->addItem(tr("All Frames (%1)").arg(m_document->frameCount()), QString());

    // 2. Find which animations include m_currentFrameIndex
    QString selectedAnim;
    const auto &anims = m_document->animations();
    for (auto it = anims.constBegin(); it != anims.constEnd(); ++it) {
        const QString &animName = it.key();
        const SpriteAnimation &anim = it.value();
        if (anim.frameIndices.isEmpty()) continue;

        bool containsCurrent = anim.frameIndices.contains(m_currentFrameIndex);
        QString displayLabel = QStringLiteral("%1 (%2 frames)").arg(animName).arg(anim.frameIndices.size());
        m_animCombo->addItem(displayLabel, animName);

        if (containsCurrent) {
            if (selectedAnim.isEmpty()) {
                selectedAnim = animName; // Prefer the first animation containing the sprite
            }
        }
    }

    // Set selection: if the sprite belongs to an animation, pick it; otherwise "All Frames"
    if (!selectedAnim.isEmpty()) {
        int idx = m_animCombo->findData(selectedAnim);
        if (idx >= 0) {
            m_animCombo->setCurrentIndex(idx);
            m_activeAnimName = selectedAnim;
            m_activeSequence = anims[selectedAnim].frameIndices;
        }
    } else {
        m_animCombo->setCurrentIndex(0);
        m_activeAnimName.clear();
        m_activeSequence.clear();
    }

    m_animCombo->blockSignals(false);

    if (m_applyToAllFramesCheck) {
        if (!m_activeAnimName.isEmpty()) {
            m_applyToAllFramesCheck->setText(tr("Apply to all '%1' frames").arg(m_activeAnimName));
            m_applyToAllFramesCheck->setToolTip(tr("Apply drawing edits and filters to all frames of animation '%1'").arg(m_activeAnimName));
        } else {
            m_applyToAllFramesCheck->setText(tr("Apply to all frames"));
            m_applyToAllFramesCheck->setToolTip(tr("Apply edits (drawing, flip, fill, filters) to all frames aligned by pivot"));
        }
    }
}

void PixelEditorDialog::onAnimationFilterChanged(int index)
{
    if (!m_animCombo || !m_document) return;

    QString animName = m_animCombo->itemData(index).toString();
    m_activeAnimName = animName;

    if (animName.isEmpty() || !m_document->hasAnimation(animName)) {
        m_activeSequence.clear();
    } else {
        m_activeSequence = m_document->animation(animName).frameIndices;
    }

    // If current frame is not in the newly selected animation, jump to its first frame
    if (!m_activeSequence.isEmpty() && !m_activeSequence.contains(m_currentFrameIndex)) {
        saveCurrentFrameToSession();
        loadFrame(m_activeSequence.first());
    } else {
        updateNavigationButtons();
        updateOnionSkinLayers();
        if (m_applyToAllFramesCheck) {
            if (!m_activeAnimName.isEmpty()) {
                m_applyToAllFramesCheck->setText(tr("Apply to all '%1' frames").arg(m_activeAnimName));
                m_applyToAllFramesCheck->setToolTip(tr("Apply drawing edits and filters to all frames of animation '%1'").arg(m_activeAnimName));
            } else {
                m_applyToAllFramesCheck->setText(tr("Apply to all frames"));
                m_applyToAllFramesCheck->setToolTip(tr("Apply edits (drawing, flip, fill, filters) to all frames aligned by pivot"));
            }
        }
        if (m_canvas && m_frameInfoLabel) {
            QImage cur = m_canvas->image();
            if (!m_activeSequence.isEmpty() && !m_activeAnimName.isEmpty()) {
                int seqIdx = m_activeSequence.indexOf(m_currentFrameIndex);
                m_frameInfoLabel->setText(tr("%1: Frame %2 / %3  [Global #%4] (%5x%6 px)")
                                          .arg(m_activeAnimName)
                                          .arg(seqIdx >= 0 ? seqIdx + 1 : 1)
                                          .arg(m_activeSequence.size())
                                          .arg(m_currentFrameIndex + 1)
                                          .arg(cur.width())
                                          .arg(cur.height()));
            } else {
                m_frameInfoLabel->setText(tr("Frame %1 / %2  (%3x%4 px)")
                                          .arg(m_currentFrameIndex + 1)
                                          .arg(m_document->frameCount())
                                          .arg(cur.width())
                                          .arg(cur.height()));
            }
        }
    }
}

void PixelEditorDialog::saveCurrentFrameToSession()
{
    if (!m_canvas) return;
    m_sessionModifiedFrames[m_currentFrameIndex] = m_canvas->image();
    if (m_canvas->hasPolygonMesh()) {
        m_sessionModifiedPolygons[m_currentFrameIndex] = m_canvas->polygonMesh();
    }
}

void PixelEditorDialog::onPreviousFrame()
{
    if (!m_document) return;

    if (!m_activeSequence.isEmpty()) {
        int curPos = m_activeSequence.indexOf(m_currentFrameIndex);
        if (curPos > 0) {
            saveCurrentFrameToSession();
            loadFrame(m_activeSequence.at(curPos - 1));
        }
    } else {
        if (m_currentFrameIndex > 0) {
            saveCurrentFrameToSession();
            loadFrame(m_currentFrameIndex - 1);
        }
    }
}

void PixelEditorDialog::onNextFrame()
{
    if (!m_document) return;

    if (!m_activeSequence.isEmpty()) {
        int curPos = m_activeSequence.indexOf(m_currentFrameIndex);
        if (curPos >= 0 && curPos < m_activeSequence.size() - 1) {
            saveCurrentFrameToSession();
            loadFrame(m_activeSequence.at(curPos + 1));
        }
    } else {
        if (m_currentFrameIndex < m_document->frameCount() - 1) {
            saveCurrentFrameToSession();
            loadFrame(m_currentFrameIndex + 1);
        }
    }
}

void PixelEditorDialog::updateNavigationButtons()
{
    if (!m_document) return;

    if (!m_activeSequence.isEmpty()) {
        int curPos = m_activeSequence.indexOf(m_currentFrameIndex);
        m_prevFrameBtn->setEnabled(curPos > 0);
        m_nextFrameBtn->setEnabled(curPos >= 0 && curPos < m_activeSequence.size() - 1);
    } else {
        m_prevFrameBtn->setEnabled(m_currentFrameIndex > 0);
        m_nextFrameBtn->setEnabled(m_currentFrameIndex < m_document->frameCount() - 1);
    }
}

void PixelEditorDialog::onPalettePresetChanged(int index)
{
    PalettePreset preset = static_cast<PalettePreset>(m_paletteCombo->itemData(index).toInt());
    m_currentPalette = getPresetPalette(preset);
    refreshPaletteSwatches();
}

void PixelEditorDialog::onOnionSkinToggled(bool enabled)
{
    if (m_canvas) {
        m_canvas->setOnionSkinEnabled(enabled);
    }
}

void PixelEditorDialog::onAllowOutsidePolygonToggled(bool checked)
{
    if (m_canvas) {
        m_canvas->setAllowEditingOutsidePolygon(checked);
    }
}

void PixelEditorDialog::setApplyToAllFrames(bool enabled)
{
    if (m_applyToAllFramesCheck) {
        m_applyToAllFramesCheck->setChecked(enabled);
    }
}

void PixelEditorDialog::restoreFrameBackup(int frameIndex, const QImage &img, const QPolygonF &poly)
{
    m_sessionModifiedFrames[frameIndex] = img;
    if (poly.size() >= 3) {
        m_sessionModifiedPolygons[frameIndex] = poly;
    } else {
        m_sessionModifiedPolygons.remove(frameIndex);
    }

    if (frameIndex == m_currentFrameIndex && m_canvas) {
        QSize envSize;
        QPoint envPivot;
        QPoint curFrameOffset;
        computeAnimationEnvelope(envSize, envPivot, curFrameOffset);

        m_canvas->setImage(img);
        m_canvas->setPolygonMesh(poly);
        m_canvas->setCanvasEnvelope(envSize, curFrameOffset, envPivot);
    }
}

void PixelEditorDialog::onMultiFrameUndoRedoDone()
{
    updateOnionSkinLayers();
    updateLivePreview();
    updateCollisionWarningUI();
    if (m_canvas && m_frameInfoLabel) {
        QImage cur = m_canvas->image();
        if (!m_activeSequence.isEmpty() && !m_activeAnimName.isEmpty()) {
            int seqIdx = m_activeSequence.indexOf(m_currentFrameIndex);
            m_frameInfoLabel->setText(tr("%1: Frame %2 / %3  [Global #%4] (%5x%6 px)")
                                      .arg(m_activeAnimName)
                                      .arg(seqIdx >= 0 ? seqIdx + 1 : 1)
                                      .arg(m_activeSequence.size())
                                      .arg(m_currentFrameIndex + 1)
                                      .arg(cur.width())
                                      .arg(cur.height()));
        } else {
            m_frameInfoLabel->setText(tr("Frame %1 / %2  (%3x%4 px)")
                                      .arg(m_currentFrameIndex + 1)
                                      .arg(m_document ? m_document->frameCount() : 0)
                                      .arg(cur.width())
                                      .arg(cur.height()));
        }
    }
}

void PixelEditorDialog::onCanvasModificationPushed(const QImage &oldImage, const QImage &newImage,
                                                  const QPolygonF &oldPolygon, const QPolygonF &newPolygon,
                                                  CanvasAction action, QUndoCommand *parentCommand)
{
    m_sessionModifiedFrames[m_currentFrameIndex] = newImage;
    if (newPolygon.size() >= 3) {
        m_sessionModifiedPolygons[m_currentFrameIndex] = newPolygon;
    } else {
        m_sessionModifiedPolygons.remove(m_currentFrameIndex);
    }

    QVector<FrameBackup> backups;
    backups.append({m_currentFrameIndex, oldImage, newImage, oldPolygon, newPolygon});

    if (m_applyToAllFramesCheck && m_applyToAllFramesCheck->isChecked() &&
        m_document && m_document->frameCount() > 1)
    {
        // Determine target frames: if activeSequence is set, use unique sequence indices; otherwise all document frames
        QList<int> targetFrames;
        if (!m_activeSequence.isEmpty()) {
            for (int idx : m_activeSequence) {
                if (idx >= 0 && idx < m_document->frameCount() && !targetFrames.contains(idx)) {
                    targetFrames.append(idx);
                }
            }
        } else {
            for (int i = 0; i < m_document->frameCount(); ++i) {
                targetFrames.append(i);
            }
        }

        // Current frame pivot
        QPoint curPivot(0, 0);
        if (m_currentFrameIndex >= 0 && m_currentFrameIndex < m_document->boxes().size()) {
            curPivot = m_document->box(m_currentFrameIndex).effectivePivot();
        } else {
            curPivot = QPoint(newImage.width() / 2, newImage.height() / 2);
        }

        for (int targetIdx : targetFrames) {
            if (targetIdx == m_currentFrameIndex) continue;

            QImage targetImg;
            if (m_sessionModifiedFrames.contains(targetIdx)) {
                targetImg = m_sessionModifiedFrames[targetIdx];
            } else {
                bool hasPoly = (targetIdx >= 0 && targetIdx < m_document->boxes().size() &&
                                m_document->box(targetIdx).polygon.size() >= 3);
                if (hasPoly) {
                    targetImg = m_document->polygonClippedFrame(targetIdx);
                } else {
                    targetImg = m_document->frame(targetIdx);
                }
            }
            if (targetImg.isNull()) continue;
            targetImg = targetImg.convertToFormat(QImage::Format_ARGB32);

            QPolygonF targetPoly;
            if (m_sessionModifiedPolygons.contains(targetIdx)) {
                targetPoly = m_sessionModifiedPolygons[targetIdx];
            } else if (targetIdx >= 0 && targetIdx < m_document->boxes().size() &&
                       m_document->box(targetIdx).polygon.size() >= 3) {
                targetPoly = m_document->box(targetIdx).polygon;
            }

            const QImage oldTargetImg = targetImg;
            const QPolygonF oldTargetPoly = targetPoly;

            QPoint targetPivot(0, 0);
            if (targetIdx >= 0 && targetIdx < m_document->boxes().size()) {
                targetPivot = m_document->box(targetIdx).effectivePivot();
            } else {
                targetPivot = QPoint(targetImg.width() / 2, targetImg.height() / 2);
            }

            const bool canEditOutside = (!m_allowOutsidePolyCheck || m_allowOutsidePolyCheck->isChecked() || targetPoly.size() < 3);

            QRect targetSelRect;
            bool hasSel = m_canvas && m_canvas->hasSelection();
            if (hasSel) {
                QRect r = m_canvas->selectionRect();
                int srx = r.x() - curPivot.x();
                int sry = r.y() - curPivot.y();
                targetSelRect = QRect(targetPivot.x() + srx, targetPivot.y() + sry, r.width(), r.height()).intersected(targetImg.rect());
            }

            if (action == CanvasAction::FlipHorizontal) {
                if (hasSel) {
                    if (!targetSelRect.isEmpty()) {
                        for (int y = targetSelRect.top(); y <= targetSelRect.bottom(); ++y) {
                            for (int x = 0; x < targetSelRect.width() / 2; ++x) {
                                int leftX = targetSelRect.left() + x;
                                int rightX = targetSelRect.right() - x;
                                QColor temp = targetImg.pixelColor(leftX, y);
                                targetImg.setPixelColor(leftX, y, targetImg.pixelColor(rightX, y));
                                targetImg.setPixelColor(rightX, y, temp);
                            }
                        }
                        if (m_allowOutsidePolyCheck && !m_allowOutsidePolyCheck->isChecked() && targetPoly.size() >= 3) {
                            for (int y = targetSelRect.top(); y <= targetSelRect.bottom(); ++y) {
                                for (int x = targetSelRect.left(); x <= targetSelRect.right(); ++x) {
                                    if (!targetPoly.containsPoint(QPointF(x + 0.5, y + 0.5), Qt::OddEvenFill)) {
                                        targetImg.setPixelColor(x, y, oldTargetImg.pixelColor(x, y));
                                    }
                                }
                            }
                        }
                    }
                } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
                    targetImg = targetImg.flipped(Qt::Horizontal);
#else
                    targetImg = targetImg.mirrored(true, false);
#endif
                    if (targetPoly.size() >= 3) {
                        QPolygonF flipped;
                        flipped.reserve(targetPoly.size());
                        double w = targetImg.width();
                        for (const QPointF &pt : targetPoly) {
                            flipped.append(QPointF(w - pt.x(), pt.y()));
                        }
                        targetPoly = flipped;
                    }
                }
            } else if (action == CanvasAction::FlipVertical) {
                if (hasSel) {
                    if (!targetSelRect.isEmpty()) {
                        for (int x = targetSelRect.left(); x <= targetSelRect.right(); ++x) {
                            for (int y = 0; y < targetSelRect.height() / 2; ++y) {
                                int topY = targetSelRect.top() + y;
                                int botY = targetSelRect.bottom() - y;
                                QColor temp = targetImg.pixelColor(x, topY);
                                targetImg.setPixelColor(x, topY, targetImg.pixelColor(x, botY));
                                targetImg.setPixelColor(x, botY, temp);
                            }
                        }
                        if (m_allowOutsidePolyCheck && !m_allowOutsidePolyCheck->isChecked() && targetPoly.size() >= 3) {
                            for (int y = targetSelRect.top(); y <= targetSelRect.bottom(); ++y) {
                                for (int x = targetSelRect.left(); x <= targetSelRect.right(); ++x) {
                                    if (!targetPoly.containsPoint(QPointF(x + 0.5, y + 0.5), Qt::OddEvenFill)) {
                                        targetImg.setPixelColor(x, y, oldTargetImg.pixelColor(x, y));
                                    }
                                }
                            }
                        }
                    }
                } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
                    targetImg = targetImg.flipped(Qt::Vertical);
#else
                    targetImg = targetImg.mirrored(false, true);
#endif
                    if (targetPoly.size() >= 3) {
                        QPolygonF flipped;
                        flipped.reserve(targetPoly.size());
                        double h = targetImg.height();
                        for (const QPointF &pt : targetPoly) {
                            flipped.append(QPointF(pt.x(), h - pt.y()));
                        }
                        targetPoly = flipped;
                    }
                }
            } else if (action == CanvasAction::Rotate90CW) {
                int oldH = targetImg.height();
                QTransform trans;
                trans.rotate(90.0);
                targetImg = targetImg.transformed(trans);
                if (targetPoly.size() >= 3) {
                    QPolygonF rotPoly;
                    rotPoly.reserve(targetPoly.size());
                    for (const QPointF &pt : targetPoly) {
                        rotPoly.append(QPointF(static_cast<double>(oldH) - pt.y(), pt.x()));
                    }
                    targetPoly = rotPoly;
                }
            } else if (action == CanvasAction::FloodFill) {
                // Command-level flood fill: run flood fill algorithm directly on targetImg
                QPoint seedPoint;
                QColor repColor;
                bool hasSeed = false;
                if (m_canvas && m_canvas->lastActionData().action == CanvasAction::FloodFill && m_canvas->lastActionData().color.isValid()) {
                    seedPoint = m_canvas->lastActionData().pos;
                    repColor = m_canvas->lastActionData().color;
                    hasSeed = true;
                }
                if (!hasSeed) {
                    for (int y = 0; y < newImage.height(); ++y) {
                        for (int x = 0; x < newImage.width(); ++x) {
                            if (oldImage.isNull() || newImage.pixel(x, y) != oldImage.pixel(x, y)) {
                                seedPoint = QPoint(x, y);
                                repColor = QColor(newImage.pixel(x, y));
                                hasSeed = true;
                                break;
                            }
                        }
                        if (hasSeed) break;
                    }
                }

                int dx = seedPoint.x() - curPivot.x();
                int dy = seedPoint.y() - curPivot.y();
                int targetStartX = targetPivot.x() + dx;
                int targetStartY = targetPivot.y() + dy;

                floodFillOnTarget(targetImg, targetStartX, targetStartY, repColor,
                                   canEditOutside, targetPoly, hasSel, targetSelRect);
            } else if (action == CanvasAction::Clear) {
                // Command-level clear: clear selection if present, else clear entire target image
                if (hasSel) {
                    if (!targetSelRect.isEmpty()) {
                        for (int y = targetSelRect.top(); y <= targetSelRect.bottom(); ++y) {
                            for (int x = targetSelRect.left(); x <= targetSelRect.right(); ++x) {
                                if (canEditOutside || targetPoly.containsPoint(QPointF(x + 0.5, y + 0.5), Qt::OddEvenFill)) {
                                    targetImg.setPixelColor(x, y, Qt::transparent);
                                }
                            }
                        }
                    }
                } else {
                    for (int y = 0; y < targetImg.height(); ++y) {
                        for (int x = 0; x < targetImg.width(); ++x) {
                            if (canEditOutside || targetPoly.containsPoint(QPointF(x + 0.5, y + 0.5), Qt::OddEvenFill)) {
                                targetImg.setPixelColor(x, y, Qt::transparent);
                            }
                        }
                    }
                }
            } else if (action == CanvasAction::Pencil || action == CanvasAction::Eraser) {
                const auto &stroke = m_canvas ? m_canvas->lastActionData().stroke : QVector<StrokeSegment>();
                if (!stroke.isEmpty()) {
                    for (const auto &seg : stroke) {
                        int p1x = targetPivot.x() + (seg.p1.x() - curPivot.x());
                        int p1y = targetPivot.y() + (seg.p1.y() - curPivot.y());
                        int p2x = targetPivot.x() + (seg.p2.x() - curPivot.x());
                        int p2y = targetPivot.y() + (seg.p2.y() - curPivot.y());
                        drawBresenhamOnTarget(targetImg, p1x, p1y, p2x, p2y, seg.color,
                                             canEditOutside, targetPoly, hasSel, targetSelRect);
                    }
                } else {
                    // Fallback to pixel diff
                    const int curW = newImage.width();
                    const int curH = newImage.height();
                    const int oldW = oldImage.width();
                    const int oldH = oldImage.height();
                    const int minW = std::min(curW, oldW);

                    for (int y = 0; y < curH; ++y) {
                        if (y < oldH && curW == oldW) {
                            const QRgb *line1 = reinterpret_cast<const QRgb*>(oldImage.constScanLine(y));
                            const QRgb *line2 = reinterpret_cast<const QRgb*>(newImage.constScanLine(y));
                            if (std::memcmp(line1, line2, minW * sizeof(QRgb)) == 0) {
                                continue;
                            }
                        }
                        for (int x = 0; x < curW; ++x) {
                            QRgb newColor = newImage.pixel(x, y);
                            QRgb oldColor = (x < oldW && y < oldH) ? oldImage.pixel(x, y) : 0;
                            if (newColor != oldColor) {
                                int dx = x - curPivot.x();
                                int dy = y - curPivot.y();
                                int tx = targetPivot.x() + dx;
                                int ty = targetPivot.y() + dy;
                                if (tx >= 0 && tx < targetImg.width() && ty >= 0 && ty < targetImg.height()) {
                                    if (canEditOutside || targetPoly.containsPoint(QPointF(tx + 0.5, ty + 0.5), Qt::OddEvenFill)) {
                                        targetImg.setPixel(tx, ty, newColor);
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                // Fallback for Paste, Generic, etc.
                const int curW = newImage.width();
                const int curH = newImage.height();
                const int oldW = oldImage.width();
                const int oldH = oldImage.height();
                const int minW = std::min(curW, oldW);

                for (int y = 0; y < curH; ++y) {
                    if (y < oldH && curW == oldW) {
                        const QRgb *line1 = reinterpret_cast<const QRgb*>(oldImage.constScanLine(y));
                        const QRgb *line2 = reinterpret_cast<const QRgb*>(newImage.constScanLine(y));
                        if (std::memcmp(line1, line2, minW * sizeof(QRgb)) == 0) {
                            continue;
                        }
                    }
                    for (int x = 0; x < curW; ++x) {
                        QRgb newColor = newImage.pixel(x, y);
                        QRgb oldColor = (x < oldW && y < oldH) ? oldImage.pixel(x, y) : 0;
                        if (newColor != oldColor) {
                            int dx = x - curPivot.x();
                            int dy = y - curPivot.y();
                            int tx = targetPivot.x() + dx;
                            int ty = targetPivot.y() + dy;
                            if (tx >= 0 && tx < targetImg.width() && ty >= 0 && ty < targetImg.height()) {
                                if (canEditOutside || targetPoly.containsPoint(QPointF(tx + 0.5, ty + 0.5), Qt::OddEvenFill)) {
                                    targetImg.setPixel(tx, ty, newColor);
                                }
                            }
                        }
                    }
                }
            }

            m_sessionModifiedFrames[targetIdx] = targetImg;
            if (targetPoly.size() >= 3) {
                m_sessionModifiedPolygons[targetIdx] = targetPoly;
            } else {
                m_sessionModifiedPolygons.remove(targetIdx);
            }

            backups.append({targetIdx, oldTargetImg, targetImg, oldTargetPoly, targetPoly});
        }
    }

    if (parentCommand && m_canvas) {
        auto undoFunc = [this, backups]() {
            bool currentFrameAffected = false;
            for (const auto &b : backups) {
                restoreFrameBackup(b.frameIndex, b.oldImage, b.oldPoly);
                if (b.frameIndex == m_currentFrameIndex) {
                    currentFrameAffected = true;
                }
            }
            if (!currentFrameAffected && !backups.isEmpty()) {
                loadFrame(backups[0].frameIndex);
            }
            onMultiFrameUndoRedoDone();
        };

        auto redoFunc = [this, backups]() {
            bool currentFrameAffected = false;
            for (const auto &b : backups) {
                restoreFrameBackup(b.frameIndex, b.newImage, b.newPoly);
                if (b.frameIndex == m_currentFrameIndex) {
                    currentFrameAffected = true;
                }
            }
            if (!currentFrameAffected && !backups.isEmpty()) {
                loadFrame(backups[0].frameIndex);
            }
            onMultiFrameUndoRedoDone();
        };

        m_canvas->setCommandCustomUndoRedo(parentCommand, undoFunc, redoFunc);
    }

    updateOnionSkinLayers();
    updateLivePreview();
    updateCollisionWarningUI();
}

void PixelEditorDialog::onOnionSkinPastChanged(int val)
{
    if (m_lblPastFrames) {
        m_lblPastFrames->setText(QString::number(val));
    }
    updateOnionSkinLayers();
}

void PixelEditorDialog::onOnionSkinFutureChanged(int val)
{
    if (m_lblFutureFrames) {
        m_lblFutureFrames->setText(QStringLiteral("%1%2").arg(val > 0 ? QStringLiteral("+") : QString()).arg(val));
    }
    updateOnionSkinLayers();
}

void PixelEditorDialog::onOnionSkinOpacityChanged(int val)
{
    if (m_lblOpacity) {
        m_lblOpacity->setText(QStringLiteral("%1%").arg(val));
    }
    if (m_canvas) {
        m_canvas->setOnionSkinOpacity(val);
    }
}

void PixelEditorDialog::onOnionSkinEffectChanged(int index)
{
    Q_UNUSED(index);
    if (m_canvas && m_comboEffect) {
        auto effect = static_cast<OnionSkinEffect>(m_comboEffect->currentData().toInt());
        m_canvas->setOnionSkinEffect(effect);
    }
}

void PixelEditorDialog::updateOnionSkinLayers()
{
    if (!m_document || !m_canvas) return;

    QVector<OnionSkinLayer> layers;
    int totalFrames = m_document->frameCount();
    if (totalFrames <= 1) {
        m_canvas->setOnionSkinLayers(layers);
        return;
    }

    // Helper: Clip image with its polygon mesh if applicable
    auto getClippedFrameImage = [this](int frameIdx) -> QImage {
        const bool hasPoly = (frameIdx >= 0 && frameIdx < m_document->boxes().size() &&
                              m_document->box(frameIdx).polygon.size() >= 3);
        if (m_sessionModifiedFrames.contains(frameIdx)) {
            QImage baseImage = m_sessionModifiedFrames[frameIdx];
            if (!hasPoly || baseImage.isNull()) {
                return baseImage;
            }
            // Clip modified frame image using polygon mask
            const SpriteBox &b = m_document->box(frameIdx);
            QImage mask(baseImage.size(), QImage::Format_ARGB32_Premultiplied);
            mask.fill(Qt::transparent);
            {
                QPainter mp(&mask);
                mp.setRenderHint(QPainter::Antialiasing, false);
                mp.setBrush(Qt::white);
                mp.setPen(QPen(Qt::white, 1.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
                mp.drawPolygon(b.polygon);
            }
            QImage clipped = baseImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            {
                QPainter p(&clipped);
                p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                p.drawImage(0, 0, mask);
            }
            return clipped.convertToFormat(QImage::Format_ARGB32);
        } else {
            return hasPoly ? m_document->polygonClippedFrame(frameIdx) : m_document->frame(frameIdx);
        }
    };

    // Calculate current frame's effective pivot
    QPoint curPivot(0, 0);
    if (m_currentFrameIndex >= 0 && m_currentFrameIndex < m_document->boxes().size()) {
        curPivot = m_document->box(m_currentFrameIndex).effectivePivot();
    } else if (m_currentFrameIndex >= 0 && m_currentFrameIndex < totalFrames) {
        QImage curImg = m_canvas->image();
        curPivot = QPoint(curImg.width() / 2, curImg.height() / 2);
    }

    // Canvas anchor pivot
    QPoint anchorPivot = m_canvas ? m_canvas->pivotPos() : curPivot;

    int pastCount = m_sliderPastFrames ? std::abs(m_sliderPastFrames->value()) : 1;
    for (int i = 1; i <= pastCount; ++i) {
        int frameIdx = -1;
        if (!m_activeSequence.isEmpty()) {
            int curSeqPos = m_activeSequence.indexOf(m_currentFrameIndex);
            int targetSeqPos = curSeqPos - i;
            if (targetSeqPos >= 0 && targetSeqPos < m_activeSequence.size()) {
                frameIdx = m_activeSequence.at(targetSeqPos);
            }
        } else {
            int candidate = m_currentFrameIndex - i;
            if (candidate >= 0 && candidate < totalFrames) {
                frameIdx = candidate;
            }
        }

        if (frameIdx >= 0 && frameIdx < totalFrames) {
            QImage img = getClippedFrameImage(frameIdx);
            QPoint layerPivot(img.width() / 2, img.height() / 2);
            if (frameIdx < m_document->boxes().size()) {
                layerPivot = m_document->box(frameIdx).effectivePivot();
            }
            QPoint alignOffset = anchorPivot - layerPivot;
            layers.append({img, -i, alignOffset});
        }
    }

    int futureCount = m_sliderFutureFrames ? m_sliderFutureFrames->value() : 0;
    for (int i = 1; i <= futureCount; ++i) {
        int frameIdx = -1;
        if (!m_activeSequence.isEmpty()) {
            int curSeqPos = m_activeSequence.indexOf(m_currentFrameIndex);
            int targetSeqPos = curSeqPos + i;
            if (targetSeqPos >= 0 && targetSeqPos < m_activeSequence.size()) {
                frameIdx = m_activeSequence.at(targetSeqPos);
            }
        } else {
            int candidate = m_currentFrameIndex + i;
            if (candidate >= 0 && candidate < totalFrames) {
                frameIdx = candidate;
            }
        }

        if (frameIdx >= 0 && frameIdx < totalFrames) {
            QImage img = getClippedFrameImage(frameIdx);
            QPoint layerPivot(img.width() / 2, img.height() / 2);
            if (frameIdx < m_document->boxes().size()) {
                layerPivot = m_document->box(frameIdx).effectivePivot();
            }
            QPoint alignOffset = anchorPivot - layerPivot;
            layers.append({img, i, alignOffset});
        }
    }

    m_canvas->setOnionSkinLayers(layers);
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
            "  border: 1px solid palette(mid);"
            "  border-radius: 4px;"
            "}"
            "QPushButton:hover {"
            "  border: 2px solid palette(highlight);"
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
            "  border: 1px solid palette(mid);"
            "  border-radius: 4px;"
            "}"
            "QPushButton:hover {"
            "  border: 2px solid palette(highlight);"
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
    return ColorPalettePresets::getPresetPalette(static_cast<ColorPalettePresets::Preset>(preset));
}

bool PixelEditorDialog::hasAtlasCollision() const
{
    return checkAtlasPolygonCollision(nullptr, nullptr);
}

QPolygonF PixelEditorDialog::sessionModifiedPolygon(int frameIndex) const
{
    if (m_sessionModifiedPolygons.contains(frameIndex)) {
        return m_sessionModifiedPolygons.value(frameIndex);
    }
    if (frameIndex == m_currentFrameIndex && m_canvas && m_canvas->hasPolygonMesh()) {
        return m_canvas->polygonMesh();
    }
    if (m_document && frameIndex >= 0 && frameIndex < m_document->boxes().size()) {
        return m_document->box(frameIndex).polygon;
    }
    return QPolygonF();
}

bool PixelEditorDialog::checkAtlasPolygonCollision(QString *outDetails, QList<int> *outCollidingIndices) const
{
    if (!m_document || m_document->frameCount() <= 1) return false;

    struct FrameAtlasGeometry {
        int index = -1;
        QRect boxRect;
        QPolygonF localPoly;
        QPolygonF atlasPoly;
        bool hasPoly = false;
        bool isModified = false;
    };

    QVector<FrameAtlasGeometry> geoms(m_document->frameCount());
    for (int i = 0; i < m_document->frameCount(); ++i) {
        geoms[i].index = i;
        geoms[i].boxRect = m_document->box(i).rect;

        if (i == m_currentFrameIndex && m_canvas) {
            geoms[i].localPoly = m_canvas->polygonMesh();
            geoms[i].hasPoly = m_canvas->hasPolygonMesh();
            geoms[i].isModified = (m_sessionModifiedPolygons.contains(i) ||
                                  (m_sessionModifiedFrames.contains(i) && m_sessionModifiedFrames[i] != m_document->frame(i)) ||
                                  (geoms[i].localPoly != m_document->box(i).polygon));
            if (!m_canvas->image().isNull()) {
                geoms[i].boxRect.setSize(m_canvas->image().size());
            }
        } else if (m_sessionModifiedPolygons.contains(i)) {
            geoms[i].localPoly = m_sessionModifiedPolygons[i];
            geoms[i].hasPoly = (geoms[i].localPoly.size() >= 3);
            geoms[i].isModified = true;
            if (m_sessionModifiedFrames.contains(i) && !m_sessionModifiedFrames[i].isNull()) {
                geoms[i].boxRect.setSize(m_sessionModifiedFrames[i].size());
            }
        } else {
            geoms[i].localPoly = m_document->box(i).polygon;
            geoms[i].hasPoly = (m_document->box(i).hasPolygonMesh && geoms[i].localPoly.size() >= 3);
            geoms[i].isModified = (m_sessionModifiedFrames.contains(i) && m_sessionModifiedFrames[i] != m_document->frame(i));
            if (m_sessionModifiedFrames.contains(i) && !m_sessionModifiedFrames[i].isNull()) {
                geoms[i].boxRect.setSize(m_sessionModifiedFrames[i].size());
            }
        }

        if (geoms[i].hasPoly) {
            geoms[i].atlasPoly = geoms[i].localPoly.translated(geoms[i].boxRect.topLeft());
        } else {
            geoms[i].atlasPoly = QPolygonF(QRectF(geoms[i].boxRect));
        }
    }

    auto calcArea = [](const QPolygonF &poly) -> double {
        if (poly.size() < 3) return 0.0;
        double a = 0.0;
        for (int p = 0; p < poly.size(); ++p) {
            const QPointF &p1 = poly[p];
            const QPointF &p2 = poly[(p + 1) % poly.size()];
            a += (p1.x() * p2.y() - p2.x() * p1.y());
        }
        return std::abs(a) * 0.5;
    };

    bool collisionFound = false;
    QStringList conflictLines;
    QSet<int> collidingIndices;

    for (int i = 0; i < geoms.size(); ++i) {
        if (!geoms[i].isModified) continue;

        for (int j = 0; j < geoms.size(); ++j) {
            if (i == j) continue;

            if (!geoms[i].atlasPoly.boundingRect().intersects(geoms[j].atlasPoly.boundingRect())) {
                continue;
            }

            QPolygonF isect = geoms[i].atlasPoly.intersected(geoms[j].atlasPoly);
            if (!isect.isEmpty() && calcArea(isect) > 0.5) {
                collisionFound = true;
                collidingIndices.insert(i);
                collidingIndices.insert(j);
                conflictLines.append(tr("• Frame %1 (%2) collides with Frame %3 (%4)")
                                     .arg(i + 1)
                                     .arg(geoms[i].hasPoly ? tr("polygon") : tr("box"))
                                     .arg(j + 1)
                                     .arg(geoms[j].hasPoly ? tr("polygon") : tr("box")));
            }
        }
    }

    if (outDetails) {
        *outDetails = conflictLines.join(QLatin1Char('\n'));
    }
    if (outCollidingIndices) {
        *outCollidingIndices = collidingIndices.values();
    }
    return collisionFound;
}

void PixelEditorDialog::updateCollisionWarningUI()
{
    if (!m_lblCollisionWarning) return;
    QString details;
    bool hasCollision = checkAtlasPolygonCollision(&details);
    if (hasCollision) {
        m_lblCollisionWarning->setText(tr("⚠️ Atlas collision: repack required on validation"));
        m_lblCollisionWarning->setToolTip(details);
        m_lblCollisionWarning->show();
        if (m_btnRepackAtlas) m_btnRepackAtlas->show();
    } else {
        m_lblCollisionWarning->hide();
        if (m_btnRepackAtlas) m_btnRepackAtlas->hide();
    }
}

bool PixelEditorDialog::openAtlasPackingDialog(bool nonInteractive)
{
    if (!m_document || m_document->frameCount() == 0) {
        return false;
    }

    // Ensure pending session modifications are committed to document first so the packing dialog operates on updated sprites
    saveCurrentFrameToSession();
    if (!m_sessionModifiedFrames.isEmpty() || !m_sessionModifiedPolygons.isEmpty()) {
        if (m_docUndoStack) {
            m_docUndoStack->push(new EditSpritePixelsCommand(m_document, m_sessionModifiedFrames, m_sessionModifiedPolygons));
        } else {
            EditSpritePixelsCommand cmd(m_document, m_sessionModifiedFrames, m_sessionModifiedPolygons);
            cmd.redo();
        }
        m_sessionModifiedFrames.clear();
        m_sessionModifiedPolygons.clear();
        updateCollisionWarningUI();
    }

    FilterRegistry &reg = FilterRegistry::instance();
    reg.initDefaultFilters();

    FilterPlugin *plugin = nullptr;
    bool hasPolygons = false;
    for (int i = 0; i < m_document->frameCount(); ++i) {
        if (i < m_document->boxes().size() && m_document->box(i).hasPolygonMesh && m_document->box(i).polygon.size() >= 3) {
            hasPolygons = true;
            break;
        }
    }

    if (hasPolygons) {
        plugin = reg.findFilter(QStringLiteral("tight_polygon_packing"));
    }
    if (!plugin) {
        plugin = reg.findFilter(QStringLiteral("atlas_packing"));
    }

    if (!plugin) {
        QMessageBox::warning(this, tr("Atlas Packing"),
                             tr("Atlas packing plugin not found. Changes were saved without repacking."));
        return false;
    }

    FilterDialogBase *dlg = plugin->createDialog(m_document, m_docUndoStack, this);
    if (!dlg) {
        return false;
    }

    int res = QDialog::Rejected;
    if (nonInteractive) {
        dlg->accept();
        res = QDialog::Accepted;
    } else {
        res = dlg->exec();
    }
    dlg->deleteLater();

    if (res == QDialog::Accepted) {
        loadFrame(m_currentFrameIndex);
        updateCollisionWarningUI();
        return true;
    }
    return false;
}

bool PixelEditorDialog::performAtlasRepack(const QMap<int, QImage> &modifiedFrames,
                                           const QMap<int, QPolygonF> &modifiedPolygons)
{
    if (!modifiedFrames.isEmpty() || !modifiedPolygons.isEmpty()) {
        if (m_docUndoStack) {
            m_docUndoStack->push(new EditSpritePixelsCommand(m_document, modifiedFrames, modifiedPolygons));
        } else {
            EditSpritePixelsCommand cmd(m_document, modifiedFrames, modifiedPolygons);
            cmd.redo();
        }
        m_sessionModifiedFrames.clear();
        m_sessionModifiedPolygons.clear();
    }
    return openAtlasPackingDialog(true);
}

bool PixelEditorDialog::applyChanges()
{
    saveCurrentFrameToSession();
    if (m_sessionModifiedFrames.isEmpty() && m_sessionModifiedPolygons.isEmpty()) {
        return true;
    }
    if (!m_document) return false;

    // Filter out unmodified frames
    QMap<int, QImage> actualFrameChanges;
    for (auto it = m_sessionModifiedFrames.constBegin(); it != m_sessionModifiedFrames.constEnd(); ++it) {
        if (m_document->frame(it.key()) != it.value()) {
            actualFrameChanges[it.key()] = it.value();
        }
    }

    QMap<int, QPolygonF> actualPolyChanges;
    for (auto it = m_sessionModifiedPolygons.constBegin(); it != m_sessionModifiedPolygons.constEnd(); ++it) {
        if (it.key() >= 0 && it.key() < m_document->boxes().size()) {
            if (m_document->box(it.key()).polygon != it.value()) {
                actualPolyChanges[it.key()] = it.value();
            }
        }
    }

    if (actualFrameChanges.isEmpty() && actualPolyChanges.isEmpty()) {
        m_sessionModifiedFrames.clear();
        m_sessionModifiedPolygons.clear();
        updateCollisionWarningUI();
        return true;
    }

    bool shouldRepack = false;

    // Check collision with other polygons/boxes in atlas
    QString conflictDetails;
    QList<int> collidingIndices;
    if (checkAtlasPolygonCollision(&conflictDetails, &collidingIndices)) {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("Atlas Collision Detected"));
        msgBox.setText(tr("Polygon or frame modifications cause collision with other sprites in the atlas:\n\n"
                          "%1\n\n"
                          "An atlas repack must be performed upon validation to resolve overlapping frames.\n\n"
                          "Would you like to repack the atlas now?")
                       .arg(conflictDetails));
        QPushButton *btnRepack = msgBox.addButton(tr("Repack Atlas Now"), QMessageBox::AcceptRole);
        QPushButton *btnProceed = msgBox.addButton(tr("Validate Without Repacking"), QMessageBox::DestructiveRole);
        Q_UNUSED(btnProceed);
        QPushButton *btnCancel = msgBox.addButton(QMessageBox::Cancel);
        msgBox.setDefaultButton(btnRepack);
        msgBox.exec();

        if (msgBox.clickedButton() == btnCancel) {
            return false;
        }

        if (msgBox.clickedButton() == btnRepack) {
            shouldRepack = true;
        }
    }

    // Commit frame & polygon changes first so document state is completely up-to-date
    if (m_docUndoStack) {
        m_docUndoStack->push(new EditSpritePixelsCommand(m_document, actualFrameChanges, actualPolyChanges));
    } else {
        EditSpritePixelsCommand cmd(m_document, actualFrameChanges, actualPolyChanges);
        cmd.redo();
    }

    m_sessionModifiedFrames.clear();
    m_sessionModifiedPolygons.clear();
    updateCollisionWarningUI();

    if (shouldRepack) {
        openAtlasPackingDialog(false);
    }

    loadFrame(m_currentFrameIndex);
    return true;
}

void PixelEditorDialog::onApplyClicked()
{
    applyChanges();
}

void PixelEditorDialog::onOkClicked()
{
    if (applyChanges()) {
        accept();
    }
}

void PixelEditorDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void PixelEditorDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_PageUp || event->key() == Qt::Key_BracketLeft) {
        onPreviousFrame();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_PageDown || event->key() == Qt::Key_BracketRight) {
        onNextFrame();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void PixelEditorDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!m_firstShowFitDone) {
        m_firstShowFitDone = true;
        if (m_canvas && m_scrollArea && m_scrollArea->viewport()->width() > 50 && m_scrollArea->viewport()->height() > 50) {
            m_canvas->zoomFit(m_scrollArea->viewport()->size());
        }
    }
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

    if (m_animLabel) m_animLabel->setText(tr("Animation:"));
    if (m_animCombo) m_animCombo->setToolTip(tr("Filter navigation and onion skinning to animation"));

    if (m_document && m_canvas && m_frameInfoLabel) {
        if (!m_activeSequence.isEmpty() && !m_activeAnimName.isEmpty()) {
            int seqIdx = m_activeSequence.indexOf(m_currentFrameIndex);
            m_frameInfoLabel->setText(tr("%1: Frame %2 / %3  [Global #%4] (%5x%6 px)")
                                      .arg(m_activeAnimName)
                                      .arg(seqIdx >= 0 ? seqIdx + 1 : 1)
                                      .arg(m_activeSequence.size())
                                      .arg(m_currentFrameIndex + 1)
                                      .arg(m_canvas->image().width())
                                      .arg(m_canvas->image().height()));
        } else {
            m_frameInfoLabel->setText(tr("Frame %1 / %2  (%3x%4 px)")
                                      .arg(m_currentFrameIndex + 1)
                                      .arg(m_document->frameCount())
                                      .arg(m_canvas->image().width())
                                      .arg(m_canvas->image().height()));
        }
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
    if (m_btnShowPivot) {
        m_btnShowPivot->setText(QStringLiteral("⌖ ") + tr("Pivot"));
        m_btnShowPivot->setToolTip(tr("Show / Hide Pivot Anchor Marker"));
    }
    if (m_allowOutsidePolyCheck) {
        m_allowOutsidePolyCheck->setText(tr("Edit outside polygon"));
        bool hasPoly = (m_document && m_currentFrameIndex >= 0 && m_currentFrameIndex < m_document->boxes().size() &&
                        m_document->box(m_currentFrameIndex).polygon.size() >= 3);
        if (hasPoly) {
            m_allowOutsidePolyCheck->setToolTip(tr("Allow editing pixels outside polygon boundaries (Unchecked: editing outside polygon is disabled)"));
        } else {
            m_allowOutsidePolyCheck->setToolTip(tr("No polygon mesh defined for this frame"));
        }
    }
    if (m_applyToAllFramesCheck) {
        if (!m_activeAnimName.isEmpty()) {
            m_applyToAllFramesCheck->setText(tr("Apply to all '%1' frames").arg(m_activeAnimName));
            m_applyToAllFramesCheck->setToolTip(tr("Apply drawing edits and filters to all frames of animation '%1'").arg(m_activeAnimName));
        } else {
            m_applyToAllFramesCheck->setText(tr("Apply to all frames"));
            m_applyToAllFramesCheck->setToolTip(tr("Apply edits (drawing, flip, fill, filters) to all frames aligned by pivot"));
        }
    }
    if (m_btnFilters) {
        m_btnFilters->setText(tr("✨ Filters ▾"));
        m_btnFilters->setToolTip(tr("Apply image and color filters (Despill, Outline, Rescale, Palette...)"));
        if (m_btnFilters->menu()) {
            populateFiltersMenu(m_btnFilters->menu());
        }
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
    if (m_colorsGroup) m_colorsGroup->setTitle(tr("Color Studio && Harmonies"));
    if (m_primarySwatchBtn) m_primarySwatchBtn->setToolTip(tr("Primary Color (Click to open Pro Color Picker)"));
    if (m_secondarySwatchBtn) m_secondarySwatchBtn->setToolTip(tr("Secondary Color (Click to open Pro Color Picker)"));
    if (m_swapBtn) m_swapBtn->setToolTip(tr("Swap Colors (X)"));
    if (m_btnPickColor) m_btnPickColor->setText(tr("⛶ Pop-out..."));
    if (m_recentLabel) m_recentLabel->setText(tr("Recent:"));
    if (m_palLabel) m_palLabel->setText(tr("Preset:"));
    if (m_btnSampleFrame) {
        m_btnSampleFrame->setText(tr("Sample Frame"));
        m_btnSampleFrame->setToolTip(tr("Extract all unique colors from current sprite frame"));
    }

    if (m_paletteCombo) {
        int curIdx = m_paletteCombo->currentIndex();
        m_paletteCombo->blockSignals(true);
        for (int i = 0; i < m_paletteCombo->count(); ++i) {
            ColorPalettePresets::Preset p = static_cast<ColorPalettePresets::Preset>(m_paletteCombo->itemData(i).toInt());
            m_paletteCombo->setItemText(i, ColorPalettePresets::getPresetName(p));
        }
        m_paletteCombo->setCurrentIndex(curIdx);
        m_paletteCombo->blockSignals(false);
    }

    // Onion Skinning
    if (m_onionSkinGroup) m_onionSkinGroup->setTitle(tr("Onion Skinning"));
    if (m_onionSkinCheck) m_onionSkinCheck->setText(tr("Enable Onion Skin"));
    if (m_lblPastTitle) m_lblPastTitle->setText(tr("Past:"));
    if (m_lblPastFrames && m_sliderPastFrames) m_lblPastFrames->setText(QString::number(m_sliderPastFrames->value()));
    if (m_lblFutureTitle) m_lblFutureTitle->setText(tr("Future:"));
    if (m_lblFutureFrames && m_sliderFutureFrames) {
        int v = m_sliderFutureFrames->value();
        m_lblFutureFrames->setText(QStringLiteral("%1%2").arg(v > 0 ? QStringLiteral("+") : QString()).arg(v));
    }
    if (m_lblOpacityTitle) m_lblOpacityTitle->setText(tr("Effect Intensity:"));
    if (m_lblOpacity && m_sliderOpacity) m_lblOpacity->setText(QStringLiteral("%1%").arg(m_sliderOpacity->value()));
    if (m_lblEffect) m_lblEffect->setText(tr("Effect mode:"));
    if (m_comboEffect) {
        int curIdx = m_comboEffect->currentIndex();
        m_comboEffect->blockSignals(true);
        m_comboEffect->setItemText(0, tr("Tinted (Blue/Red)"));
        m_comboEffect->setItemText(1, tr("Border Detection (Edge)"));
        m_comboEffect->setItemText(2, tr("Red Channel (R)"));
        m_comboEffect->setItemText(3, tr("Green Channel (G)"));
        m_comboEffect->setItemText(4, tr("Blue Channel (B)"));
        m_comboEffect->setItemText(5, tr("Monochrome Silhouette"));
        m_comboEffect->setItemText(6, tr("True Color (Ghost)"));
        m_comboEffect->setCurrentIndex(curIdx);
        m_comboEffect->blockSignals(false);
    }

    if (m_prevGroup) m_prevGroup->setTitle(tr("1:1 Scale Preview"));

    if (m_zoomLabel && m_canvas) {
        m_zoomLabel->setText(tr("Zoom: %1%").arg(static_cast<int>(std::round(m_canvas->zoom() * 100))));
    }

    // Bottom bar buttons
    if (m_btnRepackAtlas) m_btnRepackAtlas->setText(tr("Repack Atlas..."));
    if (m_cancelBtn) m_cancelBtn->setText(tr("Cancel"));
    if (m_applyBtn) m_applyBtn->setText(tr("Apply"));
    if (m_okBtn) m_okBtn->setText(tr("OK"));

    updateNavigationButtons();
}



void PixelEditorDialog::populateFiltersMenu(QMenu *menu)
{
    if (!menu) return;
    menu->clear();

    FilterRegistry &reg = FilterRegistry::instance();
    reg.initDefaultFilters();

    QList<FilterPlugin*> eligible;
    for (FilterPlugin *f : reg.filters()) {
        if (f && !f->isAtlasModifier()) {
            eligible.append(f);
        }
    }

    if (eligible.isEmpty()) {
        QAction *emptyAct = menu->addAction(tr("No filters available"));
        emptyAct->setEnabled(false);
        return;
    }

    QStringList cats;
    for (FilterPlugin *f : eligible) {
        if (!cats.contains(f->category())) {
            cats.append(f->category());
        }
    }

    for (int i = 0; i < cats.size(); ++i) {
        const QString &cat = cats[i];
        if (i > 0) {
            menu->addSeparator();
        }
        menu->addSection(cat);

        for (FilterPlugin *f : eligible) {
            if (f->category() == cat) {
                QAction *act = menu->addAction(f->name());
                act->setToolTip(f->description());
                if (!f->icon().isNull()) {
                    act->setIcon(f->icon());
                }
                connect(act, &QAction::triggered, this, [this, f]() {
                    applyFilterToSession(f);
                });
            }
        }
    }
}

void PixelEditorDialog::applyFilterToSession(FilterPlugin *filter)
{
    if (!filter || !m_canvas) return;

    // 1. Determine target frames: current frame vs entire animation
    QList<int> targetFrames;
    bool allAnim = isApplyToAllFramesEnabled();
    if (allAnim && m_document && m_document->frameCount() > 1) {
        if (!m_activeSequence.isEmpty()) {
            for (int idx : m_activeSequence) {
                if (idx >= 0 && idx < m_document->frameCount() && !targetFrames.contains(idx)) {
                    targetFrames.append(idx);
                }
            }
        } else {
            for (int i = 0; i < m_document->frameCount(); ++i) {
                targetFrames.append(i);
            }
        }
    } else {
        targetFrames.append(m_currentFrameIndex);
    }

    if (targetFrames.isEmpty()) return;

    // 2. Collect pristine current images for target frames
    QList<QImage> targetImages;
    targetImages.reserve(targetFrames.size());
    for (int frameIdx : targetFrames) {
        QImage img;
        if (frameIdx == m_currentFrameIndex) {
            img = m_canvas->image();
        } else if (m_sessionModifiedFrames.contains(frameIdx)) {
            img = m_sessionModifiedFrames[frameIdx];
        } else if (m_document) {
            bool hasPoly = (frameIdx >= 0 && frameIdx < m_document->boxes().size() &&
                            m_document->box(frameIdx).polygon.size() >= 3);
            img = hasPoly ? m_document->polygonClippedFrame(frameIdx) : m_document->frame(frameIdx);
        }
        if (img.isNull()) {
            img = QImage(32, 32, QImage::Format_ARGB32);
            img.fill(Qt::transparent);
        }
        targetImages.append(img.convertToFormat(QImage::Format_ARGB32));
    }

    // Pack into temp atlas strip with 8px margin
    const int margin = 8;
    int totalWidth = margin;
    int maxHeight = 0;
    for (const QImage &img : targetImages) {
        totalWidth += img.width() + margin;
        if (img.height() > maxHeight) {
            maxHeight = img.height();
        }
    }
    maxHeight += margin * 2;

    QImage tempAtlas(totalWidth, maxHeight, QImage::Format_ARGB32);
    tempAtlas.fill(Qt::transparent);

    QPainter painter(&tempAtlas);
    QList<QImage> tempFrames;
    QList<SpriteBox> tempBoxes;
    int curX = margin;

    for (int i = 0; i < targetImages.size(); ++i) {
        const QImage &img = targetImages[i];
        painter.drawImage(curX, margin, img);
        QRect r(curX, margin, img.width(), img.height());
        SpriteBox b(r);
        b.index = i;
        tempBoxes.append(b);
        tempFrames.append(img);
        curX += img.width() + margin;
    }
    painter.end();

    SpriteDocument tempDoc;
    tempDoc.setAtlas(tempAtlas);
    tempDoc.setFrames(tempFrames, tempBoxes);

    // 3. Open filter dialog
    FilterDialogBase *dlg = filter->createDialog(&tempDoc, nullptr, this);
    if (!dlg) return;

    int res = dlg->exec();
    delete dlg;

    if (res != QDialog::Accepted) {
        return; // User canceled
    }

    // 4. Extract filtered results
    QVector<FrameBackup> backups;
    backups.reserve(targetFrames.size());

    for (int i = 0; i < targetFrames.size(); ++i) {
        int frameIdx = targetFrames[i];
        QImage oldImg = targetImages[i];
        QPolygonF oldPoly = sessionModifiedPolygon(frameIdx);

        // Get new image from tempDoc
        QImage newImg;
        if (i < tempDoc.frameCount()) {
            newImg = tempDoc.frame(i);
        } else if (i < tempDoc.boxes().size()) {
            QRect r = tempDoc.box(i).rect.intersected(tempDoc.atlas().rect());
            newImg = tempDoc.atlas().copy(r);
        } else {
            newImg = oldImg;
        }

        QPolygonF newPoly = oldPoly;
        if (newImg.size() != oldImg.size() && oldPoly.size() >= 3) {
            double sx = static_cast<double>(newImg.width()) / std::max(1, oldImg.width());
            double sy = static_cast<double>(newImg.height()) / std::max(1, oldImg.height());
            QPolygonF scaledPoly;
            for (const QPointF &pt : oldPoly) {
                scaledPoly.append(QPointF(pt.x() * sx, pt.y() * sy));
            }
            newPoly = scaledPoly;
        }

        backups.append({frameIdx, oldImg, newImg, oldPoly, newPoly});
    }

    // 5. Apply changes and create undo command on canvas undo stack
    auto applyState = [this, backups](bool isRedo) {
        for (const auto &b : backups) {
            const QImage &img = isRedo ? b.newImage : b.oldImage;
            const QPolygonF &poly = isRedo ? b.newPoly : b.oldPoly;
            restoreFrameBackup(b.frameIndex, img, poly);
        }
        onMultiFrameUndoRedoDone();
    };

    // Execute immediately
    applyState(true);

    // Push custom command for Undo/Redo
    if (m_canvas && m_canvas->undoStack()) {
        class FilterUndoCmd : public QUndoCommand {
        public:
            FilterUndoCmd(const std::function<void(bool)> &func, const QString &name)
                : QUndoCommand(name), m_func(func) {}
            void undo() override { m_func(false); }
            void redo() override {
                if (m_first) { m_first = false; return; }
                m_func(true);
            }
        private:
            std::function<void(bool)> m_func;
            bool m_first = true;
        };

        QString cmdText = allAnim
            ? tr("Apply %1 to Animation").arg(filter->name())
            : tr("Apply %1 to Frame").arg(filter->name());

        m_canvas->undoStack()->push(new FilterUndoCmd(applyState, cmdText));
    }
}
