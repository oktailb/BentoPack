#include "widgets/polygonmeshdialog.h"
#include "geometry/contourtracer.h"
#include "geometry/polygonsimplifier.h"
#include "geometry/triangulator.h"
#include "commands/meshcommands.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsPathItem>
#include <QPainter>
#include <QPainterPath>
#include <QUndoStack>
#include <QEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QSettings>

using namespace SpriteStudioGeometry;
using namespace SpriteStudioCommands;

PolygonMeshDialog::PolygonMeshDialog(SpriteDocument *document,
                                     QUndoStack *undoStack,
                                     int targetIndex,
                                     QWidget *parent)
    : QDialog(parent)
    , m_document(document)
    , m_undoStack(undoStack)
    ,    m_targetIndex(targetIndex)
{
    resize(820, 560);

    setupUi();
    retranslateUi();
    updatePreviewAndMetrics();
}

void PolygonMeshDialog::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(16);

    // Left Panel: Live Preview
    m_previewGroup = new QGroupBox(this);
    QVBoxLayout *previewLayout = new QVBoxLayout(m_previewGroup);

    m_previewScene = new QGraphicsScene(this);
    m_previewView = new QGraphicsView(m_previewScene, this);
    m_previewView->setRenderHint(QPainter::Antialiasing, true);
    m_previewView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_previewView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_previewView->setBackgroundBrush(QBrush(QColor(30, 30, 35)));
    previewLayout->addWidget(m_previewView);

    mainLayout->addWidget(m_previewGroup, 3);

    // Right Panel: Tuning Parameters & Dashboard
    QVBoxLayout *controlsLayout = new QVBoxLayout();
    controlsLayout->setSpacing(12);

    // Parameters Group
    m_paramsGroup = new QGroupBox(this);
    QGridLayout *paramsGrid = new QGridLayout(m_paramsGroup);
    paramsGrid->setSpacing(8);

    // Load persisted settings or use tuned defaults
    QSettings settings;
    double defTol = settings.value(QStringLiteral("PolygonMesh/Tolerance"), 1.0).toDouble();
    int defAlpha = settings.value(QStringLiteral("PolygonMesh/AlphaThreshold"), 10).toInt();
    double defPad = settings.value(QStringLiteral("PolygonMesh/Padding"), 2.0).toDouble();
    int defMaxV = settings.value(QStringLiteral("PolygonMesh/MaxVertices"), 28).toInt();

    // 1. Tolerance (Epsilon)
    m_lblTolerance = new QLabel(this);
    paramsGrid->addWidget(m_lblTolerance, 0, 0);
    m_toleranceSpin = new QDoubleSpinBox(this);
    m_toleranceSpin->setRange(0.2, 10.0);
    m_toleranceSpin->setSingleStep(0.2);
    m_toleranceSpin->setValue(defTol);
    paramsGrid->addWidget(m_toleranceSpin, 0, 1);

    m_toleranceSlider = new QSlider(Qt::Horizontal, this);
    m_toleranceSlider->setRange(2, 100); // 0.2 to 10.0 (* 10)
    m_toleranceSlider->setValue(static_cast<int>(std::round(defTol * 10.0)));
    paramsGrid->addWidget(m_toleranceSlider, 0, 2);

    // 2. Alpha Threshold
    m_lblAlpha = new QLabel(this);
    paramsGrid->addWidget(m_lblAlpha, 1, 0);
    m_alphaSpin = new QSpinBox(this);
    m_alphaSpin->setRange(1, 254);
    m_alphaSpin->setValue(defAlpha);
    paramsGrid->addWidget(m_alphaSpin, 1, 1);

    m_alphaSlider = new QSlider(Qt::Horizontal, this);
    m_alphaSlider->setRange(1, 254);
    m_alphaSlider->setValue(defAlpha);
    paramsGrid->addWidget(m_alphaSlider, 1, 2);

    // 3. Padding (Outward Dilation)
    m_lblPadding = new QLabel(this);
    paramsGrid->addWidget(m_lblPadding, 2, 0);
    m_paddingSpin = new QDoubleSpinBox(this);
    m_paddingSpin->setRange(0.0, 12.0);
    m_paddingSpin->setSingleStep(0.5);
    m_paddingSpin->setValue(defPad);
    paramsGrid->addWidget(m_paddingSpin, 2, 1);

    m_paddingSlider = new QSlider(Qt::Horizontal, this);
    m_paddingSlider->setRange(0, 120); // 0.0 to 12.0 (* 10)
    m_paddingSlider->setValue(static_cast<int>(std::round(defPad * 10.0)));
    paramsGrid->addWidget(m_paddingSlider, 2, 2);

    // 4. Max Vertices
    m_lblMaxVertices = new QLabel(this);
    paramsGrid->addWidget(m_lblMaxVertices, 3, 0);
    m_maxVerticesSpin = new QSpinBox(this);
    m_maxVerticesSpin->setRange(4, 96);
    m_maxVerticesSpin->setValue(defMaxV);
    paramsGrid->addWidget(m_maxVerticesSpin, 3, 1, 1, 2);

    controlsLayout->addWidget(m_paramsGroup);

    // Metrics Dashboard
    m_metricsGroup = new QGroupBox(this);
    QVBoxLayout *metricsLayout = new QVBoxLayout(m_metricsGroup);
    metricsLayout->setSpacing(6);

    m_lblVertices = new QLabel(this);
    m_lblTriangles = new QLabel(this);
    m_lblArea = new QLabel(this);
    m_lblSavings = new QLabel(this);
    m_lblFrameStatus = new QLabel(this);

    QFont boldFont = m_lblSavings->font();
    boldFont.setBold(true);
    m_lblSavings->setFont(boldFont);
    m_lblSavings->setStyleSheet(QStringLiteral("color: #00FF88; font-size: 13px;"));

    metricsLayout->addWidget(m_lblVertices);
    metricsLayout->addWidget(m_lblTriangles);
    metricsLayout->addWidget(m_lblArea);
    metricsLayout->addWidget(m_lblSavings);
    metricsLayout->addWidget(m_lblFrameStatus);

    controlsLayout->addWidget(m_metricsGroup);

    // Actions
    m_btnApplySelection = new QPushButton(this);
    m_btnApplyAll = new QPushButton(this);
    m_btnRemoveMesh = new QPushButton(this);

    m_btnApplySelection->setStyleSheet(QStringLiteral("background-color: #0088cc; color: white; font-weight: bold; padding: 6px;"));
    m_btnApplyAll->setStyleSheet(QStringLiteral("font-weight: bold; padding: 6px;"));

    m_lblFeedback = new QLabel(this);
    m_lblFeedback->setAlignment(Qt::AlignCenter);
    m_lblFeedback->setTextFormat(Qt::PlainText);

    controlsLayout->addWidget(m_btnApplySelection);
    controlsLayout->addWidget(m_btnApplyAll);
    controlsLayout->addWidget(m_btnRemoveMesh);
    controlsLayout->addWidget(m_lblFeedback);
    controlsLayout->addStretch();

    m_btnBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(m_btnBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    controlsLayout->addWidget(m_btnBox);

    mainLayout->addLayout(controlsLayout, 2);

    // Synchronize Sliders and SpinBoxes
    connect(m_toleranceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        m_toleranceSlider->blockSignals(true);
        m_toleranceSlider->setValue(static_cast<int>(std::round(val * 10.0)));
        m_toleranceSlider->blockSignals(false);
        updatePreviewAndMetrics();
    });
    connect(m_toleranceSlider, &QSlider::valueChanged, this, [this](int ival) {
        m_toleranceSpin->blockSignals(true);
        m_toleranceSpin->setValue(ival / 10.0);
        m_toleranceSpin->blockSignals(false);
        updatePreviewAndMetrics();
    });

    connect(m_alphaSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_alphaSlider->blockSignals(true);
        m_alphaSlider->setValue(val);
        m_alphaSlider->blockSignals(false);
        updatePreviewAndMetrics();
    });
    connect(m_alphaSlider, &QSlider::valueChanged, this, [this](int ival) {
        m_alphaSpin->blockSignals(true);
        m_alphaSpin->setValue(ival);
        m_alphaSpin->blockSignals(false);
        updatePreviewAndMetrics();
    });

    connect(m_paddingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        m_paddingSlider->blockSignals(true);
        m_paddingSlider->setValue(static_cast<int>(std::round(val * 10.0)));
        m_paddingSlider->blockSignals(false);
        updatePreviewAndMetrics();
    });
    connect(m_paddingSlider, &QSlider::valueChanged, this, [this](int ival) {
        m_paddingSpin->blockSignals(true);
        m_paddingSpin->setValue(ival / 10.0);
        m_paddingSpin->blockSignals(false);
        updatePreviewAndMetrics();
    });

    connect(m_maxVerticesSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PolygonMeshDialog::updatePreviewAndMetrics);

    connect(m_btnApplySelection, &QPushButton::clicked, this, &PolygonMeshDialog::applyToSelection);
    connect(m_btnApplyAll, &QPushButton::clicked, this, &PolygonMeshDialog::applyToAllFrames);
    connect(m_btnRemoveMesh, &QPushButton::clicked, this, &PolygonMeshDialog::removeMesh);
}

void PolygonMeshDialog::retranslateUi()
{
    setWindowTitle(tr("Tight Mesh & 2D Polygon Packing"));
    if (m_previewGroup) m_previewGroup->setTitle(tr("Live Preview & Wireframe"));
    if (m_paramsGroup) m_paramsGroup->setTitle(tr("Polygon & Mesh Simplification"));
    if (m_lblTolerance) m_lblTolerance->setText(tr("Approximation Tolerance (\u03b5):"));
    if (m_lblAlpha) m_lblAlpha->setText(tr("Alpha Threshold:"));
    if (m_lblPadding) m_lblPadding->setText(tr("Outward Padding:"));
    if (m_lblMaxVertices) m_lblMaxVertices->setText(tr("Max Vertices:"));
    if (m_metricsGroup) m_metricsGroup->setTitle(tr("Overdraw & Performance Dashboard"));
    if (m_btnApplySelection) m_btnApplySelection->setText(tr("Apply to Selection"));
    if (m_btnApplyAll) m_btnApplyAll->setText(tr("Apply to All Frames"));
    if (m_btnRemoveMesh) m_btnRemoveMesh->setText(tr("Remove Mesh (Reset to Rect)"));
    if (m_btnBox) {
        if (QPushButton *btn = m_btnBox->button(QDialogButtonBox::Close)) {
            btn->setText(tr("Close"));
        }
    }
}

void PolygonMeshDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        updatePreviewAndMetrics();
    }
    QDialog::changeEvent(event);
}

void PolygonMeshDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    fitPreview();
}

void PolygonMeshDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void PolygonMeshDialog::fitPreview()
{
    if (!m_document || m_targetIndex < 0 || m_targetIndex >= m_document->frameCount()) {
        return;
    }
    QImage img = m_document->frame(m_targetIndex);
    if (img.isNull() || img.width() <= 0 || img.height() <= 0) {
        return;
    }

    QRectF targetRect(0, 0, img.width(), img.height());
    if (!m_currentPolygon.isEmpty()) {
        targetRect = targetRect.united(m_currentPolygon.boundingRect());
    }
    // Add a comfortable 6% margin so vertices and dashed wireframes are never clipped
    double mx = std::max(4.0, targetRect.width() * 0.06);
    double my = std::max(4.0, targetRect.height() * 0.06);
    targetRect.adjust(-mx, -my, mx, my);

    m_previewView->fitInView(targetRect, Qt::KeepAspectRatio);
}

void PolygonMeshDialog::computeMeshForFrame(int frameIdx, QPolygonF &outPoly, QList<QPointF> &outVerts, QList<int> &outTris)
{
    outPoly.clear();
    outVerts.clear();
    outTris.clear();

    if (!m_document || frameIdx < 0 || frameIdx >= m_document->frameCount()) {
        return;
    }

    QImage img = m_document->frame(frameIdx);
    if (img.isNull() || img.width() <= 0 || img.height() <= 0) {
        return;
    }

    int alphaThresh = m_alphaSpin->value();
    double eps = m_toleranceSpin->value();
    double pad = m_paddingSpin->value();
    int maxV = m_maxVerticesSpin->value();

    QPolygonF raw = ContourTracer::traceContour(img, alphaThresh);
    outPoly = PolygonSimplifier::simplify(raw, eps, pad, maxV, img.size());
    outVerts = outPoly.toList();
    outTris = Triangulator::triangulate(outPoly);
}

void PolygonMeshDialog::updatePreviewAndMetrics()
{
    if (!m_document || m_document->isEmpty()) return;

    if (m_targetIndex < 0 || m_targetIndex >= m_document->frameCount()) {
        m_targetIndex = 0;
    }

    computeMeshForFrame(m_targetIndex, m_currentPolygon, m_currentVertices, m_currentTriangles);
    saveSettings();

    QImage img = m_document->frame(m_targetIndex);
    m_previewScene->clear();

    // 1. Draw Checkerboard background
    QPixmap bg(16, 16);
    QPainter bgP(&bg);
    bgP.fillRect(0, 0, 8, 8, QColor(45, 45, 50));
    bgP.fillRect(8, 0, 8, 8, QColor(35, 35, 40));
    bgP.fillRect(0, 8, 8, 8, QColor(35, 35, 40));
    bgP.fillRect(8, 8, 8, 8, QColor(45, 45, 50));
    bgP.end();
    m_previewScene->setBackgroundBrush(QBrush(bg));

    // 2. Draw Sprite Image
    m_previewScene->addPixmap(QPixmap::fromImage(img));

    // 3. Draw Rectangular Bounding Box (dashed gray)
    QPen boxPen(QColor(180, 180, 180, 140), 1.0, Qt::DashLine);
    boxPen.setCosmetic(true);
    m_previewScene->addRect(QRectF(0, 0, img.width(), img.height()), boxPen);

    // 4. Draw Triangulation Wireframe
    if (!m_currentTriangles.isEmpty() && m_currentTriangles.size() % 3 == 0) {
        QPen triPen(QColor(0, 220, 255, 140), 1.0, Qt::DashLine);
        triPen.setCosmetic(true);

        const int triCount = m_currentTriangles.size() / 3;
        for (int t = 0; t < triCount; ++t) {
            int i0 = m_currentTriangles[t * 3];
            int i1 = m_currentTriangles[t * 3 + 1];
            int i2 = m_currentTriangles[t * 3 + 2];
            if (i0 < m_currentPolygon.size() && i1 < m_currentPolygon.size() && i2 < m_currentPolygon.size()) {
                const QPointF &p0 = m_currentPolygon[i0];
                const QPointF &p1 = m_currentPolygon[i1];
                const QPointF &p2 = m_currentPolygon[i2];

                m_previewScene->addLine(QLineF(p0, p1), triPen);
                m_previewScene->addLine(QLineF(p1, p2), triPen);
                m_previewScene->addLine(QLineF(p2, p0), triPen);
            }
        }
    }

    // 5. Draw Polygon Contour & Vertices
    if (!m_currentPolygon.isEmpty()) {
        QPen contourPen(QColor(0, 255, 128), 2.0);
        contourPen.setCosmetic(true);
        m_previewScene->addPolygon(m_currentPolygon, contourPen, QBrush(QColor(0, 255, 128, 25)));

        // Vertices markers
        QPen vPen(Qt::black, 1.0);
        vPen.setCosmetic(true);
        QBrush vBrush(QColor(255, 230, 40));
        for (const QPointF &pt : m_currentPolygon) {
            m_previewScene->addEllipse(pt.x() - 2.5, pt.y() - 2.5, 5.0, 5.0, vPen, vBrush);
        }
    }

    // Zoom view to fit nicely with margin
    fitPreview();

    // Update Dashboard Labels
    int vCount = m_currentPolygon.size();
    int tCount = m_currentTriangles.size() / 3;
    double polyArea = Triangulator::calculateArea(m_currentPolygon);
    double boxArea = static_cast<double>(img.width() * img.height());
    double savings = Triangulator::calculateOverdrawSavings(m_currentPolygon, img.size());

    m_lblVertices->setText(tr("Vertices: %1").arg(vCount));
    m_lblTriangles->setText(tr("Triangles: %1").arg(tCount));
    m_lblArea->setText(tr("Polygon Area: %1 px² (vs %2 px² box)").arg(static_cast<int>(polyArea)).arg(static_cast<int>(boxArea)));
    m_lblSavings->setText(tr("GPU Overdraw Eliminated: %1%").arg(QString::number(savings, 'f', 1)));

    if (m_document && m_targetIndex >= 0 && m_targetIndex < m_document->frameCount()) {
        const SpriteBox &box = m_document->box(m_targetIndex);
        if (box.hasPolygonMesh) {
            m_lblFrameStatus->setText(tr("Target Frame %1: Mesh already applied (%2 vertices, %3 tris)")
                                       .arg(m_targetIndex + 1)
                                       .arg(box.vertices.size())
                                       .arg(box.triangles.size() / 3));
            m_lblFrameStatus->setStyleSheet(QStringLiteral("color: #00E5FF; font-size: 11px;"));
        } else {
            m_lblFrameStatus->setText(tr("Target Frame %1: Rectangle mode (no mesh applied)")
                                       .arg(m_targetIndex + 1));
            m_lblFrameStatus->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px; font-style: italic;"));
        }
    }
}

void PolygonMeshDialog::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("PolygonMesh/Tolerance"), m_toleranceSpin->value());
    settings.setValue(QStringLiteral("PolygonMesh/AlphaThreshold"), m_alphaSpin->value());
    settings.setValue(QStringLiteral("PolygonMesh/Padding"), m_paddingSpin->value());
    settings.setValue(QStringLiteral("PolygonMesh/MaxVertices"), m_maxVerticesSpin->value());
}

void PolygonMeshDialog::applyToSelection()
{
    if (!m_document) return;

    QList<int> sel = m_document->selectedFrameIndices();
    if (sel.isEmpty()) {
        sel.append(m_targetIndex);
    }

    QList<MeshState> newStates;
    for (int idx : sel) {
        QPolygonF poly;
        QList<QPointF> verts;
        QList<int> tris;
        computeMeshForFrame(idx, poly, verts, tris);

        MeshState st;
        st.index = idx;
        st.hasPolygonMesh = (!poly.isEmpty() && !tris.isEmpty());
        st.polygon = poly;
        st.vertices = verts;
        st.triangles = tris;
        newStates.append(st);
    }

    if (m_undoStack) {
        m_undoStack->push(new SetPolygonMeshCommand(m_document, newStates));
    } else {
        for (const MeshState &st : newStates) {
            SpriteBox b = m_document->box(st.index);
            b.hasPolygonMesh = st.hasPolygonMesh;
            b.polygon = st.polygon;
            b.vertices = st.vertices;
            b.triangles = st.triangles;
            m_document->setBox(st.index, b);
        }
    }

    saveSettings();
    if (m_lblFeedback) {
        m_lblFeedback->setText(tr("✓ Mesh applied to %1 frame(s)!").arg(sel.size()));
        m_lblFeedback->setStyleSheet(QStringLiteral("color: #00FF88; font-weight: bold; background: rgba(0, 255, 136, 30); padding: 5px; border-radius: 4px; border: 1px solid #00FF88;"));
    }
    updatePreviewAndMetrics();
}

void PolygonMeshDialog::applyToAllFrames()
{
    if (!m_document) return;

    QList<MeshState> newStates;
    for (int idx = 0; idx < m_document->frameCount(); ++idx) {
        QPolygonF poly;
        QList<QPointF> verts;
        QList<int> tris;
        computeMeshForFrame(idx, poly, verts, tris);

        MeshState st;
        st.index = idx;
        st.hasPolygonMesh = (!poly.isEmpty() && !tris.isEmpty());
        st.polygon = poly;
        st.vertices = verts;
        st.triangles = tris;
        newStates.append(st);
    }

    if (m_undoStack) {
        m_undoStack->push(new SetPolygonMeshCommand(m_document, newStates));
    } else {
        for (const MeshState &st : newStates) {
            SpriteBox b = m_document->box(st.index);
            b.hasPolygonMesh = st.hasPolygonMesh;
            b.polygon = st.polygon;
            b.vertices = st.vertices;
            b.triangles = st.triangles;
            m_document->setBox(st.index, b);
        }
    }

    saveSettings();
    if (m_lblFeedback) {
        m_lblFeedback->setText(tr("✓ Mesh applied to all %1 frames!").arg(m_document->frameCount()));
        m_lblFeedback->setStyleSheet(QStringLiteral("color: #00FF88; font-weight: bold; background: rgba(0, 255, 136, 30); padding: 5px; border-radius: 4px; border: 1px solid #00FF88;"));
    }
    updatePreviewAndMetrics();
}

void PolygonMeshDialog::removeMesh()
{
    if (!m_document) return;

    QList<int> sel = m_document->selectedFrameIndices();
    if (sel.isEmpty()) {
        sel.append(m_targetIndex);
    }

    QList<MeshState> newStates;
    for (int idx : sel) {
        MeshState st;
        st.index = idx;
        st.hasPolygonMesh = false;
        newStates.append(st);
    }

    if (m_undoStack) {
        m_undoStack->push(new SetPolygonMeshCommand(m_document, newStates));
    } else {
        for (const MeshState &st : newStates) {
            SpriteBox b = m_document->box(st.index);
            b.hasPolygonMesh = false;
            b.polygon.clear();
            b.vertices.clear();
            b.triangles.clear();
            m_document->setBox(st.index, b);
        }
    }

    if (m_lblFeedback) {
        m_lblFeedback->setText(tr("✓ Tight mesh removed. Reverted to rectangle."));
        m_lblFeedback->setStyleSheet(QStringLiteral("color: #FFAA00; font-weight: bold; background: rgba(255, 170, 0, 30); padding: 5px; border-radius: 4px; border: 1px solid #FFAA00;"));
    }
    updatePreviewAndMetrics();
}
