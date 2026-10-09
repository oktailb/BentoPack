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

#ifndef SPRITEDETECTIONDIALOG_H
#define SPRITEDETECTIONDIALOG_H

#include "widgets/filterdialogbase.h"

class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;

class SpriteDetectionDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    explicit SpriteDetectionDialog(SpriteDocument *doc,
                                  QUndoStack *undoStack = nullptr,
                                  QWidget *parent = nullptr);
    ~SpriteDetectionDialog() override = default;

    int alphaThreshold() const;
    int verticalTolerance() const;
    bool isSmartCropEnabled() const;
    double overlapThreshold() const;
    int minSliceSize() const;

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;

private slots:
    void onParametersChanged();

private:
    void setupFilterUI();

    QImage         m_previewAtlas;
    QList<QImage>  m_previewFrames;
    QList<SpriteBox> m_previewBoxes;

    QSlider        *m_alphaThresholdSlider = nullptr;
    QSpinBox       *m_alphaThresholdSpin = nullptr;
    QSlider        *m_verticalToleranceSlider = nullptr;
    QSpinBox       *m_verticalToleranceSpin = nullptr;
    QCheckBox      *m_smartCropCheck = nullptr;
    QDoubleSpinBox *m_overlapSpin = nullptr;
    QSpinBox       *m_minSliceSizeSpin = nullptr;
};

#endif // SPRITEDETECTIONDIALOG_H
