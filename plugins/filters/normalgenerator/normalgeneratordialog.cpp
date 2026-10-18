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

#include "normalgeneratordialog.h"
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
#include <QGroupBox>
#include <QPixmap>
#include <cmath>
#include <algorithm>

NormalGeneratorDialog::NormalGeneratorDialog(SpriteDocument *doc,
                                             QUndoStack *undoStack,
                                             QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Normal Map Generator "));
    setupFilterUI();
    resetDefaults();
    schedulePreview();
}

void NormalGeneratorDialog::setupFilterUI()
{
    // 1. Bump & Height settings
    QGroupBox *bumpGroup = new QGroupBox(tr("Surface Height & Bump"), this);
    QGridLayout *bumpLayout = new QGridLayout(bumpGroup);

    QLabel *bumpLabel = new QLabel(tr("Bump Intensity:"), bumpGroup);
    m_bumpSlider = new QSlider(Qt::Horizontal, bumpGroup);
    m_bumpSlider->setRange(1, 100);
    m_bumpSlider->setValue(20); // 2.0

    m_bumpSpin = new QDoubleSpinBox(bumpGroup);
    m_bumpSpin->setRange(0.1, 10.0);
    m_bumpSpin->setSingleStep(0.1);
    m_bumpSpin->setValue(2.0);

    bumpLayout->addWidget(bumpLabel, 0, 0);
    bumpLayout->addWidget(m_bumpSlider, 0, 1);
    bumpLayout->addWidget(m_bumpSpin, 0, 2);

    QLabel *bevelLabel = new QLabel(tr("Alpha Bevel Depth:"), bumpGroup);
    m_bevelSlider = new QSlider(Qt::Horizontal, bumpGroup);
    m_bevelSlider->setRange(0, 10);
    m_bevelSlider->setValue(2);

    m_bevelSpin = new QSpinBox(bumpGroup);
    m_bevelSpin->setRange(0, 10);
    m_bevelSpin->setValue(2);
    m_bevelSpin->setSuffix(tr(" px"));

    bumpLayout->addWidget(bevelLabel, 1, 0);
    bumpLayout->addWidget(m_bevelSlider, 1, 1);
    bumpLayout->addWidget(m_bevelSpin, 1, 2);

    QHBoxLayout *flipLayout = new QHBoxLayout();
    m_invertYCheck = new QCheckBox(tr("Invert Y (DirectX / Unreal convention)"), bumpGroup);
    m_invertXCheck = new QCheckBox(tr("Invert X"), bumpGroup);
    flipLayout->addWidget(m_invertYCheck);
    flipLayout->addWidget(m_invertXCheck);
    bumpLayout->addLayout(flipLayout, 2, 0, 1, 3);

    contentLayout()->addWidget(bumpGroup);

    // 2. Destination & Target Selection
    QGroupBox *destGroup = new QGroupBox(tr("Target & Destination"), this);
    QVBoxLayout *destLayout = new QVBoxLayout(destGroup);

    QHBoxLayout *comboLayout = new QHBoxLayout();
    QLabel *destLabel = new QLabel(tr("Output Destination:"), destGroup);
    m_destCombo = new QComboBox(destGroup);
    m_destCombo->addItem(tr("New Layer (e.g. Normal (_n))"), NewM21Layer);
    m_destCombo->addItem(tr("Active Sprite Sheet (Replace albedo)"), ActiveSpriteSheet);
    m_destCombo->addItem(tr("Auxiliary Normal Map _n slave atlas)"), AuxiliaryNormalMap);

    comboLayout->addWidget(destLabel);
    comboLayout->addWidget(m_destCombo, 1);
    destLayout->addLayout(comboLayout);

    m_selectedOnlyCheck = new QCheckBox(tr("Process selected frames only"), destGroup);
    destLayout->addWidget(m_selectedOnlyCheck);

    contentLayout()->addWidget(destGroup);

    // 3. Interactive 2D Light Preview
    m_lightGroup = new QGroupBox(tr("Interactive 2D Lighting Preview"), this);
    QGridLayout *lightLayout = new QGridLayout(m_lightGroup);

    QLabel *angleLabel = new QLabel(tr("Light Angle:"), m_lightGroup);
    m_lightAngleSlider = new QSlider(Qt::Horizontal, m_lightGroup);
    m_lightAngleSlider->setRange(0, 360);
    m_lightAngleSlider->setValue(45);

    QLabel *elevLabel = new QLabel(tr("Light Height:"), m_lightGroup);
    m_lightElevSlider = new QSlider(Qt::Horizontal, m_lightGroup);
    m_lightElevSlider->setRange(10, 90);
    m_lightElevSlider->setValue(55);

    m_previewThumbnail = new QLabel(m_lightGroup);
    m_previewThumbnail->setFixedSize(140, 140);
    m_previewThumbnail->setAlignment(Qt::AlignCenter);
    m_previewThumbnail->setStyleSheet(QStringLiteral("background-color: #222; border: 1px solid #444; border-radius: 4px;"));

    lightLayout->addWidget(angleLabel, 0, 0);
    lightLayout->addWidget(m_lightAngleSlider, 0, 1);
    lightLayout->addWidget(elevLabel, 1, 0);
    lightLayout->addWidget(m_lightElevSlider, 1, 1);
    lightLayout->addWidget(m_previewThumbnail, 0, 2, 2, 1);

    contentLayout()->addWidget(m_lightGroup);

    // Connect slider <-> spinbox
    connect(m_bumpSlider, &QSlider::valueChanged, this, [this](int v) {
        m_bumpSpin->blockSignals(true);
        m_bumpSpin->setValue(v / 10.0);
        m_bumpSpin->blockSignals(false);
        onParametersChanged();
    });
    connect(m_bumpSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_bumpSlider->blockSignals(true);
        m_bumpSlider->setValue(static_cast<int>(std::round(v * 10.0)));
        m_bumpSlider->blockSignals(false);
        onParametersChanged();
    });

    connect(m_bevelSlider, &QSlider::valueChanged, m_bevelSpin, &QSpinBox::setValue);
    connect(m_bevelSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_bevelSlider, &QSlider::setValue);
    connect(m_bevelSlider, &QSlider::valueChanged, this, &NormalGeneratorDialog::onParametersChanged);

    connect(m_invertYCheck, &QCheckBox::toggled, this, &NormalGeneratorDialog::onParametersChanged);
    connect(m_invertXCheck, &QCheckBox::toggled, this, &NormalGeneratorDialog::onParametersChanged);
    connect(m_destCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NormalGeneratorDialog::onParametersChanged);
    connect(m_selectedOnlyCheck, &QCheckBox::toggled, this, &NormalGeneratorDialog::onParametersChanged);

    connect(m_lightAngleSlider, &QSlider::valueChanged, this, &NormalGeneratorDialog::onLightChanged);
    connect(m_lightElevSlider, &QSlider::valueChanged, this, &NormalGeneratorDialog::onLightChanged);
}

double NormalGeneratorDialog::bumpStrength() const
{
    return m_bumpSpin ? m_bumpSpin->value() : 2.0;
}

int NormalGeneratorDialog::bevelDepth() const
{
    return m_bevelSpin ? m_bevelSpin->value() : 2;
}

bool NormalGeneratorDialog::invertY() const
{
    return m_invertYCheck && m_invertYCheck->isChecked();
}

bool NormalGeneratorDialog::invertX() const
{
    return m_invertXCheck && m_invertXCheck->isChecked();
}

NormalGeneratorDialog::Destination NormalGeneratorDialog::destination() const
{
    return m_destCombo ? static_cast<Destination>(m_destCombo->currentData().toInt()) : NewM21Layer;
}

bool NormalGeneratorDialog::isSelectedFramesOnly() const
{
    return m_selectedOnlyCheck && m_selectedOnlyCheck->isChecked();
}

void NormalGeneratorDialog::onParametersChanged()
{
    schedulePreview();
}

void NormalGeneratorDialog::onLightChanged()
{
    if (m_previewThumbnail && m_document && m_document->frameCount() > 0) {
        int sampleIdx = 0;
        if (isSelectedFramesOnly() && !m_document->selectedFrameIndices().isEmpty()) {
            sampleIdx = m_document->selectedFrameIndices().first();
        }
        if (sampleIdx >= 0 && sampleIdx < m_initialFrames.size()) {
            QImage albedo = m_initialFrames.at(sampleIdx);
            QImage norm = generateNormalMap(albedo, bumpStrength(), bevelDepth(), invertY(), invertX());
            QImage lit = renderShadedPreview(albedo, norm, m_lightAngleSlider->value(), m_lightElevSlider->value());

            QImage scaled = lit.scaled(m_previewThumbnail->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_previewThumbnail->setPixmap(QPixmap::fromImage(scaled));
        }
    }
}

void NormalGeneratorDialog::resetDefaults()
{
    if (m_bumpSpin) m_bumpSpin->setValue(2.0);
    if (m_bevelSpin) m_bevelSpin->setValue(2);
    if (m_invertYCheck) m_invertYCheck->setChecked(false);
    if (m_invertXCheck) m_invertXCheck->setChecked(false);
    if (m_destCombo) m_destCombo->setCurrentIndex(0);
    if (m_selectedOnlyCheck) m_selectedOnlyCheck->setChecked(false);
    if (m_lightAngleSlider) m_lightAngleSlider->setValue(45);
    if (m_lightElevSlider) m_lightElevSlider->setValue(55);
}

void NormalGeneratorDialog::saveSettings()
{
}

QImage NormalGeneratorDialog::generateNormalMap(const QImage &source,
                                                double bumpStrength,
                                                int bevelDepth,
                                                bool invertY,
                                                bool invertX)
{
    if (source.isNull()) return QImage();

    QImage src = source.convertToFormat(QImage::Format_ARGB32);
    const int w = src.width();
    const int h = src.height();
    QImage dest(w, h, QImage::Format_ARGB32);

    // Compute luminance and alpha buffers
    std::vector<double> lum(w * h, 0.0);
    std::vector<double> alpha(w * h, 0.0);

    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb px = line[x];
            int a = qAlpha(px);
            alpha[y * w + x] = a / 255.0;
            if (a > 0) {
                // Perceptual luminance
                lum[y * w + x] = (0.299 * qRed(px) + 0.587 * qGreen(px) + 0.114 * qBlue(px)) / 255.0;
            } else {
                lum[y * w + x] = 0.0;
            }
        }
    }

    // If bevel is requested, blur the alpha channel to create a smooth slope at boundaries
    std::vector<double> bevel = alpha;
    if (bevelDepth > 0) {
        std::vector<double> temp = bevel;
        int r = std::clamp(bevelDepth, 1, 10);
        // Horizontal box blur
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                double sum = 0.0;
                int count = 0;
                for (int dx = -r; dx <= r; ++dx) {
                    int nx = std::clamp(x + dx, 0, w - 1);
                    sum += alpha[y * w + nx];
                    count++;
                }
                temp[y * w + x] = sum / count;
            }
        }
        // Vertical box blur
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                double sum = 0.0;
                int count = 0;
                for (int dy = -r; dy <= r; ++dy) {
                    int ny = std::clamp(y + dy, 0, h - 1);
                    sum += temp[ny * w + x];
                    count++;
                }
                bevel[y * w + x] = sum / count;
            }
        }
    }

    auto getCombinedHeight = [&](int x, int y) -> double {
        int cx = std::clamp(x, 0, w - 1);
        int cy = std::clamp(y, 0, h - 1);
        double l = lum[cy * w + cx] * bumpStrength;
        double b = (bevelDepth > 0) ? (bevel[cy * w + cx] * bevelDepth * 0.75) : 0.0;
        return l + b;
    };

    // Sobel 3x3 gradient
    for (int y = 0; y < h; ++y) {
        QRgb *destLine = reinterpret_cast<QRgb*>(dest.scanLine(y));
        const QRgb *srcLine = reinterpret_cast<const QRgb*>(src.constScanLine(y));

        for (int x = 0; x < w; ++x) {
            int srcA = qAlpha(srcLine[x]);
            if (srcA == 0) {
                // Neutral tangent-space normal: (0, 0, 1) -> (128, 128, 255)
                destLine[x] = qRgba(128, 128, 255, 0);
                continue;
            }

            // Sobel convolution
            double tl = getCombinedHeight(x - 1, y - 1);
            double t  = getCombinedHeight(x,     y - 1);
            double tr = getCombinedHeight(x + 1, y - 1);
            double l  = getCombinedHeight(x - 1, y);
            double r  = getCombinedHeight(x + 1, y);
            double bl = getCombinedHeight(x - 1, y + 1);
            double b  = getCombinedHeight(x,     y + 1);
            double br = getCombinedHeight(x + 1, y + 1);

            double dX = (tr + 2.0 * r + br) - (tl + 2.0 * l + bl);
            double dY = (bl + 2.0 * b + br) - (tl + 2.0 * t + tr);

            if (invertX) dX = -dX;
            if (invertY) dY = -dY;

            // OpenGL tangent space: X right, Y up, Z towards viewer
            // In image coordinates, Y increases downwards, so normal Y points up when dY is inverted
            double nx = -dX;
            double ny = invertY ? -dY : dY;
            double nz = 1.0;

            double len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6) {
                nx /= len;
                ny /= len;
                nz /= len;
            } else {
                nx = 0.0; ny = 0.0; nz = 1.0;
            }

            int red   = std::clamp(static_cast<int>(std::round(128.0 + 127.0 * nx)), 0, 255);
            int green = std::clamp(static_cast<int>(std::round(128.0 + 127.0 * ny)), 0, 255);
            int blue  = std::clamp(static_cast<int>(std::round(128.0 + 127.0 * nz)), 0, 255);

            destLine[x] = qRgba(red, green, blue, srcA);
        }
    }

    return dest;
}

QImage NormalGeneratorDialog::renderShadedPreview(const QImage &albedo,
                                                  const QImage &normalMap,
                                                  double lightAngleDeg,
                                                  double lightElevationDeg)
{
    if (albedo.isNull() || normalMap.isNull()) return albedo;

    QImage alb = albedo.convertToFormat(QImage::Format_ARGB32);
    QImage nrm = normalMap.convertToFormat(QImage::Format_ARGB32);
    const int w = alb.width();
    const int h = alb.height();
    QImage out(w, h, QImage::Format_ARGB32);

    // Light direction vector from spherical angles
    const double pi = 3.14159265358979323846;
    double radA = lightAngleDeg * pi / 180.0;
    double radE = lightElevationDeg * pi / 180.0;

    double lx = std::cos(radE) * std::cos(radA);
    double ly = std::cos(radE) * -std::sin(radA); // Image Y-up
    double lz = std::sin(radE);

    for (int y = 0; y < h; ++y) {
        const QRgb *albLine = reinterpret_cast<const QRgb*>(alb.constScanLine(y));
        const QRgb *nrmLine = reinterpret_cast<const QRgb*>(nrm.constScanLine(y));
        QRgb *outLine = reinterpret_cast<QRgb*>(out.scanLine(y));

        for (int x = 0; x < w; ++x) {
            QRgb apx = albLine[x];
            int a = qAlpha(apx);
            if (a == 0) {
                outLine[x] = qRgba(0, 0, 0, 0);
                continue;
            }

            QRgb npx = nrmLine[x];
            double nx = (qRed(npx)   - 128.0) / 127.0;
            double ny = (qGreen(npx) - 128.0) / 127.0;
            double nz = (qBlue(npx)  - 128.0) / 127.0;

            double ndotl = std::max(0.0, nx * lx + ny * ly + nz * lz);
            double ambient = 0.25;
            double light = ambient + (1.0 - ambient) * ndotl;

            int r = std::clamp(static_cast<int>(std::round(qRed(apx) * light)), 0, 255);
            int g = std::clamp(static_cast<int>(std::round(qGreen(apx) * light)), 0, 255);
            int b = std::clamp(static_cast<int>(std::round(qBlue(apx) * light)), 0, 255);

            outLine[x] = qRgba(r, g, b, a);
        }
    }

    return out;
}

void NormalGeneratorDialog::applyPreview()
{
    if (!m_document || m_initialAtlas.isNull()) return;

    Destination dest = destination();
    bool selectedOnly = isSelectedFramesOnly();
    QList<int> selIndices = selectedOnly ? m_document->selectedFrameIndices() : QList<int>();

    if (dest == NewM21Layer || dest == AuxiliaryNormalMap) {
        // Generate auxiliary normal frames
        m_previewAuxFrames.clear();
        const int frameCount = m_initialFrames.size();
        m_previewAuxFrames.reserve(frameCount);

        for (int i = 0; i < frameCount; ++i) {
            const QImage &orig = m_initialFrames.at(i);
            if (selectedOnly && !selIndices.contains(i)) {
                // Keep neutral flat normal or existing auxiliary frame
                if (m_initialAuxiliaryFrames.contains(MaterialMapType::Normal) &&
                    i < m_initialAuxiliaryFrames[MaterialMapType::Normal].size()) {
                    m_previewAuxFrames.append(m_initialAuxiliaryFrames[MaterialMapType::Normal].at(i));
                } else {
                    QSize sz = orig.size().isEmpty() ? QSize(32, 32) : orig.size();
                    QImage neutral(sz, QImage::Format_ARGB32);
                    neutral.fill(qRgba(128, 128, 255, 0));
                    m_previewAuxFrames.append(neutral);
                }
            } else {
                m_previewAuxFrames.append(generateNormalMap(orig, bumpStrength(), bevelDepth(), invertY(), invertX()));
            }
        }

        // Apply to document auxiliary frames
        m_document->setAuxiliaryFrames(MaterialMapType::Normal, m_previewAuxFrames);

        // For live preview feedback, also render into preview atlas and frames
        m_previewAtlas = generateNormalMap(m_initialAtlas, bumpStrength(), bevelDepth(), invertY(), invertX());
        updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes);
        m_document->setAtlas(m_previewAtlas);
        m_document->setFrames(m_previewFrames, m_previewBoxes);

        if (dest == NewM21Layer) {
            setStatusText(tr("%1 frame(s) prepared for New Layer 'Normal (_n)'").arg(frameCount));
        } else {
            setStatusText(tr("%1 frame(s) generated into Normal Map (_n)").arg(frameCount));
        }
    } else {
        // Replace active sprite sheet
        m_previewAtlas = generateNormalMap(m_initialAtlas, bumpStrength(), bevelDepth(), invertY(), invertX());
        updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes);

        m_document->setAtlas(m_previewAtlas);
        m_document->setFrames(m_previewFrames, m_previewBoxes);
        setStatusText(tr("Active sprite sheet replaced with Normal Map"));
    }

    onLightChanged();
}

QUndoCommand* NormalGeneratorDialog::createUndoCommand()
{
    if (!m_document) return nullptr;

    if (destination() == NewM21Layer) {
        // Restore initial document atlas/frames before adding layer
        m_document->setAtlas(m_initialAtlas);
        m_document->setFrames(m_initialFrames, m_initialBoxes);
        return new AddMaterialLayerCommand(
            m_document,
            MaterialMapType::Normal,
            m_previewAuxFrames,
            tr("Create Normal Layer ")
        );
    } else if (destination() == AuxiliaryNormalMap) {
        // Restore initial document atlas/frames before setting auxiliary map
        m_document->setAtlas(m_initialAtlas);
        m_document->setFrames(m_initialFrames, m_initialBoxes);
        return new SetAuxiliaryMapCommand(
            m_document,
            MaterialMapType::Normal,
            m_previewAuxFrames,
            tr("Generate Normal Map ")
        );
    } else {
        return new ApplyFilterCommand(
            m_document,
            tr("Filter: Normal Map Generator"),
            m_initialAtlas, m_initialFrames, m_initialBoxes, m_initialAnimations,
            m_previewAtlas, m_previewFrames, m_previewBoxes, m_initialAnimations,
            m_initialLayers, m_initialFrameCels, m_initialSkinProfiles
        );
    }
}
