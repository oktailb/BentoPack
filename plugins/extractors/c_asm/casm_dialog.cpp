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

#include "casm_dialog.h"
#include "casm_generator.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QTimer>

namespace Retro {

CAsmDialog::CAsmDialog(const SpriteDocument &doc, const QString &baseName, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_baseName(baseName)
{
    setWindowTitle(tr("C / C++ & Assembly Retro Code Exporter"));
    resize(860, 680);

    m_config.loadFromSettings();
    setupUi();
    populateControlsFromConfig();
    updatePreview();
}

void CAsmDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // Top: Target System Preset
    QGroupBox *grpPreset = new QGroupBox(tr("Target Retro Architecture Preset"), this);
    QHBoxLayout *presetLayout = new QHBoxLayout(grpPreset);

    m_comboPreset = new QComboBox(grpPreset);
    m_comboPreset->addItem(QStringLiteral("🎮 Sega Mega Drive / Genesis (SGDK - 68000)"), System_MegaDrive_SGDK);
    m_comboPreset->addItem(QStringLiteral("🕹️ Nintendo Game Boy / GBC (GBDK / RGBDS - SM83)"), System_GameBoy_GBDK);
    m_comboPreset->addItem(QStringLiteral("🍄 Nintendo NES / Famicom (cc65 / ca65 - 6502)"), System_NES_cc65);
    m_comboPreset->addItem(QStringLiteral("💾 Commodore Amiga 500/1200 (VASM / NDK - 68000)"), System_Amiga_OCS);
    m_comboPreset->addItem(QStringLiteral("🕹️ Atari ST (VASM / GCC - 68000)"), System_Atari_ST);
    m_comboPreset->addItem(QStringLiteral("🎮 Super Nintendo / SNES (pvsneslib - 65816)"), System_SNES_pvsneslib);
    m_comboPreset->addItem(QStringLiteral("⌨️ Commodore 64 (cc65 / KickAssembler - 6502)"), System_Commodore_64);
    m_comboPreset->addItem(QStringLiteral("🎮 Sega Master System / Game Gear (devkitSMS - Z80)"), System_MasterSystem_devkitSMS);
    m_comboPreset->addItem(QStringLiteral("📼 Amstrad CPC 464/6128 (cpctelera / RASM - Z80)"), System_Amstrad_CPC);
    m_comboPreset->addItem(QStringLiteral("🌈 Sinclair ZX Spectrum (z88dk / SJASMPLUS - Z80)"), System_ZX_Spectrum);
    m_comboPreset->addItem(QStringLiteral("💻 MS-DOS PC VGA Mode 13h (Open Watcom - x86)"), System_MSDOS_Mode13h);
    m_comboPreset->addItem(QStringLiteral("🐱 PICO-8 Fantasy Console (Lua Hex)"), System_Pico8);
    m_comboPreset->addItem(QStringLiteral("⚙️ Generic C/C++ Linear Array"), System_Generic_C);

    presetLayout->addWidget(m_comboPreset);
    mainLayout->addWidget(grpPreset);

    // Middle Split: Left Controls / Right Live Code Preview
    QHBoxLayout *midLayout = new QHBoxLayout();

    // Left Column: Parameters
    QWidget *leftCol = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftCol);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *grpFormat = new QGroupBox(tr("Language & Pixel Layout"), leftCol);
    QFormLayout *formFormat = new QFormLayout(grpFormat);

    m_comboLanguage = new QComboBox(grpFormat);
    m_comboLanguage->addItem(tr("C / C++ Header (.h)"), Lang_C_Header);
    m_comboLanguage->addItem(tr("C Source (.c)"), Lang_C_Source);
    m_comboLanguage->addItem(tr("Assembly Motorola / VASM (68000)"), Lang_ASM_Motorola_VASM);
    m_comboLanguage->addItem(tr("Assembly RGBDS (Game Boy)"), Lang_ASM_RGBDS);
    m_comboLanguage->addItem(tr("Assembly ca65 / cc65 (6502)"), Lang_ASM_ca65);
    m_comboLanguage->addItem(tr("Assembly NASM / TASM (x86)"), Lang_ASM_NASM);
    m_comboLanguage->addItem(tr("Assembly GNU as (.byte/.long)"), Lang_ASM_GNU);
    m_comboLanguage->addItem(tr("Assembly WLA-DX (Z80)"), Lang_ASM_WLA_DX);
    m_comboLanguage->addItem(tr("PICO-8 Lua String/Table"), Lang_PICO8_Lua);
    formFormat->addRow(tr("Language:"), m_comboLanguage);

    m_comboLayout = new QComboBox(grpFormat);
    m_comboLayout->addItem(tr("Planar 2bpp Tiles (8x8) [GB/NES]"), Layout_Planar2bppTiles_8x8);
    m_comboLayout->addItem(tr("Planar 4bpp Tiles (8x8) [SMS/MD/SNES]"), Layout_Planar4bppTiles_8x8);
    m_comboLayout->addItem(tr("Linear 4bpp Packed (2 px/byte)"), Layout_Linear4bpp_Packed);
    m_comboLayout->addItem(tr("Linear 8bpp Indexed (1 byte/px)"), Layout_Linear8bpp);
    m_comboLayout->addItem(tr("Amiga Planar Bitplanes (1-5 planes)"), Layout_Amiga_Bitplanes);
    m_comboLayout->addItem(tr("C64 Multicolor Sprite (24x21, 64B)"), Layout_C64_Multicolor_Sprite);
    m_comboLayout->addItem(tr("ZX Spectrum (1bpp + 8x8 attrs)"), Layout_ZX_Spectrum);
    m_comboLayout->addItem(tr("Linear 1bpp Mono (8 px/byte)"), Layout_Linear1bpp_Mono);
    formFormat->addRow(tr("Tile/Pixel Layout:"), m_comboLayout);

    m_comboNumberFormat = new QComboBox(grpFormat);
    m_comboNumberFormat->addItem(QStringLiteral("Hex 0x (0x1F)"), Num_Hex_0x);
    m_comboNumberFormat->addItem(QStringLiteral("Hex $ ($1F)"), Num_Hex_Dollar);
    m_comboNumberFormat->addItem(QStringLiteral("Hex # (#1F)"), Num_Hex_Hash);
    m_comboNumberFormat->addItem(QStringLiteral("Decimal (31)"), Num_Dec);
    m_comboNumberFormat->addItem(QStringLiteral("Binary (%00011111)"), Num_Binary);
    formFormat->addRow(tr("Number Format:"), m_comboNumberFormat);

    m_spinBytesPerLine = new QSpinBox(grpFormat);
    m_spinBytesPerLine->setRange(4, 64);
    m_spinBytesPerLine->setValue(16);
    formFormat->addRow(tr("Bytes per Line:"), m_spinBytesPerLine);

    m_txtPrefix = new QLineEdit(grpFormat);
    m_txtPrefix->setPlaceholderText(tr("Optional symbol prefix"));
    formFormat->addRow(tr("Symbol Prefix:"), m_txtPrefix);

    m_txtCustomType = new QLineEdit(grpFormat);
    m_txtCustomType->setText(QStringLiteral("uint8_t"));
    formFormat->addRow(tr("Data Type (C):"), m_txtCustomType);

    m_txtStorageQualifier = new QLineEdit(grpFormat);
    m_txtStorageQualifier->setPlaceholderText(tr("e.g. const, PROGMEM, ALIGN(2)"));
    formFormat->addRow(tr("Storage Qualifier:"), m_txtStorageQualifier);

    leftLayout->addWidget(grpFormat);

    QGroupBox *grpOptions = new QGroupBox(tr("Export Sections & Linter"), leftCol);
    QVBoxLayout *optLayout = new QVBoxLayout(grpOptions);

    m_chkExportPalette = new QCheckBox(tr("Export Hardware Color Palette"), grpOptions);
    m_chkExportAnimTable = new QCheckBox(tr("Export Animation Sequences Table"), grpOptions);
    m_chkExportMetadata = new QCheckBox(tr("Export Sprite Metadata Struct (Pivot, Size)"), grpOptions);
    m_chkHeaderGuards = new QCheckBox(tr("Include C Header Guards (#ifndef)"), grpOptions);
    m_chkLinterComments = new QCheckBox(tr("Include Scanline & Hardware Linter Warnings"), grpOptions);

    optLayout->addWidget(m_chkExportPalette);
    optLayout->addWidget(m_chkExportAnimTable);
    optLayout->addWidget(m_chkExportMetadata);
    optLayout->addWidget(m_chkHeaderGuards);
    optLayout->addWidget(m_chkLinterComments);

    leftLayout->addWidget(grpOptions);
    leftLayout->addStretch();
    midLayout->addWidget(leftCol, 1);

    // Right Column: Live Syntax-Highlighted Preview & Footprint Badge
    QWidget *rightCol = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightCol);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *badgeLayout = new QHBoxLayout();
    m_lblFootprint = new QLabel(this);
    m_lblFootprint->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px 8px; background: #2c3e50; color: #2ecc71; border-radius: 4px;"));
    badgeLayout->addWidget(m_lblFootprint);

    m_btnCopy = new QPushButton(tr("📋 Copy Code"), this);
    badgeLayout->addWidget(m_btnCopy);
    rightLayout->addLayout(badgeLayout);

    m_lblWarning = new QLabel(this);
    m_lblWarning->setStyleSheet(QStringLiteral("color: #e67e22; font-weight: bold; font-size: 11px; padding: 6px; background: rgba(230, 126, 34, 0.12); border: 1px solid #e67e22; border-radius: 4px;"));
    m_lblWarning->setWordWrap(true);
    m_lblWarning->setMaximumHeight(70);
    m_lblWarning->setVisible(false);
    rightLayout->addWidget(m_lblWarning);

    m_txtPreview = new QPlainTextEdit(rightCol);
    m_txtPreview->setReadOnly(true);
    m_txtPreview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_txtPreview->setStyleSheet(QStringLiteral("QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; }"));
    rightLayout->addWidget(m_txtPreview, 1);

    midLayout->addWidget(rightCol, 2);
    mainLayout->addLayout(midLayout, 1);

    // Bottom standard buttons
    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Ok)->setText(tr("Export Code"));
    btnBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        m_config.saveToSettings();
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);

    // Connections
    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CAsmDialog::onPresetChanged);
    connect(m_comboLanguage, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CAsmDialog::onControlChanged);
    connect(m_comboLayout, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CAsmDialog::onControlChanged);
    connect(m_comboNumberFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CAsmDialog::onControlChanged);
    connect(m_spinBytesPerLine, QOverload<int>::of(&QSpinBox::valueChanged), this, &CAsmDialog::onControlChanged);
    connect(m_txtPrefix, &QLineEdit::textChanged, this, &CAsmDialog::onControlChanged);
    connect(m_txtCustomType, &QLineEdit::textChanged, this, &CAsmDialog::onControlChanged);
    connect(m_txtStorageQualifier, &QLineEdit::textChanged, this, &CAsmDialog::onControlChanged);

    connect(m_chkExportPalette, &QCheckBox::toggled, this, &CAsmDialog::onControlChanged);
    connect(m_chkExportAnimTable, &QCheckBox::toggled, this, &CAsmDialog::onControlChanged);
    connect(m_chkExportMetadata, &QCheckBox::toggled, this, &CAsmDialog::onControlChanged);
    connect(m_chkHeaderGuards, &QCheckBox::toggled, this, &CAsmDialog::onControlChanged);
    connect(m_chkLinterComments, &QCheckBox::toggled, this, &CAsmDialog::onControlChanged);

    connect(m_btnCopy, &QPushButton::clicked, this, &CAsmDialog::onCopyToClipboard);
}

void CAsmDialog::populateControlsFromConfig()
{
    int presetIdx = m_comboPreset->findData(m_config.targetSystem);
    if (presetIdx >= 0) m_comboPreset->setCurrentIndex(presetIdx);

    int langIdx = m_comboLanguage->findData(m_config.language);
    if (langIdx >= 0) m_comboLanguage->setCurrentIndex(langIdx);

    int layoutIdx = m_comboLayout->findData(m_config.layout);
    if (layoutIdx >= 0) m_comboLayout->setCurrentIndex(layoutIdx);

    int numIdx = m_comboNumberFormat->findData(m_config.numberFormat);
    if (numIdx >= 0) m_comboNumberFormat->setCurrentIndex(numIdx);

    m_spinBytesPerLine->setValue(m_config.bytesPerLine);
    m_txtPrefix->setText(m_config.identifierPrefix);
    m_txtCustomType->setText(m_config.customType);
    m_txtStorageQualifier->setText(m_config.storageQualifier);

    m_chkExportPalette->setChecked(m_config.exportPalette);
    m_chkExportAnimTable->setChecked(m_config.exportAnimationTable);
    m_chkExportMetadata->setChecked(m_config.exportMetadataStruct);
    m_chkHeaderGuards->setChecked(m_config.includeHeaderGuards);
    m_chkLinterComments->setChecked(m_config.includeLinterComments);
}

void CAsmDialog::onPresetChanged(int index)
{
    if (m_isUpdatingControls) return;

    m_isUpdatingControls = true;
    TargetSystem sys = static_cast<TargetSystem>(m_comboPreset->itemData(index).toInt());
    m_config.applyPreset(sys);
    populateControlsFromConfig();
    m_isUpdatingControls = false;
    updatePreview();
}

void CAsmDialog::onControlChanged()
{
    if (m_isUpdatingControls) return;

    m_config.targetSystem = static_cast<TargetSystem>(m_comboPreset->currentData().toInt());
    m_config.language = static_cast<OutputLanguage>(m_comboLanguage->currentData().toInt());
    m_config.layout = static_cast<PixelLayout>(m_comboLayout->currentData().toInt());
    m_config.numberFormat = static_cast<NumberFormat>(m_comboNumberFormat->currentData().toInt());
    m_config.bytesPerLine = m_spinBytesPerLine->value();
    m_config.identifierPrefix = m_txtPrefix->text().trimmed();
    m_config.customType = m_txtCustomType->text().trimmed();
    m_config.storageQualifier = m_txtStorageQualifier->text().trimmed();

    m_config.exportPalette = m_chkExportPalette->isChecked();
    m_config.exportAnimationTable = m_chkExportAnimTable->isChecked();
    m_config.exportMetadataStruct = m_chkExportMetadata->isChecked();
    m_config.includeHeaderGuards = m_chkHeaderGuards->isChecked();
    m_config.includeLinterComments = m_chkLinterComments->isChecked();

    // If layout was manually changed away from 2bpp planar, switch preset to Generic
    if ((m_config.targetSystem == System_GameBoy_GBDK || m_config.targetSystem == System_NES_cc65) &&
        m_config.layout != Layout_Planar2bppTiles_8x8) {
        m_isUpdatingControls = true;
        m_config.targetSystem = System_Generic_C;
        int gIdx = m_comboPreset->findData(System_Generic_C);
        if (gIdx >= 0) m_comboPreset->setCurrentIndex(gIdx);
        m_isUpdatingControls = false;
    }

    updatePreview();
}

void CAsmDialog::updatePreview()
{
    int tileCount = 0;
    int romBytes = CAsmGenerator::calculateRomFootprint(m_doc, m_config, &tileCount);
    m_lblFootprint->setText(tr("Total ROM: %1 Bytes (%2 KB) | %3 Tiles (8x8)")
        .arg(romBytes)
        .arg(QString::number(romBytes / 1024.0, 'f', 2))
        .arg(tileCount));

    QStringList warnings = CAsmGenerator::checkScanlineWarnings(m_doc, m_config);
    if (!warnings.isEmpty() && m_config.includeLinterComments) {
        m_lblWarning->setText(QStringLiteral("⚠️ ") + warnings.join(QStringLiteral("\n⚠️ ")));
        m_lblWarning->setVisible(true);
        m_lblFootprint->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px 8px; background: #c0392b; color: #ffffff; border-radius: 4px;"));
    } else {
        m_lblWarning->clear();
        m_lblWarning->setVisible(false);
        m_lblFootprint->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px 8px; background: #2c3e50; color: #2ecc71; border-radius: 4px;"));
    }

    QString code = CAsmGenerator::generate(m_doc, m_config, m_baseName);
    m_txtPreview->setPlainText(code);
}

void CAsmDialog::onCopyToClipboard()
{
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setText(m_txtPreview->toPlainText());
        m_btnCopy->setText(tr("✅ Copied!"));
        QTimer::singleShot(1500, this, [this]() {
            m_btnCopy->setText(tr("📋 Copy Code"));
        });
    }
}

} // namespace Retro
