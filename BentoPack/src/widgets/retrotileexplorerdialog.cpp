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

#include "widgets/retrotileexplorerdialog.h"
#include "controller/projectcontroller.h"
#include "image/spritedetector.h"
#include "image/colorpalettepresets.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QCheckBox>
#include <QScrollArea>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

// Interactive 16-color palette strip widget inspired by SNESTilesKitten
class PaletteStripWidget : public QWidget
{
public:
    explicit PaletteStripWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedHeight(26);
        setToolTip(tr("Active 16-color palette swatches (indices 0 to 15)"));
    }

    void setPaletteColors(const QVector<QRgb> &colors, bool transp0)
    {
        m_colors = colors;
        m_transp0 = transp0;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        int count = std::max(1, static_cast<int>(m_colors.size()));
        int w = width();
        int h = height();
        int boxW = std::max(4, w / count);

        for (int i = 0; i < count; ++i) {
            QRect r(i * boxW, 0, (i == count - 1) ? (w - i * boxW) : boxW, h);
            if (i == 0 && m_transp0) {
                // Checkerboard pattern for transparent color 0
                p.fillRect(r, QColor(40, 40, 40));
                p.fillRect(r.x(), r.y(), r.width() / 2, r.height() / 2, QColor(70, 70, 70));
                p.fillRect(r.x() + r.width() / 2, r.y() + r.height() / 2, r.width() / 2, r.height() / 2, QColor(70, 70, 70));
            } else {
                p.fillRect(r, QColor(m_colors[i]));
            }
            p.setPen(QColor(20, 20, 20));
            p.drawRect(r.adjusted(0, 0, -1, -1));

            if (boxW >= 14) {
                QRgb c = m_colors[i];
                int lum = static_cast<int>(0.299 * qRed(c) + 0.587 * qGreen(c) + 0.114 * qBlue(c));
                p.setPen((i == 0 && m_transp0) ? Qt::white : (lum > 128 ? Qt::black : Qt::white));
                QFont f = p.font();
                f.setPixelSize(9);
                p.setFont(f);
                p.drawText(r, Qt::AlignCenter, QString::number(i));
            }
        }
    }

private:
    QVector<QRgb> m_colors;
    bool m_transp0 = true;
};

RetroTileExplorerDialog::RetroTileExplorerDialog(const QString &filePath,
                                                 QWidget *parent,
                                                 ProjectController *projectController)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_projectController(projectController)
{
    setWindowTitle(tr("Retro Console ROM & VRAM Tile Ripper"));
    resize(1100, 740);

    setupUi();
    loadFileData();
    updateView();
}

void RetroTileExplorerDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // Top Header info bar
    QHBoxLayout *headerLayout = new QHBoxLayout();
    m_lblConsole = new QLabel(this);
    m_lblConsole->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #3498db;"));
    headerLayout->addWidget(m_lblConsole);

    headerLayout->addSpacing(16);

    m_lblFileInfo = new QLabel(this);
    m_lblFileInfo->setStyleSheet(QStringLiteral("color: #aaa; font-size: 12px;"));
    headerLayout->addWidget(m_lblFileInfo);
    headerLayout->addStretch(1);

    mainLayout->addLayout(headerLayout);

    // Main 2-column layout: Left Controls, Right Preview
    QHBoxLayout *middleLayout = new QHBoxLayout();
    middleLayout->setSpacing(10);

    // Left Control Panel (scrollable if screen is compact)
    QWidget *controlPanel = new QWidget(this);
    controlPanel->setFixedWidth(360);
    QVBoxLayout *panelLayout = new QVBoxLayout(controlPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(8);

    // 1. Detected Graphic Banks
    QGroupBox *grpBanks = new QGroupBox(tr("🎯 Detected Graphic Banks"), controlPanel);
    QVBoxLayout *banksLayout = new QVBoxLayout(grpBanks);
    banksLayout->setContentsMargins(10, 8, 10, 8);
    m_comboBanks = new QComboBox(grpBanks);
    connect(m_comboBanks, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RetroTileExplorerDialog::onBankSelected);
    banksLayout->addWidget(m_comboBanks);
    panelLayout->addWidget(grpBanks);

    // 2. Memory Offset Navigator
    QGroupBox *grpOffset = new QGroupBox(tr("📍 Memory Offset / Address"), controlPanel);
    QVBoxLayout *offsetLayout = new QVBoxLayout(grpOffset);
    offsetLayout->setContentsMargins(10, 8, 10, 8);
    offsetLayout->setSpacing(6);

    QHBoxLayout *offsetInputs = new QHBoxLayout();
    offsetInputs->addWidget(new QLabel(tr("Hex:"), grpOffset));
    m_editOffsetHex = new QLineEdit(grpOffset);
    m_editOffsetHex->setPlaceholderText(QStringLiteral("0x000000"));
    connect(m_editOffsetHex, &QLineEdit::textEdited, this, &RetroTileExplorerDialog::onOffsetHexEdited);
    offsetInputs->addWidget(m_editOffsetHex, 1);

    offsetInputs->addWidget(new QLabel(tr("Dec:"), grpOffset));
    m_spinOffset = new QSpinBox(grpOffset);
    m_spinOffset->setRange(0, 0x1000000);
    m_spinOffset->setSingleStep(512);
    connect(m_spinOffset, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RetroTileExplorerDialog::onOffsetSpinChanged);
    offsetInputs->addWidget(m_spinOffset, 1);
    offsetLayout->addLayout(offsetInputs);

    m_sliderOffset = new QSlider(Qt::Horizontal, grpOffset);
    m_sliderOffset->setRange(0, 0x1000000);
    m_sliderOffset->setSingleStep(512);
    m_sliderOffset->setPageStep(4096);
    connect(m_sliderOffset, &QSlider::valueChanged, this, &RetroTileExplorerDialog::onOffsetSliderChanged);
    offsetLayout->addWidget(m_sliderOffset);

    // Quick Step Buttons
    QHBoxLayout *stepLayout = new QHBoxLayout();
    stepLayout->setSpacing(4);

    auto makeStepBtn = [this, grpOffset, stepLayout](const QString &text, int delta) {
        QPushButton *btn = new QPushButton(text, grpOffset);
        btn->setFixedHeight(24);
        connect(btn, &QPushButton::clicked, this, [this, delta]() { onStepOffset(delta); });
        stepLayout->addWidget(btn);
    };

    makeStepBtn(QStringLiteral("-32K"), -32768);
    makeStepBtn(QStringLiteral("-1K"), -1024);
    makeStepBtn(QStringLiteral("-Line"), -512);
    makeStepBtn(QStringLiteral("-Tile"), -32);
    makeStepBtn(QStringLiteral("+Tile"), 32);
    makeStepBtn(QStringLiteral("+Line"), 512);
    makeStepBtn(QStringLiteral("+1K"), 1024);
    makeStepBtn(QStringLiteral("+32K"), 32768);
    offsetLayout->addLayout(stepLayout);

    panelLayout->addWidget(grpOffset);

    // 3. Decoding & Layout Parameters
    QGroupBox *grpFormat = new QGroupBox(tr("⚙️ Decoding & Tile Patterns"), controlPanel);
    QGridLayout *fmtLayout = new QGridLayout(grpFormat);
    fmtLayout->setContentsMargins(10, 8, 10, 8);
    fmtLayout->setSpacing(6);

    fmtLayout->addWidget(new QLabel(tr("Format:"), grpFormat), 0, 0);
    m_comboFormat = new QComboBox(grpFormat);
    m_comboFormat->addItem(tr("SNES (4bpp Planar)"), static_cast<int>(RetroFormat::Snes4bpp));
    m_comboFormat->addItem(tr("SNES (3bpp Planar)"), static_cast<int>(RetroFormat::Snes3bpp));
    m_comboFormat->addItem(tr("Sega Mega Drive (4bpp Chunky)"), static_cast<int>(RetroFormat::Genesis4bpp));
    m_comboFormat->addItem(tr("Nintendo Game Boy (2bpp Interleaved)"), static_cast<int>(RetroFormat::GameBoy2bpp));
    m_comboFormat->addItem(tr("NES / Famicom (2bpp Planar)"), static_cast<int>(RetroFormat::Nes2bpp));
    m_comboFormat->addItem(tr("Sega Master System (4bpp Planar)"), static_cast<int>(RetroFormat::MasterSystem4bpp));
    m_comboFormat->addItem(tr("SNK Neo-Geo (4bpp 16x16)"), static_cast<int>(RetroFormat::NeoGeo4bpp));
    m_comboFormat->addItem(tr("Sony PSX (TIM Direct)"), static_cast<int>(RetroFormat::PsxTim));
    m_comboFormat->addItem(tr("Raw Linear (8bpp Chunky)"), static_cast<int>(RetroFormat::RawLinear8bpp));
    connect(m_comboFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RetroTileExplorerDialog::onFormatChanged);
    fmtLayout->addWidget(m_comboFormat, 0, 1);

    // Tile Arrangement Pattern (SNESTilesKitten-inspired)
    fmtLayout->addWidget(new QLabel(tr("Pattern:"), grpFormat), 1, 0);
    m_comboPattern = new QComboBox(grpFormat);
    m_comboPattern->addItem(tr("8x8 Linear (Tiles & Backgrounds)"), static_cast<int>(TilePattern::Linear));
    m_comboPattern->addItem(tr("16x16 Sprites (2x2 Sequential)"), static_cast<int>(TilePattern::Sprite16x16_2x2));
    m_comboPattern->addItem(tr("16x16 Sprites (SNES VRAM Stride 16)"), static_cast<int>(TilePattern::Sprite16x16_VramStride16));
    m_comboPattern->addItem(tr("16x16 Sprites (VRAM Stride 32)"), static_cast<int>(TilePattern::Sprite16x16_VramStride32));
    m_comboPattern->addItem(tr("32x32 Sprites (4x4 Matrix)"), static_cast<int>(TilePattern::Sprite32x32_4x4));
    m_comboPattern->addItem(tr("32x32 Sprites (2x2*4 Quad SNESTilesKitten)"), static_cast<int>(TilePattern::Sprite32x32_2x2x4));
    m_comboPattern->addItem(tr("32x32 Sprites (SNES VRAM Stride 16)"), static_cast<int>(TilePattern::Sprite32x32_VramStride16));
    connect(m_comboPattern, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RetroTileExplorerDialog::onPatternChanged);
    fmtLayout->addWidget(m_comboPattern, 1, 1);

    fmtLayout->addWidget(new QLabel(tr("Grid Width:"), grpFormat), 2, 0);
    QHBoxLayout *widthLayout = new QHBoxLayout();
    m_spinTilesPerRow = new QSpinBox(grpFormat);
    m_spinTilesPerRow->setRange(4, 64);
    m_spinTilesPerRow->setValue(16); // Default 16 tiles = 128 px for SNES/Consoles
    m_spinTilesPerRow->setSuffix(tr(" tiles"));
    connect(m_spinTilesPerRow, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RetroTileExplorerDialog::onTilesPerRowChanged);
    widthLayout->addWidget(m_spinTilesPerRow, 1);

    QPushButton *btnW8 = new QPushButton(QStringLiteral("8"), grpFormat);
    btnW8->setFixedWidth(28);
    connect(btnW8, &QPushButton::clicked, this, [this]() { m_spinTilesPerRow->setValue(8); });
    widthLayout->addWidget(btnW8);

    QPushButton *btnW16 = new QPushButton(QStringLiteral("16"), grpFormat);
    btnW16->setFixedWidth(28);
    connect(btnW16, &QPushButton::clicked, this, [this]() { m_spinTilesPerRow->setValue(16); });
    widthLayout->addWidget(btnW16);

    QPushButton *btnW32 = new QPushButton(QStringLiteral("32"), grpFormat);
    btnW32->setFixedWidth(28);
    connect(btnW32, &QPushButton::clicked, this, [this]() { m_spinTilesPerRow->setValue(32); });
    widthLayout->addWidget(btnW32);

    fmtLayout->addLayout(widthLayout, 2, 1);

    fmtLayout->addWidget(new QLabel(tr("Tile Count:"), grpFormat), 3, 0);
    m_spinMaxTiles = new QSpinBox(grpFormat);
    m_spinMaxTiles->setRange(64, 8192);
    m_spinMaxTiles->setValue(2048);
    m_spinMaxTiles->setSingleStep(256);
    connect(m_spinMaxTiles, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RetroTileExplorerDialog::onMaxTilesChanged);
    fmtLayout->addWidget(m_spinMaxTiles, 3, 1);

    panelLayout->addWidget(grpFormat);

    // 4. Palette Studio (ROM Hex Picker & Auto-Detection)
    QGroupBox *grpPalette = new QGroupBox(tr("🎨 Palette Studio"), controlPanel);
    QVBoxLayout *palLayout = new QVBoxLayout(grpPalette);
    palLayout->setContentsMargins(10, 8, 10, 8);
    palLayout->setSpacing(6);

    QHBoxLayout *palSourceRow = new QHBoxLayout();
    palSourceRow->addWidget(new QLabel(tr("Source:"), grpPalette));
    m_comboPalette = new QComboBox(grpPalette);
    connect(m_comboPalette, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RetroTileExplorerDialog::onPaletteChanged);
    palSourceRow->addWidget(m_comboPalette, 1);
    palLayout->addLayout(palSourceRow);

    // Auto-detected ROM palettes dropdown
    QHBoxLayout *detectedRow = new QHBoxLayout();
    detectedRow->addWidget(new QLabel(tr("Detected:"), grpPalette));
    m_comboDetectedPalettes = new QComboBox(grpPalette);
    connect(m_comboDetectedPalettes, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RetroTileExplorerDialog::onDetectedPaletteSelected);
    detectedRow->addWidget(m_comboDetectedPalettes, 1);
    palLayout->addLayout(detectedRow);

    // ROM Palette Hex Offset stepper (like SNESTilesKitten's PC location: 0x...)
    QHBoxLayout *hexPalRow = new QHBoxLayout();
    hexPalRow->addWidget(new QLabel(tr("PC Offset:"), grpPalette));
    m_editPaletteHex = new QLineEdit(grpPalette);
    m_editPaletteHex->setPlaceholderText(QStringLiteral("0x000000"));
    connect(m_editPaletteHex, &QLineEdit::textEdited, this, &RetroTileExplorerDialog::onPaletteHexEdited);
    hexPalRow->addWidget(m_editPaletteHex, 1);

    m_spinPaletteOffset = new QSpinBox(grpPalette);
    m_spinPaletteOffset->setRange(0, 0x1000000);
    m_spinPaletteOffset->setSingleStep(32);
    connect(m_spinPaletteOffset, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RetroTileExplorerDialog::onPaletteOffsetChanged);
    hexPalRow->addWidget(m_spinPaletteOffset, 1);
    palLayout->addLayout(hexPalRow);

    // Step buttons for palette (-32, -16, +16, +32)
    QHBoxLayout *palStepLayout = new QHBoxLayout();
    palStepLayout->setSpacing(4);
    auto makePalStepBtn = [this, grpPalette, palStepLayout](const QString &text, int delta) {
        QPushButton *btn = new QPushButton(text, grpPalette);
        btn->setFixedHeight(22);
        connect(btn, &QPushButton::clicked, this, [this, delta]() { onStepPaletteOffset(delta); });
        palStepLayout->addWidget(btn);
    };
    makePalStepBtn(QStringLiteral("-32B"), -32);
    makePalStepBtn(QStringLiteral("-16B"), -16);
    makePalStepBtn(QStringLiteral("+16B"), 16);
    makePalStepBtn(QStringLiteral("+32B"), 32);
    palLayout->addLayout(palStepLayout);

    // Interactive 16-Color Swatch Strip
    m_paletteStrip = new PaletteStripWidget(grpPalette);
    palLayout->addWidget(m_paletteStrip);

    panelLayout->addWidget(grpPalette);

    // 5. Visual Inspection & Zoom Options
    QGroupBox *grpViewOpts = new QGroupBox(tr("👁️ Inspection & Zoom"), controlPanel);
    QVBoxLayout *viewOptsLayout = new QVBoxLayout(grpViewOpts);
    viewOptsLayout->setContentsMargins(10, 8, 10, 8);
    viewOptsLayout->setSpacing(6);

    QHBoxLayout *chkRow = new QHBoxLayout();
    m_chkColor0Transp = new QCheckBox(tr("Transparent #0"), grpViewOpts);
    m_chkColor0Transp->setChecked(true);
    connect(m_chkColor0Transp, &QCheckBox::toggled, this, &RetroTileExplorerDialog::onOptionsToggled);
    chkRow->addWidget(m_chkColor0Transp);

    m_chkShowGrid = new QCheckBox(tr("Tile Grid"), grpViewOpts);
    m_chkShowGrid->setChecked(true);
    connect(m_chkShowGrid, &QCheckBox::toggled, this, &RetroTileExplorerDialog::onOptionsToggled);
    chkRow->addWidget(m_chkShowGrid);
    viewOptsLayout->addLayout(chkRow);

    m_chkDetectSprites = new QCheckBox(tr("Highlight Detected Sprites"), grpViewOpts);
    m_chkDetectSprites->setChecked(true);
    connect(m_chkDetectSprites, &QCheckBox::toggled, this, &RetroTileExplorerDialog::onOptionsToggled);
    viewOptsLayout->addWidget(m_chkDetectSprites);

    // Zoom slider & quick buttons
    QHBoxLayout *zoomRow = new QHBoxLayout();
    zoomRow->addWidget(new QLabel(tr("Zoom:"), grpViewOpts));

    m_sliderZoom = new QSlider(Qt::Horizontal, grpViewOpts);
    m_sliderZoom->setRange(1, 6);
    m_sliderZoom->setValue(2);
    connect(m_sliderZoom, &QSlider::valueChanged, this, &RetroTileExplorerDialog::onZoomChanged);
    zoomRow->addWidget(m_sliderZoom, 1);

    m_lblZoomVal = new QLabel(QStringLiteral("2x (200%)"), grpViewOpts);
    m_lblZoomVal->setFixedWidth(64);
    zoomRow->addWidget(m_lblZoomVal);

    QPushButton *btnZ1 = new QPushButton(QStringLiteral("1x"), grpViewOpts);
    btnZ1->setFixedWidth(26);
    connect(btnZ1, &QPushButton::clicked, this, [this]() { m_sliderZoom->setValue(1); });
    zoomRow->addWidget(btnZ1);

    QPushButton *btnZ2 = new QPushButton(QStringLiteral("2x"), grpViewOpts);
    btnZ2->setFixedWidth(26);
    connect(btnZ2, &QPushButton::clicked, this, [this]() { m_sliderZoom->setValue(2); });
    zoomRow->addWidget(btnZ2);

    QPushButton *btnZ4 = new QPushButton(QStringLiteral("4x"), grpViewOpts);
    btnZ4->setFixedWidth(26);
    connect(btnZ4, &QPushButton::clicked, this, [this]() { m_sliderZoom->setValue(4); });
    zoomRow->addWidget(btnZ4);

    viewOptsLayout->addLayout(zoomRow);

    panelLayout->addWidget(grpViewOpts);
    panelLayout->addStretch(1);

    middleLayout->addWidget(controlPanel);

    // Right Preview Canvas Area
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(6);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet(QStringLiteral("background-color: #1a1a1a; border: 1px solid #333;"));

    m_lblPreview = new QLabel(m_scrollArea);
    m_lblPreview->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_scrollArea->setWidget(m_lblPreview);
    rightLayout->addWidget(m_scrollArea, 1);

    // Stats bar below preview
    m_lblStats = new QLabel(this);
    m_lblStats->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    rightLayout->addWidget(m_lblStats);

    middleLayout->addLayout(rightLayout, 1);
    mainLayout->addLayout(middleLayout, 1);

    // Bottom Action Buttons
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(10);

    QPushButton *btnCancel = new QPushButton(tr("Cancel"), this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    actionLayout->addWidget(btnCancel);

    actionLayout->addStretch(1);

    QPushButton *btnImportSheet = new QPushButton(tr("📥 Import Entire Sheet"), this);
    btnImportSheet->setFixedHeight(36);
    btnImportSheet->setToolTip(tr("Imports the current decoded tile sheet directly as a single atlas image."));
    connect(btnImportSheet, &QPushButton::clicked, this, &RetroTileExplorerDialog::onImportSheetClicked);
    actionLayout->addWidget(btnImportSheet);

    QPushButton *btnExtractSprites = new QPushButton(tr("✨ Extract Sprites to Project"), this);
    btnExtractSprites->setFixedHeight(36);
    btnExtractSprites->setDefault(true);
    btnExtractSprites->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #27ae60; color: white; font-weight: bold; padding: 0 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #2ecc71; }"
        "QPushButton:pressed { background-color: #1e8449; }"
    ));
    btnExtractSprites->setToolTip(tr("Automatically slices and imports all detected character sprites and objects into BentoPack frames."));
    connect(btnExtractSprites, &QPushButton::clicked, this, &RetroTileExplorerDialog::onExtractSpritesClicked);
    actionLayout->addWidget(btnExtractSprites);

    mainLayout->addLayout(actionLayout);

    updatePaletteList();
}

void RetroTileExplorerDialog::loadFileData()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("File Error"),
                              tr("Cannot open ROM/VRAM file: %1").arg(file.errorString()));
        return;
    }

    m_romData = file.readAll();
    file.close();

    QFileInfo fi(m_filePath);
    m_lblFileInfo->setText(tr("<b>%1</b> — %2 bytes (%3 KB)")
                               .arg(fi.fileName())
                               .arg(m_romData.size())
                               .arg(m_romData.size() / 1024));

    m_probeResult = RomProbe::probe(m_romData, m_filePath);
    m_lblConsole->setText(tr("🎮 %1").arg(m_probeResult.consoleName));

    // Update spinbox and slider max range
    int maxOffset = std::max<int>(0, static_cast<int>(m_romData.size() - 32));
    m_spinOffset->setMaximum(maxOffset);
    m_sliderOffset->setMaximum(maxOffset);
    m_spinPaletteOffset->setMaximum(maxOffset);

    // Set Format
    for (int i = 0; i < m_comboFormat->count(); ++i) {
        if (m_comboFormat->itemData(i).toInt() == static_cast<int>(m_probeResult.format)) {
            m_comboFormat->setCurrentIndex(i);
            break;
        }
    }

    // Default Grid Width to native console width (16 tiles = 128 px for SNES)
    m_spinTilesPerRow->blockSignals(true);
    m_spinTilesPerRow->setValue(m_probeResult.tilesPerRow);
    m_spinTilesPerRow->blockSignals(false);

    populateBanks();
    populateDetectedPalettes();
}

void RetroTileExplorerDialog::populateBanks()
{
    m_comboBanks->blockSignals(true);
    m_comboBanks->clear();

    if (!m_probeResult.detectedBanks.isEmpty()) {
        for (int i = 0; i < m_probeResult.detectedBanks.size(); ++i) {
            const auto &b = m_probeResult.detectedBanks.at(i);
            QString prefix = (b.tileCount >= 256 || b.confidence >= 0.45) ? QStringLiteral("⭐ ") : QString();
            m_comboBanks->addItem(QStringLiteral("%1Bank #%2: 0x%3 (%4 tiles, %5% conf)")
                                      .arg(prefix)
                                      .arg(i + 1)
                                      .arg(QString::number(b.offset, 16).toUpper().rightJustified(6, '0'))
                                      .arg(b.tileCount)
                                      .arg(static_cast<int>(b.confidence * 100)),
                                  b.offset);
        }
    } else {
        m_comboBanks->addItem(tr("Beginning of File (0x000000)"), 0);
    }

    m_comboBanks->blockSignals(false);

    // Position at first detected bank
    int initialOffset = m_probeResult.offset;
    m_spinOffset->blockSignals(true);
    m_spinOffset->setValue(initialOffset);
    m_spinOffset->blockSignals(false);

    m_sliderOffset->blockSignals(true);
    m_sliderOffset->setValue(initialOffset);
    m_sliderOffset->blockSignals(false);

    m_editOffsetHex->setText(QStringLiteral("0x%1").arg(QString::number(initialOffset, 16).toUpper()));
}

void RetroTileExplorerDialog::populateDetectedPalettes()
{
    m_comboDetectedPalettes->blockSignals(true);
    m_comboDetectedPalettes->clear();

    if (!m_probeResult.detectedPalettes.isEmpty()) {
        for (int i = 0; i < m_probeResult.detectedPalettes.size(); ++i) {
            const auto &dp = m_probeResult.detectedPalettes.at(i);
            m_comboDetectedPalettes->addItem(QStringLiteral("⭐ Pal #%1: %2").arg(i + 1).arg(dp.label), dp.offset);
        }

        int firstPalOff = m_probeResult.detectedPalettes.first().offset;
        m_spinPaletteOffset->blockSignals(true);
        m_spinPaletteOffset->setValue(firstPalOff);
        m_spinPaletteOffset->blockSignals(false);

        m_editPaletteHex->blockSignals(true);
        m_editPaletteHex->setText(QStringLiteral("0x%1").arg(QString::number(firstPalOff, 16).toUpper()));
        m_editPaletteHex->blockSignals(false);

        m_activeCustomPalette = m_probeResult.detectedPalettes.first().colors;
        m_useRomPalette = true;
        m_comboPalette->setCurrentIndex(0); // Select ROM Auto-scan
    } else {
        m_comboDetectedPalettes->addItem(tr("None auto-detected in ROM"), 0);
        m_useRomPalette = false;
        m_comboPalette->setCurrentIndex(2); // Fallback to Harmonic preview
    }

    m_comboDetectedPalettes->blockSignals(false);
}

void RetroTileExplorerDialog::updatePaletteList()
{
    m_comboPalette->blockSignals(true);
    m_comboPalette->clear();

    m_comboPalette->addItem(tr("🎨 ROM Palette (Auto-Scan)"), QStringLiteral("rom_detected"));
    m_comboPalette->addItem(tr("🎯 ROM Palette (Manual Hex)"), QStringLiteral("rom_manual"));
    m_comboPalette->addItem(tr("Harmonic Retro Preview (Shaded)"), QStringLiteral("default_preview"));
    m_comboPalette->addItem(tr("Grayscale (Contrast)"), QStringLiteral("grayscale"));

    // Add BentoPack standard presets
    for (const auto &p : ColorPalettePresets::availablePalettes()) {
        m_comboPalette->addItem(p.name, p.id);
    }

    m_comboPalette->blockSignals(false);
}

void RetroTileExplorerDialog::onBankSelected(int index)
{
    if (index < 0) return;
    int offset = m_comboBanks->itemData(index).toInt();
    m_spinOffset->setValue(offset);
}

void RetroTileExplorerDialog::onOffsetSliderChanged(int value)
{
    m_spinOffset->setValue(value);
}

void RetroTileExplorerDialog::onOffsetSpinChanged(int value)
{
    m_sliderOffset->blockSignals(true);
    m_sliderOffset->setValue(value);
    m_sliderOffset->blockSignals(false);

    m_editOffsetHex->blockSignals(true);
    m_editOffsetHex->setText(QStringLiteral("0x%1").arg(QString::number(value, 16).toUpper()));
    m_editOffsetHex->blockSignals(false);

    updateView();
}

void RetroTileExplorerDialog::onOffsetHexEdited(const QString &text)
{
    bool ok = false;
    QString cleaned = text.trimmed();
    if (cleaned.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        cleaned = cleaned.mid(2);
    }
    int val = cleaned.toInt(&ok, 16);
    if (ok && val >= 0 && val <= m_spinOffset->maximum()) {
        m_spinOffset->setValue(val);
    }
}

void RetroTileExplorerDialog::onFormatChanged(int)
{
    RetroFormat fmt = static_cast<RetroFormat>(m_comboFormat->currentData().toInt());
    // Rescan palettes if format changed
    m_probeResult.detectedPalettes = RomProbe::scanPalettes(m_romData, fmt);
    populateDetectedPalettes();
    updateView();
}

void RetroTileExplorerDialog::onPatternChanged(int index)
{
    if (index >= 0) {
        m_currentPattern = static_cast<TilePattern>(m_comboPattern->itemData(index).toInt());
        updateView();
    }
}

void RetroTileExplorerDialog::onPaletteChanged(int index)
{
    if (index < 0) return;
    QString palId = m_comboPalette->itemData(index).toString();

    if (palId == QStringLiteral("rom_detected")) {
        m_useRomPalette = true;
        if (m_comboDetectedPalettes->count() > 0) {
            onDetectedPaletteSelected(m_comboDetectedPalettes->currentIndex());
            return;
        }
    } else if (palId == QStringLiteral("rom_manual")) {
        m_useRomPalette = true;
        onPaletteOffsetChanged(m_spinPaletteOffset->value());
        return;
    } else {
        m_useRomPalette = false;
    }

    updateView();
}

void RetroTileExplorerDialog::onDetectedPaletteSelected(int index)
{
    if (index < 0 || m_probeResult.detectedPalettes.isEmpty()) return;
    int offset = m_comboDetectedPalettes->itemData(index).toInt();

    m_spinPaletteOffset->blockSignals(true);
    m_spinPaletteOffset->setValue(offset);
    m_spinPaletteOffset->blockSignals(false);

    m_editPaletteHex->blockSignals(true);
    m_editPaletteHex->setText(QStringLiteral("0x%1").arg(QString::number(offset, 16).toUpper()));
    m_editPaletteHex->blockSignals(false);

    RetroFormat fmt = static_cast<RetroFormat>(m_comboFormat->currentData().toInt());
    const uchar *raw = reinterpret_cast<const uchar*>(m_romData.constData());
    m_activeCustomPalette = RetroTileDecoder::readRomPalette(raw, m_romData.size(), offset, fmt, 16, m_chkColor0Transp->isChecked());
    m_useRomPalette = true;

    updateView();
}

void RetroTileExplorerDialog::onPaletteOffsetChanged(int value)
{
    m_editPaletteHex->blockSignals(true);
    m_editPaletteHex->setText(QStringLiteral("0x%1").arg(QString::number(value, 16).toUpper()));
    m_editPaletteHex->blockSignals(false);

    RetroFormat fmt = static_cast<RetroFormat>(m_comboFormat->currentData().toInt());
    const uchar *raw = reinterpret_cast<const uchar*>(m_romData.constData());
    m_activeCustomPalette = RetroTileDecoder::readRomPalette(raw, m_romData.size(), value, fmt, 16, m_chkColor0Transp->isChecked());
    m_useRomPalette = true;

    updateView();
}

void RetroTileExplorerDialog::onPaletteHexEdited(const QString &text)
{
    bool ok = false;
    QString cleaned = text.trimmed();
    if (cleaned.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        cleaned = cleaned.mid(2);
    }
    int val = cleaned.toInt(&ok, 16);
    if (ok && val >= 0 && val <= m_spinPaletteOffset->maximum()) {
        m_spinPaletteOffset->setValue(val);
    }
}

void RetroTileExplorerDialog::onStepPaletteOffset(int deltaBytes)
{
    int newOffset = std::clamp(m_spinPaletteOffset->value() + deltaBytes, 0, m_spinPaletteOffset->maximum());
    m_spinPaletteOffset->setValue(newOffset);
}

void RetroTileExplorerDialog::onTilesPerRowChanged(int)
{
    updateView();
}

void RetroTileExplorerDialog::onMaxTilesChanged(int)
{
    updateView();
}

void RetroTileExplorerDialog::onOptionsToggled()
{
    // Re-read palette with updated transparency flag if ROM palette is active
    if (m_useRomPalette) {
        RetroFormat fmt = static_cast<RetroFormat>(m_comboFormat->currentData().toInt());
        const uchar *raw = reinterpret_cast<const uchar*>(m_romData.constData());
        m_activeCustomPalette = RetroTileDecoder::readRomPalette(
            raw, m_romData.size(), m_spinPaletteOffset->value(), fmt, 16, m_chkColor0Transp->isChecked());
    }
    updateView();
}

void RetroTileExplorerDialog::onStepOffset(int deltaBytes)
{
    int newOffset = std::clamp(m_spinOffset->value() + deltaBytes, 0, m_spinOffset->maximum());
    m_spinOffset->setValue(newOffset);
}

void RetroTileExplorerDialog::onZoomChanged(int value)
{
    m_zoomFactor = std::clamp(value, 1, 8);
    m_lblZoomVal->setText(QStringLiteral("%1x (%2%)").arg(m_zoomFactor).arg(static_cast<int>(m_zoomFactor * 100)));
    updateView();
}

void RetroTileExplorerDialog::updateView()
{
    if (m_romData.isEmpty()) return;

    int offset = m_spinOffset->value();
    RetroFormat fmt = static_cast<RetroFormat>(m_comboFormat->currentData().toInt());
    int tilesPerRow = m_spinTilesPerRow->value();
    int maxTiles = m_spinMaxTiles->value();
    bool transp0 = m_chkColor0Transp->isChecked();

    QVector<QRgb> pal;
    QString palId = m_comboPalette->currentData().toString();

    if (m_useRomPalette && !m_activeCustomPalette.isEmpty()) {
        pal = m_activeCustomPalette;
    } else if (palId == QStringLiteral("default_preview")) {
        pal = RetroTileDecoder::defaultPreviewPalette(fmt);
    } else if (palId == QStringLiteral("grayscale")) {
        pal = RetroTileDecoder::grayscalePalette(RetroTileDecoder::paletteSize(fmt));
    } else {
        pal = ColorPalettePresets::getPaletteById(palId);
    }

    if (pal.isEmpty()) {
        pal = RetroTileDecoder::defaultPreviewPalette(fmt);
    }

    // Update palette strip swatches
    if (m_paletteStrip) {
        static_cast<PaletteStripWidget*>(m_paletteStrip)->setPaletteColors(pal, transp0);
    }

    int availableBytes = std::max<int>(0, static_cast<int>(m_romData.size() - offset));
    const uchar *raw = reinterpret_cast<const uchar*>(m_romData.constData());

    m_currentSheet = RetroTileDecoder::decodeTileSheet(
        raw + offset,
        availableBytes,
        fmt,
        tilesPerRow,
        maxTiles,
        pal,
        transp0,
        m_currentPattern
    );

    if (m_currentSheet.isNull()) {
        m_lblPreview->setText(tr("No decodable tile data at offset 0x%1").arg(QString::number(offset, 16).toUpper()));
        m_lblStats->setText(tr("Empty sheet"));
        return;
    }

    // Run sprite detection if requested
    m_extractedFrames.clear();
    m_extractedBoxes.clear();
    if (m_chkDetectSprites->isChecked()) {
        SpriteDetectionOptions opts;
        opts.minSliceSize = 6;
        opts.alphaThreshold = 1;
        SpriteDetector::detectToImages(m_currentSheet, m_extractedFrames, m_extractedBoxes, opts);
    }

    // Paint display pixmap with zoom, grid and sprite boxes
    int zoom = std::clamp(static_cast<int>(m_zoomFactor), 1, 8);
    int displayW = m_currentSheet.width() * zoom;
    int displayH = m_currentSheet.height() * zoom;

    QPixmap pixmap(displayW, displayH);
    pixmap.fill(QColor(24, 24, 24)); // Dark canvas background

    QPainter painter(&pixmap);
    // Draw checkerboard for transparency
    QPixmap checker(16, 16);
    checker.fill(QColor(28, 28, 28));
    QPainter cp(&checker);
    cp.fillRect(0, 0, 8, 8, QColor(36, 36, 36));
    cp.fillRect(8, 8, 8, 8, QColor(36, 36, 36));
    cp.end();
    painter.drawTiledPixmap(0, 0, displayW, displayH, checker);

    // Draw scaled tile sheet using nearest-neighbor for crisp retro pixels
    painter.drawImage(QRect(0, 0, displayW, displayH), m_currentSheet);

    // Draw Tile & Sprite Grid
    if (m_chkShowGrid->isChecked()) {
        int tSize = RetroTileDecoder::tileSize(fmt) * zoom;

        // Base 8x8 tile lines (subtle white)
        painter.setPen(QColor(255, 255, 255, 30));
        for (int x = 0; x <= displayW; x += tSize) {
            painter.drawLine(x, 0, x, displayH);
        }
        for (int y = 0; y <= displayH; y += tSize) {
            painter.drawLine(0, y, displayW, y);
        }

        // Meta-tile / Sprite boundary lines (like SNESTilesKitten)
        int metaStep = 0;
        QColor metaColor;
        if (m_currentPattern == TilePattern::Sprite16x16_2x2 ||
            m_currentPattern == TilePattern::Sprite16x16_VramStride16 ||
            m_currentPattern == TilePattern::Sprite16x16_VramStride32) {
            metaStep = tSize * 2; // 16x16
            metaColor = QColor(60, 160, 255, 120); // Blue boundary
        } else if (m_currentPattern == TilePattern::Sprite32x32_4x4 ||
                   m_currentPattern == TilePattern::Sprite32x32_2x2x4 ||
                   m_currentPattern == TilePattern::Sprite32x32_VramStride16) {
            metaStep = tSize * 4; // 32x32
            metaColor = QColor(255, 200, 50, 140); // Gold boundary
        }

        if (metaStep > 0) {
            painter.setPen(QPen(metaColor, 1));
            for (int x = 0; x <= displayW; x += metaStep) {
                painter.drawLine(x, 0, x, displayH);
            }
            for (int y = 0; y <= displayH; y += metaStep) {
                painter.drawLine(0, y, displayW, y);
            }
        }
    }

    // Draw Sprite Detection Bounding Boxes
    if (m_chkDetectSprites->isChecked() && !m_extractedBoxes.isEmpty()) {
        painter.setPen(QPen(QColor(46, 204, 113, 220), 2));
        painter.setBrush(QColor(46, 204, 113, 30));
        QFont font = painter.font();
        font.setPixelSize(10);
        painter.setFont(font);

        for (const auto &box : m_extractedBoxes) {
            QRect r(box.rect.x() * zoom, box.rect.y() * zoom,
                    box.rect.width() * zoom, box.rect.height() * zoom);
            painter.drawRect(r);

            // Size badge
            QString badge = QStringLiteral("%1x%2").arg(box.rect.width()).arg(box.rect.height());
            painter.fillRect(r.x(), r.y() - 12, painter.fontMetrics().horizontalAdvance(badge) + 4, 12, QColor(0, 0, 0, 180));
            painter.setPen(QColor(46, 204, 113));
            painter.drawText(r.x() + 2, r.y() - 2, badge);
            painter.setPen(QPen(QColor(46, 204, 113, 220), 2));
        }
    }

    painter.end();

    m_lblPreview->setPixmap(pixmap);
    m_lblPreview->resize(displayW, displayH);

    int tSize = RetroTileDecoder::tileSize(fmt);
    int totalDecodedTiles = (m_currentSheet.width() / tSize) * (m_currentSheet.height() / tSize);

    m_lblStats->setText(tr("Decoded: %1 tiles (%2x%3 px) | Zoom: %4x | Pattern: %5 | Palette: 0x%6 | Offset: 0x%7")
                            .arg(totalDecodedTiles)
                            .arg(m_currentSheet.width())
                            .arg(m_currentSheet.height())
                            .arg(zoom)
                            .arg(m_comboPattern->currentText())
                            .arg(QString::number(m_spinPaletteOffset->value(), 16).toUpper())
                            .arg(QString::number(offset, 16).toUpper()));
}

void RetroTileExplorerDialog::onExtractSpritesClicked()
{
    if (m_currentSheet.isNull()) return;

    if (m_extractedFrames.isEmpty()) {
        SpriteDetectionOptions opts;
        opts.minSliceSize = 4;
        opts.alphaThreshold = 1;
        SpriteDetector::detectToImages(m_currentSheet, m_extractedFrames, m_extractedBoxes, opts);
    }

    if (m_extractedFrames.isEmpty()) {
        // Fallback: whole sheet
        m_extractedFrames.append(m_currentSheet);
        m_extractedBoxes.append(SpriteBox(QRect(0, 0, m_currentSheet.width(), m_currentSheet.height())));
    }

    if (m_projectController && m_projectController->document()) {
        SpriteDocument *doc = m_projectController->document();
        doc->setAtlas(m_currentSheet);
        doc->setFrames(m_extractedFrames, m_extractedBoxes);
        doc->setFilePath(m_filePath);
        m_projectController->setCurrentFilePath(m_filePath);
        m_projectController->addRecentFile(m_filePath);
        emit m_projectController->fileLoaded(m_filePath);
    }

    accept();
}

void RetroTileExplorerDialog::onImportSheetClicked()
{
    if (m_currentSheet.isNull()) return;

    if (m_projectController && m_projectController->document()) {
        SpriteDocument *doc = m_projectController->document();
        doc->setAtlas(m_currentSheet);
        QList<QImage> singleFrame = { m_currentSheet };
        QList<SpriteBox> singleBox = { SpriteBox(QRect(0, 0, m_currentSheet.width(), m_currentSheet.height())) };
        doc->setFrames(singleFrame, singleBox);
        doc->setFilePath(m_filePath);
        m_projectController->setCurrentFilePath(m_filePath);
        m_projectController->addRecentFile(m_filePath);
        emit m_projectController->fileLoaded(m_filePath);
    }

    accept();
}
