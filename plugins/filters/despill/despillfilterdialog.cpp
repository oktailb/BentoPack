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
    m_modeCombo->addItem(tr("Soft Color Clamping (CIELAB Perceptual)"), ColorClamping);
    m_modeCombo->addItem(tr("Chroma Spill Suppression (Matte Despill)"), ChromaSpill);
    m_modeCombo->addItem(tr("Strict Removal (Alpha = 0)"), StrictAlpha);
    m_modeCombo->setToolTip(tr("Soft clamping: replaces fringe with interior color using CIELAB ΔE.\n"
                               "Chroma spill: neutralizes green/blue bounce reflection without removing alpha.\n"
                               "Strict removal: clears the fringe pixel."));
    modeRow->addWidget(m_modeCombo, 1);
    contentLayout()->addLayout(modeRow);

    // Tolerance row
    QLabel *tolLabel = new QLabel(tr("Detection tolerance (CIELAB ΔE 1 - 100):"), this);
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

// ----------------------------------------------------------------------------
// CIELAB Color Space Conversion and Delta E calculation
// ----------------------------------------------------------------------------
namespace {
    struct LabColor {
        double L;
        double a;
        double b;
    };

    static inline LabColor rgbToLab(int r, int g, int b) {
        // sRGB to linear RGB [0, 1]
        auto toLinear = [](double c) -> double {
            return (c <= 0.04045) ? (c / 12.92) : std::pow((c + 0.055) / 1.055, 2.4);
        };
        double lr = toLinear(r / 255.0);
        double lg = toLinear(g / 255.0);
        double lb = toLinear(b / 255.0);

        // Linear RGB to CIE XYZ (D65)
        double x = lr * 0.4124564 + lg * 0.3575761 + lb * 0.1804375;
        double y = lr * 0.2126729 + lg * 0.7151522 + lb * 0.0721750;
        double z = lr * 0.0193339 + lg * 0.1191920 + lb * 0.9503041;

        // D65 reference white points
        const double xn = 0.95047;
        const double yn = 1.00000;
        const double zn = 1.08883;

        double xr = x / xn;
        double yr = y / yn;
        double zr = z / zn;

        const double delta = 6.0 / 29.0;
        const double delta3 = delta * delta * delta;

        auto f = [delta, delta3](double t) -> double {
            return (t > delta3) ? std::cbrt(t) : (t / (3.0 * delta * delta) + 4.0 / 29.0);
        };

        double fx = f(xr);
        double fy = f(yr);
        double fz = f(zr);

        double L = 116.0 * fy - 16.0;
        double a = 500.0 * (fx - fy);
        double labB = 200.0 * (fy - fz);

        return {L, a, labB};
    }

    static inline double deltaE(const LabColor &c1, const LabColor &c2) {
        double dL = c1.L - c2.L;
        double da = c1.a - c2.a;
        double db = c1.b - c2.b;
        return std::sqrt(dL * dL + da * da + db * db);
    }
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

    LabColor fringeLab = rgbToLab(fR, fG, fB);
    double maxDeltaE = static_cast<double>(tolerance);

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

        int pR = qRed(p);
        int pG = qGreen(p);
        int pB = qBlue(p);

        LabColor pLab = rgbToLab(pR, pG, pB);
        double dE = deltaE(pLab, fringeLab);

        if (mode == ChromaSpill) {
            bool isSpill = false;
            double spillFactor = 1.0;

            if (fG > fR && fG > fB && pG > std::max(pR, pB)) {
                isSpill = true;
                spillFactor = std::min(1.0, maxDeltaE / 35.0);
            } else if (fB > fR && fB > fG && pB > std::max(pR, pG)) {
                isSpill = true;
                spillFactor = std::min(1.0, maxDeltaE / 35.0);
            } else if (fR > fG && fB > fG && (pR > pG || pB > pG)) {
                isSpill = true;
                spillFactor = std::min(1.0, maxDeltaE / 35.0);
            } else if (dE <= maxDeltaE) {
                isSpill = true;
                spillFactor = 1.0 - (dE / std::max(1.0, maxDeltaE));
            }

            if (isSpill) {
                int newR = pR;
                int newG = pG;
                int newB = pB;

                if (fG > fR && fG > fB) {
                    int maxOther = std::max(pR, pB);
                    if (pG > maxOther) {
                        newG = static_cast<int>(std::round(pG * (1.0 - spillFactor) + maxOther * spillFactor));
                    }
                } else if (fB > fR && fB > fG) {
                    int maxOther = std::max(pR, pG);
                    if (pB > maxOther) {
                        newB = static_cast<int>(std::round(pB * (1.0 - spillFactor) + maxOther * spillFactor));
                    }
                } else if (fR > fG && fB > fG) {
                    int maxOther = pG;
                    if (pR > maxOther || pB > maxOther) {
                        newR = static_cast<int>(std::round(pR * (1.0 - spillFactor) + maxOther * spillFactor));
                        newB = static_cast<int>(std::round(pB * (1.0 - spillFactor) + maxOther * spillFactor));
                    }
                } else {
                    int lum = static_cast<int>(std::round(0.299 * pR + 0.587 * pG + 0.114 * pB));
                    newR = static_cast<int>(std::round(pR * (1.0 - spillFactor) + lum * spillFactor));
                    newG = static_cast<int>(std::round(pG * (1.0 - spillFactor) + lum * spillFactor));
                    newB = static_cast<int>(std::round(pB * (1.0 - spillFactor) + lum * spillFactor));
                }

                dest.setPixel(x, y, qRgba(qBound(0, newR, 255), qBound(0, newG, 255), qBound(0, newB, 255), a));
                modifiedCount++;
            }
        } else if (dE <= maxDeltaE) {
            if (mode == StrictAlpha) {
                dest.setPixel(x, y, qRgba(0, 0, 0, 0));
                modifiedCount++;
            } else { // ColorClamping
                // Color Clamping: find nearest non-fringe opaque neighbor within radius 2 using CIELAB ΔE
                QRgb bestNeighbor = p;
                double bestNeighborDeltaE = -1.0;

                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;

                        QRgb np = source.pixel(nx, ny);
                        if (qAlpha(np) > 128) {
                            LabColor npLab = rgbToLab(qRed(np), qGreen(np), qBlue(np));
                            double nDE = deltaE(npLab, fringeLab);
                            if (nDE > bestNeighborDeltaE) {
                                bestNeighborDeltaE = nDE;
                                bestNeighbor = np;
                            }
                        }
                    }
                }

                if (bestNeighborDeltaE > maxDeltaE) {
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
        m_previewAtlas, m_previewFrames, m_previewBoxes, m_initialAnimations,
        m_initialLayers, m_initialFrameCels, m_initialSkinProfiles
    );
}
