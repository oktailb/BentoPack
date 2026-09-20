#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QElapsedTimer>
#include <QDebug>

#include "model/spritedocument.h"
#include "extractor/extractor.h"
#include "extractor/extractorregistry.h"
#include "spriteextractor.h"
#include "gifextractor.h"
#include "jsonextractor.h"
#include "godotextractor.h"
#include "image/spritedetector.h"

class TestExtractors : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testExtractorRegistryBasics();
    void testSpriteExtractorCapabilities();
    void testSpriteExtractorReadPng();
    void testJsonExtractorReadWrite();
    void testGodotExtractorReadWrite();
    void testGifExtractorRead();
    void testErrorHandlingNonExistentFile();
    void testErrorHandlingCorruptedData();
    void testExtractToImagesEquivalence();
    void testExtractPerformance();
    void testSpriteDetectorBasics();
    void testSpriteDetectorNestedContainment();
    void testSpriteDetectorLargeScaleInclusionPerformance();

private:
    QString m_sampleDir;
};

void TestExtractors::initTestCase()
{
    // Locate sample directory relative to current source or executable
    QStringList candidates = {
        QStringLiteral(SAMPLE_DIR),
        QDir::current().filePath(QStringLiteral("../sample")),
        QDir::current().filePath(QStringLiteral("../../sample")),
        QDir::current().filePath(QStringLiteral("sample"))
    };

    for (const QString &cand : candidates) {
        if (QFile::exists(cand + QStringLiteral("/hero.png")) || QFile::exists(cand + QStringLiteral("/ryu.png"))) {
            m_sampleDir = QDir(cand).canonicalPath();
            break;
        }
    }

    if (m_sampleDir.isEmpty()) {
        m_sampleDir = QStringLiteral(SAMPLE_DIR);
    }

    QString binPlugins = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));
    ExtractorRegistry::instance().loadPlugins(binPlugins);
}

void TestExtractors::testExtractorRegistryBasics()
{
    auto &reg = ExtractorRegistry::instance();
    const auto &extractors = reg.extractors();
    QVERIFY(extractors.size() >= 4);

    // Verify each codec is present
    QVERIFY(reg.findExtractorById(QStringLiteral("sprite_extractor")) != nullptr);
    QVERIFY(reg.findExtractorById(QStringLiteral("gif_extractor")) != nullptr);
    QVERIFY(reg.findExtractorById(QStringLiteral("json_extractor")) != nullptr);
    QVERIFY(reg.findExtractorById(QStringLiteral("godot_extractor")) != nullptr);

    // Verify filter strings
    QString openFilters = reg.openFilterString();
    QVERIFY(openFilters.contains(QStringLiteral("*.png")));
    QVERIFY(openFilters.contains(QStringLiteral("*.json")));
    QVERIFY(openFilters.contains(QStringLiteral("*.tres")));
    QVERIFY(openFilters.contains(QStringLiteral("*.gif")));

    QString saveFilters = reg.saveFilterString();
    QVERIFY(saveFilters.contains(QStringLiteral("*.json")));
    QVERIFY(saveFilters.contains(QStringLiteral("*.tres")));
}

void TestExtractors::testSpriteExtractorCapabilities()
{
    SpriteExtractor extractor;
    QCOMPARE(extractor.id(), QStringLiteral("sprite_extractor"));
    QCOMPARE(extractor.displayName(), QStringLiteral("Sprite Sheet"));
    QVERIFY(!extractor.version().isNull());
    QVERIFY(extractor.capabilities().testFlag(Extractor::CanImport));
    QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));
    QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("png")));
    QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("bmp")));
}

void TestExtractors::testSpriteExtractorReadPng()
{
    QString pngPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(pngPath)) pngPath = m_sampleDir + QStringLiteral("/ryu.png");
    if (!QFile::exists(pngPath)) {
        QSKIP("Sample file not present.");
    }

    SpriteExtractor extractor;
    SpriteDocument doc;
    ExtractorError err;

    QSignalSpy progressSpy(&extractor, &Extractor::progress);
    QSignalSpy statusSpy(&extractor, &Extractor::statusMessage);

    bool ok = extractor.read(pngPath, doc, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(!err.isError());

    // Document must contain loaded atlas and segmented frames
    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 0);
    QCOMPARE(doc.boxes().size(), doc.frameCount());
    QVERIFY(doc.maxFrameWidth() > 0);
    QVERIFY(doc.maxFrameHeight() > 0);

    // Verify signals fired
    QVERIFY(!progressSpy.isEmpty());
    QVERIFY(!statusSpy.isEmpty());
}

void TestExtractors::testJsonExtractorReadWrite()
{
    QString jsonPath = m_sampleDir + QStringLiteral("/hero.json");
    if (!QFile::exists(jsonPath)) jsonPath = m_sampleDir + QStringLiteral("/ryu.json");
    if (!QFile::exists(jsonPath)) {
        QSKIP("Sample file not present.");
    }

    JsonExtractor extractor;
    SpriteDocument doc;
    ExtractorError err;

    bool ok = extractor.read(jsonPath, doc, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(!err.isError());

    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 0);
    QVERIFY(!doc.animations().isEmpty());

    // Set custom pivot on frame 0 to test export & import roundtrip
    QPoint customPiv(doc.box(0).rect.width() / 4, doc.box(0).rect.height() / 2);
    doc.setBoxPivot(0, customPiv);

    // Test export round-trip to a temporary directory
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString exportedJson = tempDir.filePath(QStringLiteral("exported_hero.json"));
    ExportOptions opts;
    opts.compressJson = false;

    ExtractorError writeErr;
    bool writeOk = extractor.write(exportedJson, doc, opts, &writeErr);
    QVERIFY2(writeOk, qPrintable(writeErr.toString()));
    QVERIFY(QFile::exists(exportedJson));
    QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("exported_hero.png"))));

    // Read back exported JSON
    SpriteDocument doc2;
    ExtractorError readBackErr;
    bool readBackOk = extractor.read(exportedJson, doc2, &readBackErr);
    QVERIFY2(readBackOk, qPrintable(readBackErr.toString()));
    QCOMPARE(doc2.frameCount(), doc.frameCount());
    QCOMPARE(doc2.animations().size(), doc.animations().size());
    QVERIFY(doc2.box(0).hasCustomPivot);
    QCOMPARE(doc2.box(0).pivot, customPiv);
}

void TestExtractors::testGodotExtractorReadWrite()
{
    QString godotTres = m_sampleDir + QStringLiteral("/hero_godot.tres");
    if (!QFile::exists(godotTres)) godotTres = m_sampleDir + QStringLiteral("/ryu_godot.tres");
    if (!QFile::exists(godotTres)) {
        QSKIP("Sample file not present.");
    }

    GodotExtractor extractor;
    SpriteDocument doc;
    ExtractorError err;

    bool ok = extractor.read(godotTres, doc, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(!err.isError());

    QVERIFY(!doc.atlas().isNull());
    QCOMPARE(doc.animations().size(), 3);
    if (doc.hasAnimation(QStringLiteral("idle"))) {
        QCOMPARE(doc.frameCount(), 14);
        QVERIFY(doc.hasAnimation(QStringLiteral("run")));
        QVERIFY(doc.hasAnimation(QStringLiteral("attack")));
        QCOMPARE(doc.animation(QStringLiteral("idle")).frameIndices.size(), 4);
        QCOMPARE(doc.animation(QStringLiteral("run")).frameIndices.size(), 6);
        QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices.size(), 4);
    } else {
        QCOMPARE(doc.frameCount(), 86);
        QVERIFY(doc.hasAnimation(QStringLiteral("guard")));
        QVERIFY(doc.hasAnimation(QStringLiteral("punch")));
        QVERIFY(doc.hasAnimation(QStringLiteral("side kick")));
        QCOMPARE(doc.animation(QStringLiteral("guard")).frameIndices.size(), 6);
        QCOMPARE(doc.animation(QStringLiteral("punch")).frameIndices.size(), 10);
        QCOMPARE(doc.animation(QStringLiteral("side kick")).frameIndices.size(), 10);
    }

    // Test export round-trip
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString exportedTres = tempDir.filePath(QStringLiteral("exported.tres"));
    ExportOptions opts;
    ExtractorError writeErr;

    bool writeOk = extractor.write(exportedTres, doc, opts, &writeErr);
    QVERIFY2(writeOk, qPrintable(writeErr.toString()));
    QVERIFY(QFile::exists(exportedTres));
    QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("exported.png"))));

    // Read back exported Godot resource
    SpriteDocument doc2;
    ExtractorError readBackErr;
    bool readBackOk = extractor.read(exportedTres, doc2, &readBackErr);
    QVERIFY2(readBackOk, qPrintable(readBackErr.toString()));
    QCOMPARE(doc2.frameCount(), doc.frameCount());
    QCOMPARE(doc2.animations().size(), 3);
    if (doc2.hasAnimation(QStringLiteral("idle"))) {
        QVERIFY(doc2.hasAnimation(QStringLiteral("run")));
        QVERIFY(doc2.hasAnimation(QStringLiteral("attack")));
    } else {
        QVERIFY(doc2.hasAnimation(QStringLiteral("guard")));
        QVERIFY(doc2.hasAnimation(QStringLiteral("punch")));
        QVERIFY(doc2.hasAnimation(QStringLiteral("side kick")));
    }
}

void TestExtractors::testGifExtractorRead()
{
    QString gifPath = m_sampleDir + QStringLiteral("/hero.gif");
    if (!QFile::exists(gifPath)) gifPath = m_sampleDir + QStringLiteral("/ryu_hd.gif");
    if (!QFile::exists(gifPath)) {
        QSKIP("Sample file not present.");
    }

    GifExtractor extractor;
    SpriteDocument doc;
    ExtractorError err;

    bool ok = extractor.read(gifPath, doc, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(!err.isError());

    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 1);
    QVERIFY(doc.hasAnimation(QStringLiteral("default")));
}

void TestExtractors::testErrorHandlingNonExistentFile()
{
    SpriteExtractor spriteExt;
    SpriteDocument doc;
    ExtractorError err;

    bool ok = spriteExt.read(QStringLiteral("non_existent_file_12345.png"), doc, &err);
    QVERIFY(!ok);
    QVERIFY(err.isError());
    QCOMPARE(err.code, ExtractorError::FileNotFound);
    QVERIFY(!err.message.isEmpty());

    JsonExtractor jsonExt;
    ok = jsonExt.read(QStringLiteral("non_existent_file_12345.json"), doc, &err);
    QVERIFY(!ok);
    QVERIFY(err.isError());
    QCOMPARE(err.code, ExtractorError::FileNotFound);

    GodotExtractor godotExt;
    ok = godotExt.read(QStringLiteral("non_existent_file_12345.tres"), doc, &err);
    QVERIFY(!ok);
    QVERIFY(err.isError());
    QCOMPARE(err.code, ExtractorError::FileNotFound);
}

void TestExtractors::testErrorHandlingCorruptedData()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString corruptedJson = tempDir.filePath(QStringLiteral("corrupted.json"));
    QFile f(corruptedJson);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("{ invalid json content ::: 123");
    f.close();

    JsonExtractor jsonExt;
    SpriteDocument doc;
    ExtractorError err;

    bool ok = jsonExt.read(corruptedJson, doc, &err);
    QVERIFY(!ok);
    QVERIFY(err.isError());
    QCOMPARE(err.code, ExtractorError::ParsingFailed);
}

void TestExtractors::testExtractToImagesEquivalence()
{
    // Create a 100x100 ARGB32 image with 3 separate opaque squares
    QImage testImg(100, 100, QImage::Format_ARGB32);
    testImg.fill(Qt::transparent);

    QPainter p(&testImg);
    p.fillRect(10, 10, 15, 15, Qt::red);
    p.fillRect(40, 10, 20, 20, Qt::green);
    p.fillRect(20, 60, 25, 25, Qt::blue);
    p.end();

    SpriteExtractor extractor;
    QList<QImage> outFrames;
    QList<SpriteBox> outBoxes;
    SpriteSheetOptions opts;

    bool ok = extractor.extractToImages(testImg, outFrames, outBoxes, opts);
    QVERIFY(ok);
    QCOMPARE(outBoxes.size(), 3);
    QCOMPARE(outFrames.size(), 3);

    // Verify box rects
    QCOMPARE(outBoxes[0].rect, QRect(10, 10, 15, 15));
    QCOMPARE(outBoxes[1].rect, QRect(40, 10, 20, 20));
    QCOMPARE(outBoxes[2].rect, QRect(20, 60, 25, 25));

    // Verify frame image sizes
    for (int i = 0; i < 3; ++i) {
        QCOMPARE(outFrames[i].size(), outBoxes[i].rect.size());
    }

    // Compare with extractFromImage into SpriteDocument
    SpriteDocument doc;
    bool docOk = extractor.extractFromImage(testImg, doc, opts);
    QVERIFY(docOk);
    QCOMPARE(doc.frameCount(), 3);
    for (int i = 0; i < 3; ++i) {
        QCOMPARE(doc.box(i).rect, outBoxes[i].rect);
        QCOMPARE(doc.frame(i).size(), outFrames[i].size());
    }
}

void TestExtractors::testExtractPerformance()
{
    // Construct a synthetic 512x512 image containing an 8x8 grid of sprites (64 sprites)
    QImage largeImg(512, 512, QImage::Format_ARGB32);
    largeImg.fill(Qt::transparent);

    QPainter p(&largeImg);
    for (int gy = 0; gy < 8; ++gy) {
        for (int gx = 0; gx < 8; ++gx) {
            p.fillRect(gx * 64 + 10, gy * 64 + 10, 40, 40, QColor(gx * 30, gy * 30, 150));
        }
    }
    p.end();

    SpriteExtractor extractor;
    QList<QImage> outFrames;
    QList<SpriteBox> outBoxes;

    QElapsedTimer timer;
    timer.start();

    bool ok = extractor.extractToImages(largeImg, outFrames, outBoxes);
    qint64 elapsedMs = timer.elapsed();

    QVERIFY(ok);
    QCOMPARE(outBoxes.size(), 64);
    QCOMPARE(outFrames.size(), 64);
    qDebug() << "Extracted 64 components on 512x512 in" << elapsedMs << "ms";
    // With direct scanline access, 512x512 extraction finishes well under 500ms
    QVERIFY2(elapsedMs < 500, qPrintable(QString("Extraction took too long: %1 ms").arg(elapsedMs)));
}

void TestExtractors::testSpriteDetectorBasics()
{
    QImage testImg(80, 80, QImage::Format_ARGB32);
    testImg.fill(Qt::transparent);

    QPainter p(&testImg);
    p.fillRect(5, 5, 20, 20, Qt::yellow);
    p.fillRect(45, 45, 15, 15, Qt::cyan);
    p.end();

    QList<QImage> frames;
    QList<SpriteBox> boxes;
    SpriteDetectionOptions opts;
    opts.minSliceSize = 3;

    bool ok = SpriteDetector::detectToImages(testImg, frames, boxes, opts);
    QVERIFY(ok);
    QCOMPARE(boxes.size(), 2);
    QCOMPARE(frames.size(), 2);
    QCOMPARE(boxes[0].rect, QRect(5, 5, 20, 20));
    QCOMPARE(boxes[1].rect, QRect(45, 45, 15, 15));

    // Test detectBoxes alone
    QList<SpriteBox> boxesOnly;
    bool boxesOk = SpriteDetector::detectBoxes(testImg, boxesOnly, opts);
    QVERIFY(boxesOk);
    QCOMPARE(boxesOnly.size(), 2);
    QCOMPARE(boxesOnly[0].rect, QRect(5, 5, 20, 20));
}

void TestExtractors::testSpriteDetectorNestedContainment()
{
    // Create an image with an outer hollow box containing an inner isolated component,
    // plus a standalone distinct component.
    QImage testImg(120, 80, QImage::Format_ARGB32);
    testImg.fill(Qt::transparent);

    QPainter p(&testImg);
    // Outer hollow rectangle (bounding box 10,10, 50,50)
    p.fillRect(10, 10, 50, 4, Qt::blue);  // Top
    p.fillRect(10, 56, 50, 4, Qt::blue);  // Bottom
    p.fillRect(10, 10, 4, 50, Qt::blue);  // Left
    p.fillRect(56, 10, 4, 50, Qt::blue);  // Right

    // Inner island strictly inside the outer hollow box (30,30, 10,10)
    p.fillRect(30, 30, 10, 10, Qt::green);

    // Standalone separate component (75, 15, 30, 30)
    p.fillRect(75, 15, 30, 30, Qt::red);
    p.end();

    QList<SpriteBox> boxes;
    SpriteDetectionOptions opts;
    bool ok = SpriteDetector::detectBoxes(testImg, boxes, opts);
    QVERIFY(ok);

    // The inner green island (30,30, 10,10) must be filtered out because it is fully
    // inside the outer blue hollow box (10,10, 50,50).
    // Exactly 2 master boxes should be returned.
    QCOMPARE(boxes.size(), 2);
    QCOMPARE(boxes[0].rect, QRect(10, 10, 50, 50));
    QCOMPARE(boxes[1].rect, QRect(75, 15, 30, 30));
}

void TestExtractors::testSpriteDetectorLargeScaleInclusionPerformance()
{
    // Stress test: 1024x1024 image with 64 outer master sprites (8x8 grid).
    // Inside each master sprite, place 8 separate small disjoint components (total 512 nested islands).
    // In addition, place 64 small standalone sprites outside the grid.
    // Total components = 64 (outer) + 512 (nested) + 64 (standalone) = 640 components.
    const int imgSize = 1024;
    QImage largeImg(imgSize, imgSize, QImage::Format_ARGB32);
    largeImg.fill(Qt::transparent);

    QPainter p(&largeImg);
    for (int gy = 0; gy < 8; ++gy) {
        for (int gx = 0; gx < 8; ++gx) {
            const int ox = gx * 110 + 20;
            const int oy = gy * 110 + 20;

            // Outer hollow frame 80x80
            p.fillRect(ox, oy, 80, 2, Qt::white);
            p.fillRect(ox, oy + 78, 80, 2, Qt::white);
            p.fillRect(ox, oy, 2, 80, Qt::white);
            p.fillRect(ox + 78, oy, 2, 80, Qt::white);

            // 8 small inner islands (size 4x4) inside the frame
            for (int k = 0; k < 8; ++k) {
                const int ix = ox + 10 + (k % 4) * 15;
                const int iy = oy + 10 + (k / 4) * 30;
                p.fillRect(ix, iy, 4, 4, QColor(k * 25, 120, 200));
            }

            // Standalone small sprite in the margin between grid cells
            p.fillRect(ox + 90, oy + 40, 6, 6, Qt::yellow);
        }
    }
    p.end();

    QList<SpriteBox> boxes;
    SpriteDetectionOptions opts;

    QElapsedTimer timer;
    timer.start();

    bool ok = SpriteDetector::detectBoxes(largeImg, boxes, opts);
    qint64 elapsedMs = timer.elapsed();

    QVERIFY(ok);
    // 64 outer frames + 64 standalone sprites = 128 master boxes
    // All 512 inner islands must be filtered out
    QCOMPARE(boxes.size(), 128);

    qDebug() << "SpriteDetector segmented 640 components (with 512 nested) on 1024x1024 in" << elapsedMs << "ms";
    // With SpatialGrid2D, this runs in ~10-30 ms. Threshold set to 500 ms for CI headroom.
    QVERIFY2(elapsedMs < 500, qPrintable(QString("Large scale inclusion check took too long: %1 ms").arg(elapsedMs)));
}

#include <QGuiApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestExtractors tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_extractors.moc"
