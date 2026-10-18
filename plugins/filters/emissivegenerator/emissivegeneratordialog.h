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

#ifndef EMISSIVEGENERATORDIALOG_H
#define EMISSIVEGENERATORDIALOG_H

#include "widgets/filterdialogbase.h"

class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QGroupBox;

/**
 * @brief Interactive Emissive Map Generator filter for neon, magic runes, fire, and glowing elements.
 *
 * Extracts emissive channels (_e) via luminance thresholding or selective color keying,
 * with direct multi-atlas auxiliary map integration.
 */
class EmissiveGeneratorDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    enum ExtractionMode {
        LuminanceThreshold = 0,
        ColorKeyIsolation  = 1
    };

    enum Destination {
        NewM21Layer          = 0,
        ActiveSpriteSheet    = 1,
        AuxiliaryEmissiveMap = 2
    };

    explicit EmissiveGeneratorDialog(SpriteDocument *doc,
                                     QUndoStack *undoStack = nullptr,
                                     QWidget *parent = nullptr);
    ~EmissiveGeneratorDialog() override = default;

    ExtractionMode mode() const;
    int threshold() const;
    int softKnee() const;
    QRgb targetColor() const { return m_targetColor; }
    int hueTolerance() const;
    double multiplier() const;
    bool isBlackBackground() const;
    Destination destination() const;
    bool isSelectedFramesOnly() const;
    const QList<QImage>& previewAuxFrames() const { return m_previewAuxFrames; }

    // FilterDialogBase overrides
    bool isMaterialLayerGenerator() const override { return true; }
    MaterialMapType materialMapType() const override { return MaterialMapType::Emissive; }
    QString materialLayerName() const override { return QStringLiteral("Emissive (_e)"); }
    bool createsNewMaterialLayer() const override { return destination() == NewM21Layer; }
    bool createsAuxiliaryMap() const override { return destination() == AuxiliaryEmissiveMap; }
    QList<QImage> generatedAuxiliaryFrames() const override { return m_previewAuxFrames; }

    /**
     * @brief Extracts the emissive map from source image according to parameters.
     */
    static QImage generateEmissiveMap(const QImage &source,
                                      ExtractionMode mode = LuminanceThreshold,
                                      int threshold = 180,
                                      int softKnee = 20,
                                      QRgb targetColor = qRgb(0, 255, 255),
                                      int hueTolerance = 30,
                                      double multiplier = 1.5,
                                      bool blackBackground = false);

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;

private slots:
    void onParametersChanged();
    void pickTargetColor();
    void updateModeVisibility();

private:
    void setupFilterUI();
    void updateColorSwatch(QRgb color);

    ExtractionMode                  m_mode = LuminanceThreshold;
    QRgb                            m_targetColor = qRgb(0, 255, 255); // Default neon cyan

    // UI Controls
    QComboBox*                      m_modeCombo = nullptr;

    // Luminance controls
    QGroupBox*                      m_lumGroup = nullptr;
    QSlider*                        m_thresholdSlider = nullptr;
    QSpinBox*                       m_thresholdSpin = nullptr;
    QSlider*                        m_kneeSlider = nullptr;
    QSpinBox*                       m_kneeSpin = nullptr;

    // Color key controls
    QGroupBox*                      m_colorGroup = nullptr;
    QLabel*                         m_swatchLabel = nullptr;
    QPushButton*                    m_pickColorBtn = nullptr;
    QSlider*                        m_hueTolSlider = nullptr;
    QSpinBox*                       m_hueTolSpin = nullptr;

    // Boost & Output
    QDoubleSpinBox*                 m_multiplierSpin = nullptr;
    QSlider*                        m_multiplierSlider = nullptr;
    QCheckBox*                      m_blackBgCheck = nullptr;

    QComboBox*                      m_destCombo = nullptr;
    QCheckBox*                      m_selectedOnlyCheck = nullptr;

    // Cached preview state
    QImage                          m_previewAtlas;
    QList<QImage>                   m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;
    QList<QImage>                   m_previewAuxFrames;
};

#endif // EMISSIVEGENERATORDIALOG_H
