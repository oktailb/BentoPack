#ifndef DESPILLFILTERDIALOG_H
#define DESPILLFILTERDIALOG_H

#include "widgets/filterdialogbase.h"

class QSlider;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;

/**
 * @brief Floating interactive tool dialog for edge halo cleanup (Despill / Anti-Fringe) with real-time live preview.
 */
class DespillFilterDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    enum DespillMode {
        ColorClamping = 0, // Gentle despill: replaces fringe color with interior neighbor RGB
        StrictAlpha   = 1  // Erases fringe pixels (sets alpha to 0)
    };

    explicit DespillFilterDialog(SpriteDocument *doc,
                                 QUndoStack *undoStack = nullptr,
                                 QWidget *parent = nullptr);
    ~DespillFilterDialog() override = default;

    QRgb fringeColor() const { return m_fringeColor; }
    int tolerance() const;
    DespillMode mode() const;
    bool isSelectedFramesOnly() const;

    /**
     * @brief Static helper executing the despill algorithm on an image.
     */
    static QImage applyDespill(const QImage &source,
                               QRgb fringeColor,
                               int tolerance,
                               DespillMode mode,
                               const QList<QRect> &targetAreas = QList<QRect>(),
                               int *outPixelsModified = nullptr);

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;

private slots:
    void onParametersChanged();
    void pickFringeColor();

private:
    void setupFilterUI();
    void updateColorSwatch(QRgb color);

    QRgb                            m_fringeColor = 0;

    QImage                          m_previewAtlas;
    QList<QPixmap>                  m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;

    QLabel*                         m_swatchLabel = nullptr;
    QPushButton*                    m_pickColorBtn = nullptr;

    QSlider*                        m_toleranceSlider = nullptr;
    QSpinBox*                       m_toleranceSpin = nullptr;

    QComboBox*                      m_modeCombo = nullptr;
    QCheckBox*                      m_selectedOnlyCheck = nullptr;
};

#endif // DESPILLFILTERDIALOG_H
