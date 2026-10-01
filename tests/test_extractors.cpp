#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>


#include "asepriteextractor.h"
#include "extractor/extractor.h"
#include "extractor/extractorregistry.h"
#include "gifextractor.h"
#include "godotextractor.h"
#include "image/spritedetector.h"
#include "jsonextractor.h"
#include "libgdxextractor.h"
#include "model/spritedocument.h"
#include "spriteextractor.h"


class TestExtractors : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void testExtractorRegistryBasics();
  void testSpriteExtractorCapabilities();
  void testSpriteExtractorReadPng();
  void testJsonExtractorReadWrite();
  void testGodotExtractorReadWrite();
  void testLibGdxExtractorReadWrite();
  void testAsepriteExtractorRead();
  void testGifExtractorRead();
  void testGifExtractorWriteMultiAnimations();
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

void TestExtractors::initTestCase() {
  // Locate sample directory relative to current source or executable
  QStringList candidates = {
      QStringLiteral(SAMPLE_DIR),
      QDir::current().filePath(QStringLiteral("../sample")),
      QDir::current().filePath(QStringLiteral("../../sample")),
      QDir::current().filePath(QStringLiteral("sample"))};

  for (const QString &cand : candidates) {
    if (QFile::exists(cand + QStringLiteral("/hero.png")) ||
        QFile::exists(cand + QStringLiteral("/hero.webp")) ||
        QFile::exists(cand + QStringLiteral("/hero.bento"))) {
      m_sampleDir = QDir(cand).canonicalPath();
      break;
    }
  }

  if (m_sampleDir.isEmpty()) {
    m_sampleDir = QStringLiteral(SAMPLE_DIR);
  }

  QString binPlugins = QDir(QCoreApplication::applicationDirPath())
                           .filePath(QStringLiteral("plugins"));
  ExtractorRegistry::instance().loadPlugins(binPlugins);
}

void TestExtractors::testExtractorRegistryBasics() {
  auto &reg = ExtractorRegistry::instance();
  const auto &extractors = reg.extractors();
  QVERIFY(extractors.size() >= 6);

  // Verify each codec is present
  QVERIFY(reg.findExtractorById(QStringLiteral("sprite_extractor")) != nullptr);
  QVERIFY(reg.findExtractorById(QStringLiteral("gif_extractor")) != nullptr);
  QVERIFY(reg.findExtractorById(QStringLiteral("json_extractor")) != nullptr);
  QVERIFY(reg.findExtractorById(QStringLiteral("godot_extractor")) != nullptr);
  QVERIFY(reg.findExtractorById(QStringLiteral("libgdx_spine_extractor")) !=
          nullptr);
  QVERIFY(reg.findExtractorById(QStringLiteral("aseprite_extractor")) !=
          nullptr);

  // Verify filter strings
  QString openFilters = reg.openFilterString();
  QVERIFY(openFilters.contains(QStringLiteral("*.png")));
  QVERIFY(openFilters.contains(QStringLiteral("*.json")));
  QVERIFY(openFilters.contains(QStringLiteral("*.tres")));
  QVERIFY(openFilters.contains(QStringLiteral("*.gif")));
  QVERIFY(openFilters.contains(QStringLiteral("*.atlas")));
  QVERIFY(openFilters.contains(QStringLiteral("*.ase")));

  QString saveFilters = reg.saveFilterString();
  QVERIFY(saveFilters.contains(QStringLiteral("*.json")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.tres")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.gif")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.atlas")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.ase")));
}

void TestExtractors::testSpriteExtractorCapabilities() {
  SpriteExtractor extractor;
  QCOMPARE(extractor.id(), QStringLiteral("sprite_extractor"));
  QCOMPARE(extractor.displayName(), QStringLiteral("Sprite Sheet"));
  QVERIFY(!extractor.version().isNull());
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanImport));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));
  QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("png")));
  QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("bmp")));
}

void TestExtractors::testSpriteExtractorReadPng() {
  QString pngPath = m_sampleDir + QStringLiteral("/hero.webp");
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

void TestExtractors::testJsonExtractorReadWrite() {
  QString jsonPath = m_sampleDir + QStringLiteral("/hero.json");
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

void TestExtractors::testGodotExtractorReadWrite() {
  QString godotTres = m_sampleDir + QStringLiteral("/hero_godot.tres");
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
    QCOMPARE(doc.animation(QStringLiteral("side kick")).frameIndices.size(),
             10);
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

void TestExtractors::testLibGdxExtractorReadWrite() {
  LibGdxExtractor extractor;
  QCOMPARE(extractor.id(), QStringLiteral("libgdx_spine_extractor"));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanImport));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));
  QVERIFY(extractor.capabilities().testFlag(Extractor::SupportsAnimations));

  // Construct test document with 4 quadrants and an animation
  SpriteDocument doc;
  QImage atlas(64, 64, QImage::Format_ARGB32_Premultiplied);
  atlas.fill(Qt::transparent);
  {
    QPainter p(&atlas);
    p.fillRect(0, 0, 32, 32, Qt::red);
    p.fillRect(32, 0, 32, 32, Qt::green);
    p.fillRect(0, 32, 32, 32, Qt::blue);
    p.fillRect(32, 32, 32, 32, Qt::yellow);
  }
  doc.setAtlas(atlas);

  // Add 4 frames
  doc.addFrame(atlas.copy(0, 0, 32, 32), SpriteBox(QRect(0, 0, 32, 32)));
  doc.addFrame(atlas.copy(32, 0, 32, 32), SpriteBox(QRect(32, 0, 32, 32)));
  doc.addFrame(atlas.copy(0, 32, 32, 32), SpriteBox(QRect(0, 32, 32, 32)));
  doc.addFrame(atlas.copy(32, 32, 32, 32), SpriteBox(QRect(32, 32, 32, 32)));

  // Set custom pivot on frame 0
  doc.setBoxPivot(0, QPoint(16, 32));

  // Add animation "walk" containing frames 0, 1, 2
  doc.addAnimation(QStringLiteral("walk"), {0, 1, 2}, 12,
                   SpriteAnimation::Loop);

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString exportedAtlas = tempDir.filePath(QStringLiteral("character.atlas"));

  // Write atlas
  ExportOptions opts;
  ExtractorError writeErr;
  bool writeOk = extractor.write(exportedAtlas, doc, opts, &writeErr);
  QVERIFY2(writeOk, qPrintable(writeErr.toString()));
  QVERIFY(QFile::exists(exportedAtlas));
  QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("character.png"))));

  // Test format detection
  QVERIFY(extractor.canDecode(exportedAtlas));
  QVERIFY(
      !extractor.canDecode(tempDir.filePath(QStringLiteral("character.png"))));

  // Read back atlas
  SpriteDocument doc2;
  ExtractorError readErr;
  bool readOk = extractor.read(exportedAtlas, doc2, &readErr);
  QVERIFY2(readOk, qPrintable(readErr.toString()));
  QCOMPARE(doc2.frameCount(), 4);
  QVERIFY(!doc2.atlas().isNull());
  QCOMPARE(doc2.atlas().size(), QSize(64, 64));

  // Verify animation roundtrip
  QVERIFY(doc2.hasAnimation(QStringLiteral("walk")));
  QCOMPARE(doc2.animation(QStringLiteral("walk")).frameIndices,
           QList<int>({0, 1, 2}));

  // Verify custom pivot roundtrip
  QVERIFY(doc2.box(0).hasCustomPivot);
  QCOMPARE(doc2.box(0).pivot, QPoint(16, 32));
}

void TestExtractors::testAsepriteExtractorRead() {
  AsepriteExtractor extractor;
  QCOMPARE(extractor.id(), QStringLiteral("aseprite_extractor"));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanImport));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));
  QVERIFY(extractor.capabilities().testFlag(Extractor::SupportsAnimations));

  // Test format detection on non-aseprite files
  QVERIFY(!extractor.canDecode(QStringLiteral("dummy.png")));
  QVERIFY(!extractor.canDecode(QStringLiteral("dummy.ase")));

  // Build a valid synthetic 2-frame Aseprite binary file
  QByteArray bin;
  QDataStream ds(&bin, QIODevice::WriteOnly);
  ds.setByteOrder(QDataStream::LittleEndian);

  // 1. Header (128 bytes)
  ds << static_cast<quint32>(0); // Placeholder for total file size (offset 0)
  ds << static_cast<quint16>(0xA5E0); // Magic (offset 4)
  ds << static_cast<quint16>(2);      // Frames count (offset 6)
  ds << static_cast<quint16>(8);      // Width (offset 8)
  ds << static_cast<quint16>(8);      // Height (offset 10)
  ds << static_cast<quint16>(32);     // Color depth 32-bit RGBA (offset 12)
  ds << static_cast<quint32>(1);      // Flags (offset 14)
  ds << static_cast<quint16>(100);    // Speed (offset 18)
  ds << static_cast<quint32>(0);      // Reserved (offset 20)
  ds << static_cast<quint32>(0);      // Reserved (offset 24)
  ds << static_cast<quint8>(0);       // Transparent index (offset 28)
  ds << static_cast<quint8>(0) << static_cast<quint8>(0)
     << static_cast<quint8>(0);    // Ignore (offset 29..31)
  ds << static_cast<quint16>(256); // Number of colors (offset 32)
  ds << static_cast<quint8>(1)
     << static_cast<quint8>(1); // Pixel aspect ratio (offset 34..35)
  ds << static_cast<qint16>(0)
     << static_cast<qint16>(0); // Grid X, Y (offset 36..39)
  ds << static_cast<quint16>(8)
     << static_cast<quint16>(8); // Grid W, H (offset 40..43)
  // 84 bytes reserved
  for (int i = 0; i < 84; ++i) {
    ds << static_cast<quint8>(0);
  }
  QCOMPARE(bin.size(), 128);

  // Frame 0: 16 bytes header + Cel chunk (Raw 8x8 red pixels)
  int frame0Start = bin.size();
  ds << static_cast<quint32>(0);      // Frame 0 bytes placeholder
  ds << static_cast<quint16>(0xF1FA); // Magic
  ds << static_cast<quint16>(1);      // Chunks count
  ds << static_cast<quint16>(100);    // Duration ms
  ds << static_cast<quint8>(0) << static_cast<quint8>(0); // Reserved
  ds << static_cast<quint32>(0);                          // New chunks count

  // Frame 0 - Cel Chunk (0x2005): 6 bytes chunk header + 20 bytes cel header +
  // 64*4 pixels = 282 bytes
  ds << static_cast<quint32>(282);                        // Chunk size
  ds << static_cast<quint16>(0x2005);                     // Chunk type
  ds << static_cast<quint16>(0);                          // Layer index
  ds << static_cast<qint16>(0) << static_cast<qint16>(0); // X, Y
  ds << static_cast<quint8>(255);                         // Opacity
  ds << static_cast<quint16>(0);                          // Cel type (0 = Raw)
  for (int i = 0; i < 7; ++i)
    ds << static_cast<quint8>(0);                           // Reserved
  ds << static_cast<quint16>(8) << static_cast<quint16>(8); // W, H
  for (int i = 0; i < 64; ++i) {
    // Red pixel (R=255, G=0, B=0, A=255)
    ds << static_cast<quint8>(255) << static_cast<quint8>(0)
       << static_cast<quint8>(0) << static_cast<quint8>(255);
  }
  // Patch frame 0 size
  quint32 frame0Size = bin.size() - frame0Start;
  memcpy(bin.data() + frame0Start, &frame0Size, sizeof(quint32));

  // Frame 1: 16 bytes header + Cel chunk (Raw 8x8 blue pixels) + Frame Tags
  // chunk
  int frame1Start = bin.size();
  ds << static_cast<quint32>(0);      // Frame 1 bytes placeholder
  ds << static_cast<quint16>(0xF1FA); // Magic
  ds << static_cast<quint16>(2);      // Chunks count
  ds << static_cast<quint16>(100);    // Duration ms
  ds << static_cast<quint8>(0) << static_cast<quint8>(0); // Reserved
  ds << static_cast<quint32>(0);                          // New chunks count

  // Frame 1 - Cel Chunk (0x2005)
  ds << static_cast<quint32>(282);                        // Chunk size
  ds << static_cast<quint16>(0x2005);                     // Chunk type
  ds << static_cast<quint16>(0);                          // Layer index
  ds << static_cast<qint16>(0) << static_cast<qint16>(0); // X, Y
  ds << static_cast<quint8>(255);                         // Opacity
  ds << static_cast<quint16>(0);                          // Cel type (0 = Raw)
  for (int i = 0; i < 7; ++i)
    ds << static_cast<quint8>(0);                           // Reserved
  ds << static_cast<quint16>(8) << static_cast<quint16>(8); // W, H
  for (int i = 0; i < 64; ++i) {
    // Blue pixel (R=0, G=0, B=255, A=255)
    ds << static_cast<quint8>(0) << static_cast<quint8>(0)
       << static_cast<quint8>(255) << static_cast<quint8>(255);
  }

  // Frame 1 - Frame Tags Chunk (0x2018):
  // Chunk header (6) + numTags (2) + reserved (8) + tag header (19) + name
  // "attack" (6) = 41 bytes
  QByteArray tagName = "attack";
  quint32 tagChunkSize = 6 + 10 + 19 + tagName.size();
  ds << static_cast<quint32>(tagChunkSize);
  ds << static_cast<quint16>(0x2018); // Chunk type
  ds << static_cast<quint16>(1);      // 1 tag
  for (int i = 0; i < 8; ++i)
    ds << static_cast<quint8>(0); // Reserved
  ds << static_cast<quint16>(0);  // From frame 0
  ds << static_cast<quint16>(1);  // To frame 1
  ds << static_cast<quint8>(0);   // Loop direction (0 = Forward)
  ds << static_cast<quint16>(0);  // Repeat count (0 = infinite)
  for (int i = 0; i < 6; ++i)
    ds << static_cast<quint8>(0); // Reserved
  ds << static_cast<quint8>(255) << static_cast<quint8>(200)
     << static_cast<quint8>(0);               // Tag color
  ds << static_cast<quint8>(0);               // Extra zero
  ds << static_cast<quint16>(tagName.size()); // String len
  for (int i = 0; i < tagName.size(); ++i) {
    ds << static_cast<quint8>(tagName[i]);
  }

  // Patch frame 1 size
  quint32 frame1Size = bin.size() - frame1Start;
  memcpy(bin.data() + frame1Start, &frame1Size, sizeof(quint32));

  // Patch total file size at offset 0
  quint32 totalFileSize = bin.size();
  memcpy(bin.data(), &totalFileSize, sizeof(quint32));

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString asePath = tempDir.filePath(QStringLiteral("test_hero.ase"));

  QFile aseFile(asePath);
  QVERIFY(aseFile.open(QIODevice::WriteOnly));
  aseFile.write(bin);
  aseFile.close();

  // Verify format detection on valid Aseprite file
  QVERIFY(extractor.canDecode(asePath));

  // Read with AsepriteExtractor
  SpriteDocument doc;
  ExtractorError err;
  bool ok = extractor.read(asePath, doc, &err);
  QVERIFY2(ok, qPrintable(err.toString()));
  QCOMPARE(doc.frameCount(), 2);
  QVERIFY(!doc.atlas().isNull());

  // Verify pixel colors
  QImage frame0 = doc.frame(0);
  QCOMPARE(frame0.size(), QSize(8, 8));
  QRgb redPixel = frame0.pixel(0, 0);
  QVERIFY(qRed(redPixel) > 200 && qBlue(redPixel) < 50);

  QImage frame1 = doc.frame(1);
  QCOMPARE(frame1.size(), QSize(8, 8));
  QRgb bluePixel = frame1.pixel(0, 0);
  QVERIFY(qBlue(bluePixel) > 200 && qRed(bluePixel) < 50);

  // Verify animation tag conversion
  QVERIFY(doc.hasAnimation(QStringLiteral("attack")));
  QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices,
           QList<int>({0, 1}));

  // Test Export (write) roundtrip to Aseprite binary format
  QString roundtripPath = tempDir.filePath(QStringLiteral("roundtrip.ase"));
  ExportOptions exportOpts;
  ExtractorError writeErr;
  bool writeOk = extractor.write(roundtripPath, doc, exportOpts, &writeErr);
  QVERIFY2(writeOk, qPrintable(writeErr.toString()));
  QVERIFY(QFile::exists(roundtripPath));
  QVERIFY(extractor.canDecode(roundtripPath));

  // Read back the exported .ase binary file
  SpriteDocument docRoundtrip;
  ExtractorError roundtripErr;
  bool readRoundtripOk = extractor.read(roundtripPath, docRoundtrip, &roundtripErr);
  QVERIFY2(readRoundtripOk, qPrintable(roundtripErr.toString()));
  QCOMPARE(docRoundtrip.frameCount(), 2);
  QVERIFY(!docRoundtrip.atlas().isNull());

  QImage rFrame0 = docRoundtrip.frame(0);
  QCOMPARE(rFrame0.size(), QSize(8, 8));
  QRgb rRed = rFrame0.pixel(0, 0);
  QVERIFY(qRed(rRed) > 200 && qBlue(rRed) < 50);

  QImage rFrame1 = docRoundtrip.frame(1);
  QCOMPARE(rFrame1.size(), QSize(8, 8));
  QRgb rBlue = rFrame1.pixel(0, 0);
  QVERIFY(qBlue(rBlue) > 200 && qRed(rBlue) < 50);

  QVERIFY(docRoundtrip.hasAnimation(QStringLiteral("attack")));
  QCOMPARE(docRoundtrip.animation(QStringLiteral("attack")).frameIndices,
           QList<int>({0, 1}));
}

void TestExtractors::testGifExtractorRead() {
  QString gifPath = m_sampleDir + QStringLiteral("/hero.gif");
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

void TestExtractors::testGifExtractorWriteMultiAnimations() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  SpriteDocument doc;
  doc.setFilePath(tempDir.filePath(QStringLiteral("myproject.bento")));

  // Create 4 dummy frames with transparency and distinct shapes
  QList<QImage> frames;
  QList<SpriteBox> boxes;
  for (int i = 0; i < 4; ++i) {
    QImage img(32, 32, QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setBrush(QColor(50 * i + 50, 100, 200));
    p.drawRect(i * 4, i * 4, 16, 16);
    p.end();

    SpriteBox box(QRect(i * 32, 0, 32, 32));
    box.index = i;
    frames.append(img);
    boxes.append(box);
  }
  // Define a polygon on box 0 cutting the frame in half diagonally
  QPolygonF poly;
  poly << QPointF(0, 0) << QPointF(32, 0) << QPointF(0, 32);
  boxes[0].polygon = poly;
  boxes[0].hasPolygonMesh = true;

  doc.setFrames(frames, boxes);

  // Add 3 animations:
  // 1. "idle" : loop mode Loop, fps = 10 (100ms per frame)
  doc.addAnimation(QStringLiteral("idle"), {0, 1}, 10, SpriteAnimation::Loop);

  // 2. "attack" : loop mode Once, fps = 20 (50ms per frame)
  doc.addAnimation(QStringLiteral("attack"), {1, 2, 3}, 20,
                   SpriteAnimation::Once);

  // 3. "walk" : loop mode PingPong, fps = 10, sequence {0, 1, 2} -> PingPong
  // should produce {0, 1, 2, 1}
  doc.addAnimation(QStringLiteral("walk"), {0, 1, 2}, 10,
                   SpriteAnimation::PingPong);

  GifExtractor extractor;
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));

  QString exportTarget = tempDir.filePath(QStringLiteral("myproject.gif"));
  ExtractorError err;
  bool ok = extractor.write(exportTarget, doc, ExportOptions{}, &err);
  QVERIFY2(ok, qPrintable(err.toString()));

  // Verify all 3 files exist: myproject_idle.gif, myproject_attack.gif,
  // myproject_walk.gif
  QString idleGif = tempDir.filePath(QStringLiteral("myproject_idle.gif"));
  QString attackGif = tempDir.filePath(QStringLiteral("myproject_attack.gif"));
  QString walkGif = tempDir.filePath(QStringLiteral("myproject_walk.gif"));

  QVERIFY2(QFile::exists(idleGif), qPrintable(idleGif));
  QVERIFY2(QFile::exists(attackGif), qPrintable(attackGif));
  QVERIFY2(QFile::exists(walkGif), qPrintable(walkGif));

  // Verify "idle" GIF content (and that frame 0 is clipped by polygon)
  {
    QImageReader reader(idleGif);
    QVERIFY(reader.canRead());
    QCOMPARE(reader.imageCount(), 2);
    QImage f0 = reader.read();
    QVERIFY(!f0.isNull());
    int delay = reader.nextImageDelay();
    QCOMPARE(delay, 100); // 10 fps -> 100 ms

    // Pixel inside polygon (2, 2) should be opaque
    QVERIFY(f0.pixelColor(2, 2).alpha() > 100);
    // Pixel outside polygon (30, 30) should be transparent (clipped)
    QCOMPARE(f0.pixelColor(30, 30).alpha(), 0);
  }

  // Verify "attack" GIF content
  {
    QImageReader reader(attackGif);
    QVERIFY(reader.canRead());
    QCOMPARE(reader.imageCount(), 3);
    QImage f0 = reader.read();
    QVERIFY(!f0.isNull());
    int delay = reader.nextImageDelay();
    QCOMPARE(delay, 50); // 20 fps -> 50 ms
  }

  // Verify "walk" PingPong GIF content (0, 1, 2 -> 0, 1, 2, 1 = 4 frames)
  {
    QImageReader reader(walkGif);
    QVERIFY(reader.canRead());
    QCOMPARE(reader.imageCount(), 4);
    QImage f0 = reader.read();
    QVERIFY(!f0.isNull());
    int delay = reader.nextImageDelay();
    QCOMPARE(delay, 100); // 10 fps -> 100 ms
  }
}

void TestExtractors::testErrorHandlingNonExistentFile() {
  SpriteExtractor spriteExt;
  SpriteDocument doc;
  ExtractorError err;

  bool ok =
      spriteExt.read(QStringLiteral("non_existent_file_12345.png"), doc, &err);
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

void TestExtractors::testErrorHandlingCorruptedData() {
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

void TestExtractors::testExtractToImagesEquivalence() {
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

void TestExtractors::testExtractPerformance() {
  // Construct a synthetic 512x512 image containing an 8x8 grid of sprites (64
  // sprites)
  QImage largeImg(512, 512, QImage::Format_ARGB32);
  largeImg.fill(Qt::transparent);

  QPainter p(&largeImg);
  for (int gy = 0; gy < 8; ++gy) {
    for (int gx = 0; gx < 8; ++gx) {
      p.fillRect(gx * 64 + 10, gy * 64 + 10, 40, 40,
                 QColor(gx * 30, gy * 30, 150));
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
  QVERIFY2(
      elapsedMs < 500,
      qPrintable(QString("Extraction took too long: %1 ms").arg(elapsedMs)));
}

void TestExtractors::testSpriteDetectorBasics() {
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

void TestExtractors::testSpriteDetectorNestedContainment() {
  // Create an image with an outer hollow box containing an inner isolated
  // component, plus a standalone distinct component.
  QImage testImg(120, 80, QImage::Format_ARGB32);
  testImg.fill(Qt::transparent);

  QPainter p(&testImg);
  // Outer hollow rectangle (bounding box 10,10, 50,50)
  p.fillRect(10, 10, 50, 4, Qt::blue); // Top
  p.fillRect(10, 56, 50, 4, Qt::blue); // Bottom
  p.fillRect(10, 10, 4, 50, Qt::blue); // Left
  p.fillRect(56, 10, 4, 50, Qt::blue); // Right

  // Inner island strictly inside the outer hollow box (30,30, 10,10)
  p.fillRect(30, 30, 10, 10, Qt::green);

  // Standalone separate component (75, 15, 30, 30)
  p.fillRect(75, 15, 30, 30, Qt::red);
  p.end();

  QList<SpriteBox> boxes;
  SpriteDetectionOptions opts;
  bool ok = SpriteDetector::detectBoxes(testImg, boxes, opts);
  QVERIFY(ok);

  // The inner green island (30,30, 10,10) must be filtered out because it is
  // fully inside the outer blue hollow box (10,10, 50,50). Exactly 2 master
  // boxes should be returned.
  QCOMPARE(boxes.size(), 2);
  QCOMPARE(boxes[0].rect, QRect(10, 10, 50, 50));
  QCOMPARE(boxes[1].rect, QRect(75, 15, 30, 30));
}

void TestExtractors::testSpriteDetectorLargeScaleInclusionPerformance() {
  // Stress test: 1024x1024 image with 64 outer master sprites (8x8 grid).
  // Inside each master sprite, place 8 separate small disjoint components
  // (total 512 nested islands). In addition, place 64 small standalone sprites
  // outside the grid. Total components = 64 (outer) + 512 (nested) + 64
  // (standalone) = 640 components.
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

  qDebug() << "SpriteDetector segmented 640 components (with 512 nested) on "
              "1024x1024 in"
           << elapsedMs << "ms";
  // With SpatialGrid2D, this runs in ~10-30 ms. Threshold set to 500 ms for CI
  // headroom.
  QVERIFY2(
      elapsedMs < 500,
      qPrintable(QString("Large scale inclusion check took too long: %1 ms")
                     .arg(elapsedMs)));
}

#include <QGuiApplication>

int main(int argc, char *argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);
  TestExtractors tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_extractors.moc"
