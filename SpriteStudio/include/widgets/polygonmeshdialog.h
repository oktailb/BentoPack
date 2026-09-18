#ifndef POLYGONMESHDIALOG_H
#define POLYGONMESHDIALOG_H

#include <QDialog>
#include <QPolygonF>
#include <QList>
#include "model/spritedocument.h"

class QGraphicsView;
class QGraphicsScene;
class QDoubleSpinBox;
class QSpinBox;
class QSlider;
class QLabel;
class QPushButton;
class QGroupBox;
class QDialogButtonBox;
class QUndoStack;

/**
 * @brief Interactive dialog for tuning 2D polygon and tight mesh parameters
 *        with live preview, wireframe rendering, and GPU overdraw reduction metrics.
 */
class PolygonMeshDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PolygonMeshDialog(SpriteDocument *document,
                               QUndoStack *undoStack,
                               int targetIndex = 0,
                               QWidget *parent = nullptr);
    ~PolygonMeshDialog() override = default;

    void retranslateUi();

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void fitPreview();
    void updatePreviewAndMetrics();
    void applyToSelection();
    void applyToAllFrames();
    void removeMesh();

private:
    void setupUi();
    void computeMeshForFrame(int frameIdx, QPolygonF &outPoly, QList<QPointF> &outVerts, QList<int> &outTris);

    SpriteDocument *m_document = nullptr;
    QUndoStack     *m_undoStack = nullptr;
    int             m_targetIndex = 0;

    // Current preview computed mesh
    QPolygonF       m_currentPolygon;
    QList<QPointF>  m_currentVertices;
    QList<int>      m_currentTriangles;

    // UI Widgets
    QGraphicsView  *m_previewView = nullptr;
    QGraphicsScene *m_previewScene = nullptr;

    QDoubleSpinBox *m_toleranceSpin = nullptr;
    QSlider        *m_toleranceSlider = nullptr;

    QSpinBox       *m_alphaSpin = nullptr;
    QSlider        *m_alphaSlider = nullptr;

    QDoubleSpinBox *m_paddingSpin = nullptr;
    QSlider        *m_paddingSlider = nullptr;

    QSpinBox       *m_maxVerticesSpin = nullptr;

    QGroupBox      *m_previewGroup = nullptr;
    QGroupBox      *m_paramsGroup = nullptr;
    QLabel         *m_lblTolerance = nullptr;
    QLabel         *m_lblAlpha = nullptr;
    QLabel         *m_lblPadding = nullptr;
    QLabel         *m_lblMaxVertices = nullptr;
    QGroupBox      *m_metricsGroup = nullptr;

    QLabel         *m_lblVertices = nullptr;
    QLabel         *m_lblTriangles = nullptr;
    QLabel         *m_lblArea = nullptr;
    QLabel         *m_lblSavings = nullptr;
    QLabel         *m_lblFrameStatus = nullptr;

    QPushButton    *m_btnApplySelection = nullptr;
    QPushButton    *m_btnApplyAll = nullptr;
    QPushButton    *m_btnRemoveMesh = nullptr;
    QLabel         *m_lblFeedback = nullptr;
    QDialogButtonBox *m_btnBox = nullptr;

    void saveSettings();
};

namespace SpriteStudioWidgets {
    using PolygonMeshDialog = ::PolygonMeshDialog;
}

#endif // POLYGONMESHDIALOG_H
