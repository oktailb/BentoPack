#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QUndoStack>
#include <QTemporaryDir>
#include <QFile>

#include "geometry/contourtracer.h"
#include "geometry/polygonsimplifier.h"
#include "geometry/triangulator.h"
#include "model/spritedocument.h"
#include "commands/meshcommands.h"
#include "project/projectmanager.h"
#include "atlasboxitem.h"

using namespace SpriteStudioGeometry;
using namespace SpriteStudioCommands;

class TestMesh : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // 1. Contour Tracer Tests
    void testContourTracingEmpty();
    void testContourTracingSolidRect();
    void testContourTracingDiamond();

    // 2. Polygon Simplifier Tests
    void testPolygonSimplificationRDP();
    void testPolygonSimplificationVertexBudget();
    void testPolygonSimplificationPadding();

    // 3. Triangulator Tests
    void testTriangulationConvex();
    void testTriangulationConcave();
    void testShoelaceAreaAndOverdrawSavings();

    // 4. Data Model & Commands Tests
    void testSpriteBoxDataModel();
    void testMeshCommandUndoRedo();

    // 5. Project Persistence Tests
    void testSspSerializationWithMesh();

    // 6. Interactive Atlas Vertex Manipulation Tests
    void testAtlasBoxItemVertexManipulation();
};

void TestMesh::initTestCase()
{
}

void TestMesh::cleanupTestCase()
{
}

void TestMesh::testContourTracingEmpty()
{
    // Empty / null image
    QImage nullImg;
    QPolygonF polyNull = ContourTracer::traceContour(nullImg);
    QVERIFY(polyNull.isEmpty());

    // Transparent image
    QImage transImg(32, 32, QImage::Format_ARGB32_Premultiplied);
    transImg.fill(Qt::transparent);
    QPolygonF polyTrans = ContourTracer::traceContour(transImg, 1);
    QVERIFY(polyTrans.isEmpty());
}

void TestMesh::testContourTracingSolidRect()
{
    // 30x30 image with a solid 10x10 rectangle in the center (from 10,10 to 19,19)
    QImage img(30, 30, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.fillRect(10, 10, 10, 10, QColor(255, 0, 0, 255));
    }

    QPolygonF contour = ContourTracer::traceContour(img, 128);
    QVERIFY(!contour.isEmpty());
    QVERIFY(contour.size() >= 4);
    QVERIFY(Triangulator::calculateArea(contour) > 0.0);

    // Verify bounding rect covers the 10x10 region
    QRectF b = contour.boundingRect();
    QVERIFY(b.left() <= 10.5 && b.right() >= 19.5);
    QVERIFY(b.top() <= 10.5 && b.bottom() >= 19.5);
}

void TestMesh::testContourTracingDiamond()
{
    QImage img(40, 40, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        QPolygon diamond;
        diamond << QPoint(20, 5) << QPoint(35, 20) << QPoint(20, 35) << QPoint(5, 20);
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawPolygon(diamond);
    }

    QPolygonF contour = ContourTracer::traceContour(img, 128);
    QVERIFY(!contour.isEmpty());
    QVERIFY(contour.size() >= 4);

    double area = Triangulator::calculateArea(contour);
    QVERIFY(area > 0.0);
}

void TestMesh::testPolygonSimplificationRDP()
{
    // Create a circular-like polygon with many points
    QPolygonF circlePoly;
    const int numPts = 64;
    const double radius = 20.0;
    const QPointF center(25.0, 25.0);
    for (int i = 0; i < numPts; ++i) {
        double angle = (2.0 * M_PI * i) / numPts;
        circlePoly.append(center + QPointF(radius * std::cos(angle), radius * std::sin(angle)));
    }

    // Simplify with tolerance 2.0
    QPolygonF simplified = PolygonSimplifier::simplify(circlePoly, 2.0, 0.0, 64, QSize(50, 50));
    QVERIFY(simplified.size() < circlePoly.size());
    QVERIFY(simplified.size() >= 4);
    QVERIFY(Triangulator::calculateArea(simplified) > 0.0);
}

void TestMesh::testPolygonSimplificationVertexBudget()
{
    // High-resolution contour
    QPolygonF detailedPoly;
    for (int i = 0; i < 40; ++i) {
        detailedPoly.append(QPointF(i, std::sin(i * 0.5) * 10.0 + 20.0));
    }
    detailedPoly.append(QPointF(39, 40));
    detailedPoly.append(QPointF(0, 40));
    detailedPoly.append(detailedPoly.first());

    // Request max 8 vertices
    QPolygonF capped = PolygonSimplifier::simplify(detailedPoly, 0.5, 0.0, 8, QSize(50, 50));
    // When closed, size is unique_vertices + 1
    int uniqueVertices = capped.isClosed() ? capped.size() - 1 : capped.size();
    QVERIFY(uniqueVertices <= 8);
    QVERIFY(uniqueVertices >= 3);
}

void TestMesh::testPolygonSimplificationPadding()
{
    // 10x10 square
    QPolygonF square;
    square << QPointF(10, 10) << QPointF(20, 10) << QPointF(20, 20) << QPointF(10, 20) << QPointF(10, 10);

    QSize frameBounds(30, 30);
    double initialArea = Triangulator::calculateArea(square);

    // Simplify with 2.0px outward padding
    QPolygonF padded = PolygonSimplifier::simplify(square, 0.1, 2.0, 16, frameBounds);
    double paddedArea = Triangulator::calculateArea(padded);

    // Padding must dilate outwards, increasing area
    QVERIFY(paddedArea > initialArea);

    // Must remain within frame bounds
    QRectF b = padded.boundingRect();
    QVERIFY(b.left() >= 0);
    QVERIFY(b.top() >= 0);
    QVERIFY(b.right() <= frameBounds.width());
    QVERIFY(b.bottom() <= frameBounds.height());
}

void TestMesh::testTriangulationConvex()
{
    // Simple convex quad
    QList<QPointF> quad = { QPointF(0, 0), QPointF(10, 0), QPointF(10, 10), QPointF(0, 10) };
    QList<int> tris = Triangulator::triangulate(quad);

    // For 4 vertices, triangulator produces (4 - 2) = 2 triangles = 6 indices
    QCOMPARE(tris.size(), 6);
    for (int idx : tris) {
        QVERIFY(idx >= 0 && idx < 4);
    }
}

void TestMesh::testTriangulationConcave()
{
    // L-shape (concave polygon with 6 vertices)
    //  (0,0)-----(4,0)
    //    |        |
    //    |      (4,6)-----(10,6)
    //    |                  |
    //  (0,10)------------(10,10)
    QList<QPointF> lShape = {
        QPointF(0, 0),
        QPointF(4, 0),
        QPointF(4, 6),
        QPointF(10, 6),
        QPointF(10, 10),
        QPointF(0, 10)
    };

    QList<int> tris = Triangulator::triangulate(lShape);
    // (6 - 2) = 4 triangles = 12 indices
    QCOMPARE(tris.size(), 12);
    for (int idx : tris) {
        QVERIFY(idx >= 0 && idx < 6);
    }
}

void TestMesh::testShoelaceAreaAndOverdrawSavings()
{
    // 10x10 square in a 20x20 bounding box
    QPolygonF poly;
    poly << QPointF(0, 0) << QPointF(10, 0) << QPointF(10, 10) << QPointF(0, 10) << QPointF(0, 0);

    double area = Triangulator::calculateArea(poly);
    QCOMPARE(area, 100.0);

    // Bounding box is 20x20 = 400
    double savings = Triangulator::calculateOverdrawSavings(poly, QSize(20, 20));
    // (400 - 100) / 400 = 75%
    QVERIFY(qAbs(savings - 75.0) < 0.01);
}

void TestMesh::testSpriteBoxDataModel()
{
    SpriteBox box(QRect(5, 5, 20, 20));
    QVERIFY(!box.hasPolygonMesh);
    QCOMPARE(box.polygonArea(), 400.0); // Fallback to rect area
    QCOMPARE(box.overdrawSavings(), 0.0);

    // Assign triangle mesh
    box.hasPolygonMesh = true;
    box.polygon << QPointF(0, 0) << QPointF(20, 0) << QPointF(10, 20) << QPointF(0, 0);
    box.vertices = { QPointF(0, 0), QPointF(20, 0), QPointF(10, 20) };
    box.triangles = { 0, 1, 2 };

    QVERIFY(box.hasPolygonMesh);
    QCOMPARE(box.polygonArea(), 200.0);
    QCOMPARE(box.overdrawSavings(), 50.0);

    // Check operator==
    SpriteBox box2 = box;
    QVERIFY(box == box2);

    box2.hasPolygonMesh = false;
    QVERIFY(box != box2);
}

void TestMesh::testMeshCommandUndoRedo()
{
    SpriteDocument doc;
    doc.setAtlas(QImage(100, 100, QImage::Format_ARGB32_Premultiplied));
    SpriteBox b(QRect(10, 10, 30, 30));
    doc.addFrame(QImage(30, 30, QImage::Format_ARGB32_Premultiplied), b);
    QCOMPARE(doc.frameCount(), 1);
    QVERIFY(!doc.box(0).hasPolygonMesh);

    QUndoStack stack;
    MeshState st;
    st.index = 0;
    st.hasPolygonMesh = true;
    st.polygon << QPointF(0, 0) << QPointF(30, 0) << QPointF(30, 30) << QPointF(0, 0);
    st.vertices = { QPointF(0, 0), QPointF(30, 0), QPointF(30, 30) };
    st.triangles = { 0, 1, 2 };

    // Apply command
    stack.push(new SetPolygonMeshCommand(&doc, { st }));
    QVERIFY(doc.box(0).hasPolygonMesh);
    QCOMPARE(doc.box(0).vertices.size(), 3);
    QCOMPARE(doc.box(0).triangles.size(), 3);

    // Undo
    stack.undo();
    QVERIFY(!doc.box(0).hasPolygonMesh);
    QVERIFY(doc.box(0).vertices.isEmpty());

    // Redo
    stack.redo();
    QVERIFY(doc.box(0).hasPolygonMesh);
    QCOMPARE(doc.box(0).vertices.size(), 3);
}

void TestMesh::testSspSerializationWithMesh()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Setup source document with polygon mesh
    SpriteDocument doc;
    QImage atlas(64, 64, QImage::Format_ARGB32_Premultiplied);
    atlas.fill(Qt::blue);
    doc.setAtlas(atlas);

    SpriteBox box(QRect(4, 4, 32, 32));
    box.hasPolygonMesh = true;
    box.polygon << QPointF(0, 0) << QPointF(32, 0) << QPointF(16, 32) << QPointF(0, 0);
    box.vertices = { QPointF(0, 0), QPointF(32, 0), QPointF(16, 32) };
    box.triangles = { 0, 1, 2 };
    doc.addFrame(QImage(32, 32, QImage::Format_ARGB32_Premultiplied), box);

    // 1. Direct JSON serialization fidelity
    QByteArray jsonData = ProjectManager::serializeDocumentToJson(doc);
    QVERIFY(!jsonData.isEmpty());

    // 2. Save and load via session directory (which powers .ssp archive storage)
    QString err;
    bool saveOk = ProjectManager::saveProjectToSessionDir(doc, tempDir.path(), 1.0, QPointF(0, 0), &err);
    QVERIFY2(saveOk, qPrintable(err));

    // Load into a new document
    SpriteDocument loadedDoc;
    bool loadOk = ProjectManager::loadProjectFromSessionDir(tempDir.path(), loadedDoc, nullptr, nullptr, &err);
    QVERIFY2(loadOk, qPrintable(err));
    QCOMPARE(loadedDoc.frameCount(), 1);

    const SpriteBox &loadedBox = loadedDoc.box(0);
    QVERIFY(loadedBox.hasPolygonMesh);
    QCOMPARE(loadedBox.polygon.size(), box.polygon.size());
    QCOMPARE(loadedBox.vertices.size(), 3);
    QCOMPARE(loadedBox.triangles.size(), 3);
    QCOMPARE(loadedBox.vertices.at(1), QPointF(32, 0));
    QCOMPARE(loadedBox.triangles.at(2), 2);
}

void TestMesh::testAtlasBoxItemVertexManipulation()
{
    AtlasBoxItem item(0, QRect(10, 10, 50, 50), QRect(0, 0, 200, 200));
    QPolygonF poly;
    poly << QPointF(5, 5) << QPointF(45, 5) << QPointF(45, 45) << QPointF(5, 45);
    QList<int> tris = Triangulator::triangulate(poly);
    item.setPolygonMesh(poly, tris, true);
    item.setSelectedBox(true);

    QCOMPARE(item.polygon().size(), 4);
    QCOMPARE(item.triangles().size(), 6);
    QVERIFY(!item.hasSelectedVertices());

    // 1. Nudge or delete without selection fails
    QVERIFY(!item.nudgeSelectedVertices(1, 1));
    QVERIFY(!item.deleteSelectedVertices());

    // 2. Select vertex 1 and nudge by (+2, -3)
    item.selectVertex(1);
    QVERIFY(item.hasSelectedVertices());
    QCOMPARE(item.selectedVertices().size(), 1);
    QVERIFY(item.selectedVertices().contains(1));

    bool nudgeOk = item.nudgeSelectedVertices(2, -3);
    QVERIFY(nudgeOk);
    QCOMPARE(item.polygon().at(1), QPointF(47, 2));

    // 3. Multi-selection: add vertex 2
    item.selectVertex(2, true);
    QCOMPARE(item.selectedVertices().size(), 2);
    QVERIFY(item.selectedVertices().contains(1));
    QVERIFY(item.selectedVertices().contains(2));

    // 4. Delete selected vertices: removing 2 vertices from 4 leaves 2, which is < 3 so should be rejected
    QVERIFY(!item.deleteSelectedVertices());
    QCOMPARE(item.polygon().size(), 4);

    // 5. Select only 1 vertex to delete (4 - 1 = 3 >= 3, valid triangle remains)
    item.selectVertex(3, false);
    QCOMPARE(item.selectedVertices().size(), 1);
    bool delOk = item.deleteSelectedVertices();
    QVERIFY(delOk);
    QCOMPARE(item.polygon().size(), 3);
    QCOMPARE(item.triangles().size(), 3);
    QVERIFY(!item.hasSelectedVertices());
}

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestMesh tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_mesh.moc"
