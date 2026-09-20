// This file is part of the SpriteStudio Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#include "colorswapfilterdialog.h"
#include "commands/filtercommands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QColorDialog>
#include <cmath>

ColorSwapFilterDialog::ColorSwapFilterDialog(SpriteDocument *doc,
                                             QUndoStack *undoStack,
                                             QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Color Swap & Alt-Skins"));
    setMinimumWidth(420);

    setupFilterUI();
    schedulePreview();
}

void ColorSwapFilterDialog::setupFilterUI()
{
    QGridLayout *grid = new QGridLayout();

    // Source color row
    grid->addWidget(new QLabel(tr("Source color to replace:"), this), 0, 0);
    m_srcSwatchLabel = new QLabel(this);
    m_srcSwatchLabel->setFixedSize(36, 22);
    m_srcSwatchLabel->setFrameShape(QFrame::Box);
    grid->addWidget(m_srcSwatchLabel, 0, 1);
    m_pickSrcBtn = new QPushButton(tr("Pick..."), this);
    grid->addWidget(m_pickSrcBtn, 0, 2);

    // Target color row
    grid->addWidget(new QLabel(tr("Target new color:"), this), 1, 0);
    m_dstSwatchLabel = new QLabel(this);
    m_dstSwatchLabel->setFixedSize(36, 22);
    m_dstSwatchLabel->setFrameShape(QFrame::Box);
    grid->addWidget(m_dstSwatchLabel, 1, 1);
    m_pickDstBtn = new QPushButton(tr("Pick..."), this);
    grid->addWidget(m_pickDstBtn, 1, 2);

    contentLayout()->addLayout(grid);
    updateSwatches();

    // Tolerance row
    QLabel *tolLabel = new QLabel(tr("Hue / Color tolerance (0 - 100):"), this);
    contentLayout()->addWidget(tolLabel);

    QHBoxLayout *tolRow = new QHBoxLayout();
    m_toleranceSlider = new QSlider(Qt::Horizontal, this);
    m_toleranceSlider->setRange(1, 100);
    m_toleranceSlider->setValue(30);
    tolRow->addWidget(m_toleranceSlider, 1);

    m_toleranceSpin = new QSpinBox(this);
    m_toleranceSpin->setRange(1, 100);
    m_toleranceSpin->setValue(30);
    tolRow->addWidget(m_toleranceSpin);
    contentLayout()->addLayout(tolRow);

    // Preserve Shading checkbox
    m_preserveShadingCheck = new QCheckBox(tr("Preserve shading (Original shadows, gradients and highlights)"), this);
    m_preserveShadingCheck->setChecked(true);
    m_preserveShadingCheck->setToolTip(tr("Swaps the hue while adapting relative lightness to keep pixel art depth and shading."));
    contentLayout()->addWidget(m_preserveShadingCheck);

    // Scope selection
    m_selectedOnlyCheck = new QCheckBox(tr("Apply to selected frames only"), this);
    bool hasSelection = m_document && !m_document->selectedFrameIndices().isEmpty();
    m_selectedOnlyCheck->setChecked(hasSelection);
    m_selectedOnlyCheck->setEnabled(hasSelection);
    if (!hasSelection) {
        m_selectedOnlyCheck->setToolTip(tr("No frames selected: applies to entire atlas"));
    }
    contentLayout()->addWidget(m_selectedOnlyCheck);

    // Connect signals
    connect(m_pickSrcBtn, &QPushButton::clicked, this, &ColorSwapFilterDialog::pickSourceColor);
    connect(m_pickDstBtn, &QPushButton::clicked, this, &ColorSwapFilterDialog::pickTargetColor);

    connect(m_toleranceSlider, &QSlider::valueChanged, m_toleranceSpin, &QSpinBox::setValue);
    connect(m_toleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_toleranceSlider, &QSlider::setValue);
    connect(m_toleranceSlider, &QSlider::valueChanged, this, &ColorSwapFilterDialog::onParametersChanged);
    connect(m_preserveShadingCheck, &QCheckBox::toggled, this, &ColorSwapFilterDialog::onParametersChanged);
    connect(m_selectedOnlyCheck, &QCheckBox::toggled, this, &ColorSwapFilterDialog::onParametersChanged);
}

void ColorSwapFilterDialog::updateSwatches()
{
    if (m_srcSwatchLabel) {
        m_srcSwatchLabel->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;")
                                        .arg(QColor(m_sourceColor).name()));
    }
    if (m_dstSwatchLabel) {
        m_dstSwatchLabel->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;")
                                        .arg(QColor(m_targetColor).name()));
    }
}

void ColorSwapFilterDialog::pickSourceColor()
{
    QColor chosen = QColorDialog::getColor(QColor(m_sourceColor), this, tr("Select source color to replace"));
    if (chosen.isValid()) {
        m_sourceColor = chosen.rgb();
        updateSwatches();
        onParametersChanged();
    }
}

void ColorSwapFilterDialog::pickTargetColor()
{
    QColor chosen = QColorDialog::getColor(QColor(m_targetColor), this, tr("Select new target color"));
    if (chosen.isValid()) {
        m_targetColor = chosen.rgb();
        updateSwatches();
        onParametersChanged();
    }
}

int ColorSwapFilterDialog::tolerance() const
{
    return m_toleranceSpin ? m_toleranceSpin->value() : 30;
}

bool ColorSwapFilterDialog::isPreserveShading() const
{
    return m_preserveShadingCheck && m_preserveShadingCheck->isChecked();
}

bool ColorSwapFilterDialog::isSelectedFramesOnly() const
{
    return m_selectedOnlyCheck && m_selectedOnlyCheck->isChecked();
}

void ColorSwapFilterDialog::onParametersChanged()
{
    schedulePreview();
}

void ColorSwapFilterDialog::resetDefaults()
{
    m_sourceColor = qRgb(0, 180, 0);
    m_targetColor = qRgb(220, 30, 30);
    updateSwatches();
    if (m_toleranceSlider) m_toleranceSlider->setValue(30);
    if (m_preserveShadingCheck) m_preserveShadingCheck->setChecked(true);
}

void ColorSwapFilterDialog::saveSettings()
{
}

QImage ColorSwapFilterDialog::applyColorSwap(const QImage &source,
                                             QRgb srcColor,
                                             QRgb dstColor,
                                             int tolerance,
                                             bool preserveShading,
                                             const QList<QRect> &targetAreas,
                                             int *outPixelsModified)
{
    if (source.isNull()) return source;

    QImage dest = source.convertToFormat(QImage::Format_ARGB32);
    int w = dest.width();
    int h = dest.height();

    int sr = qRed(srcColor);
    int sg = qGreen(srcColor);
    int sb = qBlue(srcColor);

    QColor srcCol(srcColor);
    QColor dstCol(dstColor);
    int sV = srcCol.value();
    int sS = srcCol.hsvSaturation();
    int dH = dstCol.hsvHue();
    int dS = dstCol.hsvSaturation();
    int dV = dstCol.value();

    double maxDist = tolerance * 4.4167;
    double maxDistSq = maxDist * maxDist;

    int modifiedCount = 0;

    auto processPixel = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        QRgb p = source.pixel(x, y);
        int a = qAlpha(p);
        if (a == 0) return;

        int dR = qRed(p) - sr;
        int dG = qGreen(p) - sg;
        int dB = qBlue(p) - sb;
        double distSq = dR * dR + dG * dG + dB * dB;

        if (distSq <= maxDistSq) {
            if (preserveShading) {
                QColor pCol(p);
                int pV = pCol.value();
                int pS = pCol.hsvSaturation();

                // Proportional shading & saturation modulation
                int newV = (sV > 0) ? qBound(0, (pV * dV) / sV, 255) : dV;
                int newS = (sS > 0) ? qBound(0, (pS * dS) / sS, 255) : dS;

                QColor swapped;
                swapped.setHsv(dH >= 0 ? dH : 0, newS, newV, a);
                dest.setPixel(x, y, swapped.rgba());
            } else {
                dest.setPixel(x, y, qRgba(qRed(dstColor), qGreen(dstColor), qBlue(dstColor), a));
            }
            modifiedCount++;
        }
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

    if (outPixelsModified) {
        *outPixelsModified = modifiedCount;
    }

    return dest;
}

void ColorSwapFilterDialog::applyPreview()
{
    if (!m_document || m_initialAtlas.isNull()) return;

    QList<QRect> targetAreas;
    if (isSelectedFramesOnly()) {
        QList<int> sel = m_document->selectedFrameIndices();
        for (int idx : sel) {
            if (idx >= 0 && idx < m_initialBoxes.size()) {
                targetAreas.append(m_initialBoxes[idx].rect);
            }
        }
    }

    int modified = 0;
    m_previewAtlas = applyColorSwap(m_initialAtlas, m_sourceColor, m_targetColor,
                                    tolerance(), isPreserveShading(), targetAreas, &modified);

    updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes);

    m_document->setAtlas(m_previewAtlas);
    m_document->setFrames(m_previewFrames, m_previewBoxes);

    if (isAutoDetectBoxesEnabled()) {
        setStatusText(tr("%1 pixel(s) modified").arg(modified) + QStringLiteral(" — ") + tr("%1 frame(s) detected").arg(m_previewBoxes.size()));
    } else {
        setStatusText(tr("%1 pixel(s) modified").arg(modified));
    }
}

QUndoCommand* ColorSwapFilterDialog::createUndoCommand()
{
    if (!m_document || m_previewAtlas.isNull()) return nullptr;

    return new ApplyFilterCommand(
        m_document,
        tr("Filter: Color Swap"),
        m_initialAtlas, m_initialFrames, m_initialBoxes, m_initialAnimations,
        m_previewAtlas, m_previewFrames, m_previewBoxes, m_initialAnimations
    );
}
