#include "widgets/filterdialogbase.h"
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
    setMinimumWidth(380);

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

    // Bottom control bar (Live preview, Status badge, Reset button)
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);

    m_livePreviewCheck = new QCheckBox(tr("Aperçu en direct"), this);
    m_livePreviewCheck->setChecked(true);
    m_livePreviewCheck->setToolTip(tr("Mettre à jour l'atlas et les frames en temps réel pendant le réglage"));
    bottomRow->addWidget(m_livePreviewCheck);

    m_statusBadge = new QLabel(this);
    m_statusBadge->setStyleSheet(QStringLiteral("color: #27ae60; font-weight: bold;"));
    bottomRow->addWidget(m_statusBadge);

    bottomRow->addStretch();

    m_resetDefaultsBtn = new QPushButton(tr("Valeurs par défaut"), this);
    m_resetDefaultsBtn->setToolTip(tr("Rétablir les valeurs recommandées pour ce filtre"));
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

    connect(m_resetDefaultsBtn, &QPushButton::clicked, this, &FilterDialogBase::resetToDefaults);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &FilterDialogBase::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &FilterDialogBase::reject);
}

bool FilterDialogBase::isLivePreviewEnabled() const
{
    return m_livePreviewCheck && m_livePreviewCheck->isChecked();
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

void FilterDialogBase::restoreInitialState()
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
