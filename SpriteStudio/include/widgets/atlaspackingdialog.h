#ifndef ATLASPACKINGDIALOG_H
#define ATLASPACKINGDIALOG_H

#include "widgets/filterdialogbase.h"
#include "packer/atlaspacker.h"

class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QGroupBox;

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
    ~AtlasPackingDialog() override = default;

    AtlasPacker::PackOptions packOptions() const;
    bool isTrimEnabled() const;
    void setDeduplicate(bool dedup);
    bool isDeduplicateEnabled() const;
    void setAlgorithm(int algo);

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;

private slots:
    void onParametersChanged();

private:
    void setupUI();
    void updateStatsUI(const AtlasPackResult &res);

    // Options UI controls
    QComboBox   *m_comboAlgorithm = nullptr;
    QSpinBox    *m_spinPadding = nullptr;
    QSpinBox    *m_spinBorderPadding = nullptr;
    QSpinBox    *m_spinExtrude = nullptr;
    QCheckBox   *m_checkPowerOfTwo = nullptr;
    QCheckBox   *m_checkForceSquare = nullptr;
    QCheckBox   *m_checkDeduplicate = nullptr;
    QCheckBox   *m_checkTrim = nullptr;

    // Live estimation labels
    QLabel      *m_lblDimensions = nullptr;
    QLabel      *m_lblEfficiency = nullptr;
    QLabel      *m_lblSavedFrames = nullptr;

    // Cache of last preview result for undo command creation
    QImage                          m_previewAtlas;
    QList<QImage>                   m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;
    QMap<QString, SpriteAnimation>  m_previewAnimations;
};

#endif // ATLASPACKINGDIALOG_H
