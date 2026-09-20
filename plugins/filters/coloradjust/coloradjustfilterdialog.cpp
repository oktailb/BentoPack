#include "coloradjustfilterdialog.h"
#include "commands/filtercommands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <algorithm>

ColorAdjustFilterDialog::ColorAdjustFilterDialog(SpriteDocument *doc,
                                                 QUndoStack *undoStack,
                                                 QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Color Adjustment (HSV & Contrast)"));
    setMinimumWidth(440);

    setupFilterUI();
    schedulePreview();
}

void ColorAdjustFilterDialog::setupFilterUI()
{
    QGroupBox *paramGroup = new QGroupBox(tr("Adjustment Parameters"), this);
    QGridLayout *gridLayout = new QGridLayout(paramGroup);
    gridLayout->setContentsMargins(10, 10, 10, 10);
    gridLayout->setSpacing(8);

    // 1. Hue Shift (-180 to +180 deg)
    QLabel *lblHue = new QLabel(tr("Hue Shift:"), this);
    m_hueSlider = new QSlider(Qt::Horizontal, this);
    m_hueSlider->setRange(-180, 180);
    m_hueSlider->setValue(0);
    m_hueSpin = new QSpinBox(this);
    m_hueSpin->setRange(-180, 180);
    m_hueSpin->setValue(0);
    m_hueSpin->setSuffix(QStringLiteral("°"));
    m_hueSpin->setFixedWidth(65);

    gridLayout->addWidget(lblHue, 0, 0);
    gridLayout->addWidget(m_hueSlider, 0, 1);
    gridLayout->addWidget(m_hueSpin, 0, 2);

    // 2. Saturation Shift (-100% to +100%)
    QLabel *lblSat = new QLabel(tr("Saturation:"), this);
    m_satSlider = new QSlider(Qt::Horizontal, this);
    m_satSlider->setRange(-100, 100);
    m_satSlider->setValue(0);
    m_satSpin = new QSpinBox(this);
    m_satSpin->setRange(-100, 100);
    m_satSpin->setValue(0);
    m_satSpin->setSuffix(QStringLiteral("%"));
    m_satSpin->setFixedWidth(65);

    gridLayout->addWidget(lblSat, 1, 0);
    gridLayout->addWidget(m_satSlider, 1, 1);
    gridLayout->addWidget(m_satSpin, 1, 2);

    // 3. Value / Brightness Shift (-100% to +100%)
    QLabel *lblVal = new QLabel(tr("Brightness:"), this);
    m_valSlider = new QSlider(Qt::Horizontal, this);
    m_valSlider->setRange(-100, 100);
    m_valSlider->setValue(0);
    m_valSpin = new QSpinBox(this);
    m_valSpin->setRange(-100, 100);
    m_valSpin->setValue(0);
    m_valSpin->setSuffix(QStringLiteral("%"));
    m_valSpin->setFixedWidth(65);

    gridLayout->addWidget(lblVal, 2, 0);
    gridLayout->addWidget(m_valSlider, 2, 1);
    gridLayout->addWidget(m_valSpin, 2, 2);

    // 4. Contrast Shift (-100% to +100%)
    QLabel *lblContrast = new QLabel(tr("Contrast:"), this);
    m_contrastSlider = new QSlider(Qt::Horizontal, this);
    m_contrastSlider->setRange(-100, 100);
    m_contrastSlider->setValue(0);
    m_contrastSpin = new QSpinBox(this);
    m_contrastSpin->setRange(-100, 100);
    m_contrastSpin->setValue(0);
    m_contrastSpin->setSuffix(QStringLiteral("%"));
    m_contrastSpin->setFixedWidth(65);

    gridLayout->addWidget(lblContrast, 3, 0);
    gridLayout->addWidget(m_contrastSlider, 3, 1);
    gridLayout->addWidget(m_contrastSpin, 3, 2);

    contentLayout()->addWidget(paramGroup);

    // Scope selection
    QGroupBox *scopeGroup = new QGroupBox(tr("Target Scope"), this);
    QVBoxLayout *scopeLayout = new QVBoxLayout(scopeGroup);

    m_selectedFramesOnlyCheck = new QCheckBox(tr("Apply to selected frames only"), this);
    bool hasSelection = m_document && !m_document->selectedFrameIndices().isEmpty();
    m_selectedFramesOnlyCheck->setChecked(hasSelection);
    m_selectedFramesOnlyCheck->setEnabled(hasSelection);
    if (!hasSelection) {
        m_selectedFramesOnlyCheck->setToolTip(tr("No frames selected: applies to entire atlas"));
    }

    scopeLayout->addWidget(m_selectedFramesOnlyCheck);
    contentLayout()->addWidget(scopeGroup);

    // Wire synchronization
    connect(m_hueSlider, &QSlider::valueChanged, m_hueSpin, &QSpinBox::setValue);
    connect(m_hueSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_hueSlider, &QSlider::setValue);
    connect(m_hueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorAdjustFilterDialog::onParametersChanged);

    connect(m_satSlider, &QSlider::valueChanged, m_satSpin, &QSpinBox::setValue);
    connect(m_satSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_satSlider, &QSlider::setValue);
    connect(m_satSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorAdjustFilterDialog::onParametersChanged);

    connect(m_valSlider, &QSlider::valueChanged, m_valSpin, &QSpinBox::setValue);
    connect(m_valSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_valSlider, &QSlider::setValue);
    connect(m_valSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorAdjustFilterDialog::onParametersChanged);

    connect(m_contrastSlider, &QSlider::valueChanged, m_contrastSpin, &QSpinBox::setValue);
    connect(m_contrastSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_contrastSlider, &QSlider::setValue);
    connect(m_contrastSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorAdjustFilterDialog::onParametersChanged);

    connect(m_selectedFramesOnlyCheck, &QCheckBox::toggled, this, &ColorAdjustFilterDialog::onParametersChanged);
}

int ColorAdjustFilterDialog::hueShift() const
{
    return m_hueSpin ? m_hueSpin->value() : 0;
}

int ColorAdjustFilterDialog::saturationShift() const
{
    return m_satSpin ? m_satSpin->value() : 0;
}

int ColorAdjustFilterDialog::valueShift() const
{
    return m_valSpin ? m_valSpin->value() : 0;
}

int ColorAdjustFilterDialog::contrastShift() const
{
    return m_contrastSpin ? m_contrastSpin->value() : 0;
}

bool ColorAdjustFilterDialog::isSelectedFramesOnly() const
{
    return m_selectedFramesOnlyCheck && m_selectedFramesOnlyCheck->isChecked();
}

void ColorAdjustFilterDialog::onParametersChanged()
{
    schedulePreview();
}

void ColorAdjustFilterDialog::resetDefaults()
{
    if (m_hueSlider) m_hueSlider->setValue(0);
    if (m_satSlider) m_satSlider->setValue(0);
    if (m_valSlider) m_valSlider->setValue(0);
    if (m_contrastSlider) m_contrastSlider->setValue(0);
}

void ColorAdjustFilterDialog::saveSettings()
{
}

QImage ColorAdjustFilterDialog::applyColorAdjust(const QImage &source,
                                                 int hueShift,
                                                 int satShift,
                                                 int valShift,
                                                 int contrastShift,
                                                 const QList<QRect> &targetAreas)
{
    if (source.isNull()) return source;

    // If all adjustments are neutral (0), return pristine copy
    if (hueShift == 0 && satShift == 0 && valShift == 0 && contrastShift == 0) {
        return source;
    }

    QImage dest = source.convertToFormat(QImage::Format_ARGB32);
    int w = dest.width();
    int h = dest.height();

    // Contrast multiplier lookup
    double contrastFactor = 1.0;
    if (contrastShift != 0) {
        double cVal = qBound(-255.0, contrastShift * 2.55, 254.0);
        contrastFactor = (259.0 * (cVal + 255.0)) / (255.0 * (259.0 - cVal));
    }

    auto processPixel = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        QRgb p = source.pixel(x, y);
        int a = qAlpha(p);
        if (a == 0) return;

        QColor col(p);
        int hue = col.hsvHue();
        int sat = col.hsvSaturation();
        int val = col.value();

        // 1. Hue Rotation
        if (hueShift != 0) {
            if (hue < 0) hue = 0;
            hue = (hue + hueShift) % 360;
            if (hue < 0) hue += 360;
        }

        // 2. Saturation Scaling
        if (satShift != 0) {
            if (satShift > 0) {
                sat = sat + qRound((255 - sat) * (satShift / 100.0));
            } else {
                sat = sat + qRound(sat * (satShift / 100.0));
            }
            sat = qBound(0, sat, 255);
        }

        // 3. Brightness / Value Scaling
        if (valShift != 0) {
            if (valShift > 0) {
                val = val + qRound((255 - val) * (valShift / 100.0));
            } else {
                val = val + qRound(val * (valShift / 100.0));
            }
            val = qBound(0, val, 255);
        }

        QColor adjusted;
        adjusted.setHsv(hue < 0 ? 0 : hue, sat, val, a);

        // 4. Contrast Adjustment
        if (contrastShift != 0) {
            int r = qBound(0, qRound(contrastFactor * (adjusted.red() - 128) + 128), 255);
            int g = qBound(0, qRound(contrastFactor * (adjusted.green() - 128) + 128), 255);
            int b = qBound(0, qRound(contrastFactor * (adjusted.blue() - 128) + 128), 255);
            dest.setPixel(x, y, qRgba(r, g, b, a));
        } else {
            dest.setPixel(x, y, adjusted.rgba());
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

    return dest;
}

void ColorAdjustFilterDialog::applyPreview()
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

    m_previewAtlas = applyColorAdjust(m_initialAtlas, hueShift(), saturationShift(),
                                      valueShift(), contrastShift(), targetAreas);

    updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes);

    m_document->setAtlas(m_previewAtlas);
    m_document->setFrames(m_previewFrames, m_previewBoxes);

    if (isAutoDetectBoxesEnabled()) {
        setStatusText(tr("Adjusted (H:%1° S:%2% V:%3% C:%4%)").arg(hueShift()).arg(saturationShift()).arg(valueShift()).arg(contrastShift())
                      + QStringLiteral(" — ") + tr("%1 frame(s) detected").arg(m_previewBoxes.size()));
    } else {
        setStatusText(tr("Adjusted (H:%1° S:%2% V:%3% C:%4%)").arg(hueShift()).arg(saturationShift()).arg(valueShift()).arg(contrastShift()));
    }
}

QUndoCommand* ColorAdjustFilterDialog::createUndoCommand()
{
    if (!m_document || m_previewAtlas.isNull()) return nullptr;

    return new ApplyFilterCommand(
        m_document,
        tr("Filter: Color Adjustment"),
        m_initialAtlas, m_initialFrames, m_initialBoxes, m_initialAnimations,
        m_previewAtlas, m_previewFrames, m_previewBoxes, m_initialAnimations
    );
}
