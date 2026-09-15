#include "widgets/backgroundremovaldialog.h"
#include "controller/projectcontroller.h"
#include "image/spritedetector.h"
#include "commands/commands.h"
#include "config/appconfig.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QUndoStack>

BackgroundRemovalDialog::BackgroundRemovalDialog(SpriteDocument *doc, QUndoStack *undoStack, QWidget *parent)
    : QDialog(parent)
    , m_document(doc)
    , m_undoStack(undoStack)
    , m_detectedBgColor(0)
    , m_previewApplied(false)
    , m_swatchLabel(nullptr)
    , m_colorInfoLabel(nullptr)
    , m_colorToleranceSlider(nullptr)
    , m_colorToleranceSpin(nullptr)
    , m_alphaThresholdSlider(nullptr)
    , m_alphaThresholdSpin(nullptr)
    , m_verticalToleranceSlider(nullptr)
    , m_verticalToleranceSpin(nullptr)
    , m_smartCropCheck(nullptr)
    , m_overlapSpin(nullptr)
    , m_livePreviewCheck(nullptr)
    , m_statusBadge(nullptr)
    , m_buttonBox(nullptr)
{
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setWindowTitle(tr("KEY_DIALOG_REMOVE_BG_TITLE", "Suppression d'arrière-plan"));
    setMinimumWidth(380);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(80); // 80 ms debounce for smooth real-time preview
    connect(&m_debounceTimer, &QTimer::timeout, this, &BackgroundRemovalDialog::onPreviewTimeout);

    // Save initial document state for pristine rollback on Cancel
    if (m_document) {
        m_initialAtlas = m_document->atlas();
        m_initialFrames = m_document->frames();
        m_initialBoxes = m_document->boxes();
        m_initialAnimations = m_document->animations();

        m_detectedBgColor = ProjectController::detectDominantBackgroundColor(m_initialAtlas);
    }

    setupUI();

    // Trigger initial preview if an image is loaded
    if (!m_initialAtlas.isNull()) {
        applyLivePreview();
    }
}

BackgroundRemovalDialog::~BackgroundRemovalDialog()
{
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
    return m_smartCropCheck ? m_smartCropCheck->isChecked() : false;
}

double BackgroundRemovalDialog::overlapThreshold() const
{
    return m_overlapSpin ? m_overlapSpin->value() : 0.5;
}

void BackgroundRemovalDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    const auto &cfg = AppConfig::instance();

    // 1. Group: Couleur de fond & Tolérance
    auto *grpColor = new QGroupBox(tr("Couleur & Tolérance du fond"), this);
    auto *colorLayout = new QVBoxLayout(grpColor);
    colorLayout->setSpacing(8);

    // Sampled color preview row
    auto *swatchRow = new QHBoxLayout();
    m_swatchLabel = new QLabel(this);
    m_swatchLabel->setFixedSize(28, 28);
    m_swatchLabel->setStyleSheet(QStringLiteral("border: 1px solid #555; border-radius: 4px;"));

    m_colorInfoLabel = new QLabel(this);
    updateColorSwatch(m_detectedBgColor);

    swatchRow->addWidget(m_swatchLabel);
    swatchRow->addWidget(m_colorInfoLabel, 1);
    colorLayout->addLayout(swatchRow);

    // Tolerance slider + spinbox
    auto *tolForm = new QFormLayout();
    tolForm->setLabelAlignment(Qt::AlignLeft);
    auto *tolRow = new QHBoxLayout();
    m_colorToleranceSlider = new QSlider(Qt::Horizontal, this);
    m_colorToleranceSlider->setRange(0, 100);
    m_colorToleranceSlider->setValue(cfg.project().backgroundRemovalTolerance);

    m_colorToleranceSpin = new QSpinBox(this);
    m_colorToleranceSpin->setRange(0, 100);
    m_colorToleranceSpin->setValue(cfg.project().backgroundRemovalTolerance);
    m_colorToleranceSpin->setFixedWidth(65);

    tolRow->addWidget(m_colorToleranceSlider, 1);
    tolRow->addWidget(m_colorToleranceSpin);
    tolForm->addRow(tr("Tolérance :"), tolRow);
    colorLayout->addLayout(tolForm);

    mainLayout->addWidget(grpColor);

    // 2. Group: Détection & Ordonnancement des Sprites
    auto *grpDetection = new QGroupBox(tr("Détection & Ordonnancement"), this);
    auto *detForm = new QFormLayout(grpDetection);
    detForm->setSpacing(8);

    // Alpha threshold row
    auto *alphaRow = new QHBoxLayout();
    m_alphaThresholdSlider = new QSlider(Qt::Horizontal, this);
    m_alphaThresholdSlider->setRange(0, 255);
    m_alphaThresholdSlider->setValue(cfg.atlas().defaultAlphaThreshold);

    m_alphaThresholdSpin = new QSpinBox(this);
    m_alphaThresholdSpin->setRange(0, 255);
    m_alphaThresholdSpin->setValue(cfg.atlas().defaultAlphaThreshold);
    m_alphaThresholdSpin->setFixedWidth(65);

    alphaRow->addWidget(m_alphaThresholdSlider, 1);
    alphaRow->addWidget(m_alphaThresholdSpin);
    detForm->addRow(tr("Seuil alpha :"), alphaRow);

    // Vertical tolerance row (reading order)
    auto *vertRow = new QHBoxLayout();
    m_verticalToleranceSlider = new QSlider(Qt::Horizontal, this);
    m_verticalToleranceSlider->setRange(0, 50);
    m_verticalToleranceSlider->setValue(cfg.atlas().defaultVerticalTolerance > 0 ? cfg.atlas().defaultVerticalTolerance : 10);

    m_verticalToleranceSpin = new QSpinBox(this);
    m_verticalToleranceSpin->setRange(0, 50);
    m_verticalToleranceSpin->setSuffix(QStringLiteral(" px"));
    m_verticalToleranceSpin->setValue(cfg.atlas().defaultVerticalTolerance > 0 ? cfg.atlas().defaultVerticalTolerance : 10);
    m_verticalToleranceSpin->setFixedWidth(65);

    vertRow->addWidget(m_verticalToleranceSlider, 1);
    vertRow->addWidget(m_verticalToleranceSpin);
    detForm->addRow(tr("Alignement lignes :"), vertRow);

    mainLayout->addWidget(grpDetection);

    // 3. Group: Options de Recadrage (Smart Crop)
    auto *grpOptions = new QGroupBox(tr("Options avancées"), this);
    auto *optLayout = new QVBoxLayout(grpOptions);
    optLayout->setSpacing(6);

    m_smartCropCheck = new QCheckBox(tr("Activer le rognage intelligent (Smart Crop)"), this);
    m_smartCropCheck->setChecked(false);
    optLayout->addWidget(m_smartCropCheck);

    auto *overlapForm = new QFormLayout();
    m_overlapSpin = new QDoubleSpinBox(this);
    m_overlapSpin->setRange(0.01, 1.0);
    m_overlapSpin->setSingleStep(0.05);
    m_overlapSpin->setValue(0.5);
    m_overlapSpin->setFixedWidth(65);
    m_overlapSpin->setEnabled(false);
    overlapForm->addRow(tr("Seuil chevauchement :"), m_overlapSpin);
    optLayout->addLayout(overlapForm);

    mainLayout->addWidget(grpOptions);

    // 4. Live Preview & Status Bar
    auto *previewRow = new QHBoxLayout();
    m_livePreviewCheck = new QCheckBox(tr("Aperçu en direct"), this);
    m_livePreviewCheck->setChecked(true);

    m_statusBadge = new QLabel(this);
    m_statusBadge->setStyleSheet(QStringLiteral("font-weight: bold; color: #27ae60;"));

    previewRow->addWidget(m_livePreviewCheck);
    previewRow->addStretch();
    previewRow->addWidget(m_statusBadge);
    mainLayout->addLayout(previewRow);

    // 5. Action buttons (OK / Cancel / Reset)
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Valider"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Annuler"));
    m_buttonBox->button(QDialogButtonBox::RestoreDefaults)->setText(tr("Réinitialiser"));

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &BackgroundRemovalDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &BackgroundRemovalDialog::reject);
    connect(m_buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &BackgroundRemovalDialog::onResetDefaults);

    mainLayout->addWidget(m_buttonBox);

    // Synchronize sliders & spinboxes
    connect(m_colorToleranceSlider, &QSlider::valueChanged, m_colorToleranceSpin, &QSpinBox::setValue);
    connect(m_colorToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_colorToleranceSlider, &QSlider::setValue);
    connect(m_colorToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BackgroundRemovalDialog::onParametersChanged);

    connect(m_alphaThresholdSlider, &QSlider::valueChanged, m_alphaThresholdSpin, &QSpinBox::setValue);
    connect(m_alphaThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_alphaThresholdSlider, &QSlider::setValue);
    connect(m_alphaThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BackgroundRemovalDialog::onParametersChanged);

    connect(m_verticalToleranceSlider, &QSlider::valueChanged, m_verticalToleranceSpin, &QSpinBox::setValue);
    connect(m_verticalToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_verticalToleranceSlider, &QSlider::setValue);
    connect(m_verticalToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BackgroundRemovalDialog::onParametersChanged);

    connect(m_smartCropCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_overlapSpin) m_overlapSpin->setEnabled(checked);
        onParametersChanged();
    });
    connect(m_overlapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BackgroundRemovalDialog::onParametersChanged);
    connect(m_livePreviewCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            applyLivePreview();
        } else {
            restoreInitialState();
        }
    });
}

void BackgroundRemovalDialog::updateColorSwatch(QRgb color)
{
    if (!m_swatchLabel || !m_colorInfoLabel) return;

    if (qAlpha(color) == 0) {
        m_swatchLabel->setStyleSheet(QStringLiteral("background-color: transparent; border: 1px dashed #888;"));
        m_colorInfoLabel->setText(tr("Couleur de fond : Non détectée"));
    } else {
        const QColor qc(color);
        m_swatchLabel->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #333; border-radius: 4px;").arg(qc.name()));
        m_colorInfoLabel->setText(tr("Couleur dominante : %1 (RGB: %2, %3, %4)")
                                      .arg(qc.name().toUpper())
                                      .arg(qc.red())
                                      .arg(qc.green())
                                      .arg(qc.blue()));
    }
}

void BackgroundRemovalDialog::onParametersChanged()
{
    if (m_livePreviewCheck && m_livePreviewCheck->isChecked()) {
        m_debounceTimer.start(); // restarts 80 ms timer
    }
}

void BackgroundRemovalDialog::onPreviewTimeout()
{
    applyLivePreview();
}

void BackgroundRemovalDialog::onResetDefaults()
{
    const auto &cfg = AppConfig::instance();
    if (m_colorToleranceSlider) m_colorToleranceSlider->setValue(cfg.project().backgroundRemovalTolerance);
    if (m_alphaThresholdSlider) m_alphaThresholdSlider->setValue(cfg.atlas().defaultAlphaThreshold);
    if (m_verticalToleranceSlider) m_verticalToleranceSlider->setValue(cfg.atlas().defaultVerticalTolerance > 0 ? cfg.atlas().defaultVerticalTolerance : 10);
    if (m_smartCropCheck) m_smartCropCheck->setChecked(false);
    if (m_overlapSpin) m_overlapSpin->setValue(0.5);
}

void BackgroundRemovalDialog::applyLivePreview()
{
    if (!m_document || m_initialAtlas.isNull()) return;

    // 1. Remove background from initial image copy
    QImage cleaned = ProjectController::removeBackgroundFromImage(m_initialAtlas, colorTolerance());
    if (cleaned.isNull()) return;

    // 2. Detect sprites with spatial grid optimization
    QList<QImage> frameImages;
    QList<SpriteBox> boxes;
    SpriteDetectionOptions opts;
    opts.alphaThreshold = alphaThreshold();
    opts.verticalTolerance = verticalTolerance();
    opts.smartCrop = isSmartCropEnabled();
    opts.overlapThreshold = overlapThreshold();

    if (!SpriteDetector::detectToImages(cleaned, frameImages, boxes, opts)) {
        return;
    }

    QList<QPixmap> frames;
    frames.reserve(frameImages.size());
    for (const QImage &img : frameImages) {
        frames.append(QPixmap::fromImage(img));
    }

    m_previewAtlas = cleaned;
    m_previewFrames = frames;
    m_previewBoxes = boxes;
    m_previewApplied = true;

    // Update document in place -> triggers atlasChanged() & framesChanged()
    m_document->setAtlas(m_previewAtlas);
    m_document->setFrames(m_previewFrames, m_previewBoxes);

    if (m_statusBadge) {
        m_statusBadge->setText(tr("%1 frame(s) détectée(s)").arg(boxes.size()));
    }
}

void BackgroundRemovalDialog::restoreInitialState()
{
    if (!m_document || m_initialAtlas.isNull()) return;

    m_document->setAtlas(m_initialAtlas);
    m_document->setFrames(m_initialFrames, m_initialBoxes);
    m_document->setAnimations(m_initialAnimations);
    m_previewApplied = false;

    if (m_statusBadge) {
        m_statusBadge->setText(tr("%1 frame(s) d'origine").arg(m_initialBoxes.size()));
    }
}

void BackgroundRemovalDialog::reject()
{
    // Revert document to initial state before closing
    if (m_previewApplied) {
        restoreInitialState();
    }
    QDialog::reject();
}

void BackgroundRemovalDialog::accept()
{
    // If preview was disabled, apply now
    if (!m_previewApplied) {
        applyLivePreview();
    }

    if (m_document && !m_previewAtlas.isNull()) {
        // Record undoable command in the undo stack
        if (m_undoStack) {
            m_undoStack->push(new RemoveBackgroundCommand(
                m_document,
                m_initialAtlas, m_initialFrames, m_initialBoxes,
                m_previewAtlas, m_previewFrames, m_previewBoxes
            ));
        }

        // Persist recent values as defaults in AppConfig
        AppConfig::instance().project().backgroundRemovalTolerance = colorTolerance();
        AppConfig::instance().atlas().defaultAlphaThreshold = alphaThreshold();
        AppConfig::instance().atlas().defaultVerticalTolerance = verticalTolerance();
        AppConfig::instance().save();
    }

    QDialog::accept();
}
