// This file is part of the SpriteStudio Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#include "backgroundremovaldialog.h"
#include "controller/projectcontroller.h"
#include "commands/commands.h"
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

BackgroundRemovalDialog::BackgroundRemovalDialog(SpriteDocument *doc, QUndoStack *undoStack, QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
    , m_detectedBgColor(0)
{
    setWindowTitle(tr("Background Removal"));

    // Detect dominant background color from initial atlas
    if (m_document && !m_initialAtlas.isNull()) {
        m_detectedBgColor = ProjectController::detectDominantBackgroundColor(m_initialAtlas);
    }

    setupFilterUI();

    // Enable auto-detection of sprite boxes by default for background removal
    setAutoDetectBoxesEnabled(true);

    // Trigger initial preview computation
    applyPreview();
    m_previewApplied = true;
}

int BackgroundRemovalDialog::colorTolerance() const
{
    return m_colorToleranceSpin ? m_colorToleranceSpin->value() : 10;
}

int BackgroundRemovalDialog::alphaThreshold() const
{
    return m_alphaThresholdSpin ? m_alphaThresholdSpin->value() : 5;
}

int BackgroundRemovalDialog::verticalTolerance() const
{
    return m_verticalToleranceSpin ? m_verticalToleranceSpin->value() : 10;
}

bool BackgroundRemovalDialog::isSmartCropEnabled() const
{
    return m_smartCropCheck ? m_smartCropCheck->isChecked() : true;
}

double BackgroundRemovalDialog::overlapThreshold() const
{
    return m_overlapSpin ? m_overlapSpin->value() : 0.10;
}

void BackgroundRemovalDialog::setupFilterUI()
{
    const auto &cfg = AppConfig::instance();

    // --- 1. Detected Background Color GroupBox ---
    QGroupBox *colorGroup = new QGroupBox(tr("Detected Background Color"), this);
    QHBoxLayout *colorLayout = new QHBoxLayout(colorGroup);

    m_swatchLabel = new QLabel(this);
    m_swatchLabel->setFixedSize(36, 24);
    m_swatchLabel->setFrameShape(QFrame::Box);
    m_swatchLabel->setLineWidth(1);
    updateColorSwatch(m_detectedBgColor);

    m_colorInfoLabel = new QLabel(this);
    m_colorInfoLabel->setText(QStringLiteral("#%1%2%3  (R:%4 G:%5 B:%6)")
        .arg(qRed(m_detectedBgColor), 2, 16, QLatin1Char('0')).toUpper()
        .arg(qGreen(m_detectedBgColor), 2, 16, QLatin1Char('0')).toUpper()
        .arg(qBlue(m_detectedBgColor), 2, 16, QLatin1Char('0')).toUpper()
        .arg(qRed(m_detectedBgColor))
        .arg(qGreen(m_detectedBgColor))
        .arg(qBlue(m_detectedBgColor)));

    colorLayout->addWidget(m_swatchLabel);
    colorLayout->addWidget(m_colorInfoLabel);
    colorLayout->addStretch();
    contentLayout()->addWidget(colorGroup);

    // --- 2. Parameters GroupBox ---
    QGroupBox *paramGroup = new QGroupBox(tr("Removal Parameters"), this);
    QVBoxLayout *paramLayout = new QVBoxLayout(paramGroup);

    // Row: Color Tolerance
    QHBoxLayout *colorTolRow = new QHBoxLayout();
    QLabel *lblTol = new QLabel(tr("Color tolerance:"), this);
    lblTol->setFixedWidth(140);
    m_colorToleranceSlider = new QSlider(Qt::Horizontal, this);
    m_colorToleranceSlider->setRange(0, 100);
    m_colorToleranceSlider->setValue(cfg.project().backgroundRemovalTolerance);

    m_colorToleranceSpin = new QSpinBox(this);
    m_colorToleranceSpin->setRange(0, 100);
    m_colorToleranceSpin->setValue(cfg.project().backgroundRemovalTolerance);
    m_colorToleranceSpin->setFixedWidth(65);

    colorTolRow->addWidget(lblTol);
    colorTolRow->addWidget(m_colorToleranceSlider, 1);
    colorTolRow->addWidget(m_colorToleranceSpin);
    paramLayout->addLayout(colorTolRow);

    // Row: Alpha Threshold
    QHBoxLayout *alphaRow = new QHBoxLayout();
    QLabel *lblAlpha = new QLabel(tr("Alpha threshold:"), this);
    lblAlpha->setFixedWidth(140);
    m_alphaThresholdSlider = new QSlider(Qt::Horizontal, this);
    m_alphaThresholdSlider->setRange(0, 255);
    m_alphaThresholdSlider->setValue(cfg.atlas().defaultAlphaThreshold);

    m_alphaThresholdSpin = new QSpinBox(this);
    m_alphaThresholdSpin->setRange(0, 255);
    m_alphaThresholdSpin->setValue(cfg.atlas().defaultAlphaThreshold);
    m_alphaThresholdSpin->setFixedWidth(65);

    alphaRow->addWidget(lblAlpha);
    alphaRow->addWidget(m_alphaThresholdSlider, 1);
    alphaRow->addWidget(m_alphaThresholdSpin);
    paramLayout->addLayout(alphaRow);

    // Row: Vertical Tolerance
    QHBoxLayout *vertRow = new QHBoxLayout();
    QLabel *lblVert = new QLabel(tr("Vertical tolerance:"), this);
    lblVert->setFixedWidth(140);
    m_verticalToleranceSlider = new QSlider(Qt::Horizontal, this);
    m_verticalToleranceSlider->setRange(0, 50);
    m_verticalToleranceSlider->setValue(cfg.atlas().defaultVerticalTolerance > 0 ? cfg.atlas().defaultVerticalTolerance : 10);

    m_verticalToleranceSpin = new QSpinBox(this);
    m_verticalToleranceSpin->setRange(0, 50);
    m_verticalToleranceSpin->setSuffix(QStringLiteral(" px"));
    m_verticalToleranceSpin->setValue(cfg.atlas().defaultVerticalTolerance > 0 ? cfg.atlas().defaultVerticalTolerance : 10);
    m_verticalToleranceSpin->setFixedWidth(65);

    vertRow->addWidget(lblVert);
    vertRow->addWidget(m_verticalToleranceSlider, 1);
    vertRow->addWidget(m_verticalToleranceSpin);
    paramLayout->addLayout(vertRow);

    // Row: Smart Crop & Overlap
    QHBoxLayout *cropRow = new QHBoxLayout();
    m_smartCropCheck = new QCheckBox(tr("Smart Crop"), this);
    m_smartCropCheck->setChecked(true);
    m_smartCropCheck->setToolTip(tr("Automatically shrink-wrap bounding boxes around opaque sprite pixels"));

    QLabel *lblOverlap = new QLabel(tr("Overlap threshold:"), this);
    m_overlapSpin = new QDoubleSpinBox(this);
    m_overlapSpin->setRange(0.0, 1.0);
    m_overlapSpin->setSingleStep(0.05);
    m_overlapSpin->setValue(0.10);
    m_overlapSpin->setFixedWidth(65);

    cropRow->addWidget(m_smartCropCheck);
    cropRow->addSpacing(16);
    cropRow->addWidget(lblOverlap);
    cropRow->addWidget(m_overlapSpin);
    cropRow->addStretch();
    paramLayout->addLayout(cropRow);

    contentLayout()->addWidget(paramGroup);

    // Synchronize Sliders <-> SpinBoxes and connect to live preview
    connect(m_colorToleranceSlider, &QSlider::valueChanged, m_colorToleranceSpin, &QSpinBox::setValue);
    connect(m_colorToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_colorToleranceSlider, &QSlider::setValue);
    connect(m_colorToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BackgroundRemovalDialog::onParametersChanged);

    connect(m_alphaThresholdSlider, &QSlider::valueChanged, m_alphaThresholdSpin, &QSpinBox::setValue);
    connect(m_alphaThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_alphaThresholdSlider, &QSlider::setValue);
    connect(m_alphaThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BackgroundRemovalDialog::onParametersChanged);

    connect(m_verticalToleranceSlider, &QSlider::valueChanged, m_verticalToleranceSpin, &QSpinBox::setValue);
    connect(m_verticalToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_verticalToleranceSlider, &QSlider::setValue);
    connect(m_verticalToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BackgroundRemovalDialog::onParametersChanged);

    connect(m_smartCropCheck, &QCheckBox::toggled, this, &BackgroundRemovalDialog::onParametersChanged);
    connect(m_overlapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BackgroundRemovalDialog::onParametersChanged);

    if (m_autoDetectBoxesCheck) {
        connect(m_autoDetectBoxesCheck, &QCheckBox::toggled, this, [this](bool enabled) {
            if (m_smartCropCheck) m_smartCropCheck->setEnabled(enabled);
            if (m_overlapSpin) m_overlapSpin->setEnabled(enabled);
        });
    }
}

void BackgroundRemovalDialog::updateColorSwatch(QRgb color)
{
    if (m_swatchLabel) {
        QColor col(color);
        m_swatchLabel->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;")
                                     .arg(col.name()));
    }
}

void BackgroundRemovalDialog::onParametersChanged()
{
    schedulePreview();
}

void BackgroundRemovalDialog::resetDefaults()
{
    const auto &cfg = AppConfig::instance();
    if (m_colorToleranceSlider) m_colorToleranceSlider->setValue(cfg.project().backgroundRemovalTolerance);
    if (m_alphaThresholdSlider) m_alphaThresholdSlider->setValue(cfg.atlas().defaultAlphaThreshold);
    if (m_verticalToleranceSlider) m_verticalToleranceSlider->setValue(cfg.atlas().defaultVerticalTolerance > 0 ? cfg.atlas().defaultVerticalTolerance : 10);
    if (m_smartCropCheck) m_smartCropCheck->setChecked(true);
    if (m_overlapSpin) m_overlapSpin->setValue(0.10);
}

void BackgroundRemovalDialog::applyPreview()
{
    if (!m_document || m_initialAtlas.isNull()) return;

    // Step 1: Remove background from pristine initial atlas
    m_previewAtlas = ProjectController::removeBackgroundFromImage(
        m_initialAtlas, colorTolerance());

    // Step 2: Extract bounding boxes and individual sprite frames using factorized helper
    SpriteDetectionOptions opts;
    opts.alphaThreshold = alphaThreshold();
    opts.verticalTolerance = verticalTolerance();
    opts.smartCrop = isSmartCropEnabled();
    opts.overlapThreshold = overlapThreshold();

    updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes, &opts);

    // Step 3: Update document directly so the main graphics scene reflects the result
    m_document->setAtlas(m_previewAtlas);
    m_document->setFrames(m_previewFrames, m_previewBoxes);

    if (isAutoDetectBoxesEnabled()) {
        setStatusText(tr("%1 frame(s) detected").arg(m_previewBoxes.size()));
    } else {
        setStatusText(tr("%1 initial frame(s)").arg(m_previewBoxes.size()));
    }
}

QUndoCommand* BackgroundRemovalDialog::createUndoCommand()
{
    if (!m_document || m_previewAtlas.isNull()) return nullptr;

    return new RemoveBackgroundCommand(
        m_document,
        m_initialAtlas, m_initialFrames, m_initialBoxes,
        m_previewAtlas, m_previewFrames, m_previewBoxes
    );
}

void BackgroundRemovalDialog::saveSettings()
{
    AppConfig::instance().project().backgroundRemovalTolerance = colorTolerance();
    AppConfig::instance().atlas().defaultAlphaThreshold = alphaThreshold();
    AppConfig::instance().atlas().defaultVerticalTolerance = verticalTolerance();
    AppConfig::instance().save();
}
