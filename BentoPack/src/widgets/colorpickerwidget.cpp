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

#include "widgets/colorpickerwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Color Harmonies Engine
// ============================================================================
static QList<QColor> computeHarmoniesList(const QColor &base, ColorHarmonyRule rule)
{
    QList<QColor> list;
    int h = base.hsvHue();
    if (h < 0) h = 0;
    int s = base.hsvSaturation();
    int v = base.value();
    int a = base.alpha();

    switch (rule) {
    case ColorHarmonyRule::None:
        break;

    case ColorHarmonyRule::Complementary:
        list.append(QColor::fromHsv((h + 180) % 360, s, v, a));
        break;

    case ColorHarmonyRule::Monochromatic: {
        int s1 = std::clamp(s - 65, 20, 255);
        int v1 = std::clamp(v + 50, 40, 255);
        int s2 = std::clamp(s + 50, 40, 255);
        int v2 = std::clamp(v - 65, 20, 255);
        list.append(QColor::fromHsv(h, s1, v1, a));
        list.append(QColor::fromHsv(h, s2, v2, a));
        break;
    }

    case ColorHarmonyRule::Analogous:
        list.append(QColor::fromHsv((h - 30 + 360) % 360, s, v, a));
        list.append(QColor::fromHsv((h + 30) % 360, s, v, a));
        break;

    case ColorHarmonyRule::SplitComplementary:
        list.append(QColor::fromHsv((h + 150) % 360, s, v, a));
        list.append(QColor::fromHsv((h + 210) % 360, s, v, a));
        break;

    case ColorHarmonyRule::Triadic:
        list.append(QColor::fromHsv((h + 120) % 360, s, v, a));
        list.append(QColor::fromHsv((h + 240) % 360, s, v, a));
        break;

    case ColorHarmonyRule::Tetradic:
        list.append(QColor::fromHsv((h + 90) % 360, s, v, a));
        list.append(QColor::fromHsv((h + 180) % 360, s, v, a));
        list.append(QColor::fromHsv((h + 270) % 360, s, v, a));
        break;
    }

    return list;
}

// ============================================================================
// ColorWheelWidget
// ============================================================================
ColorWheelWidget::ColorWheelWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(160, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
}

void ColorWheelWidget::setColor(const QColor &col)
{
    if (m_currentColor != col) {
        m_currentColor = col;
        update();
    }
}

void ColorWheelWidget::setHarmonyRule(ColorHarmonyRule rule)
{
    if (m_harmonyRule != rule) {
        m_harmonyRule = rule;
        update();
    }
}

QList<QColor> ColorWheelWidget::currentHarmonies() const
{
    return computeHarmoniesList(m_currentColor, m_harmonyRule);
}

void ColorWheelWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_cachedSize = QSize(); // Invalidate cache
}

void ColorWheelWidget::renderWheelCache()
{
    int side = std::min(width(), height());
    if (side <= 0) return;

    int val = m_currentColor.value() > 0 ? m_currentColor.value() : 255;
    if (m_wheelCache.size() == size() && m_cachedValue == val && m_cachedSize == size()) {
        return;
    }

    m_wheelCache = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    m_wheelCache.fill(Qt::transparent);

    double cx = width() / 2.0;
    double cy = height() / 2.0;
    double maxR = (side / 2.0) - 6.0;
    if (maxR <= 0) return;

    for (int y = 0; y < height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(m_wheelCache.scanLine(y));
        double dy = y - cy;
        for (int x = 0; x < width(); ++x) {
            double dx = x - cx;
            double dist = std::hypot(dx, dy);
            if (dist <= maxR) {
                double angle = std::atan2(-dy, dx);
                int hue = static_cast<int>(std::round(angle * 180.0 / M_PI));
                if (hue < 0) hue += 360;
                hue %= 360;

                int sat = std::clamp(static_cast<int>(std::round((dist / maxR) * 255.0)), 0, 255);
                QColor c = QColor::fromHsv(hue, sat, val);
                // Antialias outer edge
                if (dist > maxR - 1.0) {
                    double alpha = std::clamp(maxR - dist, 0.0, 1.0);
                    c.setAlpha(static_cast<int>(alpha * 255.0));
                }
                line[x] = c.rgba();
            }
        }
    }

    m_cachedValue = val;
    m_cachedSize = size();
}

QPointF ColorWheelWidget::colorToPoint(int hue, int sat, const QPointF &center, double radius) const
{
    if (hue < 0) hue = 0;
    double rad = hue * M_PI / 180.0;
    double dist = (sat / 255.0) * radius;
    return QPointF(center.x() + dist * std::cos(rad), center.y() - dist * std::sin(rad));
}

void ColorWheelWidget::paintEvent(QPaintEvent *)
{
    renderWheelCache();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (!m_wheelCache.isNull()) {
        p.drawImage(0, 0, m_wheelCache);
    }

    double cx = width() / 2.0;
    double cy = height() / 2.0;
    double side = std::min(width(), height());
    double maxR = (side / 2.0) - 6.0;
    if (maxR <= 0) return;

    // Outer wheel border
    p.setPen(QPen(palette().color(QPalette::Mid), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), maxR, maxR);

    // Draw Harmony markers first
    auto harmonies = currentHarmonies();
    for (const QColor &hc : harmonies) {
        int hh = hc.hsvHue();
        int hs = hc.hsvSaturation();
        QPointF hpt = colorToPoint(hh, hs, QPointF(cx, cy), maxR);

        // Outer white outline + dark border
        p.setPen(QPen(Qt::black, 1.5));
        p.setBrush(hc);
        p.drawEllipse(hpt, 5.0, 5.0);

        p.setPen(QPen(Qt::white, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(hpt, 3.5, 3.5);
    }

    // Draw Current selected color marker
    int curH = m_currentColor.hsvHue();
    int curS = m_currentColor.hsvSaturation();
    QPointF curPt = colorToPoint(curH, curS, QPointF(cx, cy), maxR);

    p.setPen(QPen(Qt::black, 2.0));
    p.setBrush(m_currentColor);
    p.drawEllipse(curPt, 7.0, 7.0);

    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(curPt, 5.0, 5.0);
}

void ColorWheelWidget::updateFromPoint(const QPoint &pos)
{
    double cx = width() / 2.0;
    double cy = height() / 2.0;
    double side = std::min(width(), height());
    double maxR = (side / 2.0) - 6.0;
    if (maxR <= 0) return;

    double dx = pos.x() - cx;
    double dy = pos.y() - cy;
    double dist = std::hypot(dx, dy);

    double angle = std::atan2(-dy, dx);
    int hue = static_cast<int>(std::round(angle * 180.0 / M_PI));
    if (hue < 0) hue += 360;
    hue %= 360;

    int sat = std::clamp(static_cast<int>(std::round((dist / maxR) * 255.0)), 0, 255);
    int val = m_currentColor.value() > 0 ? m_currentColor.value() : 255;

    m_currentColor = QColor::fromHsv(hue, sat, val, m_currentColor.alpha());
    update();
    emit colorChanged(m_currentColor);
}

void ColorWheelWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        updateFromPoint(event->pos());
    }
}

void ColorWheelWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        updateFromPoint(event->pos());
    }
}

// ============================================================================
// ColorMap2DWidget (GIMP Style 2D Saturation-Value box + Hue Bar)
// ============================================================================
ColorMap2DWidget::ColorMap2DWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(180, 140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ColorMap2DWidget::setColor(const QColor &col)
{
    if (m_currentColor != col) {
        m_currentColor = col;
        update();
    }
}

void ColorMap2DWidget::renderBoxCache()
{
    int hue = m_currentColor.hsvHue();
    if (hue < 0) hue = 0;

    int boxW = std::max(10, width() - 36);
    int boxH = std::max(10, height() - 8);

    if (m_boxCache.size() == QSize(boxW, boxH) && m_cachedHue == hue) {
        return;
    }

    m_boxCache = QImage(boxW, boxH, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < boxH; ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(m_boxCache.scanLine(y));
        double v = 1.0 - (static_cast<double>(y) / std::max(1, boxH - 1));
        for (int x = 0; x < boxW; ++x) {
            double s = static_cast<double>(x) / std::max(1, boxW - 1);
            line[x] = QColor::fromHsvF(hue / 360.0, s, v).rgb();
        }
    }

    m_cachedHue = hue;
    m_cachedBoxSize = QSize(boxW, boxH);
}

void ColorMap2DWidget::paintEvent(QPaintEvent *)
{
    renderBoxCache();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int boxX = 4;
    int boxY = 4;
    int boxW = std::max(10, width() - 36);
    int boxH = std::max(10, height() - 8);

    // 1. Draw 2D SV Box
    if (!m_boxCache.isNull()) {
        p.drawImage(boxX, boxY, m_boxCache);
    }
    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(boxX, boxY, boxW, boxH);

    // Marker in 2D box
    double s = m_currentColor.hsvSaturationF();
    if (s < 0.0) s = 0.0;
    double v = m_currentColor.valueF();
    int mx = boxX + static_cast<int>(std::round(s * (boxW - 1)));
    int my = boxY + static_cast<int>(std::round((1.0 - v) * (boxH - 1)));

    p.setPen(QPen(Qt::black, 2.0));
    p.setBrush(m_currentColor);
    p.drawEllipse(QPoint(mx, my), 5, 5);
    p.setPen(QPen(Qt::white, 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPoint(mx, my), 4, 4);

    // 2. Draw Vertical Hue Rainbow Bar
    int hueX = width() - 24;
    int hueY = 4;
    int hueW = 18;
    int hueH = boxH;

    QLinearGradient hueGrad(hueX, hueY, hueX, hueY + hueH);
    for (int step = 0; step <= 6; ++step) {
        double pos = step / 6.0;
        hueGrad.setColorAt(pos, QColor::fromHsv(static_cast<int>(pos * 359), 255, 255));
    }
    p.fillRect(hueX, hueY, hueW, hueH, hueGrad);
    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    p.drawRect(hueX, hueY, hueW, hueH);

    // Marker on Hue Bar
    int curH = m_currentColor.hsvHue();
    if (curH < 0) curH = 0;
    int hmy = hueY + static_cast<int>(std::round((curH / 360.0) * (hueH - 1)));

    p.setPen(QPen(Qt::black, 2.0));
    p.drawLine(hueX - 2, hmy, hueX + hueW + 2, hmy);
    p.setPen(QPen(Qt::white, 1.2));
    p.drawLine(hueX - 1, hmy, hueX + hueW + 1, hmy);
}

void ColorMap2DWidget::updateSVFromPoint(const QPoint &pos)
{
    int boxX = 4;
    int boxY = 4;
    int boxW = std::max(10, width() - 36);
    int boxH = std::max(10, height() - 8);

    double s = std::clamp(static_cast<double>(pos.x() - boxX) / (boxW - 1), 0.0, 1.0);
    double v = std::clamp(1.0 - (static_cast<double>(pos.y() - boxY) / (boxH - 1)), 0.0, 1.0);

    int hue = m_currentColor.hsvHue();
    if (hue < 0) hue = 0;

    m_currentColor = QColor::fromHsvF(hue / 360.0, s, v, m_currentColor.alphaF());
    update();
    emit colorChanged(m_currentColor);
}

void ColorMap2DWidget::updateHueFromPoint(const QPoint &pos)
{
    int hueY = 4;
    int hueH = std::max(10, height() - 8);

    double frac = std::clamp(static_cast<double>(pos.y() - hueY) / (hueH - 1), 0.0, 1.0);
    int hue = std::clamp(static_cast<int>(std::round(frac * 359)), 0, 359);

    double s = m_currentColor.hsvSaturationF();
    if (s < 0.0) s = 0.0;
    double v = m_currentColor.valueF();

    m_currentColor = QColor::fromHsvF(hue / 360.0, s, v, m_currentColor.alphaF());
    update();
    emit colorChanged(m_currentColor);
}

void ColorMap2DWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int hueX = width() - 28;
        if (event->pos().x() >= hueX) {
            m_draggingHue = true;
            m_draggingSV = false;
            updateHueFromPoint(event->pos());
        } else {
            m_draggingSV = true;
            m_draggingHue = false;
            updateSVFromPoint(event->pos());
        }
    }
}

void ColorMap2DWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        if (m_draggingHue) {
            updateHueFromPoint(event->pos());
        } else if (m_draggingSV) {
            updateSVFromPoint(event->pos());
        }
    }
}

// ============================================================================
// GradientSliderWidget
// ============================================================================
GradientSliderWidget::GradientSliderWidget(const QString &label, int min, int max, QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 1, 0, 1);
    layout->setSpacing(6);

    m_label = new QLabel(label, this);
    m_label->setFixedWidth(16);
    m_label->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px;"));
    layout->addWidget(m_label);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(min, max);
    layout->addWidget(m_slider, 1);

    m_spin = new QSpinBox(this);
    m_spin->setRange(min, max);
    m_spin->setFixedWidth(56);
    layout->addWidget(m_spin);

    connect(m_slider, &QSlider::valueChanged, m_spin, &QSpinBox::setValue);
    connect(m_spin, QOverload<int>::of(&QSpinBox::valueChanged), m_slider, &QSlider::setValue);
    connect(m_slider, &QSlider::valueChanged, this, &GradientSliderWidget::valueChanged);
}

int GradientSliderWidget::value() const
{
    return m_slider ? m_slider->value() : 0;
}

void GradientSliderWidget::setValue(int val)
{
    if (m_slider && m_slider->value() != val) {
        m_slider->blockSignals(true);
        m_spin->blockSignals(true);
        m_slider->setValue(val);
        m_spin->setValue(val);
        m_slider->blockSignals(false);
        m_spin->blockSignals(false);
    }
}

void GradientSliderWidget::setGradientColors(const QVector<QColor> &colors)
{
    if (colors.size() < 2 || !m_slider) return;

    QString gradStr = QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:0");
    for (int i = 0; i < colors.size(); ++i) {
        double stop = static_cast<double>(i) / (colors.size() - 1);
        gradStr += QStringLiteral(", stop:%1 %2").arg(stop).arg(colors[i].name(QColor::HexArgb));
    }
    gradStr += QStringLiteral(")");

    QString sheet = QStringLiteral(
        "QSlider::groove:horizontal {"
        "  height: 10px;"
        "  background: %1;"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: palette(button);"
        "  border: 1.5px solid palette(window-text);"
        "  width: 14px;"
        "  margin: -4px 0;"
        "  border-radius: 6px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "  border-color: palette(highlight);"
        "}"
    ).arg(gradStr);

    m_slider->setStyleSheet(sheet);
}

// ============================================================================
// ColorSlidersWidget (Aseprite F4 & GIMP Numerical / Gradient Sliders)
// ============================================================================
ColorSlidersWidget::ColorSlidersWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setColor(QColor(173, 93, 55)); // Similar to Aseprite screenshot #ad5d37
}

void ColorSlidersWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // Mode Toggle [RGB] [HSV]
    QHBoxLayout *modeRow = new QHBoxLayout();
    modeRow->setSpacing(4);

    const QString modeBtnStyle = QStringLiteral(
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  padding: 3px 8px;"
        "  font-size: 10px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: palette(alternate-base); }"
        "QPushButton:checked { background-color: palette(highlight); color: palette(highlighted-text); border-color: palette(highlight); }"
    );

    m_btnModeRGB = new QPushButton(QStringLiteral("RGB"), this);
    m_btnModeRGB->setCheckable(true);
    m_btnModeRGB->setChecked(true);
    m_btnModeRGB->setStyleSheet(modeBtnStyle);

    m_btnModeHSV = new QPushButton(QStringLiteral("HSV"), this);
    m_btnModeHSV->setCheckable(true);
    m_btnModeHSV->setStyleSheet(modeBtnStyle);

    modeRow->addWidget(m_btnModeRGB);
    modeRow->addWidget(m_btnModeHSV);
    modeRow->addStretch();
    mainLayout->addLayout(modeRow);

    m_modeStack = new QStackedWidget(this);

    // Page 0: RGB Sliders
    QWidget *rgbPage = new QWidget(m_modeStack);
    QVBoxLayout *rgbLayout = new QVBoxLayout(rgbPage);
    rgbLayout->setContentsMargins(0, 2, 0, 2);
    rgbLayout->setSpacing(3);

    m_sliderR = new GradientSliderWidget(QStringLiteral("R"), 0, 255, rgbPage);
    m_sliderG = new GradientSliderWidget(QStringLiteral("G"), 0, 255, rgbPage);
    m_sliderB = new GradientSliderWidget(QStringLiteral("B"), 0, 255, rgbPage);
    m_sliderA_rgb = new GradientSliderWidget(QStringLiteral("A"), 0, 255, rgbPage);

    rgbLayout->addWidget(m_sliderR);
    rgbLayout->addWidget(m_sliderG);
    rgbLayout->addWidget(m_sliderB);
    rgbLayout->addWidget(m_sliderA_rgb);
    m_modeStack->addWidget(rgbPage);

    // Page 1: HSV Sliders
    QWidget *hsvPage = new QWidget(m_modeStack);
    QVBoxLayout *hsvLayout = new QVBoxLayout(hsvPage);
    hsvLayout->setContentsMargins(0, 2, 0, 2);
    hsvLayout->setSpacing(3);

    m_sliderH = new GradientSliderWidget(QStringLiteral("H"), 0, 359, hsvPage);
    m_sliderS = new GradientSliderWidget(QStringLiteral("S"), 0, 100, hsvPage);
    m_sliderV = new GradientSliderWidget(QStringLiteral("V"), 0, 100, hsvPage);
    m_sliderA_hsv = new GradientSliderWidget(QStringLiteral("A"), 0, 255, hsvPage);

    hsvLayout->addWidget(m_sliderH);
    hsvLayout->addWidget(m_sliderS);
    hsvLayout->addWidget(m_sliderV);
    hsvLayout->addWidget(m_sliderA_hsv);
    m_modeStack->addWidget(hsvPage);

    mainLayout->addWidget(m_modeStack);

    // Toggle actions
    connect(m_btnModeRGB, &QPushButton::clicked, this, [this]() {
        m_btnModeRGB->setChecked(true);
        m_btnModeHSV->setChecked(false);
        m_modeStack->setCurrentIndex(0);
    });
    connect(m_btnModeHSV, &QPushButton::clicked, this, [this]() {
        m_btnModeRGB->setChecked(false);
        m_btnModeHSV->setChecked(true);
        m_modeStack->setCurrentIndex(1);
    });

    // Slider value connections
    connect(m_sliderR, &GradientSliderWidget::valueChanged, this, &ColorSlidersWidget::onRgbChanged);
    connect(m_sliderG, &GradientSliderWidget::valueChanged, this, &ColorSlidersWidget::onRgbChanged);
    connect(m_sliderB, &GradientSliderWidget::valueChanged, this, &ColorSlidersWidget::onRgbChanged);
    connect(m_sliderA_rgb, &GradientSliderWidget::valueChanged, this, &ColorSlidersWidget::onRgbChanged);

    connect(m_sliderH, &GradientSliderWidget::valueChanged, this, &ColorSlidersWidget::onHsvChanged);
    connect(m_sliderS, &GradientSliderWidget::valueChanged, this, &ColorSlidersWidget::onHsvChanged);
    connect(m_sliderV, &GradientSliderWidget::valueChanged, this, &ColorSlidersWidget::onHsvChanged);
    connect(m_sliderA_hsv, &GradientSliderWidget::valueChanged, this, &ColorSlidersWidget::onHsvChanged);
}

void ColorSlidersWidget::setColor(const QColor &col)
{
    if (m_currentColor == col) return;
    m_currentColor = col;

    m_updating = true;

    // Update RGB values
    m_sliderR->setValue(col.red());
    m_sliderG->setValue(col.green());
    m_sliderB->setValue(col.blue());
    m_sliderA_rgb->setValue(col.alpha());

    // Update HSV values
    int h = col.hsvHue();
    if (h < 0) h = 0;
    m_sliderH->setValue(h);
    m_sliderS->setValue(static_cast<int>(std::round(col.hsvSaturationF() * 100.0)));
    m_sliderV->setValue(static_cast<int>(std::round(col.valueF() * 100.0)));
    m_sliderA_hsv->setValue(col.alpha());

    updateSliderGradients();
    m_updating = false;
}

void ColorSlidersWidget::updateSliderGradients()
{
    int r = m_currentColor.red();
    int g = m_currentColor.green();
    int b = m_currentColor.blue();

    // R track
    m_sliderR->setGradientColors({QColor(0, g, b), QColor(255, g, b)});
    // G track
    m_sliderG->setGradientColors({QColor(r, 0, b), QColor(r, 255, b)});
    // B track
    m_sliderB->setGradientColors({QColor(r, g, 0), QColor(r, g, 255)});
    // A track
    m_sliderA_rgb->setGradientColors({QColor(r, g, b, 0), QColor(r, g, b, 255)});

    // HSV tracks
    // H: full spectrum
    QVector<QColor> rainbow;
    for (int step = 0; step <= 6; ++step) {
        rainbow.append(QColor::fromHsv(static_cast<int>(step * 60) % 360, 255, 255));
    }
    m_sliderH->setGradientColors(rainbow);

    int curH = m_currentColor.hsvHue();
    if (curH < 0) curH = 0;
    double curV = m_currentColor.valueF();
    m_sliderS->setGradientColors({QColor::fromHsvF(curH / 360.0, 0.0, curV), QColor::fromHsvF(curH / 360.0, 1.0, curV)});

    double curS = m_currentColor.hsvSaturationF();
    m_sliderV->setGradientColors({QColor::fromHsvF(curH / 360.0, curS, 0.0), QColor::fromHsvF(curH / 360.0, curS, 1.0)});
    m_sliderA_hsv->setGradientColors({QColor(r, g, b, 0), QColor(r, g, b, 255)});
}

void ColorSlidersWidget::onRgbChanged()
{
    if (m_updating) return;
    m_updating = true;

    int r = m_sliderR->value();
    int g = m_sliderG->value();
    int b = m_sliderB->value();
    int a = m_sliderA_rgb->value();
    m_currentColor = QColor(r, g, b, a);

    int h = m_currentColor.hsvHue();
    if (h < 0) h = 0;
    m_sliderH->setValue(h);
    m_sliderS->setValue(static_cast<int>(std::round(m_currentColor.hsvSaturationF() * 100.0)));
    m_sliderV->setValue(static_cast<int>(std::round(m_currentColor.valueF() * 100.0)));
    m_sliderA_hsv->setValue(a);

    updateSliderGradients();
    m_updating = false;

    emit colorChanged(m_currentColor);
}

void ColorSlidersWidget::onHsvChanged()
{
    if (m_updating) return;
    m_updating = true;

    double h = m_sliderH->value() / 360.0;
    double s = m_sliderS->value() / 100.0;
    double v = m_sliderV->value() / 100.0;
    int a = m_sliderA_hsv->value();

    m_currentColor = QColor::fromHsvF(h, s, v, a / 255.0);

    m_sliderR->setValue(m_currentColor.red());
    m_sliderG->setValue(m_currentColor.green());
    m_sliderB->setValue(m_currentColor.blue());
    m_sliderA_rgb->setValue(a);

    updateSliderGradients();
    m_updating = false;

    emit colorChanged(m_currentColor);
}

// ============================================================================
// ColorPickerWidget (Integrated Suite)
// ============================================================================
ColorPickerWidget::ColorPickerWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setColor(QColor(173, 93, 55));
}

void ColorPickerWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // 1. View Mode Tab Buttons
    QHBoxLayout *tabRow = new QHBoxLayout();
    tabRow->setSpacing(4);

    const QString tabBtnStyle = QStringLiteral(
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  padding: 4px 6px;"
        "  font-size: 10px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: palette(alternate-base); }"
        "QPushButton:checked { background-color: palette(highlight); color: palette(highlighted-text); border-color: palette(highlight); }"
    );

    m_btnWheelTab = new QPushButton(tr("🎡 Wheel"), this);
    m_btnWheelTab->setCheckable(true);
    m_btnWheelTab->setChecked(true);
    m_btnWheelTab->setStyleSheet(tabBtnStyle);

    m_btnMap2DTab = new QPushButton(tr("⊞ 2D Map"), this);
    m_btnMap2DTab->setCheckable(true);
    m_btnMap2DTab->setStyleSheet(tabBtnStyle);

    m_btnSlidersTab = new QPushButton(tr("🎛️ Sliders"), this);
    m_btnSlidersTab->setCheckable(true);
    m_btnSlidersTab->setStyleSheet(tabBtnStyle);

    tabRow->addWidget(m_btnWheelTab);
    tabRow->addWidget(m_btnMap2DTab);
    tabRow->addWidget(m_btnSlidersTab);
    mainLayout->addLayout(tabRow);

    // 2. View Stack
    m_viewStack = new QStackedWidget(this);

    // Page 0: Wheel View + Value Slider + Harmonies
    QWidget *wheelPage = new QWidget(m_viewStack);
    QVBoxLayout *wheelLayout = new QVBoxLayout(wheelPage);
    wheelLayout->setContentsMargins(0, 0, 0, 0);
    wheelLayout->setSpacing(4);

    m_wheelView = new ColorWheelWidget(wheelPage);
    wheelLayout->addWidget(m_wheelView, 1);

    // Wheel Brightness/Value Slider
    QHBoxLayout *valRow = new QHBoxLayout();
    valRow->setSpacing(4);
    QLabel *lblVal = new QLabel(tr("Val:"), wheelPage);
    lblVal->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold;"));
    valRow->addWidget(lblVal);

    m_wheelValueSlider = new QSlider(Qt::Horizontal, wheelPage);
    m_wheelValueSlider->setRange(0, 255);
    m_wheelValueSlider->setValue(255);
    m_wheelValueSlider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 8px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #000000, stop:1 #ffffff); border: 1px solid palette(mid); border-radius: 4px; }"
        "QSlider::handle:horizontal { background: palette(button); border: 1.5px solid palette(window-text); width: 12px; margin: -3px 0; border-radius: 5px; }"
    ));
    valRow->addWidget(m_wheelValueSlider, 1);
    wheelLayout->addLayout(valRow);

    // Harmony selector combo
    QHBoxLayout *harmRow = new QHBoxLayout();
    harmRow->setSpacing(4);
    QLabel *lblHarm = new QLabel(tr("Harmonies:"), wheelPage);
    lblHarm->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: bold;"));
    harmRow->addWidget(lblHarm);

    m_harmonyCombo = new QComboBox(wheelPage);
    m_harmonyCombo->addItem(tr("Triadic (Triangle)"), static_cast<int>(ColorHarmonyRule::Triadic));
    m_harmonyCombo->addItem(tr("Complementary (Opposite)"), static_cast<int>(ColorHarmonyRule::Complementary));
    m_harmonyCombo->addItem(tr("Analogous (Adjacent)"), static_cast<int>(ColorHarmonyRule::Analogous));
    m_harmonyCombo->addItem(tr("Split-Complementary"), static_cast<int>(ColorHarmonyRule::SplitComplementary));
    m_harmonyCombo->addItem(tr("Tetradic (Square)"), static_cast<int>(ColorHarmonyRule::Tetradic));
    m_harmonyCombo->addItem(tr("Monochromatic (Shades)"), static_cast<int>(ColorHarmonyRule::Monochromatic));
    m_harmonyCombo->addItem(tr("None"), static_cast<int>(ColorHarmonyRule::None));
    harmRow->addWidget(m_harmonyCombo, 1);
    wheelLayout->addLayout(harmRow);

    // Harmony Swatches row
    m_harmonySwatchesContainer = new QWidget(wheelPage);
    QHBoxLayout *hSwatchLayout = new QHBoxLayout(m_harmonySwatchesContainer);
    hSwatchLayout->setContentsMargins(0, 2, 0, 2);
    hSwatchLayout->setSpacing(4);
    wheelLayout->addWidget(m_harmonySwatchesContainer);

    m_viewStack->addWidget(wheelPage);

    // Page 1: 2D Map
    m_map2DView = new ColorMap2DWidget(m_viewStack);
    m_viewStack->addWidget(m_map2DView);

    // Page 2: Sliders
    m_slidersView = new ColorSlidersWidget(m_viewStack);
    m_viewStack->addWidget(m_slidersView);

    mainLayout->addWidget(m_viewStack, 1);

    // 3. Hex code & Comparison Swatches
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(6);

    // New vs Old Swatches
    QHBoxLayout *swatchPair = new QHBoxLayout();
    swatchPair->setSpacing(2);

    m_newSwatchBtn = new QPushButton(this);
    m_newSwatchBtn->setFixedSize(28, 24);
    m_newSwatchBtn->setToolTip(tr("Current selected color"));
    m_newSwatchBtn->setStyleSheet(QStringLiteral("border: 1.5px solid palette(window-text); border-radius: 3px;"));
    swatchPair->addWidget(m_newSwatchBtn);

    m_oldSwatchBtn = new QPushButton(this);
    m_oldSwatchBtn->setFixedSize(28, 24);
    m_oldSwatchBtn->setToolTip(tr("Previous color (click to restore)"));
    m_oldSwatchBtn->setStyleSheet(QStringLiteral("border: 1px solid palette(mid); border-radius: 3px;"));
    connect(m_oldSwatchBtn, &QPushButton::clicked, this, [this]() {
        setColor(m_oldColor);
        emit colorChanged(m_currentColor);
    });
    swatchPair->addWidget(m_oldSwatchBtn);
    bottomRow->addLayout(swatchPair);

    // Hex input
    QLabel *lblHex = new QLabel(QStringLiteral("#"), this);
    lblHex->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px;"));
    bottomRow->addWidget(lblHex);

    m_hexEdit = new QLineEdit(this);
    m_hexEdit->setMaxLength(8);
    m_hexEdit->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px;"));
    bottomRow->addWidget(m_hexEdit, 1);

    mainLayout->addLayout(bottomRow);

    // Connect Tab Buttons
    connect(m_btnWheelTab, &QPushButton::clicked, this, [this]() { setViewMode(WheelMode); });
    connect(m_btnMap2DTab, &QPushButton::clicked, this, [this]() { setViewMode(Map2DMode); });
    connect(m_btnSlidersTab, &QPushButton::clicked, this, [this]() { setViewMode(SlidersMode); });

    // Connect View Color Changes
    connect(m_wheelView, &ColorWheelWidget::colorChanged, this, [this](const QColor &col) {
        if (!m_updating) {
            m_updating = true;
            m_currentColor = col;
            m_map2DView->setColor(col);
            m_slidersView->setColor(col);
            m_wheelValueSlider->setValue(col.value());
            updateHarmonySwatches();
            syncHexText();
            m_updating = false;
            emit colorChanged(m_currentColor);
        }
    });

    connect(m_map2DView, &ColorMap2DWidget::colorChanged, this, [this](const QColor &col) {
        if (!m_updating) {
            m_updating = true;
            m_currentColor = col;
            m_wheelView->setColor(col);
            m_slidersView->setColor(col);
            m_wheelValueSlider->setValue(col.value());
            updateHarmonySwatches();
            syncHexText();
            m_updating = false;
            emit colorChanged(m_currentColor);
        }
    });

    connect(m_slidersView, &ColorSlidersWidget::colorChanged, this, [this](const QColor &col) {
        if (!m_updating) {
            m_updating = true;
            m_currentColor = col;
            m_wheelView->setColor(col);
            m_map2DView->setColor(col);
            m_wheelValueSlider->setValue(col.value());
            updateHarmonySwatches();
            syncHexText();
            m_updating = false;
            emit colorChanged(m_currentColor);
        }
    });

    // Value slider for Wheel
    connect(m_wheelValueSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_updating) {
            int h = m_currentColor.hsvHue();
            if (h < 0) h = 0;
            int s = m_currentColor.hsvSaturation();
            QColor updated = QColor::fromHsv(h, s, val, m_currentColor.alpha());
            setColor(updated);
            emit colorChanged(m_currentColor);
        }
    });

    // Harmony combo
    connect(m_harmonyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        ColorHarmonyRule rule = static_cast<ColorHarmonyRule>(m_harmonyCombo->itemData(idx).toInt());
        m_wheelView->setHarmonyRule(rule);
        updateHarmonySwatches();
    });

    // Hex text edited
    connect(m_hexEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        QString hex = text.trimmed();
        if (hex.startsWith(QLatin1Char('#'))) hex = hex.mid(1);
        if (hex.length() == 6 || hex.length() == 8) {
            bool ok = false;
            uint val = hex.toUInt(&ok, 16);
            if (ok) {
                QColor c;
                if (hex.length() == 8) {
                    c = QColor((val >> 24) & 0xFF, (val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
                } else {
                    c = QColor((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
                }
                setColor(c);
                emit colorChanged(m_currentColor);
            }
        }
    });

    updateHarmonySwatches();
    syncHexText();
}

void ColorPickerWidget::setColor(const QColor &col)
{
    if (m_currentColor == col && !m_updating) return;

    m_updating = true;
    m_currentColor = col;

    m_wheelView->setColor(col);
    m_map2DView->setColor(col);
    m_slidersView->setColor(col);
    m_wheelValueSlider->setValue(col.value());

    updateHarmonySwatches();
    syncHexText();

    m_updating = false;
}

void ColorPickerWidget::setOldColor(const QColor &col)
{
    m_oldColor = col;
    if (m_oldSwatchBtn) {
        m_oldSwatchBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid palette(mid); border-radius: 3px;")
                                      .arg(col.name(QColor::HexArgb)));
    }
}

ColorPickerWidget::ViewMode ColorPickerWidget::viewMode() const
{
    return static_cast<ViewMode>(m_viewStack->currentIndex());
}

void ColorPickerWidget::setViewMode(ViewMode mode)
{
    m_btnWheelTab->setChecked(mode == WheelMode);
    m_btnMap2DTab->setChecked(mode == Map2DMode);
    m_btnSlidersTab->setChecked(mode == SlidersMode);
    m_viewStack->setCurrentIndex(static_cast<int>(mode));
}

ColorHarmonyRule ColorPickerWidget::harmonyRule() const
{
    return m_wheelView ? m_wheelView->harmonyRule() : ColorHarmonyRule::None;
}

void ColorPickerWidget::setHarmonyRule(ColorHarmonyRule rule)
{
    int idx = m_harmonyCombo->findData(static_cast<int>(rule));
    if (idx >= 0) {
        m_harmonyCombo->setCurrentIndex(idx);
    }
}

void ColorPickerWidget::syncHexText()
{
    if (m_hexEdit && !m_hexEdit->hasFocus()) {
        m_hexEdit->setText(m_currentColor.name(QColor::HexRgb).mid(1));
    }
    if (m_newSwatchBtn) {
        m_newSwatchBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1.5px solid palette(window-text); border-radius: 3px;")
                                      .arg(m_currentColor.name(QColor::HexArgb)));
    }
}

void ColorPickerWidget::updateHarmonySwatches()
{
    if (!m_harmonySwatchesContainer) return;

    QLayout *layout = m_harmonySwatchesContainer->layout();
    if (!layout) return;

    // Clear existing buttons
    QLayoutItem *item = nullptr;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    // Add Base color swatch
    auto addSwatch = [this, layout](const QColor &c, const QString &tooltip) {
        QPushButton *btn = new QPushButton(m_harmonySwatchesContainer);
        btn->setFixedSize(22, 18);
        btn->setToolTip(tooltip);
        btn->setStyleSheet(QStringLiteral(
            "background-color: %1; border: 1px solid palette(mid); border-radius: 3px;"
        ).arg(c.name(QColor::HexArgb)));
        connect(btn, &QPushButton::clicked, this, [this, c]() {
            setColor(c);
            emit colorChanged(m_currentColor);
        });
        layout->addWidget(btn);
    };

    addSwatch(m_currentColor, tr("Base Color: %1").arg(m_currentColor.name()));

    auto harmonies = m_wheelView ? m_wheelView->currentHarmonies() : QList<QColor>();
    for (int i = 0; i < harmonies.size(); ++i) {
        addSwatch(harmonies[i], tr("Harmony %1: %2").arg(i + 1).arg(harmonies[i].name()));
    }

    layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
}

// ============================================================================
// ProColorPickerDialog
// ============================================================================
ProColorPickerDialog::ProColorPickerDialog(const QColor &initial, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Color Picker — Professional"));
    setMinimumSize(320, 420);
    resize(340, 450);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    m_picker = new ColorPickerWidget(this);
    m_picker->setColor(initial);
    m_picker->setOldColor(initial);
    layout->addWidget(m_picker, 1);

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(btnBox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, [this, initial]() {
        m_picker->setColor(initial);
    });
    layout->addWidget(btnBox);
}

QColor ProColorPickerDialog::selectedColor() const
{
    return m_picker ? m_picker->color() : Qt::white;
}

QColor ProColorPickerDialog::getColor(const QColor &initial, QWidget *parent, const QString &title)
{
    ProColorPickerDialog dlg(initial, parent);
    if (!title.isEmpty()) {
        dlg.setWindowTitle(title);
    }
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.selectedColor();
    }
    return QColor();
}
