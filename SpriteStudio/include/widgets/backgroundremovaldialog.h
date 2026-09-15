#ifndef BACKGROUNDREMOVALDIALOG_H
#define BACKGROUNDREMOVALDIALOG_H

#include <QDialog>
#include <QImage>
#include <QPixmap>
#include <QList>
#include <QMap>
#include <QTimer>
#include "model/spritedocument.h"

class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QDialogButtonBox;
class QGroupBox;
class SpriteDocument;
class QUndoStack;

/**
 * @brief Floating interactive tool dialog for background removal with real-time live preview.
 *
 * Mimics GIMP/Photoshop filter dialogs:
 * - Shows sampled background color and controls for tolerance, alpha, and vertical reading order.
 * - Updates the atlas and detected frame bounding boxes live as parameters change.
 * - Reverts back to the original document state on Cancel.
 * - Commits changes via an undoable command on OK.
 */
class BackgroundRemovalDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BackgroundRemovalDialog(SpriteDocument *doc, QUndoStack *undoStack = nullptr, QWidget *parent = nullptr);
    ~BackgroundRemovalDialog() override;

    int colorTolerance() const;
    int alphaThreshold() const;
    int verticalTolerance() const;
    bool isSmartCropEnabled() const;
    double overlapThreshold() const;

protected:
    void reject() override;
    void accept() override;

private slots:
    void onParametersChanged();
    void onPreviewTimeout();
    void onResetDefaults();

private:
    void setupUI();
    void updateColorSwatch(QRgb color);
    void applyLivePreview();
    void restoreInitialState();

    SpriteDocument*                 m_document;
    QUndoStack*                     m_undoStack;
    QTimer                          m_debounceTimer;

    // Initial state backups for full restoration on Cancel
    QImage                          m_initialAtlas;
    QList<QPixmap>                  m_initialFrames;
    QList<SpriteBox>                m_initialBoxes;
    QMap<QString, SpriteAnimation>  m_initialAnimations;
    QRgb                            m_detectedBgColor;

    // Current preview state
    QImage                          m_previewAtlas;
    QList<QPixmap>                  m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;
    bool                            m_previewApplied;

    // UI Widgets
    QLabel*                         m_swatchLabel;
    QLabel*                         m_colorInfoLabel;

    QSlider*                        m_colorToleranceSlider;
    QSpinBox*                       m_colorToleranceSpin;

    QSlider*                        m_alphaThresholdSlider;
    QSpinBox*                       m_alphaThresholdSpin;

    QSlider*                        m_verticalToleranceSlider;
    QSpinBox*                       m_verticalToleranceSpin;

    QCheckBox*                      m_smartCropCheck;
    QDoubleSpinBox*                 m_overlapSpin;

    QCheckBox*                      m_livePreviewCheck;
    QLabel*                         m_statusBadge;

    QDialogButtonBox*               m_buttonBox;
};

#endif // BACKGROUNDREMOVALDIALOG_H
