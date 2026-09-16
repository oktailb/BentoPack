#include "widgets/filterdialogbase.h"
#include "image/spritedetector.h"
#include "config/appconfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFrame>
#include <QUndoStack>
#include <QUndoCommand>

FilterDialogBase::FilterDialogBase(SpriteDocument *doc, QUndoStack *undoStack, QWidget *parent)
    : QDialog(parent)
    , m_document(doc)
    , m_undoStack(undoStack)
    , m_previewApplied(false)
{
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setMinimumWidth(460);

    // Capture initial document state for guaranteed rollback on Cancel
    if (m_document) {
        m_initialAtlas = m_document->atlas();
        m_initialFrames = m_document->frames();
        m_initialBoxes = m_document->boxes();
        m_initialAnimations = m_document->animations();
    }

    setupBaseUI();

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(80); // 80 ms debounce for smooth real-time preview
    connect(&m_debounceTimer, &QTimer::timeout, this, &FilterDialogBase::onPreviewTimeout);
}

void FilterDialogBase::setupBaseUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(12, 12, 12, 12);
    m_mainLayout->setSpacing(10);

    // Dedicated area where derived filter dialogs place their specific controls
    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(8);
    m_mainLayout->addLayout(m_contentLayout, 1);

    // Separator line above control bar
    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    m_mainLayout->addWidget(sep);

    // Bottom control bar (Live preview, Auto-detect boxes, Status badge, Reset button)
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(12);

    m_livePreviewCheck = new QCheckBox(tr("Live Preview"), this);
    m_livePreviewCheck->setChecked(true);
    m_livePreviewCheck->setToolTip(tr("Update atlas and frames in real-time while adjusting parameters"));
    bottomRow->addWidget(m_livePreviewCheck);

    m_autoDetectBoxesCheck = new QCheckBox(tr("Auto-detect Sprite Boxes"), this);
    m_autoDetectBoxesCheck->setChecked(false);
    m_autoDetectBoxesCheck->setToolTip(tr("Automatically recalculate sprite bounding boxes after filtering"));
    bottomRow->addWidget(m_autoDetectBoxesCheck);

    m_statusBadge = new QLabel(this);
    m_statusBadge->setStyleSheet(QStringLiteral("color: #27ae60; font-weight: bold;"));
    bottomRow->addWidget(m_statusBadge);

    bottomRow->addStretch();

    m_resetDefaultsBtn = new QPushButton(tr("Reset Defaults"), this);
    m_resetDefaultsBtn->setToolTip(tr("Restore recommended default values for this filter"));
    bottomRow->addWidget(m_resetDefaultsBtn);

    m_mainLayout->addLayout(bottomRow);

    // Standard OK / Cancel buttons
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_mainLayout->addWidget(m_buttonBox);

    // Wire up common interactions
    connect(m_livePreviewCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            schedulePreview();
        } else {
            restoreInitialState();
        }
    });

    connect(m_autoDetectBoxesCheck, &QCheckBox::toggled, this, [this](bool) {
        schedulePreview();
    });

    connect(m_resetDefaultsBtn, &QPushButton::clicked, this, &FilterDialogBase::resetToDefaults);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &FilterDialogBase::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &FilterDialogBase::reject);
}

bool FilterDialogBase::isLivePreviewEnabled() const
{
    return m_livePreviewCheck && m_livePreviewCheck->isChecked();
}

bool FilterDialogBase::isAutoDetectBoxesEnabled() const
{
    return m_autoDetectBoxesCheck && m_autoDetectBoxesCheck->isChecked();
}

void FilterDialogBase::setAutoDetectBoxesEnabled(bool enabled)
{
    if (m_autoDetectBoxesCheck) {
        m_autoDetectBoxesCheck->setChecked(enabled);
    }
}

void FilterDialogBase::setStatusText(const QString &text)
{
    if (m_statusBadge) {
        m_statusBadge->setText(text);
    }
}

void FilterDialogBase::schedulePreview()
{
    if (isLivePreviewEnabled()) {
        m_debounceTimer.start();
    }
}

void FilterDialogBase::resetToDefaults()
{
    if (m_autoDetectBoxesCheck) {
        m_autoDetectBoxesCheck->setChecked(defaultAutoDetectBoxes());
    }
    resetDefaults();
    schedulePreview();
}

void FilterDialogBase::onPreviewTimeout()
{
    if (m_document && isLivePreviewEnabled()) {
        applyPreview();
        m_previewApplied = true;
    }
}

void FilterDialogBase::updatePreviewFramesAndBoxes(const QImage &previewAtlas,
                                                   QList<QImage> &outFrames,
                                                   QList<SpriteBox> &outBoxes,
                                                   const SpriteDetectionOptions *customOpts)
{
    outFrames.clear();
    outBoxes.clear();

    if (previewAtlas.isNull()) return;

    if (isAutoDetectBoxesEnabled()) {
        SpriteDetectionOptions opts;
        if (customOpts) {
            opts = *customOpts;
        } else {
            const auto &cfg = AppConfig::instance();
            opts.alphaThreshold = cfg.atlas().defaultAlphaThreshold;
            opts.verticalTolerance = cfg.atlas().defaultVerticalTolerance > 0 ? cfg.atlas().defaultVerticalTolerance : 10;
            opts.minSliceSize = cfg.atlas().minSliceSize;
            opts.smartCrop = true;
            opts.overlapThreshold = 0.10;
        }

        SpriteDetector::detectToImages(previewAtlas, outFrames, outBoxes, opts);
    } else {
        outBoxes = m_initialBoxes;
        outFrames.reserve(outBoxes.size());
        for (const SpriteBox &box : outBoxes) {
            QRect r = box.rect.intersected(previewAtlas.rect());
            outFrames.append(previewAtlas.copy(r));
        }
    }
}

void FilterDialogBase::restoreInitialState()
{
    if (!m_document || m_initialAtlas.isNull()) return;

    m_document->setAtlas(m_initialAtlas);
    m_document->setFrames(m_initialFrames, m_initialBoxes);
    m_document->setAnimations(m_initialAnimations);
    m_previewApplied = false;

    if (m_statusBadge) {
        m_statusBadge->setText(tr("%1 initial frame(s)").arg(m_initialBoxes.size()));
    }
}

void FilterDialogBase::reject()
{
    // Guarantee full non-destructive rollback before closing
    if (m_previewApplied) {
        restoreInitialState();
    }
    QDialog::reject();
}

void FilterDialogBase::accept()
{
    // If preview was disabled or not yet run, apply filter now
    if (!m_previewApplied) {
        applyPreview();
        m_previewApplied = true;
    }

    if (m_document) {
        QUndoCommand *cmd = createUndoCommand();
        if (cmd) {
            if (m_undoStack) {
                m_undoStack->push(cmd);
            } else {
                delete cmd;
            }
        }

        saveSettings();
    }

    QDialog::accept();
}
