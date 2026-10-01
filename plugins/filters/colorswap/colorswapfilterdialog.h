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

#ifndef COLORSWAPFILTERDIALOG_H
#define COLORSWAPFILTERDIALOG_H

#include "widgets/filterdialogbase.h"

class QSlider;
class QSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;

/**
 * @brief Floating interactive tool dialog for color swapping and alt-skin generation with live preview.
 */
class ColorSwapFilterDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    explicit ColorSwapFilterDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr);
    ~ColorSwapFilterDialog() override = default;

    QRgb sourceColor() const { return m_sourceColor; }
    QRgb targetColor() const { return m_targetColor; }
    int tolerance() const;
    bool isPreserveShading() const;
    bool isSelectedFramesOnly() const;

    /**
     * @brief Static helper executing color swap on an image.
     */
    static QImage applyColorSwap(const QImage &source,
                                 QRgb srcColor,
                                 QRgb dstColor,
                                 int tolerance,
                                 bool preserveShading,
                                 const QList<QRect> &targetAreas = QList<QRect>(),
                                 int *outPixelsModified = nullptr);

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;

private slots:
    void onParametersChanged();
    void pickSourceColor();
    void pickTargetColor();

private:
    void setupFilterUI();
    void updateSwatches();

    QRgb                            m_sourceColor = qRgb(0, 180, 0); // Default green
    QRgb                            m_targetColor = qRgb(220, 30, 30); // Default red (fire variant)

    QImage                          m_previewAtlas;
    QList<QImage>                  m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;

    QLabel*                         m_srcSwatchLabel = nullptr;
    QPushButton*                    m_pickSrcBtn = nullptr;

    QLabel*                         m_dstSwatchLabel = nullptr;
    QPushButton*                    m_pickDstBtn = nullptr;

    QSlider*                        m_toleranceSlider = nullptr;
    QSpinBox*                       m_toleranceSpin = nullptr;

    QCheckBox*                      m_preserveShadingCheck = nullptr;
    QCheckBox*                      m_selectedOnlyCheck = nullptr;
};

#endif // COLORSWAPFILTERDIALOG_H
