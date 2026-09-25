#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QUndoStack>

#include "model/spritedocument.h"
#include "project/projectmanager.h"
#include "controller/projectcontroller.h"
#include "packer/maxrectspacker.h"
#include "packer/tightpolygonpacker.h"
#include "extractor/extractorregistry.h"
#include "extractor/extractor.h"
#include "zip/miniz.h"

class TestRobustness : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Corrupted and Malformed .bento archives
    void testZeroByteBentoFile();
    void testTruncatedZipBentoFile();
    void testZipMissingProjectJson();
    void testZipCorruptedJsonContent();
    void testZipMissingAtlasImage();

    // Extractor resilience
    void testAsepriteMalformedJson();
    void testTexturePackerMalformedJson();
    void testSliceEmptyOrNullImage();

    // Packing edge cases
    void testPackingZeroSlices();
    void testPackingOversizedSprite();
    void testTightPackingEmptyPolygon();

    // Filepath encodings (spaces, accents, CJK)
    void testUnicodeAndSpacedPaths();

private:
    QTemporaryDir m_tempDir;
};

void TestRobustness::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    QString appDir = QCoreApplication::applicationDirPath();
    ExtractorRegistry::instance().loadPlugins(QDir(appDir).filePath(QStringLiteral("plugins/extractors")));
    ExtractorRegistry::instance().loadPlugins(QDir(appDir).filePath(QStringLiteral("plugins")));
    ExtractorRegistry::instance().loadPlugins(appDir);
}

void TestRobustness::cleanupTestCase()
{
}

void TestRobustness::testZeroByteBentoFile()
{
    QString emptyPath = m_tempDir.filePath(QStringLiteral("empty.bento"));
    QFile f(emptyPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
    QCOMPARE(QFileInfo(emptyPath).size(), 0);

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QString errorMsg;
    bool ok = pc.openProject(emptyPath, &errorMsg);
    QVERIFY(!ok);
    QVERIFY(!errorMsg.isEmpty());
}

void TestRobustness::testTruncatedZipBentoFile()
{
    QString truncPath = m_tempDir.filePath(QStringLiteral("truncated.bento"));
    QFile f(truncPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    // Write standard PK zip header but cut off abruptly
    const char zipHeader[] = { 'P', 'K', 0x03, 0x04, 0x14, 0x00, 0x00, 0x00 };
    f.write(zipHeader, sizeof(zipHeader));
    f.close();

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QString errorMsg;
    bool ok = pc.openProject(truncPath, &errorMsg);
    QVERIFY(!ok);
    QVERIFY(!errorMsg.isEmpty());
}

void TestRobustness::testZipMissingProjectJson()
{
    // Create a valid zip archive containing only a dummy text file, not project.json
    QString noJsonZip = m_tempDir.filePath(QStringLiteral("no_json.bento"));
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    QVERIFY(mz_zip_writer_init_file(&zip, noJsonZip.toUtf8().constData(), 0));

    const char dummy[] = "hello";
    QVERIFY(mz_zip_writer_add_mem(&zip, "dummy.txt", dummy, sizeof(dummy), MZ_DEFAULT_COMPRESSION));
    QVERIFY(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QString errorMsg;
    bool ok = pc.openProject(noJsonZip, &errorMsg);
    QVERIFY(!ok);
    QVERIFY(!errorMsg.isEmpty());
}

void TestRobustness::testZipCorruptedJsonContent()
{
    // Create a zip with project.json containing garbage syntax
    QString badJsonZip = m_tempDir.filePath(QStringLiteral("bad_json.bento"));
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    QVERIFY(mz_zip_writer_init_file(&zip, badJsonZip.toUtf8().constData(), 0));

    const char corruptJson[] = "{ \"version\": 1, \"frames\": [ invalid_json!@#$%^ ";
    QVERIFY(mz_zip_writer_add_mem(&zip, "project.json", corruptJson, strlen(corruptJson), MZ_DEFAULT_COMPRESSION));
    QVERIFY(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QString errorMsg;
    bool ok = pc.openProject(badJsonZip, &errorMsg);
    QVERIFY(!ok);
    QVERIFY(!errorMsg.isEmpty());
}

void TestRobustness::testZipMissingAtlasImage()
{
    // Create a zip with valid project.json pointing to assets/atlas.png, but atlas.png is missing
    QString missingImgZip = m_tempDir.filePath(QStringLiteral("missing_atlas.bento"));
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    QVERIFY(mz_zip_writer_init_file(&zip, missingImgZip.toUtf8().constData(), 0));

    const char validJson[] = "{\n"
                             "  \"formatVersion\": \"1.0\",\n"
                             "  \"atlas\": \"assets/atlas.png\",\n"
                             "  \"frames\": [],\n"
                             "  \"animations\": {}\n"
                             "}";
    QVERIFY(mz_zip_writer_add_mem(&zip, "project.json", validJson, strlen(validJson), MZ_DEFAULT_COMPRESSION));
    QVERIFY(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QString errorMsg;
    pc.openProject(missingImgZip, &errorMsg);
    // When atlas image file is missing, the document atlas remains safely null without crashing
    QVERIFY(doc.atlas().isNull());
}

void TestRobustness::testAsepriteMalformedJson()
{
    Extractor *extractor = ExtractorRegistry::instance().findExtractorById(QStringLiteral("json"));
    if (!extractor) {
        QSKIP("JSON extractor plugin not available");
    }

    // 1. Incomplete JSON
    QString badJson = m_tempDir.filePath(QStringLiteral("corrupted_aseprite.json"));
    {
        QFile f(badJson);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("{ \"frames\": { \"frame1\": { \"frame\": { \"x\": -999, \"y\": 0 "); // truncated
    }

    SpriteDocument doc;
    QString errorMsg;
    bool ok = extractor->extract(badJson, doc, &errorMsg);
    QVERIFY(!ok);

    // 2. Negative dimensions in frame rect
    QString negativeRectJson = m_tempDir.filePath(QStringLiteral("negative_rect.json"));
    {
        QFile f(negativeRectJson);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("{\n"
                "  \"frames\": {\n"
                "    \"f0\": { \"frame\": { \"x\": 10, \"y\": 10, \"w\": -50, \"h\": -20 } }\n"
                "  }\n"
                "}");
    }
    SpriteDocument negDoc;
    extractor->extract(negativeRectJson, negDoc, &errorMsg);
    for (const SpriteBox &b : negDoc.boxes()) {
        QVERIFY(b.rect.width() <= 0 || b.rect.height() <= 0);
    }
}

void TestRobustness::testTexturePackerMalformedJson()
{
    Extractor *extractor = ExtractorRegistry::instance().findExtractorById(QStringLiteral("json"));
    if (!extractor) {
        QSKIP("JSON extractor plugin not available");
    }

    QString emptyJson = m_tempDir.filePath(QStringLiteral("empty_obj.json"));
    {
        QFile f(emptyJson);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("{}");
    }

    SpriteDocument doc;
    QString errorMsg;
    extractor->extract(emptyJson, doc, &errorMsg);
    QVERIFY(doc.boxes().isEmpty());
}

void TestRobustness::testSliceEmptyOrNullImage()
{
    SpriteDocument doc;
    QImage nullImg;
    QVERIFY(nullImg.isNull());

    doc.setAtlas(nullImg);
    QCOMPARE(doc.atlas().isNull(), true);

    // Slicing on null atlas should gracefully return -1 and leave boxes empty
    int idx = doc.addSlice(QRect(0, 0, 10, 10));
    QCOMPARE(idx, -1);
    QCOMPARE(doc.boxes().size(), 0);
}

void TestRobustness::testPackingZeroSlices()
{
    MaxRectsPacker packer(2048, 2048);
    QList<QRect> placed = packer.insert(QList<QSize>{});
    QVERIFY(placed.isEmpty());
}

void TestRobustness::testPackingOversizedSprite()
{
    MaxRectsPacker packer(2048, 2048);
    // Sprite 5000x5000 exceeds maximum bin dimension 2048
    QRect placed = packer.insert(5000, 5000);
    // Should fail cleanly (empty or invalid rect), not crash or loop infinitely
    QVERIFY(!placed.isValid() || placed.isEmpty());
}

void TestRobustness::testTightPackingEmptyPolygon()
{
    AtlasPacker::PackOptions opts;
    opts.maxWidth = 2048;
    opts.maxHeight = 2048;

    AtlasPackResult res = TightPolygonPacker::pack({}, {}, opts, {});
    QVERIFY(res.success || res.frameRects.isEmpty());
}

void TestRobustness::testUnicodeAndSpacedPaths()
{
    // Create nested directory with accents, spaces, and Japanese glyphs
    QString specialDirName = QStringLiteral("Dossier Projet_é_à_テスト/Sous Dossier avec espaces");
    QDir dir(m_tempDir.path());
    QVERIFY(dir.mkpath(specialDirName));

    QString projectPath = dir.filePath(specialDirName + QStringLiteral("/mon_projet_héros.bento"));

    SpriteDocument doc;
    QImage testImg(64, 64, QImage::Format_ARGB32);
    testImg.fill(Qt::cyan);
    doc.setAtlas(testImg);
    doc.addSlice(QRect(0, 0, 32, 32));
    doc.addAnimation(QStringLiteral("anim_test"), {0}, 12);

    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QString errorMsg;
    bool saved = pc.saveProject(projectPath, &errorMsg);
    QVERIFY2(saved, qPrintable(errorMsg));
    QVERIFY(QFile::exists(projectPath));

    // Reload from Unicode spaced path
    SpriteDocument reloadedDoc;
    QUndoStack reloadedStack;
    ProjectController reloadedPc(&reloadedDoc, &reloadedStack);

    bool opened = reloadedPc.openProject(projectPath, &errorMsg);
    QVERIFY2(opened, qPrintable(errorMsg));
    QCOMPARE(reloadedDoc.frameCount(), 1);
    QCOMPARE(reloadedDoc.boxes().first().rect, QRect(0, 0, 32, 32));
    QVERIFY(reloadedDoc.hasAnimation(QStringLiteral("anim_test")));
    QCOMPARE(reloadedDoc.atlas().size(), QSize(64, 64));
}

QTEST_MAIN(TestRobustness)
#include "test_robustness.moc"
