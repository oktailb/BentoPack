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

#include "spritedetectiondialog.h"
#include "commands/filtercommands.h"
#include "config/appconfig.h"
#include "image/spritedetector.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>

SpriteDetectionDialog::SpriteDetectionDialog(SpriteDocument *doc, QUndoStack *undoStack, QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Sprite Detection"));

    setupFilterUI();

    // Enable auto-detection of sprite boxes by default
    setAutoDetectBoxesEnabled(true);

    // Initial preview computation
    applyPreview();
    m_previewApplied = true;
}

int SpriteDetectionDialog::alphaThreshold() const
{
    return m_alphaThresholdSpin ? m_alphaThresholdSpin->value() : 5;
}

int SpriteDetectionDialog::verticalTolerance() const
{
    return m_verticalToleranceSpin ? m_verticalToleranceSpin->value() : 10;
}

bool SpriteDetectionDialog::isSmartCropEnabled() const
{
    return m_smartCropCheck ? m_smartCropCheck->isChecked() : true;
}

double SpriteDetectionDialog::overlapThreshold() const
{
    return m_overlapSpin ? m_overlapSpin->value() : 0.10;
}

int SpriteDetectionDialog::minSliceSize() const
{
    return m_minSliceSizeSpin ? m_minSliceSizeSpin->value() : 1;
}

void SpriteDetectionDialog::setupFilterUI()
{
    const auto &cfg = AppConfig::instance();

    // Detection Parameters GroupBox
    QGroupBox *paramGroup = new QGroupBox(tr("Detection Parameters"), this);
    QGridLayout *paramLayout = new QGridLayout(paramGroup);
    paramLayout->setSpacing(8);

    // 1. Alpha Threshold
    QLabel *lblAlpha = new QLabel(tr("Alpha Threshold:"), this);
    lblAlpha->setToolTip(tr("Minimum alpha value (1-255) to consider a pixel as solid sprite content"));
    m_alphaThresholdSlider = new QSlider(Qt::Horizontal, this);
    m_alphaThresholdSlider->setRange(1, 255);
    m_alphaThresholdSlider->setValue(cfg.atlas().defaultAlphaThreshold);

    m_alphaThresholdSpin = new QSpinBox(this);
    m_alphaThresholdSpin->setRange(1, 255);
    m_alphaThresholdSpin->setValue(cfg.atlas().defaultAlphaThreshold);

    connect(m_alphaThresholdSlider, &QSlider::valueChanged, m_alphaThresholdSpin, &QSpinBox::setValue);
    connect(m_alphaThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_alphaThresholdSlider, &QSlider::setValue);
    connect(m_alphaThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SpriteDetectionDialog::onParametersChanged);

    paramLayout->addWidget(lblAlpha, 0, 0);
    paramLayout->addWidget(m_alphaThresholdSlider, 0, 1);
    paramLayout->addWidget(m_alphaThresholdSpin, 0, 2);

    // 2. Vertical Tolerance
    QLabel *lblVert = new QLabel(tr("Vertical Row Tolerance:"), this);
    lblVert->setToolTip(tr("Vertical row grouping tolerance (0-100 px) for sprite strip clustering"));
    m_verticalToleranceSlider = new QSlider(Qt::Horizontal, this);
    m_verticalToleranceSlider->setRange(0, 100);
    int defaultVert = cfg.atlas().defaultVerticalTolerance > 0 ? cfg.atlas().defaultVerticalTolerance : 10;
    m_verticalToleranceSlider->setValue(defaultVert);

    m_verticalToleranceSpin = new QSpinBox(this);
    m_verticalToleranceSpin->setRange(0, 100);
    m_verticalToleranceSpin->setValue(defaultVert);

    connect(m_verticalToleranceSlider, &QSlider::valueChanged, m_verticalToleranceSpin, &QSpinBox::setValue);
    connect(m_verticalToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_verticalToleranceSlider, &QSlider::setValue);
    connect(m_verticalToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SpriteDetectionDialog::onParametersChanged);

    paramLayout->addWidget(lblVert, 1, 0);
    paramLayout->addWidget(m_verticalToleranceSlider, 1, 1);
    paramLayout->addWidget(m_verticalToleranceSpin, 1, 2);

    // 3. Overlap Tolerance
    QLabel *lblOverlap = new QLabel(tr("Overlap Tolerance:"), this);
    lblOverlap->setToolTip(tr("Maximum allowed horizontal/vertical overlap ratio (0.00-1.00) between distinct sprites"));
    m_overlapSpin = new QDoubleSpinBox(this);
    m_overlapSpin->setRange(0.00, 1.00);
    m_overlapSpin->setSingleStep(0.05);
    m_overlapSpin->setValue(0.10);
    m_overlapSpin->setDecimals(2);
    connect(m_overlapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SpriteDetectionDialog::onParametersChanged);

    paramLayout->addWidget(lblOverlap, 2, 0);
    paramLayout->addWidget(m_overlapSpin, 2, 1, 1, 2);

    // 4. Min Slice Size
    QLabel *lblMinSize = new QLabel(tr("Min Slice Size (px):"), this);
    lblMinSize->setToolTip(tr("Ignore detected regions smaller than this width or height in pixels"));
    m_minSliceSizeSpin = new QSpinBox(this);
    m_minSliceSizeSpin->setRange(1, 512);
    m_minSliceSizeSpin->setValue(cfg.atlas().minSliceSize > 0 ? cfg.atlas().minSliceSize : 1);
    connect(m_minSliceSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SpriteDetectionDialog::onParametersChanged);

    paramLayout->addWidget(lblMinSize, 3, 0);
    paramLayout->addWidget(m_minSliceSizeSpin, 3, 1, 1, 2);

    // 5. Smart Crop
    m_smartCropCheck = new QCheckBox(tr("Smart Crop (Tight Bounding Boxes)"), this);
    m_smartCropCheck->setChecked(true);
    m_smartCropCheck->setToolTip(tr("Automatically shrink bounding boxes to tightly wrap opaque pixels"));
    connect(m_smartCropCheck, &QCheckBox::toggled, this, &SpriteDetectionDialog::onParametersChanged);

    paramLayout->addWidget(m_smartCropCheck, 4, 0, 1, 3);

    contentLayout()->addWidget(paramGroup);
}

void SpriteDetectionDialog::onParametersChanged()
{
    schedulePreview();
}

void SpriteDetectionDialog::resetDefaults()
{
    const auto &cfg = AppConfig::instance();
    if (m_alphaThresholdSlider) m_alphaThresholdSlider->setValue(cfg.atlas().defaultAlphaThreshold);
    if (m_verticalToleranceSlider) m_verticalToleranceSlider->setValue(cfg.atlas().defaultVerticalTolerance > 0 ? cfg.atlas().defaultVerticalTolerance : 10);
    if (m_overlapSpin) m_overlapSpin->setValue(0.10);
    if (m_minSliceSizeSpin) m_minSliceSizeSpin->setValue(cfg.atlas().minSliceSize > 0 ? cfg.atlas().minSliceSize : 1);
    if (m_smartCropCheck) m_smartCropCheck->setChecked(true);
}

void SpriteDetectionDialog::applyPreview()
{
    if (!m_document || m_initialAtlas.isNull()) return;

    // Preserves atlas as-is without background color modification
    m_previewAtlas = m_initialAtlas;

    SpriteDetectionOptions opts;
    opts.alphaThreshold = alphaThreshold();
    opts.verticalTolerance = verticalTolerance();
    opts.smartCrop = isSmartCropEnabled();
    opts.overlapThreshold = overlapThreshold();
    opts.minSliceSize = minSliceSize();

    updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes, &opts);

    m_document->setFrames(m_previewFrames, m_previewBoxes);

    setStatusText(tr("%1 frame(s) detected").arg(m_previewBoxes.size()));
}

QUndoCommand* SpriteDetectionDialog::createUndoCommand()
{
    if (!m_document || m_initialAtlas.isNull()) return nullptr;

    return new ApplyFilterCommand(
        m_document,
        tr("Sprite Detection"),
        m_initialAtlas, m_initialFrames, m_initialBoxes, m_initialAnimations,
        m_previewAtlas, m_previewFrames, m_previewBoxes, m_initialAnimations,
        m_initialLayers, m_initialFrameCels, m_initialSkinProfiles
    );
}

void SpriteDetectionDialog::saveSettings()
{
    AppConfig::instance().atlas().defaultAlphaThreshold = alphaThreshold();
    AppConfig::instance().atlas().defaultVerticalTolerance = verticalTolerance();
    AppConfig::instance().atlas().minSliceSize = minSliceSize();
    AppConfig::instance().save();
}
