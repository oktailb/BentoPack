#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
#include "packer/vramtexturecompressor.h"
#include "extractor/godotextractor.h"
#include "extractor/jsonextractor.h"
#include "extractor/unityextractor.h"
#include "extractor/unrealextractor.h"
#include "model/spritedocument.h"
#include "cli/cliparser.h"

using namespace SpriteStudioCli;

class TestVramCompression : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testVramCompressorAvailability();
    void testKtx2MagicHeaderValidation();
    void testUastcCompressionAlphaPreservation();
    void testEtc1sCompression();
    void testZstdSupercompressionRatio();
    void testTranscodingRoundtrip();
    void testCompressToFile();
    void testVramEstimate();
    void testCliTexturePackerKtx2Export();
    void testCliGodotKtx2Export();
    void testGodotExtractorReadKtx2Roundtrip();
    void testJsonExtractorReadKtx2Roundtrip();
    void testEmptyBaseNameExportRejection();
    void testReadUserRyuTres();
    void testUnityExtractorReadRoundtrip();
    void testUnrealExtractorReadRoundtrip();

private:
    QTemporaryDir m_tempDir;
    QImage createTestImage(int width, int height);
};

void TestVramCompression::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestVramCompression::cleanupTestCase()
{
}

QImage TestVramCompression::createTestImage(int width, int height)
{
    QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    // Draw colorful patterns with alpha
    p.fillRect(0, 0, width / 2, height / 2, QColor(255, 0, 0, 255));
    p.fillRect(width / 2, 0, width / 2, height / 2, QColor(0, 255, 0, 180));
    p.fillRect(0, height / 2, width / 2, height / 2, QColor(0, 0, 255, 120));
    p.fillRect(width / 2, height / 2, width / 2, height / 2, QColor(255, 255, 0, 255));
    p.setPen(Qt::white);
    p.drawText(img.rect(), Qt::AlignCenter, QStringLiteral("SS"));
    p.end();

    return img;
}

void TestVramCompression::testVramCompressorAvailability()
{
    QVERIFY2(VramTextureCompressor::isAvailable(), "VramTextureCompressor should be compiled and available");
}

void TestVramCompression::testKtx2MagicHeaderValidation()
{
    QImage img = createTestImage(64, 64);
    VramCompressionOptions opts;
    opts.format = VramFormat::KTX2_UASTC;
    opts.qualityLevel = 1;
    opts.zstdSupercompression = false;

    VramCompressionStats stats;
    QString err;
    QByteArray ktx2Data = VramTextureCompressor::compressToKtx2(img, opts, &stats, &err);

    QVERIFY2(!ktx2Data.isEmpty(), qPrintable(err));
    QVERIFY2(VramTextureCompressor::isValidKtx2(ktx2Data), "Data must be a valid KTX2 container");

    // Explicit check on KTX2 identifier bytes: 0xAB 0x4B 0x54 0x58 0x20 0x32 0x30 0xBB 0x0D 0x0A 0x1A 0x0A
    const unsigned char ktx2Id[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
    QCOMPARE(memcmp(ktx2Data.constData(), ktx2Id, 12), 0);
    QVERIFY(stats.compressedBytes > 0);
    QVERIFY(stats.originalBytes == 64 * 64 * 4);
}

void TestVramCompression::testUastcCompressionAlphaPreservation()
{
    QImage img = createTestImage(64, 64);
    VramCompressionOptions opts;
    opts.format = VramFormat::KTX2_UASTC;
    opts.qualityLevel = 2;
    opts.zstdSupercompression = true;

    VramCompressionStats stats;
    QString err;
    QByteArray ktx2 = VramTextureCompressor::compressToKtx2(img, opts, &stats, &err);

    QVERIFY2(!ktx2.isEmpty(), qPrintable(err));
    QVERIFY(VramTextureCompressor::isValidKtx2(ktx2));
    QVERIFY2(stats.vramSavingsPercent > 50.0, "UASTC 4x4 should yield ~75% VRAM savings compared to RGBA8888");
}

void TestVramCompression::testEtc1sCompression()
{
    QImage img = createTestImage(128, 128);
    VramCompressionOptions opts;
    opts.format = VramFormat::KTX2_ETC1S;
    opts.qualityLevel = 2;
    opts.zstdSupercompression = false;

    VramCompressionStats stats;
    QString err;
    QByteArray ktx2 = VramTextureCompressor::compressToKtx2(img, opts, &stats, &err);

    QVERIFY2(!ktx2.isEmpty(), qPrintable(err));
    QVERIFY(VramTextureCompressor::isValidKtx2(ktx2));
    QVERIFY2(stats.vramSavingsPercent > 80.0, "ETC1S should yield ~87.5% VRAM savings compared to RGBA8888");
}

void TestVramCompression::testZstdSupercompressionRatio()
{
    QImage img = createTestImage(128, 128);

    VramCompressionOptions optsNoZstd;
    optsNoZstd.format = VramFormat::KTX2_UASTC;
    optsNoZstd.qualityLevel = 1;
    optsNoZstd.zstdSupercompression = false;

    VramCompressionOptions optsZstd;
    optsZstd.format = VramFormat::KTX2_UASTC;
    optsZstd.qualityLevel = 1;
    optsZstd.zstdSupercompression = true;
    optsZstd.zstdLevel = 9;

    QByteArray rawKtx2 = VramTextureCompressor::compressToKtx2(img, optsNoZstd);
    QByteArray zstdKtx2 = VramTextureCompressor::compressToKtx2(img, optsZstd);

    QVERIFY(!rawKtx2.isEmpty());
    QVERIFY(!zstdKtx2.isEmpty());
    // Zstandard supercompression should significantly reduce disk footprint
    QVERIFY2(zstdKtx2.size() < rawKtx2.size(), "Zstandard supercompression should reduce KTX2 payload size");
}

void TestVramCompression::testTranscodingRoundtrip()
{
    QImage original = createTestImage(64, 64);
    VramCompressionOptions opts;
    opts.format = VramFormat::KTX2_UASTC;
    opts.qualityLevel = 1;
    opts.zstdSupercompression = false;

    QByteArray ktx2Data = VramTextureCompressor::compressToKtx2(original, opts);
    QVERIFY(!ktx2Data.isEmpty());

    QString transcodeErr;
    QImage decoded = VramTextureCompressor::transcodeToRgba(ktx2Data, &transcodeErr);
    QVERIFY2(!decoded.isNull(), qPrintable(transcodeErr));
    QCOMPARE(decoded.width(), 64);
    QCOMPARE(decoded.height(), 64);

    // Verify some non-zero alpha and color
    QRgb pixelCenter = decoded.pixel(16, 16);
    QVERIFY(qAlpha(pixelCenter) > 200);
    QVERIFY(qRed(pixelCenter) > 200);
}

void TestVramCompression::testCompressToFile()
{
    QImage img = createTestImage(64, 64);
    QString outPath = m_tempDir.filePath(QStringLiteral("test_atlas.ktx2"));

    VramCompressionOptions opts;
    opts.format = VramFormat::KTX2_UASTC;
    VramCompressionStats stats;
    QString err;

    bool ok = VramTextureCompressor::compressToFile(img, outPath, opts, &stats, &err);
    QVERIFY2(ok, qPrintable(err));
    QVERIFY(QFile::exists(outPath));
    QVERIFY(QFileInfo(outPath).size() > 0);
}

void TestVramCompression::testVramEstimate()
{
    qint64 uastcBytes = VramTextureCompressor::estimateVramBytes(1024, 1024, VramFormat::KTX2_UASTC);
    qint64 etc1sBytes = VramTextureCompressor::estimateVramBytes(1024, 1024, VramFormat::KTX2_ETC1S);
    qint64 rgbaBytes = 1024 * 1024 * 4;

    QCOMPARE(uastcBytes, 1024 * 1024 * 1); // 1 byte per pixel
    QCOMPARE(etc1sBytes, (1024 * 1024) / 2); // 0.5 byte per pixel
    QVERIFY(uastcBytes < rgbaBytes);
    QVERIFY(etc1sBytes < uastcBytes);
}

void TestVramCompression::testCliTexturePackerKtx2Export()
{
    QString sampleHero = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.png");
    if (!QFile::exists(sampleHero)) {
        QSKIP("Sample hero.png not found, skipping CLI VRAM test.");
    }

    QString sheetOut = m_tempDir.filePath(QStringLiteral("cli_hero.ktx2"));
    QString dataOut = m_tempDir.filePath(QStringLiteral("cli_hero.json"));

    CliParser parser;
    CliResult res = parser.parseAndExecute({
        QStringLiteral("spritestudio-cli"),
        QStringLiteral("--sheet"), sheetOut,
        QStringLiteral("--data"), dataOut,
        QStringLiteral("--texture-format"), QStringLiteral("ktx2"),
        QStringLiteral("--opt"), QStringLiteral("ASTC_4x4"),
        sampleHero
    });

    QCOMPARE(res.exitCode, ExitSuccess);
    QVERIFY(QFile::exists(sheetOut));
    QVERIFY(QFile::exists(dataOut));

    // Verify KTX2 header
    QFile f(sheetOut);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray header = f.read(12);
    f.close();
    QVERIFY(VramTextureCompressor::isValidKtx2(header));

    // Verify JSON payload
    QVERIFY(res.json.contains(QStringLiteral("vram_format")));
    QVERIFY(res.json.contains(QStringLiteral("vram_savings_percent")));
    QVERIFY(res.json[QStringLiteral("vram_savings_percent")].toDouble() > 50.0);
}

void TestVramCompression::testCliGodotKtx2Export()
{
    QString sampleHero = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.png");
    if (!QFile::exists(sampleHero)) {
        QSKIP("Sample hero.png not found, skipping CLI Godot VRAM test.");
    }

    QString sheetOut = m_tempDir.filePath(QStringLiteral("godot_hero.ktx2"));
    QString tresOut = m_tempDir.filePath(QStringLiteral("godot_hero.tres"));

    CliParser parser;
    CliResult res = parser.parseAndExecute({
        QStringLiteral("spritestudio-cli"),
        QStringLiteral("--sheet"), sheetOut,
        QStringLiteral("--data"), tresOut,
        QStringLiteral("--format"), QStringLiteral("godot"),
        sampleHero
    });

    QCOMPARE(res.exitCode, ExitSuccess);
    QVERIFY(QFile::exists(sheetOut));
    QVERIFY(QFile::exists(tresOut));

    // Check that .tres references godot_hero.ktx2
    QFile tf(tresOut);
    QVERIFY(tf.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(tf.readAll());
    tf.close();
    QVERIFY2(content.contains(QStringLiteral("path=\"res://godot_hero.ktx2\"")), "Godot .tres must reference .ktx2 companion texture");
}

void TestVramCompression::testGodotExtractorReadKtx2Roundtrip()
{
    // Create a SpriteDocument with test frames
    SpriteDocument doc;
    QList<QImage> frames;
    QList<SpriteBox> boxes;
    for (int i = 0; i < 4; ++i) {
        frames.append(createTestImage(32, 32));
        SpriteBox b;
        b.rect = QRect(0, 0, 32, 32);
        b.index = i;
        boxes.append(b);
    }
    doc.setFrames(frames, boxes);

    // Export with GodotExtractor and KTX2 UASTC
    GodotExtractor godot;
    ExportOptions opts;
    opts.format = FORMAT_GODOT;
    opts.textureFormat = TEXTURE_FORMAT_KTX2_UASTC;
    opts.vramOptions.format = VramFormat::KTX2_UASTC;
    opts.vramOptions.qualityLevel = 1;

    QString exportPath = m_tempDir.filePath(QStringLiteral("godot_roundtrip.tres"));
    QString ktxPath = m_tempDir.filePath(QStringLiteral("godot_roundtrip.ktx2"));
    ExtractorError err;
    bool writeOk = godot.write(exportPath, doc, opts, &err);
    QVERIFY2(writeOk, qPrintable(err.toString()));
    QVERIFY(QFile::exists(exportPath));
    QVERIFY(QFile::exists(ktxPath));

    // Re-import the exported .tres (which references .ktx2)
    SpriteDocument readDoc;
    ExtractorError readErr;
    bool readOk = godot.read(exportPath, readDoc, &readErr);
    QVERIFY2(readOk, qPrintable(readErr.toString()));
    QCOMPARE(readDoc.frameCount(), 4);
    QVERIFY(!readDoc.atlas().isNull());
    QVERIFY(readDoc.atlas().width() > 0);
    QVERIFY(readDoc.atlas().height() > 0);
}

void TestVramCompression::testJsonExtractorReadKtx2Roundtrip()
{
    // Create a SpriteDocument with test frames
    SpriteDocument doc;
    QList<QImage> frames;
    QList<SpriteBox> boxes;
    for (int i = 0; i < 3; ++i) {
        frames.append(createTestImage(32, 32));
        SpriteBox b;
        b.rect = QRect(0, 0, 32, 32);
        b.index = i;
        boxes.append(b);
    }
    doc.setFrames(frames, boxes);

    // Export with JsonExtractor and KTX2 UASTC
    JsonExtractor json;
    ExportOptions opts;
    opts.format = FORMAT_TEXTUREPACKER_JSON;
    opts.textureFormat = TEXTURE_FORMAT_KTX2_UASTC;
    opts.vramOptions.format = VramFormat::KTX2_UASTC;
    opts.vramOptions.qualityLevel = 1;

    QString exportPath = m_tempDir.filePath(QStringLiteral("json_roundtrip.json"));
    QString ktxPath = m_tempDir.filePath(QStringLiteral("json_roundtrip.ktx2"));
    ExtractorError err;
    bool writeOk = json.write(exportPath, doc, opts, &err);
    QVERIFY2(writeOk, qPrintable(err.toString()));
    QVERIFY(QFile::exists(exportPath));
    QVERIFY(QFile::exists(ktxPath));

    // Re-import the exported .json (which references .ktx2)
    SpriteDocument readDoc;
    ExtractorError readErr;
    bool readOk = json.read(exportPath, readDoc, &readErr);
    QVERIFY2(readOk, qPrintable(readErr.toString()));
    QCOMPARE(readDoc.frameCount(), 3);
    QVERIFY(!readDoc.atlas().isNull());
}

void TestVramCompression::testEmptyBaseNameExportRejection()
{
    SpriteDocument doc;
    QList<QImage> frames = { createTestImage(32, 32) };
    SpriteBox b;
    b.rect = QRect(0, 0, 32, 32);
    b.index = 0;
    QList<SpriteBox> boxes = { b };
    doc.setFrames(frames, boxes);

    GodotExtractor godot;
    ExportOptions opts;
    ExtractorError err;

    // A path with no base name (e.g. just ".tres" or directory) must fail
    QString invalidTres = m_tempDir.filePath(QStringLiteral(".tres"));
    bool ok = godot.write(invalidTres, doc, opts, &err);
    QVERIFY(!ok);
    QVERIFY(!err.toString().isEmpty());

    JsonExtractor json;
    QString invalidJson = m_tempDir.filePath(QStringLiteral(".json"));
    ok = json.write(invalidJson, doc, opts, &err);
    QVERIFY(!ok);
    QVERIFY(!err.toString().isEmpty());
}

void TestVramCompression::testReadUserRyuTres()
{
    QString ryuTres = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/ryu.tres");
    if (!QFile::exists(ryuTres)) {
        QSKIP("sample/ryu.tres not found");
    }
    GodotExtractor godot;
    SpriteDocument doc;
    ExtractorError err;
    bool ok = godot.read(ryuTres, doc, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(doc.frameCount() > 0);
    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.atlas().width() > 0);
    QVERIFY(doc.atlas().height() > 0);
    QVERIFY2(doc.box(0).hasPolygonMesh, "Frame 0 must have restored polygon mesh from _mesh.tres");
    QVERIFY(!doc.box(0).polygon.isEmpty());
    QVERIFY(!doc.box(0).triangles.isEmpty());
}

void TestVramCompression::testUnityExtractorReadRoundtrip()
{
    SpriteDocument doc;
    QList<QImage> frames;
    QList<SpriteBox> boxes;
    for (int i = 0; i < 2; ++i) {
        frames.append(createTestImage(32, 32));
        SpriteBox b;
        b.rect = QRect(0, 0, 32, 32);
        b.index = i;
        b.hasPolygonMesh = true;
        b.polygon = QPolygonF({ QPointF(0, 0), QPointF(32, 0), QPointF(16, 32) });
        b.vertices = b.polygon.toList();
        b.triangles = { 0, 1, 2 };
        boxes.append(b);
    }
    doc.setFrames(frames, boxes);

    UnityExtractor unity;
    ExportOptions opts;
    opts.format = FORMAT_UNITY;
    opts.textureFormat = TEXTURE_FORMAT_KTX2_UASTC;
    opts.vramOptions.format = VramFormat::KTX2_UASTC;

    QString exportPath = m_tempDir.filePath(QStringLiteral("hero.unity.json"));
    QString ktxPath = m_tempDir.filePath(QStringLiteral("hero.ktx2"));
    ExtractorError err;
    bool ok = unity.write(exportPath, doc, opts, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(QFile::exists(exportPath));
    QVERIFY(QFile::exists(ktxPath));

    SpriteDocument readDoc;
    bool readOk = unity.read(exportPath, readDoc, &err);
    QVERIFY2(readOk, qPrintable(err.toString()));
    QCOMPARE(readDoc.frameCount(), 2);
    QVERIFY(!readDoc.atlas().isNull());
    QVERIFY2(readDoc.box(0).hasPolygonMesh, "Unity frame 0 must have tight polygon mesh");
    QCOMPARE(readDoc.box(0).polygon.size(), 3);
    QCOMPARE(readDoc.box(0).triangles.size(), 3);
}

void TestVramCompression::testUnrealExtractorReadRoundtrip()
{
    SpriteDocument doc;
    QList<QImage> frames;
    QList<SpriteBox> boxes;
    for (int i = 0; i < 2; ++i) {
        frames.append(createTestImage(32, 32));
        SpriteBox b;
        b.rect = QRect(0, 0, 32, 32);
        b.index = i;
        b.hasPolygonMesh = true;
        b.polygon = QPolygonF({ QPointF(0, 0), QPointF(32, 0), QPointF(16, 32) });
        b.vertices = b.polygon.toList();
        b.triangles = { 0, 1, 2 };
        boxes.append(b);
    }
    doc.setFrames(frames, boxes);

    UnrealExtractor unreal;
    ExportOptions opts;
    opts.format = FORMAT_UNREAL;
    opts.textureFormat = TEXTURE_FORMAT_KTX2_UASTC;
    opts.vramOptions.format = VramFormat::KTX2_UASTC;

    QString exportPath = m_tempDir.filePath(QStringLiteral("hero.paper2d.json"));
    QString ktxPath = m_tempDir.filePath(QStringLiteral("hero.ktx2"));
    ExtractorError err;
    bool ok = unreal.write(exportPath, doc, opts, &err);
    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(QFile::exists(exportPath));
    QVERIFY(QFile::exists(ktxPath));

    SpriteDocument readDoc;
    bool readOk = unreal.read(exportPath, readDoc, &err);
    QVERIFY2(readOk, qPrintable(err.toString()));
    QCOMPARE(readDoc.frameCount(), 2);
    QVERIFY(!readDoc.atlas().isNull());
    QVERIFY2(readDoc.box(0).hasPolygonMesh, "Unreal frame 0 must have tight polygon mesh");
    QCOMPARE(readDoc.box(0).polygon.size(), 3);
    QCOMPARE(readDoc.box(0).triangles.size(), 3);
}

QTEST_MAIN(TestVramCompression)
#include "test_vram_compression.moc"
