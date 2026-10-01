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

#ifndef ATLASPACKINGDIALOG_H
#define ATLASPACKINGDIALOG_H

#include "widgets/filterdialogbase.h"
#include "packer/atlaspacker.h"
#include <QFutureWatcher>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QGroupBox;
class QProgressBar;
class QPushButton;

struct AsyncPackJobResult {
    uint64_t jobId = 0;
    AtlasPackResult packResult;
    QList<QImage> workingFrames;
    QList<SpriteBox> workingBoxes;
    AtlasPacker::PackOptions opts;
};

/**
 * @brief Interactive in-editor tool dialog for packing document atlas using MaxRects,
 * with real-time live preview and animation preservation.
 */
class AtlasPackingDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    explicit AtlasPackingDialog(SpriteDocument *doc,
                                QUndoStack *undoStack = nullptr,
                                QWidget *parent = nullptr);
    ~AtlasPackingDialog() override;

    AtlasPacker::PackOptions packOptions() const;
    bool isTrimEnabled() const;
    void setDeduplicate(bool dedup);
    bool isDeduplicateEnabled() const;
    void setAlgorithm(int algo);

    void accept() override;
    void reject() override;
    void waitForPendingPreview();

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;

private slots:
    void onParametersChanged();
    void onPackingFinished();

private:
    void setupUI();
    void updateStatsUI(const AtlasPackResult &res);
    void setProcessingState(bool processing);

    // Options UI controls
    QComboBox       *m_comboAlgorithm = nullptr;
    QSpinBox        *m_spinPadding = nullptr;
    QSpinBox        *m_spinBorderPadding = nullptr;
    QSpinBox        *m_spinExtrude = nullptr;
    QCheckBox       *m_checkPowerOfTwo = nullptr;
    QCheckBox       *m_checkForceSquare = nullptr;
    QCheckBox       *m_checkDeduplicate = nullptr;
    QCheckBox       *m_checkTrim = nullptr;
    QSpinBox        *m_spinThreads = nullptr;
    QPushButton     *m_btnPackNow = nullptr;
    QProgressBar    *m_progressBar = nullptr;

    // Live estimation labels
    QLabel          *m_lblDimensions = nullptr;
    QLabel          *m_lblEfficiency = nullptr;
    QLabel          *m_lblSavedFrames = nullptr;

    // Asynchronous background computation
    QFutureWatcher<AsyncPackJobResult> m_futureWatcher;
    uint64_t m_currentJobId = 0;
    bool m_isProcessing = false;

    // Cache of last preview result for undo command creation
    QImage                          m_previewAtlas;
    QList<QImage>                   m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;
    QMap<QString, SpriteAnimation>  m_previewAnimations;
};

#endif // ATLASPACKINGDIALOG_H
