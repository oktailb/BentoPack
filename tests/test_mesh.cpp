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
#include "packer/tightpolygonpacker.h"
#include "packer/atlaspacker.h"
#include "jsonextractor.h"
#include "unityextractor.h"
#include "unrealextractor.h"
#include "godotextractor.h"
#include "atlaspackingdialog.h"
#include "geometry/polygonmerger.h"
#include "commands/commands.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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

    // 7. Tight Polygon Packing & Hit Testing
    void testTightPolygonPackingAlgorithm();
    void testAtlasBoxItemPolygonShapeHitTest();

    // 8. Multi-Engine Polygon Mesh Export/Import Tests
    void testTexturePackerJsonPolygonExportAndImport();
    void testUnityExtractorExport();
    void testUnrealExtractorExport();
    void testGodotExtractorCompanionTres();
    void testAtlasPackingDialogPreservesPolygons();
    void testPolygonClippedFrame();

    // 9. Modernized Polygon Merger Tests
    void testPolygonMergerTouching();
    void testPolygonMergerDisjoint();
    void testPolygonMergerMultiIslandContourTracer();
    void testPolygonMergerSpriteBoxes();
    void testSpriteDocumentMergeFramesPreservesPolygonsAndUndo();
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

void TestMesh::testTightPolygonPackingAlgorithm()
{
    // Create two 50x50 images with interlocking triangular shapes
    // Triangle A: Top-left triangle (x + y <= 35)
    QImage imgA(50, 50, QImage::Format_ARGB32_Premultiplied);
    imgA.fill(Qt::transparent);
    {
        QPainter p(&imgA);
        QPolygonF poly;
        poly << QPointF(0, 0) << QPointF(35, 0) << QPointF(0, 35) << QPointF(0, 0);
        p.setBrush(Qt::red);
        p.setPen(Qt::NoPen);
        p.drawPolygon(poly);
    }
    QPolygonF polyA;
    polyA << QPointF(0, 0) << QPointF(35, 0) << QPointF(0, 35) << QPointF(0, 0);

    // Triangle B: Bottom-right triangle (x + y >= 45)
    QImage imgB(50, 50, QImage::Format_ARGB32_Premultiplied);
    imgB.fill(Qt::transparent);
    {
        QPainter p(&imgB);
        QPolygonF poly;
        poly << QPointF(50, 15) << QPointF(50, 50) << QPointF(15, 50) << QPointF(50, 15);
        p.setBrush(Qt::blue);
        p.setPen(Qt::NoPen);
        p.drawPolygon(poly);
    }
    QPolygonF polyB;
    polyB << QPointF(50, 15) << QPointF(50, 50) << QPointF(15, 50) << QPointF(50, 15);

    QList<QImage> frames = { imgA, imgB };
    QList<QPolygonF> polys = { polyA, polyB };

    AtlasPacker::PackOptions options;
    options.algorithm = AtlasPacker::TightPolygon;
    options.padding = 1;
    options.borderPadding = 1;
    options.powerOfTwo = false;
    options.forceSquare = false;

    AtlasPackResult res = AtlasPacker::pack(frames, options, polys);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 2);
    QVERIFY(!res.atlas.isNull());

    // Verify interlocking: bounding boxes intersect or atlas width is less than naive 100px
    bool boxesOverlap = res.frameRects[0].intersects(res.frameRects[1]);
    QVERIFY(boxesOverlap || res.dimensions.width() < 100 || res.dimensions.height() < 100);

    // Verify non-transparent pixels in the generated atlas do not corrupt each other
    int redCount = 0;
    int blueCount = 0;
    for (int y = 0; y < res.atlas.height(); ++y) {
        for (int x = 0; x < res.atlas.width(); ++x) {
            QRgb p = res.atlas.pixel(x, y);
            if (qRed(p) > 200 && qBlue(p) < 50) redCount++;
            if (qBlue(p) > 200 && qRed(p) < 50) blueCount++;
        }
    }
    QVERIFY(redCount > 0);
    QVERIFY(blueCount > 0);
}

void TestMesh::testAtlasBoxItemPolygonShapeHitTest()
{
    // A 100x100 box at (10, 10) with a triangle polygon (0,0)-(60,0)-(0,60)
    AtlasBoxItem item(0, QRect(10, 10, 100, 100), QRect(0, 0, 500, 500));
    QPolygonF poly;
    poly << QPointF(0, 0) << QPointF(60, 0) << QPointF(0, 60);
    QList<int> tris = { 0, 1, 2 };
    item.setPolygonMesh(poly, tris, true);
    item.setSelectedBox(false);

    // Point (20, 20) is at local (10, 10) inside the triangle -> shape must contain it
    QVERIFY(item.shape().contains(QPointF(20, 20)));
    QVERIFY(item.contains(QPointF(20, 20)));

    // Point (90, 90) is inside the 100x100 bounding box (10..110, 10..110),
    // but OUTSIDE the triangle polygon (local 80, 80) -> shape must NOT contain it!
    QVERIFY(!item.shape().contains(QPointF(90, 90)));
    QVERIFY(!item.contains(QPointF(90, 90)));
}

void TestMesh::testTexturePackerJsonPolygonExportAndImport()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString jsonPath = tempDir.filePath("spritesheet.json");

    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::green);
    SpriteBox box(QRect(0, 0, 32, 32));
    box.hasPolygonMesh = true;
    box.polygon << QPointF(0, 0) << QPointF(32, 0) << QPointF(16, 32) << QPointF(0, 0);
    box.vertices = { QPointF(0, 0), QPointF(32, 0), QPointF(16, 32) };
    box.triangles = { 0, 1, 2 };
    doc.addFrame(img, box);

    // Export with JsonExtractor
    JsonExtractor jsonExt;
    ExportOptions opts;
    opts.format = FORMAT_TEXTUREPACKER_JSON;
    opts.packOptions.algorithm = AtlasPacker::MaxRects;
    opts.packOptions.padding = 1;

    ExtractorError err;
    bool writeOk = jsonExt.write(jsonPath, doc, opts, &err);
    QVERIFY2(writeOk, qPrintable(err.message));
    QVERIFY(QFile::exists(jsonPath));

    // Verify JSON content has polygon fields
    QFile f(jsonPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QJsonDocument jdoc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QVERIFY(jdoc.isObject());
    QJsonObject framesObj = jdoc.object()["frames"].toObject();
    QVERIFY(!framesObj.isEmpty());
    QJsonObject f0 = framesObj.begin().value().toObject();
    QVERIFY(f0.contains("vertices"));
    QVERIFY(f0.contains("verticesUV"));
    QVERIFY(f0.contains("triangles"));
    QCOMPARE(f0["vertices"].toArray().size(), 3);
    QCOMPARE(f0["triangles"].toArray().size(), 1); // 1 triangle = [ [0, 1, 2] ]

    // Import back with JsonExtractor
    SpriteDocument importedDoc;
    bool readOk = jsonExt.read(jsonPath, importedDoc, &err);
    QVERIFY2(readOk, qPrintable(err.message));
    QCOMPARE(importedDoc.frameCount(), 1);
    const SpriteBox &impBox = importedDoc.box(0);
    QVERIFY(impBox.hasPolygonMesh);
    QCOMPARE(impBox.vertices.size(), 3);
    QCOMPARE(impBox.triangles.size(), 3);
}

void TestMesh::testUnityExtractorExport()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString unityPath = tempDir.filePath("character.unity.json");

    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::yellow);
    SpriteBox box(QRect(0, 0, 32, 32));
    box.hasPolygonMesh = true;
    box.polygon << QPointF(0, 0) << QPointF(32, 0) << QPointF(16, 32) << QPointF(0, 0);
    box.vertices = { QPointF(0, 0), QPointF(32, 0), QPointF(16, 32) };
    box.triangles = { 0, 1, 2 };
    doc.addFrame(img, box);

    UnityExtractor unityExt;
    ExportOptions opts;
    opts.format = FORMAT_UNITY;
    opts.packOptions.algorithm = AtlasPacker::MaxRects;

    ExtractorError err;
    bool writeOk = unityExt.write(unityPath, doc, opts, &err);
    QVERIFY2(writeOk, qPrintable(err.message));
    QVERIFY(QFile::exists(unityPath));
    QVERIFY(QFile::exists(tempDir.filePath("character.png")));

    // Verify JSON structure
    QFile f(unityPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QJsonDocument jdoc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QVERIFY(jdoc.isObject());
    QJsonObject root = jdoc.object();
    QCOMPARE(root["format"].toString(), QStringLiteral("Unity2D_SpriteMesh"));
    QJsonArray sprites = root["sprites"].toArray();
    QCOMPARE(sprites.size(), 1);
    QJsonObject s0 = sprites[0].toObject();
    QVERIFY(s0.contains("vertices"));
    QVERIFY(s0.contains("uvs"));
    QVERIFY(s0.contains("triangles"));
    QCOMPARE(s0["vertices"].toArray().size(), 3);
    QCOMPARE(s0["triangles"].toArray().size(), 3);
}

void TestMesh::testUnrealExtractorExport()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString unrealPath = tempDir.filePath("paper_char.paper2d.json");

    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::magenta);
    SpriteBox box(QRect(0, 0, 32, 32));
    box.hasPolygonMesh = true;
    box.polygon << QPointF(0, 0) << QPointF(32, 0) << QPointF(16, 32) << QPointF(0, 0);
    box.vertices = { QPointF(0, 0), QPointF(32, 0), QPointF(16, 32) };
    box.triangles = { 0, 1, 2 };
    doc.addFrame(img, box);

    UnrealExtractor unrealExt;
    ExportOptions opts;
    opts.format = FORMAT_UNREAL;
    opts.packOptions.algorithm = AtlasPacker::MaxRects;

    ExtractorError err;
    bool writeOk = unrealExt.write(unrealPath, doc, opts, &err);
    QVERIFY2(writeOk, qPrintable(err.message));
    QVERIFY(QFile::exists(unrealPath));
    QVERIFY(QFile::exists(tempDir.filePath("paper_char.png")));

    // Verify Paper2D JSON structure
    QFile f(unrealPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QJsonDocument jdoc = QJsonDocument::fromJson(f.readAll());
    f.close();
    QVERIFY(jdoc.isObject());
    QJsonObject root = jdoc.object();
    QCOMPARE(root["format"].toString(), QStringLiteral("UnrealEngine_Paper2D"));
    QJsonArray sprites = root["sprites"].toArray();
    QCOMPARE(sprites.size(), 1);
    QJsonObject s0 = sprites[0].toObject();
    QVERIFY(s0.contains("renderGeometry"));
    QVERIFY(s0.contains("collisionGeometry"));
    QJsonObject renderGeom = s0["renderGeometry"].toObject();
    QCOMPARE(renderGeom["vertices"].toArray().size(), 3);
    QCOMPARE(renderGeom["triangles"].toArray().size(), 1);
}

void TestMesh::testGodotExtractorCompanionTres()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString godotPath = tempDir.filePath("player.tres");

    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::cyan);
    SpriteBox box(QRect(0, 0, 32, 32));
    box.hasPolygonMesh = true;
    box.polygon << QPointF(0, 0) << QPointF(32, 0) << QPointF(16, 32) << QPointF(0, 0);
    box.vertices = { QPointF(0, 0), QPointF(32, 0), QPointF(16, 32) };
    box.triangles = { 0, 1, 2 };
    doc.addFrame(img, box);

    GodotExtractor godotExt;
    ExportOptions opts;
    opts.format = FORMAT_GODOT;
    opts.packOptions.algorithm = AtlasPacker::MaxRects;

    ExtractorError err;
    bool writeOk = godotExt.write(godotPath, doc, opts, &err);
    QVERIFY2(writeOk, qPrintable(err.message));
    QVERIFY(QFile::exists(godotPath));
    QVERIFY(QFile::exists(tempDir.filePath("player.png")));

    // Check companion player_mesh.tres
    QString meshPath = tempDir.filePath("player_mesh.tres");
    QVERIFY(QFile::exists(meshPath));

    QFile mf(meshPath);
    QVERIFY(mf.open(QIODevice::ReadOnly | QIODevice::Text));
    QString meshContent = QString::fromUtf8(mf.readAll());
    mf.close();

    QVERIFY(meshContent.contains("metadata/frame_0/polygon"));
    QVERIFY(meshContent.contains("metadata/frame_0/uv"));
    QVERIFY(meshContent.contains("metadata/frame_0/triangles"));
}

void TestMesh::testAtlasPackingDialogPreservesPolygons()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32_Premultiplied);
    atlas.fill(Qt::transparent);
    doc.setAtlas(atlas);

    QImage img1(30, 30, QImage::Format_ARGB32_Premultiplied);
    img1.fill(Qt::red);
    SpriteBox box1(QRect(0, 0, 30, 30));
    box1.hasPolygonMesh = true;
    box1.polygon << QPointF(0, 0) << QPointF(30, 0) << QPointF(0, 30) << QPointF(0, 0);
    box1.vertices = { QPointF(0, 0), QPointF(30, 0), QPointF(0, 30) };
    box1.triangles = { 0, 1, 2 };
    doc.addFrame(img1, box1);

    QImage img2(30, 30, QImage::Format_ARGB32_Premultiplied);
    img2.fill(Qt::blue);
    SpriteBox box2(QRect(30, 0, 30, 30));
    box2.hasPolygonMesh = true;
    box2.polygon << QPointF(0, 0) << QPointF(30, 0) << QPointF(30, 30) << QPointF(0, 0);
    box2.vertices = { QPointF(0, 0), QPointF(30, 0), QPointF(30, 30) };
    box2.triangles = { 0, 1, 2 };
    doc.addFrame(img2, box2);

    QUndoStack undoStack;
    struct TestPackingDialog : public AtlasPackingDialog {
        using AtlasPackingDialog::AtlasPackingDialog;
        using AtlasPackingDialog::applyPreview;
    };
    TestPackingDialog dlg(&doc, &undoStack, nullptr);

    // Opening dialog automatically defaults to TightPolygon when meshes exist
    QCOMPARE(dlg.packOptions().algorithm, AtlasPacker::TightPolygon);

    dlg.applyPreview();
    dlg.waitForPendingPreview();

    QCOMPARE(doc.frameCount(), 2);
    const SpriteBox &resultBox1 = doc.box(0);
    QVERIFY(resultBox1.hasPolygonMesh);
    QVERIFY(!resultBox1.polygon.isEmpty());
    QCOMPARE(resultBox1.polygon.size(), 4);
    QCOMPARE(resultBox1.triangles.size(), 3);

    const SpriteBox &resultBox2 = doc.box(1);
    QVERIFY(resultBox2.hasPolygonMesh);
    QVERIFY(!resultBox2.polygon.isEmpty());
    QCOMPARE(resultBox2.polygon.size(), 4);
    QCOMPARE(resultBox2.triangles.size(), 3);
}

void TestMesh::testPolygonClippedFrame()
{
    SpriteDocument doc;
    // 40x40 image:
    // Green triangle (0,0)-(30,0)-(0,30)
    // Red artifact at (35, 35) simulating a nested neighbor sprite from tight packing
    QImage img(40, 40, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 255, 0, 255));
        QPolygonF poly;
        poly << QPointF(0, 0) << QPointF(30, 0) << QPointF(0, 30);
        p.drawPolygon(poly);
        p.fillRect(32, 32, 6, 6, QColor(255, 0, 0, 255));
    }

    SpriteBox box(QRect(0, 0, 40, 40));
    doc.addFrame(img, box);

    // 1. Without polygon mesh: returns raw unclipped frame
    QCOMPARE(doc.polygonClippedFrame(0).pixelColor(35, 35), QColor(255, 0, 0, 255));

    // 2. With polygon mesh enabled
    box.hasPolygonMesh = true;
    box.polygon << QPointF(0, 0) << QPointF(30, 0) << QPointF(0, 30);
    box.triangles = { 0, 1, 2 };
    doc.setBox(0, box);

    // Raw frame still has the neighbor artifact
    QCOMPARE(doc.frame(0).pixelColor(35, 35), QColor(255, 0, 0, 255));

    // polygonClippedFrame masks out the artifact completely
    QImage clipped = doc.polygonClippedFrame(0);
    QCOMPARE(clipped.pixelColor(35, 35).alpha(), 0);
    // Green pixels inside the polygon are fully preserved
    QCOMPARE(clipped.pixelColor(5, 5), QColor(0, 255, 0, 255));
}

void TestMesh::testPolygonMergerTouching()
{
    // Two squares touching along x=20
    QPolygonF polyA;
    polyA << QPointF(0, 0) << QPointF(20, 0) << QPointF(20, 20) << QPointF(0, 20);

    QPolygonF polyB;
    polyB << QPointF(20, 0) << QPointF(40, 0) << QPointF(40, 20) << QPointF(20, 20);

    QPolygonF merged = PolygonMerger::mergePolygons(polyA, polyB);
    QVERIFY(!merged.isEmpty());
    QVERIFY(merged.size() >= 4);

    QRectF b = merged.boundingRect();
    QCOMPARE(b.left(), 0.0);
    QCOMPARE(b.right(), 40.0);
    QCOMPARE(b.top(), 0.0);
    QCOMPARE(b.bottom(), 20.0);

    double area = Triangulator::calculateArea(merged);
    QVERIFY(std::abs(area - 800.0) < 5.0);

    QList<int> tris = Triangulator::triangulate(merged);
    QVERIFY(!tris.isEmpty());
    QCOMPARE(tris.size() % 3, 0);
}

void TestMesh::testPolygonMergerDisjoint()
{
    // Two disjoint squares separated by a 10px gap: [0,0]->[20,20] and [30,0]->[50,20]
    QPolygonF polyA;
    polyA << QPointF(0, 0) << QPointF(20, 0) << QPointF(20, 20) << QPointF(0, 20);

    QPolygonF polyB;
    polyB << QPointF(30, 0) << QPointF(50, 0) << QPointF(50, 20) << QPointF(30, 20);

    QPolygonF merged = PolygonMerger::mergePolygons(polyA, polyB, 2.0);
    QVERIFY(!merged.isEmpty());
    QVERIFY(merged.size() >= 4);

    QRectF b = merged.boundingRect();
    QVERIFY(b.left() <= 0.5);
    QVERIFY(b.right() >= 49.5);

    double area = Triangulator::calculateArea(merged);
    // Area should be sum of both squares (~800) + bridge corridor (~10*2 = 20) => ~820
    // Convex hull area would be 50 * 20 = 1000.
    // By keeping it strictly non-convex, area is significantly less than 950!
    QVERIFY(area > 750.0);
    QVERIFY(area < 900.0);

    QList<int> tris = Triangulator::triangulate(merged);
    QVERIFY(!tris.isEmpty());
    QCOMPARE(tris.size() % 3, 0);
}

void TestMesh::testPolygonMergerMultiIslandContourTracer()
{
    // 60x30 image with two disconnected 10x10 squares
    QImage img(60, 30, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.fillRect(5, 10, 10, 10, Qt::red);
        p.fillRect(45, 10, 10, 10, Qt::blue);
    }

    QPolygonF contour = ContourTracer::traceContour(img, 128);
    QVERIFY(!contour.isEmpty());

    // Both islands must be covered
    QRectF b = contour.boundingRect();
    QVERIFY(b.left() <= 6.0);
    QVERIFY(b.right() >= 54.0);

    // Should minimize empty space and not fill the entire 60x30 bounding box
    double area = Triangulator::calculateArea(contour);
    QVERIFY(area < 600.0); // 60x30 would be 1800

    QList<int> tris = Triangulator::triangulate(contour);
    QVERIFY(!tris.isEmpty());
    QCOMPARE(tris.size() % 3, 0);
}

void TestMesh::testPolygonMergerSpriteBoxes()
{
    SpriteBox src;
    src.rect = QRect(10, 10, 20, 20);
    src.hasPolygonMesh = true;
    src.polygon << QPointF(0, 0) << QPointF(20, 0) << QPointF(20, 20) << QPointF(0, 20);
    src.vertices = src.polygon.toList();
    src.triangles = Triangulator::triangulate(src.polygon);
    src.hasCustomPivot = true;
    src.pivot = QPoint(5, 5);

    SpriteBox tgt;
    tgt.rect = QRect(40, 10, 20, 20);
    tgt.hasPolygonMesh = true;
    tgt.polygon << QPointF(0, 0) << QPointF(20, 0) << QPointF(20, 20) << QPointF(0, 20);
    tgt.vertices = tgt.polygon.toList();
    tgt.triangles = Triangulator::triangulate(tgt.polygon);

    QImage dummyImg(20, 20, QImage::Format_ARGB32_Premultiplied);
    dummyImg.fill(Qt::transparent);

    SpriteBox res = PolygonMerger::mergeSpriteBoxes(src, tgt, dummyImg, dummyImg);

    QCOMPARE(res.rect, QRect(10, 10, 50, 20));
    QVERIFY(res.hasPolygonMesh);
    QVERIFY(!res.polygon.isEmpty());
    QVERIFY(!res.triangles.isEmpty());
    QVERIFY(res.hasCustomPivot);
    // Custom pivot from src translated to unitedRect origin:
    // src.rect.topLeft() (10, 10) + pivot(5, 5) - unitedRect.topLeft() (10, 10) = (5, 5)
    QCOMPARE(res.pivot, QPoint(5, 5));
}

void TestMesh::testSpriteDocumentMergeFramesPreservesPolygonsAndUndo()
{
    SpriteDocument doc;

    QImage atlas(100, 50, QImage::Format_ARGB32_Premultiplied);
    atlas.fill(Qt::transparent);
    {
        QPainter p(&atlas);
        p.fillRect(10, 10, 20, 20, Qt::red);
        p.fillRect(40, 10, 20, 20, Qt::blue);
    }
    doc.setAtlas(atlas);

    SpriteBox b0(QRect(10, 10, 20, 20));
    b0.hasPolygonMesh = true;
    b0.polygon << QPointF(0, 0) << QPointF(20, 0) << QPointF(20, 20) << QPointF(0, 20);
    b0.triangles = Triangulator::triangulate(b0.polygon);

    SpriteBox b1(QRect(40, 10, 20, 20));
    b1.hasPolygonMesh = true;
    b1.polygon << QPointF(0, 0) << QPointF(20, 0) << QPointF(20, 20) << QPointF(0, 20);
    b1.triangles = Triangulator::triangulate(b1.polygon);

    doc.addFrame(atlas.copy(b0.rect), b0);
    doc.addFrame(atlas.copy(b1.rect), b1);
    QCOMPARE(doc.frameCount(), 2);

    // Merge frame 1 into frame 0 using MergeFramesCommand
    MergeFramesCommand cmd(&doc, 1, 0);
    cmd.redo();

    QCOMPARE(doc.frameCount(), 1);
    SpriteBox merged = doc.box(0);
    QCOMPARE(merged.rect, QRect(10, 10, 50, 20));
    QVERIFY(merged.hasPolygonMesh);
    QVERIFY(merged.polygon.size() >= 4);
    QVERIFY(!merged.triangles.isEmpty());

    // Undo should restore both original frames with their exact polygon meshes
    cmd.undo();
    QCOMPARE(doc.frameCount(), 2);
    QVERIFY(doc.box(0).hasPolygonMesh);
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));
    QCOMPARE(doc.box(0).polygon.size(), 4);

    QVERIFY(doc.box(1).hasPolygonMesh);
    QCOMPARE(doc.box(1).rect, QRect(40, 10, 20, 20));
    QCOMPARE(doc.box(1).polygon.size(), 4);
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
