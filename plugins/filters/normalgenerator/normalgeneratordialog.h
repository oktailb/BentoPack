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

#ifndef NORMALGENERATORDIALOG_H
#define NORMALGENERATORDIALOG_H

#include "widgets/filterdialogbase.h"

class QDoubleSpinBox;
class QSpinBox;
class QSlider;
class QCheckBox;
class QComboBox;
class QLabel;
class QGroupBox;

/**
 * @brief Interactive Normal Map Generator filter with volumetric edge beveling and 2D point lighting preview.
 *
 * Supports generating tangent-space normal maps (_n) directly into auxiliary maps or replacing the active sprite sheet.
 */
class NormalGeneratorDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    enum Destination {
        NewM21Layer        = 0,
        ActiveSpriteSheet  = 1,
        AuxiliaryNormalMap = 2
    };

    explicit NormalGeneratorDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr);
    ~NormalGeneratorDialog() override = default;

    double bumpStrength() const;
    int bevelDepth() const;
    bool invertY() const;
    bool invertX() const;
    Destination destination() const;
    bool isSelectedFramesOnly() const;
    const QList<QImage>& previewAuxFrames() const { return m_previewAuxFrames; }

    // FilterDialogBase overrides
    bool isMaterialLayerGenerator() const override { return true; }
    MaterialMapType materialMapType() const override { return MaterialMapType::Normal; }
    QString materialLayerName() const override { return QStringLiteral("Normal (_n)"); }
    bool createsNewMaterialLayer() const override { return destination() == NewM21Layer; }
    bool createsAuxiliaryMap() const override { return destination() == AuxiliaryNormalMap; }
    QList<QImage> generatedAuxiliaryFrames() const override { return m_previewAuxFrames; }

    /**
     * @brief Generates a tangent-space normal map from a source albedo/height image.
     */
    static QImage generateNormalMap(const QImage &source,
                                    double bumpStrength = 2.0,
                                    int bevelDepth = 2,
                                    bool invertY = false,
                                    bool invertX = false);

    /**
     * @brief Renders a 2D Phong-illuminated preview of albedo + normal map with a virtual point light.
     */
    static QImage renderShadedPreview(const QImage &albedo,
                                      const QImage &normalMap,
                                      double lightAngleDeg = 45.0,
                                      double lightElevationDeg = 45.0);

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;

private slots:
    void onParametersChanged();
    void onLightChanged();

private:
    void setupFilterUI();

    // UI Controls
    QDoubleSpinBox*                 m_bumpSpin = nullptr;
    QSlider*                        m_bumpSlider = nullptr;

    QSpinBox*                       m_bevelSpin = nullptr;
    QSlider*                        m_bevelSlider = nullptr;

    QCheckBox*                      m_invertYCheck = nullptr;
    QCheckBox*                      m_invertXCheck = nullptr;

    QComboBox*                      m_destCombo = nullptr;
    QCheckBox*                      m_selectedOnlyCheck = nullptr;

    // Interactive Shading Preview
    QGroupBox*                      m_lightGroup = nullptr;
    QSlider*                        m_lightAngleSlider = nullptr;
    QSlider*                        m_lightElevSlider = nullptr;
    QLabel*                         m_previewThumbnail = nullptr;

    // Cached preview state
    QImage                          m_previewAtlas;
    QList<QImage>                   m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;
    QList<QImage>                   m_previewAuxFrames;
};

#endif // NORMALGENERATORDIALOG_H
