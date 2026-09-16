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
    QList<QPixmap>                  m_previewFrames;
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
