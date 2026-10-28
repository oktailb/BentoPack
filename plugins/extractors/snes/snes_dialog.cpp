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

#include "snes_dialog.h"
#include "image/spritedetector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QTabWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QtEndian>

SnesDialog::SnesDialog(const QString &filePath, const QByteArray &fileData, QWidget *parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_fileData(fileData)
{
    setWindowTitle(tr("Super Nintendo / Super Famicom (SNES) Ripper"));
    resize(1150, 800);

    m_isSavestate = SnesSavestateLoader::isSavestate(m_fileData, m_filePath);
    if (m_isSavestate) {
        m_savestate = SnesSavestateLoader::loadFromData(m_fileData, m_filePath);
    }
    m_hasCopierHeader = SnesAddress::hasCopierHeader(m_fileData.size());
    m_isHiRom = SnesAddress::detectHiRom(m_fileData, m_hasCopierHeader);
    m_romTitle = SnesAddress::readInternalRomTitle(m_fileData, m_hasCopierHeader, m_isHiRom);

    m_presets = SnesPresetManager::builtInPresets();

    runHeuristicAnalysis();
    setupUi();
    updatePaletteSwatches();
    updateDecoding();
}

void SnesDialog::runHeuristicAnalysis()
{
    if (m_fileData.isEmpty()) return;

    if (!m_isSavestate) {
        // 1. Scan for SNES BGR555 palettes with monotonic luminance ramps & backdrop 0
        m_detectedPalettes = SnesPaletteScanner::scanRomPalettes(m_fileData, 32, 0.40);

        // 2. Scan for sprite banks (uncompressed 4bpp, Nintendo LC-LZ2, LZSS, RLE)
        m_detectedBanks = SnesRomAnalyzer::scanRomForBanks(m_fileData, 64);
    }

    // Initialize active palette
    if (m_isSavestate && !m_savestate.cgram.isEmpty()) {
        m_activePalette = m_savestate.getPalette(8, true);
    } else if (!m_detectedPalettes.isEmpty()) {
        m_activePalette = m_detectedPalettes.first().colors;
    } else {
        m_activePalette = SnesPaletteScanner::getHarmonicPresetPalette(0);
    }
}

void SnesDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Top Info Banner
    m_infoBanner = new QLabel(this);
    m_infoBanner->setStyleSheet(QStringLiteral(
        "background-color: #2b2d30; color: #f0f0f0; border-radius: 6px; padding: 10px; font-size: 13px;"));

    QString bannerText;
    if (m_isSavestate) {
        bannerText = QStringLiteral("<b>🎮 %1</b> | VRAM: %2 KB | CGRAM: %3 Palettes")
            .arg(m_savestate.emulatorName)
            .arg(m_savestate.vram.size() / 1024)
            .arg(m_savestate.cgram.size() / 32);
    } else {
        bannerText = QStringLiteral("<b>🎮 SNES Cartridge: %1</b> | Mode: %2 | %3 Banques Détectées | %4 Palettes ROM")
            .arg(m_romTitle.isEmpty() ? tr("Custom ROM") : m_romTitle)
            .arg(m_isHiRom ? QStringLiteral("HiROM") : QStringLiteral("LoROM"))
            .arg(m_detectedBanks.size())
            .arg(m_detectedPalettes.size());
    }
    m_infoBanner->setText(bannerText);
    mainLayout->addWidget(m_infoBanner);

    auto *splitLayout = new QHBoxLayout();

    // Left Panel: Controls
    auto *leftPanel = new QWidget(this);
    leftPanel->setFixedWidth(410);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // Tabs for Source
    m_tabWidget = new QTabWidget(leftPanel);

    // Tab 1: Savestate / VRAM Live (if savestate)
    if (m_isSavestate) {
        auto *tabSavestate = new QWidget(m_tabWidget);
        auto *savestateLayout = new QVBoxLayout(tabSavestate);

        auto *vramInfoLbl = new QLabel(tr("PPU VRAM is directly extracted from the snapshot. All graphics are already decompressed by the game engine."), tabSavestate);
        vramInfoLbl->setWordWrap(true);
        savestateLayout->addWidget(vramInfoLbl);

        auto *palRow = new QHBoxLayout();
        palRow->addWidget(new QLabel(tr("Active Palette:"), tabSavestate));
        m_savestatePaletteCombo = new QComboBox(tabSavestate);
        for (int i = 0; i < 16; ++i) {
            if (i < 8) {
                m_savestatePaletteCombo->addItem(tr("Palette %1 (Background BG)").arg(i));
            } else {
                m_savestatePaletteCombo->addItem(tr("Palette %1 (Sprite / OBJ %2)").arg(i).arg(i - 8));
            }
        }
        m_savestatePaletteCombo->setCurrentIndex(8);
        connect(m_savestatePaletteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SnesDialog::onPaletteIndexChanged);
        palRow->addWidget(m_savestatePaletteCombo);
        savestateLayout->addLayout(palRow);

        auto *loadExtPalBtn = new QPushButton(tr("📁 Load External Palette / CGRAM..."), tabSavestate);
        connect(loadExtPalBtn, &QPushButton::clicked, this, &SnesDialog::onLoadExternalPaletteClicked);
        savestateLayout->addWidget(loadExtPalBtn);
        savestateLayout->addStretch();
        m_tabWidget->addTab(tabSavestate, tr("⚡ Savestate / VRAM"));
    }

    // Tab 2: Heuristic & Compression Bank Scanner
    auto *tabHeuristic = new QWidget(m_tabWidget);
    auto *heuristicLayout = new QVBoxLayout(tabHeuristic);

    m_banksSummaryLabel = new QLabel(
        tr("Automated entropy analysis, 4bpp bitplane correlation & compression signatures (Nintendo LC-LZ2, LZSS, RLE):"),
        tabHeuristic);
    m_banksSummaryLabel->setWordWrap(true);
    m_banksSummaryLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 11px;"));
    heuristicLayout->addWidget(m_banksSummaryLabel);

    m_banksList = new QListWidget(tabHeuristic);
    m_banksList->setStyleSheet(QStringLiteral(
        "QListWidget { background-color: #1e1e1e; color: #ddd; border: 1px solid #3c3c3c; border-radius: 4px; font-family: monospace; font-size: 11px; }"
        "QListWidget::item { padding: 4px; }"
        "QListWidget::item:selected { background-color: #0d47a1; color: white; }"));

    for (int i = 0; i < m_detectedBanks.size(); ++i) {
        const auto &b = m_detectedBanks[i];
        QString icon = b.isCompressed ? QStringLiteral("📦") : QStringLiteral("🎨");
        QString itemText = QStringLiteral("%1 0x%2 | %3 | %4 KB | Conf: %5%")
            .arg(icon)
            .arg(QString::number(b.offset, 16).toUpper().rightJustified(6, '0'))
            .arg(b.type)
            .arg(std::max(1, b.length / 1024))
            .arg(static_cast<int>(b.confidence * 100));
        m_banksList->addItem(itemText);
    }
    if (m_detectedBanks.isEmpty()) {
        m_banksList->addItem(tr("(No distinct banks detected - use Manual ROM or Presets)"));
    }
    connect(m_banksList, &QListWidget::itemClicked, this, &SnesDialog::onBankSelected);
    heuristicLayout->addWidget(m_banksList);
    m_tabWidget->addTab(tabHeuristic, tr("🔍 Heuristiques (%1)").arg(m_detectedBanks.size()));

    // Tab 3: ROM Cartridge Manual
    auto *tabRom = new QWidget(m_tabWidget);
    auto *romLayout = new QVBoxLayout(tabRom);

    auto *romModeRow = new QHBoxLayout();
    m_loRomRadio = new QRadioButton(QStringLiteral("LoROM"), tabRom);
    m_hiRomRadio = new QRadioButton(QStringLiteral("HiROM"), tabRom);
    if (m_isHiRom) m_hiRomRadio->setChecked(true);
    else m_loRomRadio->setChecked(true);
    connect(m_loRomRadio, &QRadioButton::toggled, this, &SnesDialog::onSourceChanged);
    connect(m_hiRomRadio, &QRadioButton::toggled, this, &SnesDialog::onSourceChanged);
    romModeRow->addWidget(m_loRomRadio);
    romModeRow->addWidget(m_hiRomRadio);
    m_copierHeaderCheck = new QCheckBox(tr("512B Header"), tabRom);
    m_copierHeaderCheck->setChecked(m_hasCopierHeader);
    connect(m_copierHeaderCheck, &QCheckBox::toggled, this, &SnesDialog::onSourceChanged);
    romModeRow->addWidget(m_copierHeaderCheck);
    romLayout->addLayout(romModeRow);

    auto *addrGrid = new QGridLayout();
    addrGrid->addWidget(new QLabel(tr("SNES Bus Address:"), tabRom), 0, 0);
    m_snesAddressEdit = new QLineEdit(QStringLiteral("$00:8000"), tabRom);
    addrGrid->addWidget(m_snesAddressEdit, 0, 1);
    connect(m_snesAddressEdit, &QLineEdit::textChanged, this, &SnesDialog::onAddressChanged);

    addrGrid->addWidget(new QLabel(tr("PC File Offset:"), tabRom), 1, 0);
    m_pcOffsetEdit = new QLineEdit(QStringLiteral("0x000000"), tabRom);
    addrGrid->addWidget(m_pcOffsetEdit, 1, 1);
    connect(m_pcOffsetEdit, &QLineEdit::textChanged, this, &SnesDialog::onSourceChanged);

    addrGrid->addWidget(new QLabel(tr("Data Length:"), tabRom), 2, 0);
    m_dataLengthSpin = new QSpinBox(tabRom);
    m_dataLengthSpin->setRange(16, 2 * 1024 * 1024);
    m_dataLengthSpin->setValue(32768);
    m_dataLengthSpin->setSuffix(QStringLiteral(" bytes"));
    connect(m_dataLengthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SnesDialog::onSourceChanged);
    addrGrid->addWidget(m_dataLengthSpin, 2, 1);
    romLayout->addLayout(addrGrid);

    romLayout->addStretch();
    m_tabWidget->addTab(tabRom, tr("💾 ROM Manuel"));

    // Tab 4: Presets
    auto *tabPresets = new QWidget(m_tabWidget);
    auto *presetLayout = new QVBoxLayout(tabPresets);

    presetLayout->addWidget(new QLabel(tr("Select Classic Game Preset:"), tabPresets));
    m_presetCombo = new QComboBox(tabPresets);
    for (const auto &p : m_presets) {
        m_presetCombo->addItem(QStringLiteral("%1 - %2").arg(p.gameName, p.title));
    }
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SnesDialog::onPresetSelected);
    presetLayout->addWidget(m_presetCombo);

    m_presetDescLabel = new QLabel(tabPresets);
    m_presetDescLabel->setWordWrap(true);
    m_presetDescLabel->setStyleSheet(QStringLiteral("color: #888; font-style: italic;"));
    presetLayout->addWidget(m_presetDescLabel);

    auto *loadPresetBtn = new QPushButton(tr("📂 Load Custom Preset File (.stk / .json)..."), tabPresets);
    connect(loadPresetBtn, &QPushButton::clicked, this, &SnesDialog::onLoadPresetClicked);
    presetLayout->addWidget(loadPresetBtn);
    presetLayout->addStretch();
    m_tabWidget->addTab(tabPresets, tr("⭐ Presets"));

    // Initial Tab Selection
    if (m_isSavestate) {
        m_tabWidget->setCurrentIndex(0);
    } else if (!m_detectedBanks.isEmpty()) {
        m_tabWidget->setCurrentIndex(1); // Heuristic tab first!
    } else {
        m_tabWidget->setCurrentIndex(2);
    }
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &SnesDialog::onSourceChanged);
    leftLayout->addWidget(m_tabWidget);

    // Palette Color Scheme Group (solves "les couleurs restent étranges")
    auto *paletteGroup = new QGroupBox(tr("🎨 Palette & Couleurs"), leftPanel);
    auto *palLayout = new QGridLayout(paletteGroup);

    palLayout->addWidget(new QLabel(tr("Source:"), paletteGroup), 0, 0);
    m_paletteSourceTypeCombo = new QComboBox(paletteGroup);
    m_paletteSourceTypeCombo->addItem(tr("Palettes Détectées (Heuristique ROM)"));
    m_paletteSourceTypeCombo->addItem(tr("Palettes Harmonieuses (Styles SNES)"));
    if (m_isSavestate) {
        m_paletteSourceTypeCombo->addItem(tr("Savestate CGRAM (16 Palettes VRAM)"));
    }
    m_paletteSourceTypeCombo->addItem(tr("Offset Personnalisé (BGR555)"));
    connect(m_paletteSourceTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SnesDialog::onPaletteSourceChanged);
    palLayout->addWidget(m_paletteSourceTypeCombo, 0, 1);

    // Sub-selector 1: Detected Palettes
    m_detectedPalettesCombo = new QComboBox(paletteGroup);
    for (int i = 0; i < m_detectedPalettes.size(); ++i) {
        const auto &dp = m_detectedPalettes[i];
        m_detectedPalettesCombo->addItem(
            QStringLiteral("[0x%1] %2 (%3%)")
                .arg(QString::number(dp.offset, 16).toUpper().rightJustified(6, '0'))
                .arg(dp.tag)
                .arg(static_cast<int>(dp.score * 100)));
    }
    if (m_detectedPalettes.isEmpty()) {
        m_detectedPalettesCombo->addItem(tr("(Aucune palette SNES nette trouvée)"));
    }
    connect(m_detectedPalettesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SnesDialog::onDetectedPaletteSelected);
    palLayout->addWidget(m_detectedPalettesCombo, 1, 0, 1, 2);

    // Sub-selector 2: Harmonic Presets
    m_harmonicPalettesCombo = new QComboBox(paletteGroup);
    m_harmonicPalettesCombo->addItems(SnesPaletteScanner::harmonicPresetNames());
    connect(m_harmonicPalettesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SnesDialog::onHarmonicPaletteSelected);
    m_harmonicPalettesCombo->setVisible(false);
    palLayout->addWidget(m_harmonicPalettesCombo, 2, 0, 1, 2);

    // Sub-selector 3: Custom Offset
    m_customPaletteOffsetSpin = new QSpinBox(paletteGroup);
    m_customPaletteOffsetSpin->setRange(0, std::max(1, static_cast<int>(m_fileData.size() - 32)));
    m_customPaletteOffsetSpin->setDisplayIntegerBase(16);
    m_customPaletteOffsetSpin->setPrefix(QStringLiteral("0x"));
    m_customPaletteOffsetSpin->setSingleStep(32);
    connect(m_customPaletteOffsetSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SnesDialog::onCustomPaletteOffsetChanged);
    m_customPaletteOffsetSpin->setVisible(false);
    palLayout->addWidget(m_customPaletteOffsetSpin, 3, 0, 1, 2);

    auto *loadPalBtn2 = new QPushButton(tr("📁 Charger Palette Externe (.cgram, .pal, .bin)..."), paletteGroup);
    connect(loadPalBtn2, &QPushButton::clicked, this, &SnesDialog::onLoadExternalPaletteClicked);
    palLayout->addWidget(loadPalBtn2, 4, 0, 1, 2);

    leftLayout->addWidget(paletteGroup);

    // Format & Geometry Box
    auto *geomGroup = new QGroupBox(tr("Tile Layout & Dimensions"), leftPanel);
    auto *geomLayout = new QGridLayout(geomGroup);

    geomLayout->addWidget(new QLabel(tr("Color Depth:"), geomGroup), 0, 0);
    m_bppCombo = new QComboBox(geomGroup);
    m_bppCombo->addItem(tr("4bpp (16 colors) - Standard SNES"), static_cast<int>(RetroFormat::Snes4bpp));
    m_bppCombo->addItem(tr("3bpp (8 colors)"), static_cast<int>(RetroFormat::Snes3bpp));
    m_bppCombo->addItem(tr("2bpp (4 colors)"), static_cast<int>(RetroFormat::Nes2bpp));
    m_bppCombo->addItem(tr("8bpp (256 colors)"), static_cast<int>(RetroFormat::RawLinear8bpp));
    connect(m_bppCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SnesDialog::onPatternChanged);
    geomLayout->addWidget(m_bppCombo, 0, 1);

    geomLayout->addWidget(new QLabel(tr("Tile Pattern:"), geomGroup), 1, 0);
    m_patternCombo = new QComboBox(geomGroup);
    m_patternCombo->addItem(tr("8x8 Linear (BG & Fonts)"), static_cast<int>(TilePattern::Linear));
    m_patternCombo->addItem(tr("16x16 Sprites (2x2 Sequential)"), static_cast<int>(TilePattern::Sprite16x16_2x2));
    m_patternCombo->addItem(tr("16x16 Sprites (SNES VRAM Stride 16)"), static_cast<int>(TilePattern::Sprite16x16_VramStride16));
    m_patternCombo->addItem(tr("16x16 Sprites (VRAM Stride 32)"), static_cast<int>(TilePattern::Sprite16x16_VramStride32));
    m_patternCombo->addItem(tr("32x32 Sprites (4x4 Matrix)"), static_cast<int>(TilePattern::Sprite32x32_4x4));
    m_patternCombo->addItem(tr("32x32 Sprites (SNES VRAM Stride 16)"), static_cast<int>(TilePattern::Sprite32x32_VramStride16));
    m_patternCombo->setCurrentIndex(1); // Default to 16x16 2x2 sequential
    connect(m_patternCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SnesDialog::onPatternChanged);
    geomLayout->addWidget(m_patternCombo, 1, 1);

    geomLayout->addWidget(new QLabel(tr("Grid Width:"), geomGroup), 2, 0);
    m_gridWidthSpin = new QSpinBox(geomGroup);
    m_gridWidthSpin->setRange(1, 64);
    m_gridWidthSpin->setValue(16);
    m_gridWidthSpin->setSuffix(tr(" tiles"));
    connect(m_gridWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SnesDialog::onPatternChanged);
    geomLayout->addWidget(m_gridWidthSpin, 2, 1);

    leftLayout->addWidget(geomGroup);

    // Sprite Detection Constraints Group
    auto *constraintGroup = new QGroupBox(tr("Contraintes de Découpage de Sprites"), leftPanel);
    auto *constraintLayout = new QGridLayout(constraintGroup);

    constraintLayout->addWidget(new QLabel(tr("Taille Maximale:"), constraintGroup), 0, 0);
    m_maxSizeCombo = new QComboBox(constraintGroup);
    m_maxSizeCombo->addItem(tr("Automatique (Sans limite)"), 0);
    m_maxSizeCombo->addItem(tr("16x16 (Petits sprites)"), 16);
    m_maxSizeCombo->addItem(tr("32x32 (Standard SNES)"), 32);
    m_maxSizeCombo->addItem(tr("32x48 (Personnages verticaux / Permissif)"), 48);
    m_maxSizeCombo->addItem(tr("48x48 (Moyens sprites)"), 48);
    m_maxSizeCombo->addItem(tr("64x64 (Grands sprites / Boss)"), 64);
    m_maxSizeCombo->setCurrentIndex(2); // 32x32 default
    connect(m_maxSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SnesDialog::onConstraintChanged);
    constraintLayout->addWidget(m_maxSizeCombo, 0, 1);

    m_disallowOverlapsCheck = new QCheckBox(tr("Sans croisements de rectangles (Non-overlapping)"), constraintGroup);
    m_disallowOverlapsCheck->setChecked(true);
    connect(m_disallowOverlapsCheck, &QCheckBox::toggled, this, &SnesDialog::onConstraintChanged);
    constraintLayout->addWidget(m_disallowOverlapsCheck, 1, 0, 1, 2);

    m_alignToGridCheck = new QCheckBox(tr("Aligner sur grille de tuiles (Snap 8px)"), constraintGroup);
    m_alignToGridCheck->setChecked(true);
    connect(m_alignToGridCheck, &QCheckBox::toggled, this, &SnesDialog::onConstraintChanged);
    constraintLayout->addWidget(m_alignToGridCheck, 2, 0, 1, 2);

    leftLayout->addWidget(constraintGroup);

    // Visual options
    auto *viewOptGroup = new QGroupBox(tr("View Options"), leftPanel);
    auto *viewOptLayout = new QGridLayout(viewOptGroup);

    m_transpZeroCheck = new QCheckBox(tr("Transparent Color #0"), viewOptGroup);
    m_transpZeroCheck->setChecked(true);
    connect(m_transpZeroCheck, &QCheckBox::toggled, this, &SnesDialog::onPatternChanged);
    viewOptLayout->addWidget(m_transpZeroCheck, 0, 0);

    m_showGridCheck = new QCheckBox(tr("Tile Grid"), viewOptGroup);
    m_showGridCheck->setChecked(true);
    connect(m_showGridCheck, &QCheckBox::toggled, this, &SnesDialog::onPatternChanged);
    viewOptLayout->addWidget(m_showGridCheck, 0, 1);

    m_highlightSpritesCheck = new QCheckBox(tr("Highlight Detected Sprites"), viewOptGroup);
    m_highlightSpritesCheck->setChecked(true);
    connect(m_highlightSpritesCheck, &QCheckBox::toggled, this, &SnesDialog::onPatternChanged);
    viewOptLayout->addWidget(m_highlightSpritesCheck, 1, 0);

    viewOptLayout->addWidget(new QLabel(tr("Zoom:"), viewOptGroup), 1, 1);
    m_zoomCombo = new QComboBox(viewOptGroup);
    m_zoomCombo->addItems({ QStringLiteral("1x"), QStringLiteral("2x"), QStringLiteral("3x"), QStringLiteral("4x"), QStringLiteral("6x"), QStringLiteral("8x") });
    m_zoomCombo->setCurrentIndex(1); // 2x default
    connect(m_zoomCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SnesDialog::onZoomChanged);
    viewOptLayout->addWidget(m_zoomCombo, 1, 2);

    leftLayout->addWidget(viewOptGroup);

    // Active Palette Swatches Display
    auto *swatchGroup = new QGroupBox(tr("Active Palette Swatches (16 Colors)"), leftPanel);
    auto *swatchLayout = new QVBoxLayout(swatchGroup);
    m_swatchesWidget = new QWidget(swatchGroup);
    m_swatchesWidget->setFixedHeight(28);
    swatchLayout->addWidget(m_swatchesWidget);
    leftLayout->addWidget(swatchGroup);

    splitLayout->addWidget(leftPanel);

    // Right Panel: Interactive Zoomable Canvas
    auto *rightPanel = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea(rightPanel);
    m_scrollArea->setStyleSheet(QStringLiteral("background-color: #1e1e1e;"));
    m_previewLabel = new QLabel(m_scrollArea);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidget(m_previewLabel);
    m_scrollArea->setWidgetResizable(true);
    rightLayout->addWidget(m_scrollArea);

    m_statusLabel = new QLabel(rightPanel);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #aaa; font-size: 11px;"));
    rightLayout->addWidget(m_statusLabel);

    splitLayout->addWidget(rightPanel, 1);
    mainLayout->addLayout(splitLayout);

    // Bottom Action Buttons
    auto *bottomRow = new QHBoxLayout();
    auto *cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomRow->addWidget(cancelBtn);

    bottomRow->addStretch();

    auto *importAtlasBtn = new QPushButton(tr("📥 Import Entire Sheet as Atlas"), this);
    importAtlasBtn->setStyleSheet(QStringLiteral("padding: 8px 16px; font-weight: bold;"));
    connect(importAtlasBtn, &QPushButton::clicked, this, &SnesDialog::onImportAtlasClicked);
    bottomRow->addWidget(importAtlasBtn);

    auto *extractBtn = new QPushButton(tr("✨ Slice & Extract Sprites to Project"), this);
    extractBtn->setStyleSheet(QStringLiteral("background-color: #2e7d32; color: white; padding: 8px 18px; font-weight: bold; border-radius: 4px;"));
    connect(extractBtn, &QPushButton::clicked, this, &SnesDialog::onExtractSpritesClicked);
    bottomRow->addWidget(extractBtn);

    mainLayout->addLayout(bottomRow);

    if (!m_detectedBanks.isEmpty()) {
        m_banksList->setCurrentRow(0);
        onBankSelected(m_banksList->item(0));
    } else if (!m_presets.isEmpty()) {
        onPresetSelected(0);
    }
}

void SnesDialog::onBankSelected(QListWidgetItem *item)
{
    int row = m_banksList->row(item);
    if (row < 0 || row >= m_detectedBanks.size()) return;

    const auto &bank = m_detectedBanks[row];
    if (bank.isCompressed && !bank.decompressedData.isEmpty()) {
        m_overrideData = bank.decompressedData;
    } else {
        m_overrideData.clear();
        m_pcOffsetEdit->blockSignals(true);
        m_pcOffsetEdit->setText(QStringLiteral("0x%1").arg(QString::number(bank.offset, 16).toUpper().rightJustified(6, '0')));
        m_pcOffsetEdit->blockSignals(false);

        m_dataLengthSpin->blockSignals(true);
        m_dataLengthSpin->setValue(bank.length);
        m_dataLengthSpin->blockSignals(false);
    }

    // Set suggested pattern
    for (int i = 0; i < m_patternCombo->count(); ++i) {
        if (m_patternCombo->itemData(i).toInt() == static_cast<int>(bank.pattern)) {
            m_patternCombo->setCurrentIndex(i);
            break;
        }
    }

    updateDecoding();
}

void SnesDialog::onPaletteSourceChanged(int index)
{
    m_detectedPalettesCombo->setVisible(index == 0);
    m_harmonicPalettesCombo->setVisible(index == 1);
    bool hasSavestate = m_isSavestate;
    if (hasSavestate && index == 2) {
        // Savestate palette
        onPaletteIndexChanged(m_savestatePaletteCombo ? m_savestatePaletteCombo->currentIndex() : 8);
    } else if ((hasSavestate && index == 3) || (!hasSavestate && index == 2)) {
        // Custom offset
        m_customPaletteOffsetSpin->setVisible(true);
        onCustomPaletteOffsetChanged(m_customPaletteOffsetSpin->value());
    } else {
        m_customPaletteOffsetSpin->setVisible(false);
        if (index == 0) {
            onDetectedPaletteSelected(m_detectedPalettesCombo->currentIndex());
        } else if (index == 1) {
            onHarmonicPaletteSelected(m_harmonicPalettesCombo->currentIndex());
        }
    }
}

void SnesDialog::onDetectedPaletteSelected(int index)
{
    if (index >= 0 && index < m_detectedPalettes.size()) {
        m_activePalette = m_detectedPalettes[index].colors;
        updatePaletteSwatches();
        updateDecoding();
    }
}

void SnesDialog::onHarmonicPaletteSelected(int index)
{
    if (index >= 0) {
        m_activePalette = SnesPaletteScanner::getHarmonicPresetPalette(index);
        updatePaletteSwatches();
        updateDecoding();
    }
}

void SnesDialog::onCustomPaletteOffsetChanged(int value)
{
    if (value >= 0 && value + 32 <= m_fileData.size()) {
        m_activePalette.resize(16);
        const uchar *pRaw = reinterpret_cast<const uchar*>(m_fileData.constData()) + value;
        for (int i = 0; i < 16; ++i) {
            quint16 word = qFromLittleEndian<quint16>(pRaw + (i * 2));
            int a = (m_transpZeroCheck->isChecked() && i == 0) ? 0 : 255;
            m_activePalette[i] = SnesSavestateData::bgr555ToRgb(word, a);
        }
        updatePaletteSwatches();
        updateDecoding();
    }
}

void SnesDialog::onAddressChanged()
{
    QString txt = m_snesAddressEdit->text().trimmed();
    if (txt.startsWith(QLatin1Char('$'))) txt.remove(0, 1);
    txt.replace(QLatin1Char(':'), QString());

    bool ok = false;
    quint32 snesAddr = txt.toUInt(&ok, 16);
    if (ok) {
        int bank = (snesAddr >> 16) & 0xFF;
        int addr = snesAddr & 0xFFFF;
        int pc = m_hiRomRadio->isChecked()
            ? SnesAddress::snesToPcHiRom(bank, addr, m_copierHeaderCheck->isChecked())
            : SnesAddress::snesToPcLoRom(bank, addr, m_copierHeaderCheck->isChecked());

        m_overrideData.clear();
        m_pcOffsetEdit->blockSignals(true);
        m_pcOffsetEdit->setText(QStringLiteral("0x%1").arg(QString::number(pc, 16).toUpper().rightJustified(6, '0')));
        m_pcOffsetEdit->blockSignals(false);
        updateDecoding();
    }
}

void SnesDialog::onSourceChanged()
{
    m_overrideData.clear();
    updateDecoding();
}

void SnesDialog::onPresetSelected(int index)
{
    if (index < 0 || index >= m_presets.size()) return;
    const SnesPreset &p = m_presets[index];

    m_presetDescLabel->setText(p.description);

    m_overrideData.clear();
    m_loRomRadio->blockSignals(true);
    m_hiRomRadio->blockSignals(true);
    if (p.isHiRom) m_hiRomRadio->setChecked(true);
    else m_loRomRadio->setChecked(true);
    m_loRomRadio->blockSignals(false);
    m_hiRomRadio->blockSignals(false);

    m_pcOffsetEdit->blockSignals(true);
    m_pcOffsetEdit->setText(QStringLiteral("0x%1").arg(QString::number(p.tileOffset, 16).toUpper().rightJustified(6, '0')));
    m_pcOffsetEdit->blockSignals(false);

    m_dataLengthSpin->blockSignals(true);
    m_dataLengthSpin->setValue(p.tileLength);
    m_dataLengthSpin->blockSignals(false);

    m_gridWidthSpin->blockSignals(true);
    m_gridWidthSpin->setValue(p.tilesPerRow);
    m_gridWidthSpin->blockSignals(false);

    // Match pattern in combo
    for (int i = 0; i < m_patternCombo->count(); ++i) {
        if (m_patternCombo->itemData(i).toInt() == static_cast<int>(p.pattern)) {
            m_patternCombo->setCurrentIndex(i);
            break;
        }
    }

    if (p.paletteOffset >= 0 && p.paletteOffset + 32 <= m_fileData.size()) {
        m_activePalette.resize(16);
        const uchar *pRaw = reinterpret_cast<const uchar*>(m_fileData.constData()) + p.paletteOffset;
        for (int i = 0; i < 16; ++i) {
            quint16 word = qFromLittleEndian<quint16>(pRaw + (i * 2));
            int a = (m_transpZeroCheck->isChecked() && i == 0) ? 0 : 255;
            m_activePalette[i] = SnesSavestateData::bgr555ToRgb(word, a);
        }
    }

    updatePaletteSwatches();
    updateDecoding();
}

void SnesDialog::onLoadPresetClicked()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open SNES Preset File"), QString(), tr("SNES Presets (*.stk *.json);;All Files (*.*)"));
    if (path.isEmpty()) return;

    SnesPreset p = SnesPresetManager::loadFromFile(path);
    if (p.isValid()) {
        m_presets.append(p);
        m_presetCombo->addItem(QStringLiteral("%1 - %2").arg(p.gameName, p.title));
        m_presetCombo->setCurrentIndex(m_presetCombo->count() - 1);
    } else {
        QMessageBox::warning(this, tr("Invalid Preset"), tr("Could not parse preset file."));
    }
}

void SnesDialog::onLoadExternalPaletteClicked()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Load External Palette or Savestate"), QString(),
        tr("SNES Savestates & Dumps (*.s96 *.000 *.001 *.zst *.cgram *.pal *.bin);;All Files (*.*)"));
    if (path.isEmpty()) return;

    SnesSavestateData extState = SnesSavestateLoader::load(path);
    if (!extState.cgram.isEmpty()) {
        m_savestate.cgram = extState.cgram;
        m_activePalette = m_savestate.getPalette(m_savestatePaletteCombo ? m_savestatePaletteCombo->currentIndex() : 8, m_transpZeroCheck->isChecked());
        updatePaletteSwatches();
        updateDecoding();
        QMessageBox::information(this, tr("Palette Loaded"), tr("Loaded 256 hardware colors from %1!").arg(QFileInfo(path).fileName()));
    } else {
        QMessageBox::warning(this, tr("No CGRAM Found"), tr("Could not find CGRAM palette data in the selected file."));
    }
}

void SnesDialog::onPaletteIndexChanged(int index)
{
    if (m_savestate.isValid || !m_savestate.cgram.isEmpty()) {
        m_activePalette = m_savestate.getPalette(index, m_transpZeroCheck->isChecked());
    }
    updatePaletteSwatches();
    updateDecoding();
}

void SnesDialog::onPatternChanged()
{
    updateDecoding();
}

void SnesDialog::onConstraintChanged()
{
    updateDecoding();
}

void SnesDialog::onZoomChanged()
{
    updateDecoding();
}

void SnesDialog::updatePaletteSwatches()
{
    if (m_activePalette.isEmpty()) {
        m_activePalette.resize(16);
        for (int i = 0; i < 16; ++i) {
            int v = (i * 255) / 15;
            int a = (m_transpZeroCheck->isChecked() && i == 0) ? 0 : 255;
            m_activePalette[i] = qRgba(v, v, v, a);
        }
    }

    QImage swatchImg(16 * 16, 24, QImage::Format_ARGB32);
    swatchImg.fill(Qt::transparent);
    QPainter p(&swatchImg);

    for (int i = 0; i < 16; ++i) {
        QRect r(i * 16, 0, 16, 24);
        p.fillRect(r, QColor(m_activePalette[i]));
        p.setPen(QColor(60, 60, 60));
        p.drawRect(r);
    }
    p.end();

    m_swatchesWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *l = m_swatchesWidget->layout();
    if (!l) {
        l = new QHBoxLayout(m_swatchesWidget);
        l->setContentsMargins(0, 0, 0, 0);
        auto *lbl = new QLabel(m_swatchesWidget);
        lbl->setObjectName(QStringLiteral("swatchLabel"));
        l->addWidget(lbl);
    }
    auto *lbl = m_swatchesWidget->findChild<QLabel*>(QStringLiteral("swatchLabel"));
    if (lbl) {
        lbl->setPixmap(QPixmap::fromImage(swatchImg));
    }
}

void SnesDialog::updateDecoding()
{
    QByteArray dataToDecode;
    int offset = 0;

    if (!m_overrideData.isEmpty()) {
        // Decompressed bank or explicit stream
        dataToDecode = m_overrideData;
        offset = 0;
    } else {
        int curTab = m_tabWidget->currentIndex();
        if (curTab == 0 && m_isSavestate && !m_savestate.vram.isEmpty()) {
            dataToDecode = m_savestate.vram;
            offset = 0;
        } else {
            bool ok = false;
            QString pcText = m_pcOffsetEdit->text().trimmed();
            if (pcText.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) pcText.remove(0, 2);
            offset = pcText.toInt(&ok, 16);
            if (!ok) offset = 0;

            int maxLen = m_dataLengthSpin->value();
            if (offset < m_fileData.size()) {
                dataToDecode = m_fileData.mid(offset, maxLen);
            }
        }
    }

    if (dataToDecode.isEmpty()) {
        m_previewLabel->setText(tr("No data available to decode."));
        return;
    }

    RetroFormat format = static_cast<RetroFormat>(m_bppCombo->currentData().toInt());
    TilePattern pattern = static_cast<TilePattern>(m_patternCombo->currentData().toInt());
    int gridWidth = m_gridWidthSpin->value();

    if (!m_activePalette.isEmpty()) {
        int a0 = m_transpZeroCheck->isChecked() ? 0 : 255;
        m_activePalette[0] = qRgba(qRed(m_activePalette[0]), qGreen(m_activePalette[0]), qBlue(m_activePalette[0]), a0);
    }

    m_decodedImage = RetroTileDecoder::decodeTileSheet(
        reinterpret_cast<const uchar*>(dataToDecode.constData()),
        dataToDecode.size(),
        format,
        gridWidth,
        8192,
        m_activePalette,
        m_transpZeroCheck->isChecked(),
        pattern
    );

    if (m_decodedImage.isNull()) {
        m_previewLabel->setText(tr("Failed to decode tiles."));
        return;
    }

    // Auto-detect sprite bounding boxes using SpriteDetector with user constraints
    m_detectedBoxes.clear();
    if (m_highlightSpritesCheck->isChecked()) {
        QList<SpriteBox> boxes;
        SpriteDetectionOptions detOpts;
        detOpts.alphaThreshold = 1;
        detOpts.minSliceSize = 3;

        int sizeIdx = m_maxSizeCombo->currentIndex();
        if (sizeIdx == 1) { // 16x16
            detOpts.maxWidth = 16;
            detOpts.maxHeight = 16;
        } else if (sizeIdx == 2) { // 32x32
            detOpts.maxWidth = 32;
            detOpts.maxHeight = 32;
        } else if (sizeIdx == 3) { // 32x48 permissive tall
            detOpts.maxWidth = 32;
            detOpts.maxHeight = 32;
            detOpts.maxDimensionPermissive = 48;
        } else if (sizeIdx == 4) { // 48x48
            detOpts.maxWidth = 48;
            detOpts.maxHeight = 48;
        } else if (sizeIdx == 5) { // 64x64
            detOpts.maxWidth = 64;
            detOpts.maxHeight = 64;
        }

        detOpts.disallowOverlaps = m_disallowOverlapsCheck->isChecked();
        detOpts.alignToGrid = m_alignToGridCheck->isChecked();
        detOpts.gridCellSize = (pattern == TilePattern::Linear) ? 8 : 16;

        if (SpriteDetector::detectBoxes(m_decodedImage, boxes, detOpts)) {
            for (const SpriteBox &b : boxes) {
                m_detectedBoxes.append(b.rect);
            }
        }
    }

    // Render preview with Zoom & Grid
    int zoomFactor = m_zoomCombo->currentIndex() + 1;
    if (m_zoomCombo->currentIndex() == 4) zoomFactor = 6;
    if (m_zoomCombo->currentIndex() == 5) zoomFactor = 8;

    QImage displayImg(m_decodedImage.width() * zoomFactor, m_decodedImage.height() * zoomFactor, QImage::Format_ARGB32);
    displayImg.fill(QColor(30, 30, 30));

    QPainter painter(&displayImg);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    // Draw scaled tiles
    painter.drawImage(QRect(0, 0, displayImg.width(), displayImg.height()), m_decodedImage);

    // Draw Tile Grid
    if (m_showGridCheck->isChecked()) {
        painter.setPen(QPen(QColor(255, 255, 255, 40), 1, Qt::DotLine));
        int step = 8 * zoomFactor;
        for (int x = 0; x < displayImg.width(); x += step) {
            painter.drawLine(x, 0, x, displayImg.height());
        }
        for (int y = 0; y < displayImg.height(); y += step) {
            painter.drawLine(0, y, displayImg.width(), y);
        }
    }

    // Draw Highlight Bounding Boxes
    if (m_highlightSpritesCheck->isChecked()) {
        painter.setPen(QPen(QColor(0, 230, 118, 220), 2, Qt::SolidLine));
        painter.setBrush(QColor(0, 230, 118, 35));
        for (const QRect &box : m_detectedBoxes) {
            QRect scaledBox(box.x() * zoomFactor, box.y() * zoomFactor, box.width() * zoomFactor, box.height() * zoomFactor);
            painter.drawRect(scaledBox);
        }
    }
    painter.end();

    m_previewLabel->setPixmap(QPixmap::fromImage(displayImg));

    int tileBytes = RetroTileDecoder::bytesPerTile(format);
    int totalTiles = tileBytes > 0 ? (dataToDecode.size() / tileBytes) : 0;
    m_statusLabel->setText(tr("Decoded: %1 tiles (%2x%3 px) | Zoom: %4x | Distinct Sprites: %5 | Source: %6")
        .arg(totalTiles)
        .arg(m_decodedImage.width())
        .arg(m_decodedImage.height())
        .arg(zoomFactor)
        .arg(m_detectedBoxes.size())
        .arg(m_overrideData.isEmpty() ? QStringLiteral("0x%1").arg(QString::number(offset, 16).toUpper().rightJustified(6, '0')) : tr("Decompressed Stream")));
}

void SnesDialog::onExtractSpritesClicked()
{
    m_extractAsSprites = true;
    accept();
}

void SnesDialog::onImportAtlasClicked()
{
    m_extractAsSprites = false;
    accept();
}
