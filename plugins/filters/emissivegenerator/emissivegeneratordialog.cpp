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

#include "emissivegeneratordialog.h"
#include "commands/filtercommands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>
#include <QGroupBox>
#include <cmath>
#include <algorithm>

EmissiveGeneratorDialog::EmissiveGeneratorDialog(SpriteDocument *doc,
                                                 QUndoStack *undoStack,
                                                 QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Emissive Map Generator "));
    setupFilterUI();
    resetDefaults();
    schedulePreview();
}

void EmissiveGeneratorDialog::setupFilterUI()
{
    // Mode selector
    QHBoxLayout *modeLayout = new QHBoxLayout();
    QLabel *modeLabel = new QLabel(tr("Extraction Method:"), this);
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Luminance Threshold (Highlights & Hotspots)"), LuminanceThreshold);
    m_modeCombo->addItem(tr("Color Key / Hue Isolation (Magic & Neon)"), ColorKeyIsolation);
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(m_modeCombo, 1);
    contentLayout()->addLayout(modeLayout);

    // 1. Luminance group
    m_lumGroup = new QGroupBox(tr("Luminance Threshold Parameters"), this);
    QGridLayout *lumLayout = new QGridLayout(m_lumGroup);

    QLabel *threshLabel = new QLabel(tr("Threshold:"), m_lumGroup);
    m_thresholdSlider = new QSlider(Qt::Horizontal, m_lumGroup);
    m_thresholdSlider->setRange(0, 255);
    m_thresholdSlider->setValue(180);

    m_thresholdSpin = new QSpinBox(m_lumGroup);
    m_thresholdSpin->setRange(0, 255);
    m_thresholdSpin->setValue(180);

    lumLayout->addWidget(threshLabel, 0, 0);
    lumLayout->addWidget(m_thresholdSlider, 0, 1);
    lumLayout->addWidget(m_thresholdSpin, 0, 2);

    QLabel *kneeLabel = new QLabel(tr("Soft Knee Falloff:"), m_lumGroup);
    m_kneeSlider = new QSlider(Qt::Horizontal, m_lumGroup);
    m_kneeSlider->setRange(0, 100);
    m_kneeSlider->setValue(20);

    m_kneeSpin = new QSpinBox(m_lumGroup);
    m_kneeSpin->setRange(0, 100);
    m_kneeSpin->setValue(20);

    lumLayout->addWidget(kneeLabel, 1, 0);
    lumLayout->addWidget(m_kneeSlider, 1, 1);
    lumLayout->addWidget(m_kneeSpin, 1, 2);

    contentLayout()->addWidget(m_lumGroup);

    // 2. Color key group
    m_colorGroup = new QGroupBox(tr("Selective Color Key Parameters"), this);
    QGridLayout *colorLayout = new QGridLayout(m_colorGroup);

    QLabel *targetLabel = new QLabel(tr("Target Glow Color:"), m_colorGroup);
    m_swatchLabel = new QLabel(m_colorGroup);
    m_swatchLabel->setFixedSize(36, 22);
    m_pickColorBtn = new QPushButton(tr("Choose..."), m_colorGroup);

    QHBoxLayout *swatchLayout = new QHBoxLayout();
    swatchLayout->addWidget(m_swatchLabel);
    swatchLayout->addWidget(m_pickColorBtn);
    swatchLayout->addStretch();

    colorLayout->addWidget(targetLabel, 0, 0);
    colorLayout->addLayout(swatchLayout, 0, 1, 1, 2);

    QLabel *hueLabel = new QLabel(tr("Hue Tolerance:"), m_colorGroup);
    m_hueTolSlider = new QSlider(Qt::Horizontal, m_colorGroup);
    m_hueTolSlider->setRange(1, 180);
    m_hueTolSlider->setValue(30);

    m_hueTolSpin = new QSpinBox(m_colorGroup);
    m_hueTolSpin->setRange(1, 180);
    m_hueTolSpin->setValue(30);
    m_hueTolSpin->setSuffix(tr("°"));

    colorLayout->addWidget(hueLabel, 1, 0);
    colorLayout->addWidget(m_hueTolSlider, 1, 1);
    colorLayout->addWidget(m_hueTolSpin, 1, 2);

    contentLayout()->addWidget(m_colorGroup);

    // 3. Boost & Output Settings
    QGroupBox *boostGroup = new QGroupBox(tr("Intensity & Output Format"), this);
    QGridLayout *boostLayout = new QGridLayout(boostGroup);

    QLabel *boostLabel = new QLabel(tr("Emission Boost Multiplier:"), boostGroup);
    m_multiplierSlider = new QSlider(Qt::Horizontal, boostGroup);
    m_multiplierSlider->setRange(50, 500);
    m_multiplierSlider->setValue(150); // 1.5x

    m_multiplierSpin = new QDoubleSpinBox(boostGroup);
    m_multiplierSpin->setRange(0.5, 5.0);
    m_multiplierSpin->setSingleStep(0.1);
    m_multiplierSpin->setValue(1.5);
    m_multiplierSpin->setSuffix(tr("x"));

    boostLayout->addWidget(boostLabel, 0, 0);
    boostLayout->addWidget(m_multiplierSlider, 0, 1);
    boostLayout->addWidget(m_multiplierSpin, 0, 2);

    m_blackBgCheck = new QCheckBox(tr("Opaque black background (unlit = RGB 0,0,0)"), boostGroup);
    boostLayout->addWidget(m_blackBgCheck, 1, 0, 1, 3);

    contentLayout()->addWidget(boostGroup);

    // 4. Destination & Target Selection
    QGroupBox *destGroup = new QGroupBox(tr("Destination & Target"), this);
    QVBoxLayout *destLayout = new QVBoxLayout(destGroup);

    QHBoxLayout *comboLayout = new QHBoxLayout();
    QLabel *destLabel = new QLabel(tr("Output Destination:"), destGroup);
    m_destCombo = new QComboBox(destGroup);
    m_destCombo->addItem(tr("New Layer (e.g. Emissive (_e))"), NewM21Layer);
    m_destCombo->addItem(tr("Active Sprite Sheet (Replace albedo)"), ActiveSpriteSheet);
    m_destCombo->addItem(tr("Auxiliary Emissive Map _e slave atlas)"), AuxiliaryEmissiveMap);

    comboLayout->addWidget(destLabel);
    comboLayout->addWidget(m_destCombo, 1);
    destLayout->addLayout(comboLayout);

    m_selectedOnlyCheck = new QCheckBox(tr("Process selected frames only"), destGroup);
    destLayout->addWidget(m_selectedOnlyCheck);

    contentLayout()->addWidget(destGroup);

    // Connect slider <-> spinbox
    connect(m_thresholdSlider, &QSlider::valueChanged, m_thresholdSpin, &QSpinBox::setValue);
    connect(m_thresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_thresholdSlider, &QSlider::setValue);
    connect(m_thresholdSlider, &QSlider::valueChanged, this, &EmissiveGeneratorDialog::onParametersChanged);

    connect(m_kneeSlider, &QSlider::valueChanged, m_kneeSpin, &QSpinBox::setValue);
    connect(m_kneeSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_kneeSlider, &QSlider::setValue);
    connect(m_kneeSlider, &QSlider::valueChanged, this, &EmissiveGeneratorDialog::onParametersChanged);

    connect(m_hueTolSlider, &QSlider::valueChanged, m_hueTolSpin, &QSpinBox::setValue);
    connect(m_hueTolSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_hueTolSlider, &QSlider::setValue);
    connect(m_hueTolSlider, &QSlider::valueChanged, this, &EmissiveGeneratorDialog::onParametersChanged);

    connect(m_multiplierSlider, &QSlider::valueChanged, this, [this](int v) {
        m_multiplierSpin->blockSignals(true);
        m_multiplierSpin->setValue(v / 100.0);
        m_multiplierSpin->blockSignals(false);
        onParametersChanged();
    });
    connect(m_multiplierSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_multiplierSlider->blockSignals(true);
        m_multiplierSlider->setValue(static_cast<int>(std::round(v * 100.0)));
        m_multiplierSlider->blockSignals(false);
        onParametersChanged();
    });

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EmissiveGeneratorDialog::updateModeVisibility);
    connect(m_pickColorBtn, &QPushButton::clicked, this, &EmissiveGeneratorDialog::pickTargetColor);
    connect(m_blackBgCheck, &QCheckBox::toggled, this, &EmissiveGeneratorDialog::onParametersChanged);
    connect(m_destCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EmissiveGeneratorDialog::onParametersChanged);
    connect(m_selectedOnlyCheck, &QCheckBox::toggled, this, &EmissiveGeneratorDialog::onParametersChanged);

    updateColorSwatch(m_targetColor);
    updateModeVisibility();
}

void EmissiveGeneratorDialog::updateModeVisibility()
{
    m_mode = static_cast<ExtractionMode>(m_modeCombo->currentData().toInt());
    m_lumGroup->setVisible(m_mode == LuminanceThreshold);
    m_colorGroup->setVisible(m_mode == ColorKeyIsolation);
    onParametersChanged();
}

void EmissiveGeneratorDialog::updateColorSwatch(QRgb color)
{
    if (!m_swatchLabel) return;
    QColor c(color);
    m_swatchLabel->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;")
                                 .arg(c.name()));
}

void EmissiveGeneratorDialog::pickTargetColor()
{
    QColor initialColor(m_targetColor);
    QColor chosen = QColorDialog::getColor(initialColor, this, tr("Select Target Glow Color"));
    if (chosen.isValid()) {
        m_targetColor = chosen.rgb();
        updateColorSwatch(m_targetColor);
        onParametersChanged();
    }
}

EmissiveGeneratorDialog::ExtractionMode EmissiveGeneratorDialog::mode() const
{
    return m_mode;
}

int EmissiveGeneratorDialog::threshold() const
{
    return m_thresholdSpin ? m_thresholdSpin->value() : 180;
}

int EmissiveGeneratorDialog::softKnee() const
{
    return m_kneeSpin ? m_kneeSpin->value() : 20;
}

int EmissiveGeneratorDialog::hueTolerance() const
{
    return m_hueTolSpin ? m_hueTolSpin->value() : 30;
}

double EmissiveGeneratorDialog::multiplier() const
{
    return m_multiplierSpin ? m_multiplierSpin->value() : 1.5;
}

bool EmissiveGeneratorDialog::isBlackBackground() const
{
    return m_blackBgCheck && m_blackBgCheck->isChecked();
}

EmissiveGeneratorDialog::Destination EmissiveGeneratorDialog::destination() const
{
    return m_destCombo ? static_cast<Destination>(m_destCombo->currentData().toInt()) : NewM21Layer;
}

bool EmissiveGeneratorDialog::isSelectedFramesOnly() const
{
    return m_selectedOnlyCheck && m_selectedOnlyCheck->isChecked();
}

void EmissiveGeneratorDialog::onParametersChanged()
{
    schedulePreview();
}

void EmissiveGeneratorDialog::resetDefaults()
{
    if (m_modeCombo) m_modeCombo->setCurrentIndex(0);
    if (m_thresholdSpin) m_thresholdSpin->setValue(180);
    if (m_kneeSpin) m_kneeSpin->setValue(20);
    m_targetColor = qRgb(0, 255, 255);
    updateColorSwatch(m_targetColor);
    if (m_hueTolSpin) m_hueTolSpin->setValue(30);
    if (m_multiplierSpin) m_multiplierSpin->setValue(1.5);
    if (m_blackBgCheck) m_blackBgCheck->setChecked(false);
    if (m_destCombo) m_destCombo->setCurrentIndex(0);
    if (m_selectedOnlyCheck) m_selectedOnlyCheck->setChecked(false);
}

void EmissiveGeneratorDialog::saveSettings()
{
}

QImage EmissiveGeneratorDialog::generateEmissiveMap(const QImage &source,
                                                    ExtractionMode mode,
                                                    int threshold,
                                                    int softKnee,
                                                    QRgb targetColor,
                                                    int hueTolerance,
                                                    double multiplier,
                                                    bool blackBackground)
{
    if (source.isNull()) return QImage();

    QImage src = source.convertToFormat(QImage::Format_ARGB32);
    const int w = src.width();
    const int h = src.height();
    QImage dest(w, h, QImage::Format_ARGB32);

    QColor targetQColor(targetColor);
    int targetH = targetQColor.hsvHue();
    if (targetH < 0) targetH = 0;

    const double minLum = std::max(0.0, static_cast<double>(threshold - softKnee));
    const double maxLum = std::min(255.0, static_cast<double>(threshold + softKnee));
    const double kneeSpan = (maxLum > minLum) ? (maxLum - minLum) : 1.0;

    for (int y = 0; y < h; ++y) {
        const QRgb *srcLine = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        QRgb *destLine = reinterpret_cast<QRgb*>(dest.scanLine(y));

        for (int x = 0; x < w; ++x) {
            QRgb px = srcLine[x];
            int a = qAlpha(px);
            if (a == 0) {
                destLine[x] = blackBackground ? qRgba(0, 0, 0, 0) : qRgba(0, 0, 0, 0);
                continue;
            }

            int r = qRed(px);
            int g = qGreen(px);
            int b = qBlue(px);
            double factor = 0.0;

            if (mode == LuminanceThreshold) {
                double lum = 0.299 * r + 0.587 * g + 0.114 * b;
                if (lum <= minLum) {
                    factor = 0.0;
                } else if (lum >= maxLum) {
                    factor = 1.0;
                } else {
                    factor = (lum - minLum) / kneeSpan;
                }
            } else { // ColorKeyIsolation
                QColor pxColor(px);
                int pxH = pxColor.hsvHue();
                int pxS = pxColor.hsvSaturation();
                if (pxH >= 0 && pxS >= 25) { // Ensure there is actual chromatic saturation
                    int deltaH = std::abs(pxH - targetH);
                    if (deltaH > 180) deltaH = 360 - deltaH;

                    if (deltaH <= hueTolerance) {
                        factor = (1.0 - static_cast<double>(deltaH) / hueTolerance) * (pxS / 255.0);
                    }
                }
            }

            if (factor <= 0.001) {
                if (blackBackground) {
                    destLine[x] = qRgba(0, 0, 0, 255);
                } else {
                    destLine[x] = qRgba(0, 0, 0, 0);
                }
            } else {
                int outR = std::clamp(static_cast<int>(std::round(r * factor * multiplier)), 0, 255);
                int outG = std::clamp(static_cast<int>(std::round(g * factor * multiplier)), 0, 255);
                int outB = std::clamp(static_cast<int>(std::round(b * factor * multiplier)), 0, 255);
                int outA = blackBackground ? 255 : std::clamp(static_cast<int>(std::round(a * factor)), 0, 255);

                destLine[x] = qRgba(outR, outG, outB, outA);
            }
        }
    }

    return dest;
}

void EmissiveGeneratorDialog::applyPreview()
{
    if (!m_document || m_initialAtlas.isNull()) return;

    Destination dest = destination();
    bool selectedOnly = isSelectedFramesOnly();
    QList<int> selIndices = selectedOnly ? m_document->selectedFrameIndices() : QList<int>();

    if (dest == NewM21Layer || dest == AuxiliaryEmissiveMap) {
        // Generate auxiliary emissive frames
        m_previewAuxFrames.clear();
        const int frameCount = m_initialFrames.size();
        m_previewAuxFrames.reserve(frameCount);

        for (int i = 0; i < frameCount; ++i) {
            const QImage &orig = m_initialFrames.at(i);
            if (selectedOnly && !selIndices.contains(i)) {
                // Keep neutral black emissive or existing auxiliary frame
                if (m_initialAuxiliaryFrames.contains(MaterialMapType::Emissive) &&
                    i < m_initialAuxiliaryFrames[MaterialMapType::Emissive].size()) {
                    m_previewAuxFrames.append(m_initialAuxiliaryFrames[MaterialMapType::Emissive].at(i));
                } else {
                    QSize sz = orig.size().isEmpty() ? QSize(32, 32) : orig.size();
                    QImage neutral(sz, QImage::Format_ARGB32);
                    neutral.fill(qRgba(0, 0, 0, 0));
                    m_previewAuxFrames.append(neutral);
                }
            } else {
                m_previewAuxFrames.append(generateEmissiveMap(
                    orig, mode(), threshold(), softKnee(), targetColor(),
                    hueTolerance(), multiplier(), isBlackBackground()));
            }
        }

        // Apply to document auxiliary frames
        m_document->setAuxiliaryFrames(MaterialMapType::Emissive, m_previewAuxFrames);

        // For live preview feedback, also render into preview atlas and frames
        m_previewAtlas = generateEmissiveMap(
            m_initialAtlas, mode(), threshold(), softKnee(), targetColor(),
            hueTolerance(), multiplier(), isBlackBackground());
        updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes);
        m_document->setAtlas(m_previewAtlas);
        m_document->setFrames(m_previewFrames, m_previewBoxes);

        if (dest == NewM21Layer) {
            setStatusText(tr("%1 frame(s) prepared for New Layer 'Emissive (_e)'").arg(frameCount));
        } else {
            setStatusText(tr("%1 frame(s) generated into Emissive Map (_e)").arg(frameCount));
        }
    } else {
        // Replace active sprite sheet
        m_previewAtlas = generateEmissiveMap(
            m_initialAtlas, mode(), threshold(), softKnee(), targetColor(),
            hueTolerance(), multiplier(), isBlackBackground());
        updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes);

        m_document->setAtlas(m_previewAtlas);
        m_document->setFrames(m_previewFrames, m_previewBoxes);
        setStatusText(tr("Active sprite sheet replaced with Emissive Map"));
    }
}

QUndoCommand* EmissiveGeneratorDialog::createUndoCommand()
{
    if (!m_document) return nullptr;

    if (destination() == NewM21Layer) {
        // Restore initial document atlas/frames before adding layer
        m_document->setAtlas(m_initialAtlas);
        m_document->setFrames(m_initialFrames, m_initialBoxes);
        return new AddMaterialLayerCommand(
            m_document,
            MaterialMapType::Emissive,
            m_previewAuxFrames,
            tr("Create Emissive Layer ")
        );
    } else if (destination() == AuxiliaryEmissiveMap) {
        // Restore initial document atlas/frames before setting auxiliary map
        m_document->setAtlas(m_initialAtlas);
        m_document->setFrames(m_initialFrames, m_initialBoxes);
        return new SetAuxiliaryMapCommand(
            m_document,
            MaterialMapType::Emissive,
            m_previewAuxFrames,
            tr("Generate Emissive Map ")
        );
    } else {
        return new ApplyFilterCommand(
            m_document,
            tr("Filter: Emissive Map Generator"),
            m_initialAtlas, m_initialFrames, m_initialBoxes, m_initialAnimations,
            m_previewAtlas, m_previewFrames, m_previewBoxes, m_initialAnimations,
            m_initialLayers, m_initialFrameCels, m_initialSkinProfiles
        );
    }
}
