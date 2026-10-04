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

#include "retropalettefilterdialog.h"
#include "commands/filtercommands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace {

// Bayer 2x2 matrix normalized to [0, 3]
const int BAYER_2X2[2][2] = {
    { 0, 2 },
    { 3, 1 }
};

// Bayer 4x4 matrix normalized to [0, 15]
const int BAYER_4X4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

// Bayer 8x8 matrix normalized to [0, 63]
const int BAYER_8X8[8][8] = {
    {  0, 32,  8, 40,  2, 34, 10, 42 },
    { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44,  4, 36, 14, 46,  6, 38 },
    { 60, 28, 52, 20, 62, 30, 54, 22 },
    {  3, 35, 11, 43,  1, 33,  9, 41 },
    { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47,  7, 39, 13, 45,  5, 37 },
    { 63, 31, 55, 23, 61, 29, 53, 21 }
};

} // namespace

RetroPaletteFilterDialog::RetroPaletteFilterDialog(SpriteDocument *doc,
                                                   QUndoStack *undoStack,
                                                   QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Retro Palette & Dithering"));
    setMinimumWidth(480);

    m_activePalette = getPresetPalette(GameBoyDMG);

    setupFilterUI();
    schedulePreview();
}

void RetroPaletteFilterDialog::setupFilterUI()
{
    // Group 1: Palette Selection
    QGroupBox *paletteGroup = new QGroupBox(tr("Retro Hardware Palette"), this);
    QVBoxLayout *paletteLayout = new QVBoxLayout(paletteGroup);

    QHBoxLayout *topRow = new QHBoxLayout();
    QLabel *lblPreset = new QLabel(tr("Palette Preset:"), this);
    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem(tr("Game Boy DMG (4 Greens)"), GameBoyDMG);
    m_presetCombo->addItem(tr("Game Boy Pocket (4 Grays)"), GameBoyPocket);
    m_presetCombo->addItem(tr("PICO-8 (16 Colors)"), Pico8);
    m_presetCombo->addItem(tr("NES / Famicom (54 Colors)"), NES);
    m_presetCombo->addItem(tr("Commodore 64 (16 Colors)"), Commodore64);
    m_presetCombo->addItem(tr("CGA Mode 1 (Cyan/Magenta/White)"), CGAMode1);
    m_presetCombo->addItem(tr("CGA Mode 2 (Red/Green/Yellow)"), CGAMode2);
    m_presetCombo->addItem(tr("Endesga 32 (32 Pixel Art Colors)"), Endesga32);
    m_presetCombo->addItem(tr("Custom / Imported Palette"), Custom);
    topRow->addWidget(lblPreset);
    topRow->addWidget(m_presetCombo, 1);

    m_importBtn = new QPushButton(tr("Import..."), this);
    m_importBtn->setToolTip(tr("Import palette from .hex, .gpl, .pal or .png image"));
    topRow->addWidget(m_importBtn);
    paletteLayout->addLayout(topRow);

    // Swatch preview area
    m_swatchContainer = new QWidget(this);
    m_swatchContainer->setMinimumHeight(24);
    paletteLayout->addWidget(m_swatchContainer);

    m_paletteInfoLabel = new QLabel(this);
    m_paletteInfoLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
    paletteLayout->addWidget(m_paletteInfoLabel);

    contentLayout()->addWidget(paletteGroup);

    // Group 2: Dithering Engine
    QGroupBox *ditherGroup = new QGroupBox(tr("Ordered Dithering (Bayer Matrix)"), this);
    QGridLayout *ditherLayout = new QGridLayout(ditherGroup);

    QLabel *lblDither = new QLabel(tr("Dither Pattern:"), this);
    m_ditherCombo = new QComboBox(this);
    m_ditherCombo->addItem(tr("None (Exact Nearest Match)"), DitherNone);
    m_ditherCombo->addItem(tr("Bayer 2x2 Matrix"), Bayer2x2);
    m_ditherCombo->addItem(tr("Bayer 4x4 Matrix (Classic Retro)"), Bayer4x4);
    m_ditherCombo->addItem(tr("Bayer 8x8 Matrix (Smooth Gradients)"), Bayer8x8);
    m_ditherCombo->setCurrentIndex(2); // Default to Bayer 4x4

    ditherLayout->addWidget(lblDither, 0, 0);
    ditherLayout->addWidget(m_ditherCombo, 0, 1, 1, 2);

    QLabel *lblStrength = new QLabel(tr("Dither Strength:"), this);
    m_strengthSlider = new QSlider(Qt::Horizontal, this);
    m_strengthSlider->setRange(0, 100);
    m_strengthSlider->setValue(50);
    m_strengthSpin = new QSpinBox(this);
    m_strengthSpin->setRange(0, 100);
    m_strengthSpin->setValue(50);
    m_strengthSpin->setSuffix(QStringLiteral("%"));
    m_strengthSpin->setFixedWidth(65);

    ditherLayout->addWidget(lblStrength, 1, 0);
    ditherLayout->addWidget(m_strengthSlider, 1, 1);
    ditherLayout->addWidget(m_strengthSpin, 1, 2);

    contentLayout()->addWidget(ditherGroup);

    // Group 3: Scope
    QGroupBox *scopeGroup = new QGroupBox(tr("Target Scope"), this);
    QVBoxLayout *scopeLayout = new QVBoxLayout(scopeGroup);

    m_selectedFramesOnlyCheck = new QCheckBox(tr("Apply to selected frames only"), this);
    bool hasSelection = m_document && !m_document->selectedFrameIndices().isEmpty();
    m_selectedFramesOnlyCheck->setChecked(hasSelection);
    m_selectedFramesOnlyCheck->setEnabled(hasSelection);
    if (!hasSelection) {
        m_selectedFramesOnlyCheck->setToolTip(tr("No frames selected: applies to entire atlas"));
    }

    scopeLayout->addWidget(m_selectedFramesOnlyCheck);
    contentLayout()->addWidget(scopeGroup);

    updatePalettePreview();

    // Wire events
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RetroPaletteFilterDialog::onPresetChanged);
    connect(m_importBtn, &QPushButton::clicked, this, &RetroPaletteFilterDialog::importPalette);
    connect(m_ditherCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RetroPaletteFilterDialog::onParametersChanged);

    connect(m_strengthSlider, &QSlider::valueChanged, m_strengthSpin, &QSpinBox::setValue);
    connect(m_strengthSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_strengthSlider, &QSlider::setValue);
    connect(m_strengthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &RetroPaletteFilterDialog::onParametersChanged);

    connect(m_selectedFramesOnlyCheck, &QCheckBox::toggled, this, &RetroPaletteFilterDialog::onParametersChanged);
}

RetroPaletteFilterDialog::Preset RetroPaletteFilterDialog::activePreset() const
{
    return m_presetCombo ? static_cast<Preset>(m_presetCombo->currentData().toInt()) : GameBoyDMG;
}

RetroPaletteFilterDialog::DitherMatrix RetroPaletteFilterDialog::ditherMatrix() const
{
    return m_ditherCombo ? static_cast<DitherMatrix>(m_ditherCombo->currentData().toInt()) : Bayer4x4;
}

int RetroPaletteFilterDialog::ditherStrength() const
{
    return m_strengthSpin ? m_strengthSpin->value() : 50;
}

bool RetroPaletteFilterDialog::isSelectedFramesOnly() const
{
    return m_selectedFramesOnlyCheck && m_selectedFramesOnlyCheck->isChecked();
}

QVector<QRgb> RetroPaletteFilterDialog::currentPalette() const
{
    return m_activePalette;
}

void RetroPaletteFilterDialog::onPresetChanged(int index)
{
    Preset preset = static_cast<Preset>(m_presetCombo->itemData(index).toInt());
    if (preset == Custom) {
        if (m_customPalette.isEmpty()) {
            importPalette();
            return;
        }
        m_activePalette = m_customPalette;
    } else {
        m_activePalette = getPresetPalette(preset);
    }
    updatePalettePreview();
    schedulePreview();
}

void RetroPaletteFilterDialog::importPalette()
{
    QString path = QFileDialog::getOpenFileName(this,
        tr("Import Color Palette"),
        QString(),
        tr("Palette Files (*.hex *.gpl *.pal *.png *.bmp);;All Files (*)"));

    if (path.isEmpty()) return;

    QString error;
    QVector<QRgb> pal = loadPaletteFromFile(path, &error);
    if (pal.isEmpty()) {
        QMessageBox::warning(this, tr("Import Failed"),
                             error.isEmpty() ? tr("No valid colors found in file.") : error);
        return;
    }

    m_customPalette = pal;
    m_activePalette = pal;

    // Set combo to Custom
    int customIdx = m_presetCombo->findData(Custom);
    if (customIdx >= 0) {
        m_presetCombo->setCurrentIndex(customIdx);
    }

    updatePalettePreview();
    schedulePreview();
}

void RetroPaletteFilterDialog::updatePalettePreview()
{
    if (!m_swatchContainer) return;

    // Clear old swatch layout
    qDeleteAll(m_swatchContainer->children());

    QHBoxLayout *layout = new QHBoxLayout(m_swatchContainer);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(2);

    int maxDisplay = std::min(64, static_cast<int>(m_activePalette.size()));
    for (int i = 0; i < maxDisplay; ++i) {
        QLabel *swatch = new QLabel(m_swatchContainer);
        swatch->setFixedSize(14, 18);
        QColor c(m_activePalette[i]);
        swatch->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #444; border-radius: 1px;")
                              .arg(c.name()));
        layout->addWidget(swatch);
    }
    layout->addStretch();

    if (m_paletteInfoLabel) {
        m_paletteInfoLabel->setText(tr("%1 active color(s)").arg(m_activePalette.size()));
    }
}

void RetroPaletteFilterDialog::onParametersChanged()
{
    schedulePreview();
}

void RetroPaletteFilterDialog::resetDefaults()
{
    if (m_presetCombo) m_presetCombo->setCurrentIndex(0); // GameBoyDMG
    if (m_ditherCombo) m_ditherCombo->setCurrentIndex(2); // Bayer 4x4
    if (m_strengthSlider) m_strengthSlider->setValue(50);
    m_activePalette = getPresetPalette(GameBoyDMG);
    updatePalettePreview();
}

void RetroPaletteFilterDialog::saveSettings()
{
}

QVector<QRgb> RetroPaletteFilterDialog::getPresetPalette(Preset preset)
{
    return ColorPalettePresets::getPresetPalette(static_cast<ColorPalettePresets::Preset>(preset));
}

QVector<QRgb> RetroPaletteFilterDialog::loadPaletteFromFile(const QString &filePath, QString *outError)
{
    return ColorPalettePresets::loadPaletteFromFile(filePath, outError);
}

QImage RetroPaletteFilterDialog::applyRetroPalette(const QImage &source,
                                                   const QVector<QRgb> &palette,
                                                   DitherMatrix dither,
                                                   int strengthPercent,
                                                   const QList<QRect> &targetAreas)
{
    if (source.isNull() || palette.isEmpty()) return source;

    QImage dest = source.convertToFormat(QImage::Format_ARGB32);
    int w = dest.width();
    int h = dest.height();

    double ditherScale = (strengthPercent / 100.0);

    // Fast nearest color finder using weighted human-eye perceptual color difference:
    // deltaE^2 = 2 * (dR)^2 + 4 * (dG)^2 + 3 * (dB)^2
    auto findNearestColor = [&](int r, int g, int b) -> QRgb {
        int bestDist = 100000000;
        QRgb bestColor = palette[0];

        for (QRgb palColor : palette) {
            int dR = r - qRed(palColor);
            int dG = g - qGreen(palColor);
            int dB = b - qBlue(palColor);
            int dist = 2 * dR * dR + 4 * dG * dG + 3 * dB * dB;
            if (dist < bestDist) {
                bestDist = dist;
                bestColor = palColor;
                if (dist == 0) break;
            }
        }
        return bestColor;
    };

    auto processPixel = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        QRgb p = source.pixel(x, y);
        int a = qAlpha(p);
        if (a == 0) return;

        int r = qRed(p);
        int g = qGreen(p);
        int b = qBlue(p);

        if (dither != DitherNone && ditherScale > 0.0) {
            double threshold = 0.0;
            if (dither == Bayer2x2) {
                threshold = (BAYER_2X2[y % 2][x % 2] / 4.0) - 0.5;
            } else if (dither == Bayer4x4) {
                threshold = (BAYER_4X4[y % 4][x % 4] / 16.0) - 0.5;
            } else if (dither == Bayer8x8) {
                threshold = (BAYER_8X8[y % 8][x % 8] / 64.0) - 0.5;
            }

            int offset = qRound(threshold * ditherScale * 64.0);
            r = std::clamp(r + offset, 0, 255);
            g = std::clamp(g + offset, 0, 255);
            b = std::clamp(b + offset, 0, 255);
        }

        QRgb matched = findNearestColor(r, g, b);
        dest.setPixel(x, y, qRgba(qRed(matched), qGreen(matched), qBlue(matched), a));
    };

    if (targetAreas.isEmpty()) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                processPixel(x, y);
            }
        }
    } else {
        for (const QRect &rect : targetAreas) {
            QRect bounded = rect.intersected(QRect(0, 0, w, h));
            for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
                for (int x = bounded.left(); x <= bounded.right(); ++x) {
                    processPixel(x, y);
                }
            }
        }
    }

    return dest;
}

void RetroPaletteFilterDialog::applyPreview()
{
    if (!m_document || m_initialAtlas.isNull() || m_activePalette.isEmpty()) return;

    QList<QRect> targetAreas;
    if (isSelectedFramesOnly()) {
        QList<int> sel = m_document->selectedFrameIndices();
        for (int idx : sel) {
            if (idx >= 0 && idx < m_initialBoxes.size()) {
                targetAreas.append(m_initialBoxes[idx].rect);
            }
        }
    }

    m_previewAtlas = applyRetroPalette(m_initialAtlas, m_activePalette, ditherMatrix(), ditherStrength(), targetAreas);

    updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes);

    m_document->setAtlas(m_previewAtlas);
    m_document->setFrames(m_previewFrames, m_previewBoxes);

    QString ditherLabel;
    switch (ditherMatrix()) {
    case Bayer2x2: ditherLabel = QStringLiteral("Bayer 2x2"); break;
    case Bayer4x4: ditherLabel = QStringLiteral("Bayer 4x4"); break;
    case Bayer8x8: ditherLabel = QStringLiteral("Bayer 8x8"); break;
    case DitherNone:
    default:       ditherLabel = QStringLiteral("No Dither"); break;
    }

    if (isAutoDetectBoxesEnabled()) {
        setStatusText(tr("Quantized (%1 colors, %2)").arg(m_activePalette.size()).arg(ditherLabel)
                      + QStringLiteral(" — ") + tr("%1 frame(s) detected").arg(m_previewBoxes.size()));
    } else {
        setStatusText(tr("Quantized (%1 colors, %2)").arg(m_activePalette.size()).arg(ditherLabel));
    }
}

QUndoCommand* RetroPaletteFilterDialog::createUndoCommand()
{
    if (!m_document || m_previewAtlas.isNull()) return nullptr;

    return new ApplyFilterCommand(
        m_document,
        tr("Filter: Retro Palette & Dithering"),
        m_initialAtlas, m_initialFrames, m_initialBoxes, m_initialAnimations,
        m_previewAtlas, m_previewFrames, m_previewBoxes, m_initialAnimations
    );
}
