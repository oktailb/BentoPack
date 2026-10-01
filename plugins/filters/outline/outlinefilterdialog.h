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

#ifndef OUTLINEFILTERDIALOG_H
#define OUTLINEFILTERDIALOG_H

#include "widgets/filterdialogbase.h"

class QSlider;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

/**
 * @brief Floating interactive tool dialog for generating sprite outlines and silhouettes.
 */
class OutlineFilterDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    enum Connectivity {
        FourConnected = 0, // Orthogonal cross (crisp pixel art style)
        EightConnected = 1  // Diagonal inclusive (smooth contour)
    };

    explicit OutlineFilterDialog(SpriteDocument *doc,
                                 QUndoStack *undoStack = nullptr,
                                 QWidget *parent = nullptr);
    ~OutlineFilterDialog() override = default;

    int thickness() const;
    QRgb strokeColor() const { return m_strokeColor; }
    Connectivity connectivity() const;
    bool isSilhouetteMode() const;
    bool isSelectedFramesOnly() const;

    /**
     * @brief Static helper executing the outline generation algorithm on an image.
     */
    static QImage applyOutline(const QImage &source,
                               int thickness,
                               QRgb strokeColor,
                               Connectivity connectivity,
                               bool silhouette,
                               const QList<QRect> &targetAreas = QList<QRect>());

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;
    bool defaultAutoDetectBoxes() const override { return true; }

private slots:
    void onParametersChanged();
    void pickStrokeColor();
    void setPresetColor(const QColor &c);

private:
    void setupFilterUI();
    void updateColorSwatch(QRgb color);

    QRgb                            m_strokeColor = qRgb(0, 0, 0); // Default black outline

    QImage                          m_previewAtlas;
    QList<QImage>                  m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;

    QLabel*                         m_swatchLabel = nullptr;
    QPushButton*                    m_pickColorBtn = nullptr;

    QSlider*                        m_thicknessSlider = nullptr;
    QSpinBox*                       m_thicknessSpin = nullptr;

    QComboBox*                      m_connectCombo = nullptr;
    QCheckBox*                      m_silhouetteCheck = nullptr;
    QCheckBox*                      m_selectedOnlyCheck = nullptr;
};

#endif // OUTLINEFILTERDIALOG_H
