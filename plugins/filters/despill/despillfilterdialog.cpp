// This file is part of the SpriteStudio Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#include "despillfilterdialog.h"
#include "controller/projectcontroller.h"
#include "commands/filtercommands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QColorDialog>
#include <cmath>

DespillFilterDialog::DespillFilterDialog(SpriteDocument *doc,
                                         QUndoStack *undoStack,
                                         QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Despill & Edge Cleanup"));
    setMinimumWidth(400);

    // Default fringe color: dominant background color if present, or magenta/green default
    if (m_document && !m_document->atlas().isNull()) {
        m_fringeColor = ProjectController::detectDominantBackgroundColor(m_document->atlas());
    }
    if (qAlpha(m_fringeColor) == 0) {
        m_fringeColor = qRgb(0, 255, 0); // fallback to green screen hue
    }

    setupFilterUI();
    schedulePreview();
}

void DespillFilterDialog::setupFilterUI()
{
    // Color selector row
    QHBoxLayout *colorRow = new QHBoxLayout();
    QLabel *colorLabel = new QLabel(tr("Fringe color (Halo):"), this);
    colorRow->addWidget(colorLabel);

    m_swatchLabel = new QLabel(this);
    m_swatchLabel->setFixedSize(36, 22);
    m_swatchLabel->setFrameShape(QFrame::Box);
    updateColorSwatch(m_fringeColor);
    colorRow->addWidget(m_swatchLabel);

    m_pickColorBtn = new QPushButton(tr("Pick..."), this);
    m_pickColorBtn->setToolTip(tr("Select the peripheral fringe color to eliminate"));
    colorRow->addWidget(m_pickColorBtn);
    colorRow->addStretch();
    contentLayout()->addLayout(colorRow);

    // Mode row
    QHBoxLayout *modeRow = new QHBoxLayout();
    QLabel *modeLabel = new QLabel(tr("Action mode:"), this);
    modeRow->addWidget(modeLabel);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Soft Color Clamping (Recommended - Preserves fine edges)"), ColorClamping);
    m_modeCombo->addItem(tr("Strict removal (Alpha = 0)"), StrictAlpha);
    m_modeCombo->setToolTip(tr("Soft clamping replaces fringe hue with interior neighbor color.\nStrict removal clears the pixel."));
    modeRow->addWidget(m_modeCombo, 1);
    contentLayout()->addLayout(modeRow);

    // Tolerance row
    QLabel *tolLabel = new QLabel(tr("Detection tolerance (0 - 100):"), this);
    contentLayout()->addWidget(tolLabel);

    QHBoxLayout *tolRow = new QHBoxLayout();
    m_toleranceSlider = new QSlider(Qt::Horizontal, this);
    m_toleranceSlider->setRange(1, 100);
    m_toleranceSlider->setValue(35);
    tolRow->addWidget(m_toleranceSlider, 1);

    m_toleranceSpin = new QSpinBox(this);
    m_toleranceSpin->setRange(1, 100);
    m_toleranceSpin->setValue(35);
    tolRow->addWidget(m_toleranceSpin);
    contentLayout()->addLayout(tolRow);

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
    connect(m_pickColorBtn, &QPushButton::clicked, this, &DespillFilterDialog::pickFringeColor);
    connect(m_toleranceSlider, &QSlider::valueChanged, m_toleranceSpin, &QSpinBox::setValue);
    connect(m_toleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_toleranceSlider, &QSlider::setValue);
    connect(m_toleranceSlider, &QSlider::valueChanged, this, &DespillFilterDialog::onParametersChanged);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DespillFilterDialog::onParametersChanged);
    connect(m_selectedOnlyCheck, &QCheckBox::toggled, this, &DespillFilterDialog::onParametersChanged);
}

void DespillFilterDialog::updateColorSwatch(QRgb color)
{
    if (!m_swatchLabel) return;
    QColor c(color);
    m_swatchLabel->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;")
                                 .arg(c.name()));
}

void DespillFilterDialog::pickFringeColor()
{
    QColor initialColor(m_fringeColor);
    QColor chosen = QColorDialog::getColor(initialColor, this, tr("Select Fringe Color"));
    if (chosen.isValid()) {
        m_fringeColor = chosen.rgb();
        updateColorSwatch(m_fringeColor);
        onParametersChanged();
    }
}

int DespillFilterDialog::tolerance() const
{
    return m_toleranceSpin ? m_toleranceSpin->value() : 35;
}

DespillFilterDialog::DespillMode DespillFilterDialog::mode() const
{
    return m_modeCombo ? static_cast<DespillMode>(m_modeCombo->currentData().toInt()) : ColorClamping;
}

bool DespillFilterDialog::isSelectedFramesOnly() const
{
    return m_selectedOnlyCheck && m_selectedOnlyCheck->isChecked();
}

void DespillFilterDialog::onParametersChanged()
{
    schedulePreview();
}

void DespillFilterDialog::resetDefaults()
{
    if (m_toleranceSlider) m_toleranceSlider->setValue(35);
    if (m_modeCombo) m_modeCombo->setCurrentIndex(0);
}

void DespillFilterDialog::saveSettings()
{
    // Settings can be extended in AppConfig if needed
}

QImage DespillFilterDialog::applyDespill(const QImage &source,
                                        QRgb fringeColor,
                                        int tolerance,
                                        DespillMode mode,
                                        const QList<QRect> &targetAreas,
                                        int *outPixelsModified)
{
    if (source.isNull()) return source;

    QImage dest = source.convertToFormat(QImage::Format_ARGB32);
    int w = dest.width();
    int h = dest.height();

    int fR = qRed(fringeColor);
    int fG = qGreen(fringeColor);
    int fB = qBlue(fringeColor);

    // Normalize tolerance to max Euclidean distance (0 to ~441.67)
    double maxDist = tolerance * 4.4167;
    double maxDistSq = maxDist * maxDist;

    int modifiedCount = 0;

    auto processPixel = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        QRgb p = source.pixel(x, y);
        int a = qAlpha(p);
        if (a == 0) return;

        // Check if adjacent to transparent pixel (border pixel)
        bool isBorder = (x == 0 || y == 0 || x == w - 1 || y == h - 1 ||
                         qAlpha(source.pixel(x - 1, y)) == 0 ||
                         qAlpha(source.pixel(x + 1, y)) == 0 ||
                         qAlpha(source.pixel(x, y - 1)) == 0 ||
                         qAlpha(source.pixel(x, y + 1)) == 0);

        if (!isBorder) return;

        int dR = qRed(p) - fR;
        int dG = qGreen(p) - fG;
        int dB = qBlue(p) - fB;
        double distSq = dR * dR + dG * dG + dB * dB;

        if (distSq <= maxDistSq) {
            if (mode == StrictAlpha) {
                dest.setPixel(x, y, qRgba(0, 0, 0, 0));
                modifiedCount++;
            } else {
                // Color clamping: find nearest non-fringe opaque neighbor within radius 2
                QRgb bestNeighbor = p;
                double bestNeighborDistSq = -1.0;

                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;

                        QRgb np = source.pixel(nx, ny);
                        if (qAlpha(np) > 128) {
                            int ndR = qRed(np) - fR;
                            int ndG = qGreen(np) - fG;
                            int ndB = qBlue(np) - fB;
                            double nDistSq = ndR * ndR + ndG * ndG + ndB * ndB;
                            if (nDistSq > bestNeighborDistSq) {
                                bestNeighborDistSq = nDistSq;
                                bestNeighbor = np;
                            }
                        }
                    }
                }

                if (bestNeighborDistSq > maxDistSq) {
                    dest.setPixel(x, y, qRgba(qRed(bestNeighbor), qGreen(bestNeighbor), qBlue(bestNeighbor), a));
                    modifiedCount++;
                } else {
                    // Fully surrounded by fringe: erase
                    dest.setPixel(x, y, qRgba(0, 0, 0, 0));
                    modifiedCount++;
                }
            }
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

void DespillFilterDialog::applyPreview()
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
    m_previewAtlas = applyDespill(m_initialAtlas, m_fringeColor, tolerance(), mode(), targetAreas, &modified);

    updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes);

    m_document->setAtlas(m_previewAtlas);
    m_document->setFrames(m_previewFrames, m_previewBoxes);

    if (isAutoDetectBoxesEnabled()) {
        setStatusText(tr("%1 fringe pixel(s) processed").arg(modified) + QStringLiteral(" — ") + tr("%1 frame(s) detected").arg(m_previewBoxes.size()));
    } else {
        setStatusText(tr("%1 fringe pixel(s) processed").arg(modified));
    }
}

QUndoCommand* DespillFilterDialog::createUndoCommand()
{
    if (!m_document || m_previewAtlas.isNull()) return nullptr;

    return new ApplyFilterCommand(
        m_document,
        tr("Filter: Despill & Edge Cleanup"),
        m_initialAtlas, m_initialFrames, m_initialBoxes, m_initialAnimations,
        m_previewAtlas, m_previewFrames, m_previewBoxes, m_initialAnimations
    );
}
