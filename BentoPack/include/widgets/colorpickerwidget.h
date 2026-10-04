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

#ifndef COLORPICKERWIDGET_H
#define COLORPICKERWIDGET_H

#include <QWidget>
#include <QDialog>
#include <QColor>
#include <QList>
#include <QVector>
#include <QImage>
#include <QSlider>
#include "bentopackwidgets_export.h"

class QComboBox;
class QSpinBox;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QLabel;

/**
 * @brief Color harmony calculation rules (Aseprite-style).
 */
enum class ColorHarmonyRule {
    None = 0,
    Complementary,      ///< Direct opposite on color wheel (+180°)
    Monochromatic,      ///< Same hue with lightness/saturation steps
    Analogous,          ///< Neighboring hues (-30° and +30°)
    SplitComplementary, ///< Hues at +150° and +210°
    Triadic,            ///< Equilateral triangle (+120° and +240°)
    Tetradic            ///< Square / 4 hues (+90°, +180°, +270°)
};

/**
 * @brief Interactive HSV Color Wheel with real-time harmony indicators & swatches (Aseprite style).
 */
class BENTOPACK_WIDGETS_EXPORT ColorWheelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColorWheelWidget(QWidget *parent = nullptr);

    QColor color() const { return m_currentColor; }
    void setColor(const QColor &col);

    ColorHarmonyRule harmonyRule() const { return m_harmonyRule; }
    void setHarmonyRule(ColorHarmonyRule rule);

    QList<QColor> currentHarmonies() const;

signals:
    void colorChanged(const QColor &color);
    void harmonySelected(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void renderWheelCache();
    void updateFromPoint(const QPoint &pos);
    QPointF colorToPoint(int hue, int sat, const QPointF &center, double radius) const;

    QColor m_currentColor = Qt::red;
    ColorHarmonyRule m_harmonyRule = ColorHarmonyRule::Triadic;
    QImage m_wheelCache;
    int m_cachedValue = -1;
    QSize m_cachedSize;
};

/**
 * @brief 2D Saturation-Value box + vertical Hue slider (GIMP / Photoshop style).
 */
class BENTOPACK_WIDGETS_EXPORT ColorMap2DWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColorMap2DWidget(QWidget *parent = nullptr);

    QColor color() const { return m_currentColor; }
    void setColor(const QColor &col);

signals:
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void renderBoxCache();
    void updateSVFromPoint(const QPoint &pos);
    void updateHueFromPoint(const QPoint &pos);

    QColor m_currentColor = Qt::red;
    QImage m_boxCache;
    int m_cachedHue = -1;
    QSize m_cachedBoxSize;
    bool m_draggingSV = false;
    bool m_draggingHue = false;
};

/**
 * @brief Live gradient-track slider widget.
 */
class BENTOPACK_WIDGETS_EXPORT GradientSliderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GradientSliderWidget(const QString &label, int min, int max, QWidget *parent = nullptr);

    int value() const;
    void setValue(int val);
    void setGradientColors(const QVector<QColor> &colors);

signals:
    void valueChanged(int val);

private:
    QLabel *m_label = nullptr;
    QSlider *m_slider = nullptr;
    QSpinBox *m_spin = nullptr;
    QVector<QColor> m_gradientColors;
};

/**
 * @brief RGB and HSV live numerical sliders with live gradient preview tracks (Aseprite F4 & GIMP style).
 */
class BENTOPACK_WIDGETS_EXPORT ColorSlidersWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColorSlidersWidget(QWidget *parent = nullptr);

    QColor color() const { return m_currentColor; }
    void setColor(const QColor &col);

signals:
    void colorChanged(const QColor &color);

private:
    void setupUI();
    void updateSliderGradients();
    void onRgbChanged();
    void onHsvChanged();

    QColor m_currentColor = Qt::red;
    bool m_updating = false;

    // Tabs / Mode toggle
    QPushButton *m_btnModeRGB = nullptr;
    QPushButton *m_btnModeHSV = nullptr;
    QStackedWidget *m_modeStack = nullptr;

    // RGB Mode
    GradientSliderWidget *m_sliderR = nullptr;
    GradientSliderWidget *m_sliderG = nullptr;
    GradientSliderWidget *m_sliderB = nullptr;
    GradientSliderWidget *m_sliderA_rgb = nullptr;

    // HSV Mode
    GradientSliderWidget *m_sliderH = nullptr;
    GradientSliderWidget *m_sliderS = nullptr;
    GradientSliderWidget *m_sliderV = nullptr;
    GradientSliderWidget *m_sliderA_hsv = nullptr;
};

/**
 * @brief Integrated Pro Color Picker widget with Wheel+Harmonies, 2D Map, and RGB/HSV Sliders.
 */
class BENTOPACK_WIDGETS_EXPORT ColorPickerWidget : public QWidget
{
    Q_OBJECT

public:
    enum ViewMode {
        WheelMode = 0,
        Map2DMode = 1,
        SlidersMode = 2
    };

    explicit ColorPickerWidget(QWidget *parent = nullptr);

    QColor color() const { return m_currentColor; }
    void setColor(const QColor &col);

    QColor oldColor() const { return m_oldColor; }
    void setOldColor(const QColor &col);

    ViewMode viewMode() const;
    void setViewMode(ViewMode mode);

    ColorHarmonyRule harmonyRule() const;
    void setHarmonyRule(ColorHarmonyRule rule);

signals:
    void colorChanged(const QColor &color);

private:
    void setupUI();
    void updateHarmonySwatches();
    void syncHexText();

    QColor m_currentColor = Qt::red;
    QColor m_oldColor = Qt::black;
    bool m_updating = false;

    // Header controls
    QPushButton *m_btnWheelTab = nullptr;
    QPushButton *m_btnMap2DTab = nullptr;
    QPushButton *m_btnSlidersTab = nullptr;
    QStackedWidget *m_viewStack = nullptr;

    // Views
    ColorWheelWidget *m_wheelView = nullptr;
    ColorMap2DWidget *m_map2DView = nullptr;
    ColorSlidersWidget *m_slidersView = nullptr;

    // Wheel extras (Harmonies)
    QWidget *m_wheelExtrasContainer = nullptr;
    QComboBox *m_harmonyCombo = nullptr;
    QWidget *m_harmonySwatchesContainer = nullptr;
    QSlider *m_wheelValueSlider = nullptr;

    // Swatches & Hex readout
    QPushButton *m_newSwatchBtn = nullptr;
    QPushButton *m_oldSwatchBtn = nullptr;
    QLineEdit *m_hexEdit = nullptr;
};

/**
 * @brief Standalone Pro Color Picker Dialog (Aseprite & GIMP features).
 */
class BENTOPACK_WIDGETS_EXPORT ProColorPickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProColorPickerDialog(const QColor &initial = Qt::white, QWidget *parent = nullptr);

    QColor selectedColor() const;

    static QColor getColor(const QColor &initial = Qt::white,
                           QWidget *parent = nullptr,
                           const QString &title = QString());

private:
    ColorPickerWidget *m_picker = nullptr;
};

#endif // COLORPICKERWIDGET_H
