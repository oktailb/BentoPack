// This file is part of the SpriteStudio Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#ifndef COLORADJUSTFILTERDIALOG_H
#define COLORADJUSTFILTERDIALOG_H

#include "widgets/filterdialogbase.h"

class QSlider;
class QSpinBox;
class QCheckBox;
class QLabel;

/**
 * @brief Floating interactive tool dialog for Global Color Adjustment (Hue, Saturation, Value, Contrast).
 */
class ColorAdjustFilterDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    explicit ColorAdjustFilterDialog(SpriteDocument *doc,
                                     QUndoStack *undoStack = nullptr,
                                     QWidget *parent = nullptr);
    ~ColorAdjustFilterDialog() override = default;

    int hueShift() const;
    int saturationShift() const;
    int valueShift() const;
    int contrastShift() const;
    bool isSelectedFramesOnly() const;

    /**
     * @brief Static helper executing the HSV and Contrast adjustment algorithm on an image.
     */
    static QImage applyColorAdjust(const QImage &source,
                                   int hueShift,
                                   int satShift,
                                   int valShift,
                                   int contrastShift,
                                   const QList<QRect> &targetAreas = QList<QRect>());

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;
    bool defaultAutoDetectBoxes() const override { return false; }

private slots:
    void onParametersChanged();

private:
    void setupFilterUI();

    // Preview state
    QImage                          m_previewAtlas;
    QList<QImage>                  m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;

    // UI controls
    QSlider*                        m_hueSlider = nullptr;
    QSpinBox*                       m_hueSpin = nullptr;

    QSlider*                        m_satSlider = nullptr;
    QSpinBox*                       m_satSpin = nullptr;

    QSlider*                        m_valSlider = nullptr;
    QSpinBox*                       m_valSpin = nullptr;

    QSlider*                        m_contrastSlider = nullptr;
    QSpinBox*                       m_contrastSpin = nullptr;

    QCheckBox*                      m_selectedFramesOnlyCheck = nullptr;
};

#endif // COLORADJUSTFILTERDIALOG_H
