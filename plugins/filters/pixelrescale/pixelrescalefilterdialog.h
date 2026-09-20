// This file is part of the SpriteStudio Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#ifndef PIXELRESCALEFILTERDIALOG_H
#define PIXELRESCALEFILTERDIALOG_H

#include "widgets/filterdialogbase.h"

class QComboBox;
class QLabel;

/**
 * @brief Floating interactive tool dialog for clean pixel art rescaling (Nearest-Neighbor & Scale2x).
 */
class PixelRescaleFilterDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    enum Algorithm {
        NearestNeighbor = 0,
        Scale2x = 1
    };

    explicit PixelRescaleFilterDialog(SpriteDocument *doc,
                                      QUndoStack *undoStack = nullptr,
                                      QWidget *parent = nullptr);
    ~PixelRescaleFilterDialog() override = default;

    double scaleFactor() const;
    Algorithm algorithm() const;

    /**
     * @brief Static helper executing Nearest-Neighbor integer/fractional rescaling.
     */
    static QImage applyNearest(const QImage &source, double factor);

    /**
     * @brief Static helper executing Scale2x (AdvMAME2x) procedural pixel art scaling.
     */
    static QImage applyScale2x(const QImage &source);

    /**
     * @brief Static helper executing Scale3x procedural pixel art scaling.
     */
    static QImage applyScale3x(const QImage &source);

    /**
     * @brief General rescaling dispatcher.
     */
    static QImage applyRescale(const QImage &source, double factor, Algorithm algo);

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
    void updateDimensionLabel();

    // Preview state
    QImage                          m_previewAtlas;
    QList<QImage>                  m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;

    // UI controls
    QComboBox*                      m_scaleCombo = nullptr;
    QComboBox*                      m_algoCombo = nullptr;
    QLabel*                         m_dimLabel = nullptr;
};

#endif // PIXELRESCALEFILTERDIALOG_H
