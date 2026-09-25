// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#ifndef BACKGROUNDREMOVALDIALOG_H
#define BACKGROUNDREMOVALDIALOG_H

#include "widgets/filterdialogbase.h"

class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;

/**
 * @brief Floating interactive tool dialog for background removal with real-time live preview.
 *
 * Derives from FilterDialogBase:
 * - Detects dominant background color and presents controls for tolerance, alpha, and vertical order.
 * - Live preview re-segments the atlas as parameters change.
 * - Guaranteed restoration on Cancel.
 * - Commits via RemoveBackgroundCommand on OK.
 */
class BackgroundRemovalDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    explicit BackgroundRemovalDialog(SpriteDocument *doc,
                                     QUndoStack *undoStack = nullptr,
                                     QWidget *parent = nullptr);
    ~BackgroundRemovalDialog() override = default;

    int colorTolerance() const;
    int alphaThreshold() const;
    int verticalTolerance() const;
    bool isSmartCropEnabled() const;
    double overlapThreshold() const;

protected:
    // FilterDialogBase implementation
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;
    bool defaultAutoDetectBoxes() const override { return true; }

private slots:
    void onParametersChanged();

private:
    void setupFilterUI();
    void updateColorSwatch(QRgb color);

    QRgb                            m_detectedBgColor = 0;

    // Current preview state (result of last segmentation)
    QImage                          m_previewAtlas;
    QList<QImage>                  m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;

    // Filter-specific UI Widgets
    QLabel*                         m_swatchLabel = nullptr;
    QLabel*                         m_colorInfoLabel = nullptr;

    QSlider*                        m_colorToleranceSlider = nullptr;
    QSpinBox*                       m_colorToleranceSpin = nullptr;

    QSlider*                        m_alphaThresholdSlider = nullptr;
    QSpinBox*                       m_alphaThresholdSpin = nullptr;

    QSlider*                        m_verticalToleranceSlider = nullptr;
    QSpinBox*                       m_verticalToleranceSpin = nullptr;

    QCheckBox*                      m_smartCropCheck = nullptr;
    QDoubleSpinBox*                 m_overlapSpin = nullptr;
};

#endif // BACKGROUNDREMOVALDIALOG_H
