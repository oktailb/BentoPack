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
#include "unityextractor.h"
#include "unrealextractor.h"
#include "libgdxextractor.h"
#include "model/spritedocument.h"
#include "packer/multiatlaspacker.h"
#include "project/projectmanager.h"
#include "spriteextractor.h"
#include "plistextractor.h"
#include "pcxextractor.h"
#include "pico8extractor.h"
#include "iffextractor.h"
#include "retrotileextractor.h"


class TestExtractors : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void testMultiAtlasLightingAndMaterials();
  void testExtractorRegistryBasics();
  void testSpriteExtractorCapabilities();
  void testSpriteExtractorReadPng();
  void testJsonExtractorReadWrite();
  void testGodotExtractorReadWrite();
  void testLibGdxExtractorReadWrite();
  void testAsepriteExtractorRead();
  void testAsepriteMultiLayerAndSkinVariants();
  void testAsepriteExtractorWritePolygonClipping();
  void testGifExtractorRead();
  void testGifExtractorWriteMultiAnimations();
  void testPlistExtractorReadWrite();
  void testPcxExtractorReadWrite();
  void testPico8ExtractorReadWrite();
  void testIffExtractorReadWrite();
  void testRetroTileExtractorReadWrite();
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

void TestExtractors::testMultiAtlasLightingAndMaterials() {
  // 1. Detection of map types from names
  QCOMPARE(MultiAtlasPacker::detectMapType(QStringLiteral("player_n")), MaterialMapType::Normal);
  QCOMPARE(MultiAtlasPacker::detectMapType(QStringLiteral("Player_Normal")), MaterialMapType::Normal);
  QCOMPARE(MultiAtlasPacker::detectMapType(QStringLiteral("Layer_N")), MaterialMapType::Normal);
  QCOMPARE(MultiAtlasPacker::detectMapType(QStringLiteral("torch_e")), MaterialMapType::Emissive);
  QCOMPARE(MultiAtlasPacker::detectMapType(QStringLiteral("magic_emissive")), MaterialMapType::Emissive);
  QCOMPARE(MultiAtlasPacker::detectMapType(QStringLiteral("sword_s")), MaterialMapType::Specular);
  QCOMPARE(MultiAtlasPacker::detectMapType(QStringLiteral("shield_specular")), MaterialMapType::Specular);
  QCOMPARE(MultiAtlasPacker::detectMapType(QStringLiteral("player_diffuse")), MaterialMapType::Albedo);
  QCOMPARE(MultiAtlasPacker::detectMapType(QStringLiteral("Layer 1")), MaterialMapType::Albedo);

  // 2. Neutral fill colors
  QCOMPARE(MultiAtlasPacker::neutralColor(MaterialMapType::Normal), QColor(128, 128, 255, 0));
  QCOMPARE(MultiAtlasPacker::neutralColor(MaterialMapType::Emissive), QColor(0, 0, 0, 0));
  QCOMPARE(MultiAtlasPacker::neutralColor(MaterialMapType::Specular), QColor(0, 0, 0, 0));

  // 3. Normal Map Y-Flip (Green channel inversion)
  QImage normTest(4, 4, QImage::Format_ARGB32);
  normTest.fill(qRgba(128, 60, 255, 255));
  QImage flipped = MultiAtlasPacker::flipNormalMapY(normTest);
  QCOMPARE(qGreen(flipped.pixel(0, 0)), 255 - 60);
  QCOMPARE(qRed(flipped.pixel(0, 0)), 128);
  QCOMPARE(qBlue(flipped.pixel(0, 0)), 255);

  // 4. MultiAtlas master-slave packing synchronization
  SpriteDocument doc;
  QImage albedo0(16, 16, QImage::Format_ARGB32);
  albedo0.fill(QColor(255, 0, 0)); // Red
  QImage albedo1(24, 20, QImage::Format_ARGB32);
  albedo1.fill(QColor(0, 255, 0)); // Green

  QImage normal0(16, 16, QImage::Format_ARGB32);
  normal0.fill(qRgba(128, 128, 255, 255)); // Tangent normal
  QImage normal1(24, 20, QImage::Format_ARGB32);
  normal1.fill(qRgba(140, 110, 255, 255));

  QImage emissive0(16, 16, QImage::Format_ARGB32);
  emissive0.fill(qRgba(255, 200, 50, 255)); // Glowing yellow
  QImage emissive1(24, 20, QImage::Format_ARGB32);
  emissive1.fill(qRgba(0, 0, 0, 0));

  doc.addFrame(albedo0, SpriteBox(QRect(0, 0, 16, 16)));
  doc.addFrame(albedo1, SpriteBox(QRect(0, 0, 24, 20)));
  doc.setAuxiliaryFrames(MaterialMapType::Normal, {normal0, normal1});
  doc.setAuxiliaryFrames(MaterialMapType::Emissive, {emissive0, emissive1});

  QVERIFY(doc.hasAuxiliaryMap(MaterialMapType::Normal));
  QVERIFY(doc.hasAuxiliaryMap(MaterialMapType::Emissive));
  QVERIFY(!doc.hasAuxiliaryMap(MaterialMapType::Specular));
  QVERIFY(doc.hasAnyAuxiliaryMaps());

  AtlasPacker::PackOptions packOpts;
  packOpts.algorithm = AtlasPacker::MaxRects;
  packOpts.padding = 2;

  MultiAtlasPackResult multiRes = MultiAtlasPacker::pack(doc, packOpts, false);
  QVERIFY(multiRes.success);
  QVERIFY(multiRes.hasMap(MaterialMapType::Normal));
  QVERIFY(multiRes.hasMap(MaterialMapType::Emissive));
  QVERIFY(!multiRes.hasMap(MaterialMapType::Specular));

  // Verify dimensions match perfectly
  QCOMPARE(multiRes.master.dimensions, multiRes.maps[MaterialMapType::Normal].size());
  QCOMPARE(multiRes.master.dimensions, multiRes.maps[MaterialMapType::Emissive].size());
  QCOMPARE(multiRes.master.frameRects.size(), 2);

  // 5. Godot 4 CanvasTexture export
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString godotFile = tempDir.filePath(QStringLiteral("player_anim.tres"));

  GodotExtractor godotExt;
  ExportOptions expOpts;
  expOpts.exportMaterialMaps = true;
  expOpts.normalMapYFlip = false;
  ExtractorError godotErr;

  QVERIFY2(godotExt.write(godotFile, doc, expOpts, &godotErr), qPrintable(godotErr.message));
  QVERIFY(QFile::exists(godotFile));
  QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("player_anim.png"))));
  QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("player_anim_n.png"))));
  QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("player_anim_e.png"))));

  QFile gFile(godotFile);
  QVERIFY(gFile.open(QIODevice::ReadOnly | QIODevice::Text));
  QString gContent = QString::fromUtf8(gFile.readAll());
  gFile.close();

  QVERIFY(gContent.contains(QStringLiteral("CanvasTexture_main")));
  QVERIFY(gContent.contains(QStringLiteral("diffuse_texture")));
  QVERIFY(gContent.contains(QStringLiteral("normal_texture")));
  QVERIFY(gContent.contains(QStringLiteral("atlas = SubResource(\"CanvasTexture_main\")")));

  // 6. Unity 2D Sprite Mesh export
  QString unityFile = tempDir.filePath(QStringLiteral("player_unity.unity.json"));
  UnityExtractor unityExt;
  ExtractorError unityErr;
  QVERIFY2(unityExt.write(unityFile, doc, expOpts, &unityErr), qPrintable(unityErr.message));
  QVERIFY(QFile::exists(unityFile));
  QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("player_unity_n.png"))));

  QFile uFile(unityFile);
  QVERIFY(uFile.open(QIODevice::ReadOnly | QIODevice::Text));
  QString uContent = QString::fromUtf8(uFile.readAll());
  uFile.close();
  QVERIFY(uContent.contains(QStringLiteral("\"textures\"")));
  QVERIFY(uContent.contains(QStringLiteral("\"normal\"")));

  // 7. Unreal Engine Paper2D export
  QString unrealFile = tempDir.filePath(QStringLiteral("player_unreal.paper2d.json"));
  UnrealExtractor unrealExt;
  ExtractorError unrealErr;
  QVERIFY2(unrealExt.write(unrealFile, doc, expOpts, &unrealErr), qPrintable(unrealErr.message));
  QVERIFY(QFile::exists(unrealFile));
  QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("player_unreal_n.png"))));

  QFile unFile(unrealFile);
  QVERIFY(unFile.open(QIODevice::ReadOnly | QIODevice::Text));
  QString unContent = QString::fromUtf8(unFile.readAll());
  unFile.close();
  QVERIFY(unContent.contains(QStringLiteral("\"textures\"")));
  QVERIFY(unContent.contains(QStringLiteral("\"normal\"")));

  // 8. Standard JSON export
  QString jsonFile = tempDir.filePath(QStringLiteral("player_sheet.json"));
  JsonExtractor jsonExt;
  ExtractorError jsonErr;
  QVERIFY2(jsonExt.write(jsonFile, doc, expOpts, &jsonErr), qPrintable(jsonErr.message));
  QVERIFY(QFile::exists(jsonFile));
  QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("player_sheet_n.png"))));

  QFile jFile(jsonFile);
  QVERIFY(jFile.open(QIODevice::ReadOnly | QIODevice::Text));
  QString jContent = QString::fromUtf8(jFile.readAll());
  jFile.close();
  QVERIFY(jContent.contains(QStringLiteral("\"textures\"")));
  QVERIFY(jContent.contains(QStringLiteral("\"normal\"")));
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
  QVERIFY(reg.findExtractorById(QStringLiteral("plist_extractor")) != nullptr);
  QVERIFY(reg.findExtractorById(QStringLiteral("pcx_extractor")) != nullptr);
  QVERIFY(reg.findExtractorById(QStringLiteral("pico8_extractor")) != nullptr);
  QVERIFY(reg.findExtractorById(QStringLiteral("iff_extractor")) != nullptr);
  QVERIFY(reg.findExtractorById(QStringLiteral("retro_tile_extractor")) != nullptr);

  // Verify filter strings
  QString openFilters = reg.openFilterString();
  QVERIFY(openFilters.contains(QStringLiteral("*.png")));
  QVERIFY(openFilters.contains(QStringLiteral("*.json")));
  QVERIFY(openFilters.contains(QStringLiteral("*.tres")));
  QVERIFY(openFilters.contains(QStringLiteral("*.gif")));
  QVERIFY(openFilters.contains(QStringLiteral("*.atlas")));
  QVERIFY(openFilters.contains(QStringLiteral("*.ase")));
  QVERIFY(openFilters.contains(QStringLiteral("*.plist")));
  QVERIFY(openFilters.contains(QStringLiteral("*.pcx")));
  QVERIFY(openFilters.contains(QStringLiteral("*.p8")));
  QVERIFY(openFilters.contains(QStringLiteral("*.iff")));
  QVERIFY(openFilters.contains(QStringLiteral("*.lbm")));
  QVERIFY(openFilters.contains(QStringLiteral("*.nes")));
  QVERIFY(openFilters.contains(QStringLiteral("*.gb")));
  QVERIFY(openFilters.contains(QStringLiteral("*.tim")));

  QString saveFilters = reg.saveFilterString();
  QVERIFY(saveFilters.contains(QStringLiteral("*.json")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.tres")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.gif")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.atlas")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.ase")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.plist")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.pcx")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.p8")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.iff")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.lbm")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.nes")));
  QVERIFY(saveFilters.contains(QStringLiteral("*.chr")));
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


void TestExtractors::testAsepriteMultiLayerAndSkinVariants() {
  QByteArray bin;
  QDataStream ds(&bin, QIODevice::WriteOnly);
  ds.setByteOrder(QDataStream::LittleEndian);

  // 1. Header (128 bytes)
  ds << static_cast<quint32>(0);      // File size placeholder
  ds << static_cast<quint16>(0xA5E0); // Magic
  ds << static_cast<quint16>(2);      // 2 Frames
  ds << static_cast<quint16>(16);     // Width 16
  ds << static_cast<quint16>(16);     // Height 16
  ds << static_cast<quint16>(32);     // RGBA 32-bit
  ds << static_cast<quint32>(1);      // Flags
  ds << static_cast<quint16>(100);    // Speed
  ds << static_cast<quint32>(0) << static_cast<quint32>(0);
  ds << static_cast<quint8>(0);       // Transparent index
  ds << static_cast<quint8>(0) << static_cast<quint8>(0) << static_cast<quint8>(0);
  ds << static_cast<quint16>(256);    // Number of colors
  ds << static_cast<quint8>(1) << static_cast<quint8>(1);
  ds << static_cast<qint16>(0) << static_cast<qint16>(0);
  ds << static_cast<quint16>(16) << static_cast<quint16>(16);
  for (int i = 0; i < 84; ++i) {
    ds << static_cast<quint8>(0);
  }
  QCOMPARE(bin.size(), 128);

  // Helper lambda to write a Layer chunk (0x2004)
  auto writeLayerChunk = [&](quint16 flags, const QString &name) {
    QByteArray nameUtf8 = name.toUtf8();
    quint32 chunkSize = 6 + 2 + 2 + 2 + 2 + 2 + 2 + 1 + 3 + 2 + nameUtf8.size();
    ds << static_cast<quint32>(chunkSize);
    ds << static_cast<quint16>(0x2004); // CHUNK_LAYER
    ds << static_cast<quint16>(flags);  // Flags (1=visible)
    ds << static_cast<quint16>(0);      // Type (0=normal)
    ds << static_cast<quint16>(0);      // Child level
    ds << static_cast<quint16>(0) << static_cast<quint16>(0); // Default w, h
    ds << static_cast<quint16>(0);      // Blend mode (0=normal)
    ds << static_cast<quint8>(255);     // Opacity
    ds << static_cast<quint8>(0) << static_cast<quint8>(0) << static_cast<quint8>(0); // Reserved
    ds << static_cast<quint16>(nameUtf8.size());
    ds.writeRawData(nameUtf8.constData(), nameUtf8.size());
  };

  // Helper lambda to write a Raw Cel chunk (0x2005)
  auto writeCelChunk = [&](quint16 layerIdx, qint16 x, qint16 y, quint16 w, quint16 h, quint8 r, quint8 g, quint8 b, quint8 a) {
    quint32 chunkSize = 6 + 20 + w * h * 4;
    ds << static_cast<quint32>(chunkSize);
    ds << static_cast<quint16>(0x2005); // CHUNK_CEL
    ds << static_cast<quint16>(layerIdx);
    ds << static_cast<qint16>(x) << static_cast<qint16>(y);
    ds << static_cast<quint8>(255);     // Opacity
    ds << static_cast<quint16>(0);      // Cel type (0=raw)
    for (int i = 0; i < 7; ++i) ds << static_cast<quint8>(0);
    ds << static_cast<quint16>(w) << static_cast<quint16>(h);
    for (int i = 0; i < w * h; ++i) {
      ds << r << g << b << a;
    }
  };

  // Frame 0 Start
  int frame0Start = bin.size();
  ds << static_cast<quint32>(0);      // Frame size placeholder
  ds << static_cast<quint16>(0xF1FA); // Magic
  ds << static_cast<quint16>(6);      // 3 layers + 3 cels = 6 chunks
  ds << static_cast<quint16>(100);    // Duration ms
  ds << static_cast<quint8>(0) << static_cast<quint8>(0);
  ds << static_cast<quint32>(0);

  // Layers in Frame 0
  writeLayerChunk(1, QStringLiteral("Body"));             // Layer 0: Visible
  writeLayerChunk(1, QStringLiteral("Weapon: Sword"));    // Layer 1: Visible
  writeLayerChunk(0, QStringLiteral("Weapon: Flowers"));  // Layer 2: Hidden

  // Cels in Frame 0
  // Layer 0: Body (Blue 16x16)
  writeCelChunk(0, 0, 0, 16, 16, 0, 0, 255, 255);
  // Layer 1: Sword (Red 4x4 at x=6, y=6)
  writeCelChunk(1, 6, 6, 4, 4, 255, 0, 0, 255);
  // Layer 2: Flowers (Green 4x4 at x=6, y=6)
  writeCelChunk(2, 6, 6, 4, 4, 0, 255, 0, 255);

  quint32 frame0Size = bin.size() - frame0Start;
  memcpy(bin.data() + frame0Start, &frame0Size, sizeof(quint32));

  // Frame 1 Start
  int frame1Start = bin.size();
  ds << static_cast<quint32>(0);
  ds << static_cast<quint16>(0xF1FA);
  ds << static_cast<quint16>(4);      // 1 tag chunk + 3 cels = 4 chunks
  ds << static_cast<quint16>(100);
  ds << static_cast<quint8>(0) << static_cast<quint8>(0);
  ds << static_cast<quint32>(0);

  // Frame 1 - Tags chunk (0x2018)
  QByteArray tagName = "Slash";
  quint32 tagChunkSize = 6 + 10 + 19 + tagName.size();
  ds << static_cast<quint32>(tagChunkSize);
  ds << static_cast<quint16>(0x2018);
  ds << static_cast<quint16>(1);      // 1 tag
  for (int i = 0; i < 8; ++i) ds << static_cast<quint8>(0);
  ds << static_cast<quint16>(0) << static_cast<quint16>(1); // from 0 to 1
  ds << static_cast<quint8>(0);       // Loop forward
  ds << static_cast<quint16>(0);      // Repeat
  for (int i = 0; i < 6; ++i) ds << static_cast<quint8>(0);
  ds << static_cast<quint8>(0) << static_cast<quint8>(0) << static_cast<quint8>(0);
  ds << static_cast<quint8>(0);
  ds << static_cast<quint16>(tagName.size());
  ds.writeRawData(tagName.constData(), tagName.size());

  // Cels in Frame 1
  writeCelChunk(0, 0, 0, 16, 16, 0, 0, 255, 255);
  writeCelChunk(1, 6, 6, 4, 4, 255, 0, 0, 255);
  writeCelChunk(2, 6, 6, 4, 4, 0, 255, 0, 255);

  quint32 frame1Size = bin.size() - frame1Start;
  memcpy(bin.data() + frame1Start, &frame1Size, sizeof(quint32));

  // Patch total file size
  quint32 totalSize = bin.size();
  memcpy(bin.data(), &totalSize, sizeof(quint32));

  // Write temporary Aseprite file
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());
  QString asePath = tmpDir.filePath(QStringLiteral("hero_multi_layers.aseprite"));
  {
    QFile f(asePath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bin);
    f.close();
  }

  // 1. Read file with AsepriteExtractor
  AsepriteExtractor extractor;
  SpriteDocument doc;
  ExtractorError err;
  QVERIFY2(extractor.read(asePath, doc, &err), qPrintable(err.message));

  // Verify basic extraction
  QCOMPARE(doc.frameCount(), 2);
  QCOMPARE(doc.layerCount(), 3);
  QCOMPARE(doc.layer(0).name, QStringLiteral("Body"));
  QCOMPARE(doc.layer(1).name, QStringLiteral("Weapon: Sword"));
  QCOMPARE(doc.layer(2).name, QStringLiteral("Weapon: Flowers"));
  QVERIFY(doc.layer(0).visible);
  QVERIFY(doc.layer(1).visible);
  QVERIFY(!doc.layer(2).visible); // Layer 2 was hidden

  // Verify Cels presence
  QVERIFY(doc.hasFrameCels(0));
  QVERIFY(doc.hasFrameCels(1));
  QCOMPARE(doc.frameCels(0).size(), 3);
  QCOMPARE(doc.frameCels(1).size(), 3);

  // Verify automated Skin Profile generation from "Weapon: Sword" vs "Weapon: Flowers"
  QVERIFY(!doc.skinProfiles().isEmpty());
  qDebug() << "Detected skin profiles count:" << doc.skinProfiles().size();

  // Verify compositing: default composite (Body + Sword visible)
  // Pixel at (0,0) is Body (Blue), pixel at (7,7) is Sword (Red)
  QImage compDefault = doc.compositeFrame(0);
  QCOMPARE(compDefault.size(), QSize(16, 16));
  QCOMPARE(compDefault.pixelColor(0, 0), QColor(0, 0, 255, 255));
  QCOMPARE(compDefault.pixelColor(7, 7), QColor(255, 0, 0, 255));

  // Compositing with variant skin (Body + Flowers)
  QString bodyId = doc.layer(0).id;
  QString flowerId = doc.layer(2).id;
  QImage compFlowers = doc.compositeFrame(0, {bodyId, flowerId});
  QCOMPARE(compFlowers.pixelColor(0, 0), QColor(0, 0, 255, 255));
  QCOMPARE(compFlowers.pixelColor(7, 7), QColor(0, 255, 0, 255)); // Flowers are green!

  // 2. Test bakeAnimationVariants()
  int initialFrames = doc.frameCount();
  doc.bakeAnimationVariants();
  QVERIFY(doc.frameCount() > initialFrames);
  QVERIFY(doc.animationNames().contains(QStringLiteral("Slash_sword")) ||
          doc.animationNames().contains(QStringLiteral("Slash_flowers")));
  qDebug() << "Animations after baking variants:" << doc.animationNames();

  // 3. Test Bento Project serialization and round-trip
  QString projectSessionDir = tmpDir.filePath(QStringLiteral("bento_project"));
  QString projErr;
  QVERIFY(ProjectManager::saveProjectToSessionDir(doc, projectSessionDir, 1.0, QPointF(0, 0), &projErr));

  SpriteDocument loadedDoc;
  QVERIFY2(ProjectManager::loadProjectFromSessionDir(projectSessionDir, loadedDoc, nullptr, nullptr, &projErr),
           qPrintable(projErr));

  QCOMPARE(loadedDoc.frameCount(), doc.frameCount());
  QCOMPARE(loadedDoc.layerCount(), 3);
  QCOMPARE(loadedDoc.layer(0).name, QStringLiteral("Body"));
  QCOMPARE(loadedDoc.layer(1).name, QStringLiteral("Weapon: Sword"));
  QCOMPARE(loadedDoc.layer(2).name, QStringLiteral("Weapon: Flowers"));
  QVERIFY(loadedDoc.hasFrameCels(0));
  QCOMPARE(loadedDoc.frameCels(0).size(), 3);

  // 4. Test multi-layer Aseprite writing and re-reading
  QString exportedAse = tmpDir.filePath(QStringLiteral("exported_multi.aseprite"));
  ExportOptions opts;
  ExtractorError writeErr;
  QVERIFY2(extractor.write(exportedAse, doc, opts, &writeErr), qPrintable(writeErr.message));

  SpriteDocument reimportedDoc;
  ExtractorError reimportErr;
  QVERIFY2(extractor.read(exportedAse, reimportedDoc, &reimportErr), qPrintable(reimportErr.message));
  QCOMPARE(reimportedDoc.layerCount(), doc.layerCount());
  QCOMPARE(reimportedDoc.layer(0).name, QStringLiteral("Body"));
  QCOMPARE(reimportedDoc.layer(1).name, QStringLiteral("Weapon: Sword"));

  // 5. Test real-world fixtures from odin-aseprite test suite if present
  QString odinDir = QStringLiteral("C:/Users/ec135/Downloads/odin-aseprite-main/odin-aseprite-main/tests/asefile");
  if (QDir(odinDir).exists()) {
    QDir d(odinDir);
    QStringList files = d.entryList(QStringList() << QStringLiteral("*.ase") << QStringLiteral("*.aseprite"), QDir::Files);
    int passed = 0;
    for (const QString &fn : files) {
      QString fullP = d.filePath(fn);
      SpriteDocument realDoc;
      ExtractorError realErr;
      if (extractor.read(fullP, realDoc, &realErr)) {
        passed++;
      } else {
        qDebug() << "Odin fixture skipped/unsupported:" << fn << realErr.toString();
      }
    }
    qDebug() << "Odin Aseprite fixtures passed:" << passed << "out of" << files.size();
    QVERIFY(passed >= 30); // At least majority of complex fixtures pass
  }
}

void TestExtractors::testAsepriteExtractorWritePolygonClipping() {
  SpriteDocument doc;
  QList<QImage> frames;
  QList<SpriteBox> boxes;

  // Frame 0: 32x32 completely red (representing a frame on a polygonally packed atlas)
  QImage img0(32, 32, QImage::Format_ARGB32);
  img0.fill(QColor(255, 0, 0, 255));
  SpriteBox box0(QRect(0, 0, 32, 32));
  box0.index = 0;
  // Polygon 0: Upper-left triangle (0,0)-(32,0)-(0,32)
  box0.polygon << QPointF(0, 0) << QPointF(32, 0) << QPointF(0, 32);
  frames.append(img0);
  boxes.append(box0);

  // Frame 1: 32x32 completely blue
  QImage img1(32, 32, QImage::Format_ARGB32);
  img1.fill(QColor(0, 0, 255, 255));
  SpriteBox box1(QRect(0, 0, 32, 32));
  box1.index = 1;
  // Polygon 1: Lower-right triangle (32,32)-(32,0)-(0,32)
  box1.polygon << QPointF(32, 32) << QPointF(32, 0) << QPointF(0, 32);
  frames.append(img1);
  boxes.append(box1);

  doc.setFrames(frames, boxes);

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString aseFile = tempDir.filePath(QStringLiteral("polygon_clipped.ase"));

  AsepriteExtractor extractor;
  ExportOptions opts;
  ExtractorError writeErr;
  bool ok = extractor.write(aseFile, doc, opts, &writeErr);
  QVERIFY2(ok, qPrintable(writeErr.toString()));

  // Re-read with AsepriteExtractor
  SpriteDocument readDoc;
  ExtractorError readErr;
  bool readOk = extractor.read(aseFile, readDoc, &readErr);
  QVERIFY2(readOk, qPrintable(readErr.toString()));
  QCOMPARE(readDoc.frameCount(), 2);

  // Frame 0: Inside polygon (2, 2) should be Red; Outside polygon (30, 30) should be transparent (clipped)
  QImage f0 = readDoc.frame(0);
  QCOMPARE(f0.size(), QSize(32, 32));
  QRgb f0_inside = f0.pixel(2, 2);
  QVERIFY(qAlpha(f0_inside) > 200);
  QVERIFY(qRed(f0_inside) > 200);
  QRgb f0_outside = f0.pixel(30, 30);
  QCOMPARE(qAlpha(f0_outside), 0);

  // Frame 1: Inside polygon (30, 30) should be Blue; Outside polygon (2, 2) should be transparent (clipped)
  QImage f1 = readDoc.frame(1);
  QCOMPARE(f1.size(), QSize(32, 32));
  QRgb f1_inside = f1.pixel(30, 30);
  QVERIFY(qAlpha(f1_inside) > 200);
  QVERIFY(qBlue(f1_inside) > 200);
  QRgb f1_outside = f1.pixel(2, 2);
  QCOMPARE(qAlpha(f1_outside), 0);
}

void TestExtractors::testPlistExtractorReadWrite() {
  PlistExtractor extractor;
  QCOMPARE(extractor.id(), QStringLiteral("plist_extractor"));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanImport));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));

  // Geometry parsing helpers
  QCOMPARE(PlistExtractor::parseRect(QStringLiteral("{{10,20},{30,40}}")), QRect(10, 20, 30, 40));
  QCOMPARE(PlistExtractor::parsePoint(QStringLiteral("{5,-15}")), QPoint(5, -15));
  QCOMPARE(PlistExtractor::parseSize(QStringLiteral("{64,128}")), QSize(64, 128));

  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString plistPath = tempDir.filePath(QStringLiteral("sprites.plist"));

  // Build document with 2 frames
  QImage atlas(128, 64, QImage::Format_ARGB32);
  atlas.fill(Qt::transparent);
  {
    QPainter p(&atlas);
    p.fillRect(0, 0, 32, 32, Qt::red);
    p.fillRect(40, 0, 48, 48, Qt::blue);
  }

  SpriteDocument doc;
  doc.setAtlas(atlas);
  QList<QImage> frames;
  frames.append(atlas.copy(0, 0, 32, 32));
  frames.append(atlas.copy(40, 0, 48, 48));
  QList<SpriteBox> boxes;
  boxes.append(SpriteBox(QRect(0, 0, 32, 32)));
  boxes.append(SpriteBox(QRect(40, 0, 48, 48)));
  doc.setFrames(frames, boxes);

  ExportOptions expOpts;
  ExtractorError writeErr;
  bool ok = extractor.write(plistPath, doc, expOpts, &writeErr);
  QVERIFY2(ok, qPrintable(writeErr.toString()));
  QVERIFY(QFile::exists(plistPath));
  QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("sprites.png"))));

  // Verify canDecode
  QVERIFY(extractor.canDecode(plistPath));

  // Read back
  SpriteDocument readDoc;
  ExtractorError readErr;
  bool readOk = extractor.read(plistPath, readDoc, &readErr);
  QVERIFY2(readOk, qPrintable(readErr.toString()));
  QCOMPARE(readDoc.frameCount(), 2);
  QCOMPARE(readDoc.boxes().size(), 2);
  QCOMPARE(readDoc.box(0).rect, QRect(0, 0, 32, 32));
  QCOMPARE(readDoc.box(1).rect, QRect(40, 0, 48, 48));
}

void TestExtractors::testPcxExtractorReadWrite() {
  PcxExtractor extractor;
  QCOMPARE(extractor.id(), QStringLiteral("pcx_extractor"));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanImport));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));

  // Test encode and decode in-memory
  QImage testImg(16, 16, QImage::Format_ARGB32);
  testImg.fill(qRgb(255, 0, 0)); // Pure red
  for (int y = 4; y < 12; ++y) {
    for (int x = 4; x < 12; ++x) {
      testImg.setPixel(x, y, qRgb(0, 255, 0)); // Green center
    }
  }

  QByteArray pcxBytes;
  QString encErr;
  bool encOk = PcxExtractor::encodePcx(testImg, pcxBytes, &encErr);
  QVERIFY2(encOk, qPrintable(encErr));
  QVERIFY(pcxBytes.size() > 128 + 769);
  QCOMPARE(static_cast<uchar>(pcxBytes[0]), 0x0A); // Manufacturer ZSoft
  QCOMPARE(static_cast<uchar>(pcxBytes[pcxBytes.size() - 769]), 0x0C); // 256-color palette marker

  QImage decodedImg;
  QString decErr;
  bool decOk = PcxExtractor::decodePcx(pcxBytes, decodedImg, &decErr);
  QVERIFY2(decOk, qPrintable(decErr));
  QCOMPARE(decodedImg.size(), QSize(16, 16));
  // Check red corner and green center
  QCOMPARE(qRed(decodedImg.pixel(0, 0)), 255);
  QCOMPARE(qGreen(decodedImg.pixel(6, 6)), 255);

  // Test file roundtrip
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString pcxFile = tempDir.filePath(QStringLiteral("test.pcx"));

  SpriteDocument doc;
  doc.setAtlas(testImg);
  QList<QImage> frames = { testImg };
  QList<SpriteBox> boxes = { SpriteBox(QRect(0, 0, 16, 16)) };
  doc.setFrames(frames, boxes);

  ExportOptions expOpts;
  ExtractorError writeErr;
  bool writeOk = extractor.write(pcxFile, doc, expOpts, &writeErr);
  QVERIFY2(writeOk, qPrintable(writeErr.toString()));
  QVERIFY(QFile::exists(pcxFile));
  QVERIFY(extractor.canDecode(pcxFile));

  SpriteDocument readDoc;
  ExtractorError readErr;
  bool readDocOk = extractor.read(pcxFile, readDoc, &readErr);
  QVERIFY2(readDocOk, qPrintable(readErr.toString()));
  QCOMPARE(readDoc.atlas().size(), QSize(16, 16));
  QVERIFY(readDoc.frameCount() >= 1);
}

void TestExtractors::testPico8ExtractorReadWrite() {
  Pico8Extractor extractor;
  QCOMPARE(extractor.id(), QStringLiteral("pico8_extractor"));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanImport));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));

  // Palette checks
  const auto &pal = Pico8Extractor::pico8Palette();
  QCOMPARE(pal.size(), 16);
  QCOMPARE(qAlpha(pal[0]), 0); // Color 0 is transparent

  // Nearest color matching
  QCOMPARE(Pico8Extractor::findNearestPico8Color(qRgba(0, 0, 0, 0)), 0);
  QCOMPARE(Pico8Extractor::findNearestPico8Color(qRgb(255, 0, 77)), 8);   // Red
  QCOMPARE(Pico8Extractor::findNearestPico8Color(qRgb(255, 236, 39)), 10); // Yellow
  QCOMPARE(Pico8Extractor::findNearestPico8Color(qRgb(41, 173, 255)), 12); // Light Blue

  // GFX parsing & formatting
  QImage img(128, 128, QImage::Format_ARGB32);
  img.fill(Qt::transparent);
  // Put a red 8x8 square in sprite 0 (0,0 to 7,7)
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      img.setPixel(x, y, pal[8]); // Red
    }
  }

  QStringList gfx = Pico8Extractor::formatGfx(img);
  QCOMPARE(gfx.size(), 128);
  QVERIFY(gfx[0].startsWith(QStringLiteral("88888888")));

  QImage parsedImg;
  QVERIFY(Pico8Extractor::parseGfx(gfx, parsedImg));
  QCOMPARE(parsedImg.size(), QSize(128, 128));
  QCOMPARE(parsedImg.pixel(0, 0), pal[8]);

  // GFF flags roundtrip
  QVector<quint8> flags(256, 0);
  flags[0] = 0x05; // Bits 0 and 2
  flags[15] = 0xFF;
  QStringList gffLines = Pico8Extractor::formatGff(flags);
  QVector<quint8> parsedFlags = Pico8Extractor::parseGff(gffLines);
  QCOMPARE(parsedFlags[0], static_cast<quint8>(0x05));
  QCOMPARE(parsedFlags[15], static_cast<quint8>(0xFF));

  // File roundtrip
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString p8Path = tempDir.filePath(QStringLiteral("cart.p8"));

  SpriteDocument doc;
  doc.setAtlas(img);
  QList<QImage> frames = { img.copy(0, 0, 8, 8) };
  QList<SpriteBox> boxes = { SpriteBox(QRect(0, 0, 8, 8)) };
  doc.setFrames(frames, boxes);

  ExportOptions expOpts;
  ExtractorError writeErr;
  bool writeOk = extractor.write(p8Path, doc, expOpts, &writeErr);
  QVERIFY2(writeOk, qPrintable(writeErr.toString()));
  QVERIFY(QFile::exists(p8Path));
  QVERIFY(extractor.canDecode(p8Path));

  SpriteDocument readDoc;
  ExtractorError readErr;
  bool readOk = extractor.read(p8Path, readDoc, &readErr);
  QVERIFY2(readOk, qPrintable(readErr.toString()));
  QCOMPARE(readDoc.atlas().size(), QSize(128, 128));
  QVERIFY(readDoc.frameCount() >= 1);
  QCOMPARE(readDoc.box(0).rect, QRect(0, 0, 8, 8));
}

void TestExtractors::testIffExtractorReadWrite() {
  IffExtractor extractor;
  QCOMPARE(extractor.id(), QStringLiteral("iff_extractor"));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanImport));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));
  QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("iff")));
  QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("lbm")));

  // 1. ByteRun1 (PackBits) RLE roundtrip test
  QByteArray rawTest(256, 0);
  for (int i = 0; i < 50; ++i) rawTest[i] = static_cast<char>(0xAA); // Repeat run
  for (int i = 50; i < 100; ++i) rawTest[i] = static_cast<char>(i);  // Literal run
  for (int i = 100; i < 256; ++i) rawTest[i] = static_cast<char>(0x55); // Repeat run

  QByteArray comp = IffExtractor::byteRun1Compress(reinterpret_cast<const uchar*>(rawTest.constData()), rawTest.size());
  QVERIFY(comp.size() < rawTest.size());

  QByteArray decomp(rawTest.size(), 0);
  bool decompOk = IffExtractor::byteRun1Decompress(reinterpret_cast<const uchar*>(comp.constData()), comp.size(),
                                                  reinterpret_cast<uchar*>(decomp.data()), decomp.size());
  QVERIFY(decompOk);
  QCOMPARE(decomp, rawTest);

  // 2. In-memory ILBM encode & decode roundtrip test
  QImage testImg(32, 32, QImage::Format_ARGB32);
  testImg.fill(qRgb(20, 30, 40));
  for (int y = 8; y < 24; ++y) {
    for (int x = 8; x < 24; ++x) {
      testImg.setPixel(x, y, qRgb(240, 120, 60));
    }
  }

  QByteArray iffBytes;
  QString encErr;
  bool encOk = IffExtractor::encodeIff(testImg, iffBytes, &encErr);
  QVERIFY2(encOk, qPrintable(encErr));
  QVERIFY(iffBytes.size() > 64);
  QVERIFY(iffBytes.startsWith("FORM"));
  QCOMPARE(iffBytes.mid(8, 4), QByteArray("ILBM"));

  QImage decodedImg;
  QString decErr;
  bool decOk = IffExtractor::decodeIff(iffBytes, decodedImg, &decErr);
  QVERIFY2(decOk, qPrintable(decErr));
  QCOMPARE(decodedImg.size(), QSize(32, 32));
  // Verify colors at corners and center
  QRgb corner = decodedImg.pixel(0, 0);
  QRgb center = decodedImg.pixel(16, 16);
  QVERIFY(qRed(corner) <= 60 && qBlue(corner) >= 20);
  QVERIFY(qRed(center) > 200 && qGreen(center) > 80);

  // 3. EHB (Extra Half-Brite) mode test
  // Craft a 6-plane ILBM with CAMG EHB bit set:
  // 16x16 pixels, rowBytes = 2, 6 planes -> 12 bytes per scanline.
  // We'll set pixel (0,0) to color 1 (200, 100, 50), and pixel (1,0) to color 33 (1 + 32)
  QByteArray ehbData;
  ehbData.append("FORM", 4);
  // Placeholder for length
  int ehbLenPos = ehbData.size();
  ehbData.append("\0\0\0\0", 4);
  ehbData.append("ILBM", 4);

  // BMHD
  ehbData.append("BMHD", 4);
  quint32 bmhdSize = 20;
  char bmhdLen[4];
  qToBigEndian<quint32>(bmhdSize, reinterpret_cast<uchar*>(bmhdLen));
  ehbData.append(bmhdLen, 4);
  QByteArray bmhdPayload(20, 0);
  qToBigEndian<quint16>(16, reinterpret_cast<uchar*>(bmhdPayload.data()));     // w
  qToBigEndian<quint16>(16, reinterpret_cast<uchar*>(bmhdPayload.data() + 2)); // h
  bmhdPayload.data()[8] = 6; // nPlanes = 6 (EHB)
  bmhdPayload.data()[9] = 0; // masking = none
  bmhdPayload.data()[10] = 0; // compression = none
  ehbData.append(bmhdPayload);

  // CMAP (32 colors)
  ehbData.append("CMAP", 4);
  quint32 cmapSize = 32 * 3;
  char cmapLen[4];
  qToBigEndian<quint32>(cmapSize, reinterpret_cast<uchar*>(cmapLen));
  ehbData.append(cmapLen, 4);
  QByteArray cmapPayload(96, 0);
  // Color 1 = (200, 100, 50)
  cmapPayload.data()[3] = static_cast<char>(200);
  cmapPayload.data()[4] = static_cast<char>(100);
  cmapPayload.data()[5] = static_cast<char>(50);
  ehbData.append(cmapPayload);

  // CAMG (Amiga ViewPort mode with EHB bit 0x0080 set)
  ehbData.append("CAMG", 4);
  char camgLen[4];
  qToBigEndian<quint32>(4, reinterpret_cast<uchar*>(camgLen));
  ehbData.append(camgLen, 4);
  char camgPayload[4];
  qToBigEndian<quint32>(0x0080, reinterpret_cast<uchar*>(camgPayload));
  ehbData.append(camgPayload, 4);

  // BODY (16 lines * 12 bytes = 192 bytes)
  ehbData.append("BODY", 4);
  quint32 bodySize = 16 * 12;
  char bodyLen[4];
  qToBigEndian<quint32>(bodySize, reinterpret_cast<uchar*>(bodyLen));
  ehbData.append(bodyLen, 4);
  QByteArray bodyPayload(bodySize, 0);
  // On line 0:
  // Pixel (0,0): color 1 -> bit 0 of plane 0 is 1. Byte 0 of plane 0: 0x80.
  bodyPayload.data()[0 * 2 + 0] = static_cast<char>(0x80);
  // Pixel (1,0): color 33 (1 + 32) -> bit 1 of plane 0 is 1 (0x40), and bit 1 of plane 5 is 1 (0x40)!
  bodyPayload.data()[0 * 2 + 0] |= static_cast<char>(0x40);
  bodyPayload.data()[5 * 2 + 0] = static_cast<char>(0x40);
  ehbData.append(bodyPayload);

  // Update FORM length
  quint32 totalForm = ehbData.size() - 8;
  qToBigEndian<quint32>(totalForm, reinterpret_cast<uchar*>(ehbData.data() + ehbLenPos));

  QImage ehbImg;
  QString ehbErr;
  bool ehbOk = IffExtractor::decodeIff(ehbData, ehbImg, &ehbErr);
  QVERIFY2(ehbOk, qPrintable(ehbErr));
  QCOMPARE(ehbImg.size(), QSize(16, 16));
  // Pixel (0,0) must be base color (200, 100, 50)
  QCOMPARE(ehbImg.pixel(0, 0), qRgb(200, 100, 50));
  // Pixel (1,0) must be EHB half-brite: (100, 50, 25)
  QCOMPARE(ehbImg.pixel(1, 0), qRgb(100, 50, 25));

  // 4. File roundtrip test
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString iffPath = tempDir.filePath(QStringLiteral("deluxepaint.iff"));

  SpriteDocument doc;
  doc.setAtlas(testImg);
  QList<QImage> frames = { testImg.copy(0, 0, 16, 16) };
  QList<SpriteBox> boxes = { SpriteBox(QRect(0, 0, 16, 16)) };
  doc.setFrames(frames, boxes);

  ExportOptions expOpts;
  ExtractorError writeErr;
  bool writeOk = extractor.write(iffPath, doc, expOpts, &writeErr);
  QVERIFY2(writeOk, qPrintable(writeErr.toString()));
  QVERIFY(QFile::exists(iffPath));
  QVERIFY(extractor.canDecode(iffPath));

  SpriteDocument readDoc;
  ExtractorError readErr;
  bool readDocOk = extractor.read(iffPath, readDoc, &readErr);
  QVERIFY2(readDocOk, qPrintable(readErr.toString()));
  QCOMPARE(readDoc.atlas().size(), QSize(32, 32));
  QVERIFY(readDoc.frameCount() >= 1);
}

void TestExtractors::testRetroTileExtractorReadWrite() {
  RetroTileExtractor extractor;
  QCOMPARE(extractor.id(), QStringLiteral("retro_tile_extractor"));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanImport));
  QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));
  QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("nes")));
  QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("gb")));
  QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("sms")));
  QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("md")));
  QVERIFY(extractor.supportedExtensions().contains(QStringLiteral("tim")));

  QVector<QRgb> pal4 = {
    qRgba(0, 0, 0, 0),        // Color 0 transparent
    qRgb(100, 100, 100),      // Color 1
    qRgb(180, 180, 180),      // Color 2
    qRgb(255, 255, 255)       // Color 3
  };

  // 1. NES 2bpp Planar Tile test
  QByteArray nesTile(16, 0);
  nesTile[0] = static_cast<char>(0x80); // Row 0 bit 0
  nesTile[8] = static_cast<char>(0x80); // Row 0 bit 1 -> Color 3 at (0,0)
  QImage nesSheet = RetroTileDecoder::decodeTileSheet(
    reinterpret_cast<const uchar*>(nesTile.constData()), 16,
    RetroFormat::Nes2bpp, 1, 1, pal4, true
  );
  QCOMPARE(nesSheet.size(), QSize(8, 8));
  QCOMPARE(nesSheet.pixel(0, 0), pal4[3]);
  QCOMPARE(qAlpha(nesSheet.pixel(1, 0)), 0);

  // 2. Game Boy 2bpp Interleaved Tile test
  QByteArray gbTile(16, 0);
  gbTile[0] = static_cast<char>(0x80); // Row 0 plane 0
  gbTile[1] = static_cast<char>(0x80); // Row 0 plane 1 -> Color 3 at (0,0)
  QImage gbSheet = RetroTileDecoder::decodeTileSheet(
    reinterpret_cast<const uchar*>(gbTile.constData()), 16,
    RetroFormat::GameBoy2bpp, 1, 1, pal4, true
  );
  QCOMPARE(gbSheet.size(), QSize(8, 8));
  QCOMPARE(gbSheet.pixel(0, 0), pal4[3]);

  // 3. Sega Genesis 4bpp Chunky Tile test
  QByteArray mdTile(32, 0);
  mdTile[0] = static_cast<char>(0x12); // Pixel (0,0)=1, Pixel (1,0)=2
  QVector<QRgb> pal16(16, qRgb(0, 0, 0));
  pal16[1] = qRgb(255, 0, 0);
  pal16[2] = qRgb(0, 255, 0);
  QImage mdSheet = RetroTileDecoder::decodeTileSheet(
    reinterpret_cast<const uchar*>(mdTile.constData()), 32,
    RetroFormat::Genesis4bpp, 1, 1, pal16, true
  );
  QCOMPARE(mdSheet.pixel(0, 0), pal16[1]);
  QCOMPARE(mdSheet.pixel(1, 0), pal16[2]);

  // 4. SNES 4bpp Planar Tile test
  QByteArray snesTile(32, 0);
  // Color 15 = all 4 planes high: b0=1, b1=1, b2=1, b3=1
  snesTile[0] = static_cast<char>(0x80);
  snesTile[1] = static_cast<char>(0x80);
  snesTile[16] = static_cast<char>(0x80);
  snesTile[17] = static_cast<char>(0x80);
  pal16[15] = qRgb(255, 255, 0);
  QImage snesSheet = RetroTileDecoder::decodeTileSheet(
    reinterpret_cast<const uchar*>(snesTile.constData()), 32,
    RetroFormat::Snes4bpp, 1, 1, pal16, true
  );
  QCOMPARE(snesSheet.pixel(0, 0), pal16[15]);

  // 5. PSX TIM 4bpp with CLUT test
  QByteArray timData;
  timData.append("\x10\x00\x00\x00", 4); // Magic 0x10
  timData.append("\x08\x00\x00\x00", 4); // Flags: 4bpp (0), hasClut (8)

  // CLUT block: len 44, x 0, y 0, w 16, h 1, 16 BGR555 words
  timData.append("\x2c\x00\x00\x00", 4); // len = 44
  timData.append("\x00\x00\x00\x00", 4); // x, y
  timData.append("\x10\x00\x01\x00", 4); // w=16, h=1
  // 16 words: word 0 = transparent (0x0000), word 1 = Red (0x001F)
  timData.append("\x00\x00", 2);
  timData.append("\x1f\x00", 2);
  for (int i = 2; i < 16; ++i) timData.append("\x00\x00", 2);

  // Pixel data block: 16x4 pixels (4 words wide * 4 lines = 16 words = 32 bytes + 12 header = 44 bytes)
  timData.append("\x2c\x00\x00\x00", 4); // len = 44
  timData.append("\x00\x00\x00\x00", 4); // x, y
  timData.append("\x04\x00\x04\x00", 4); // w=4 words (16 pixels), h=4
  // Row 0: Pixel 0 has index 1 (Red), Pixel 1 has index 0 (transp)
  timData.append("\x01", 1);
  timData.append(QByteArray(31, 0));

  QImage timImg;
  QString timErr;
  bool timOk = RetroTileDecoder::decodePsxTim(timData, timImg, &timErr);
  QVERIFY2(timOk, qPrintable(timErr));
  QCOMPARE(timImg.size(), QSize(16, 4));
  QCOMPARE(timImg.pixel(0, 0), qRgb(248, 0, 0)); // BGR555 0x001F scaled to 8-bit
  QCOMPARE(qAlpha(timImg.pixel(1, 0)), 0);

  // 6. iNES ROM probe & auto-slicing test
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  QString nesPath = tempDir.filePath(QStringLiteral("mario.nes"));

  QByteArray nesRom;
  // 16-byte iNES header
  nesRom.append("NES\x1a", 4);
  nesRom.append(static_cast<char>(1)); // 1 PRG bank (16KB)
  nesRom.append(static_cast<char>(1)); // 1 CHR bank (8KB)
  nesRom.append(QByteArray(10, 0));   // Flags / padding

  // 16KB PRG dummy data
  nesRom.append(QByteArray(16384, static_cast<char>(0xEA))); // NOPs

  // 8KB CHR tile data (put our NES tile at the beginning of CHR-ROM)
  nesRom.append(nesTile);
  nesRom.append(QByteArray(8192 - 16, 0));

  QFile nesFile(nesPath);
  QVERIFY(nesFile.open(QIODevice::WriteOnly));
  nesFile.write(nesRom);
  nesFile.close();

  // Verify canDecode and automatic probe
  QVERIFY(extractor.canDecode(nesPath));

  SpriteDocument nesDoc;
  ExtractorError readErr;
  bool readOk = extractor.read(nesPath, nesDoc, &readErr);
  QVERIFY2(readOk, qPrintable(readErr.toString()));
  QCOMPARE(nesDoc.atlas().width(), 128); // 16 tiles * 8 pixels
  QVERIFY(nesDoc.frameCount() >= 1);
  QVERIFY(!nesDoc.boxes().isEmpty());

  // 7. Homebrew CHR export test
  QString chrOutPath = tempDir.filePath(QStringLiteral("sprites.chr"));
  ExportOptions expOpts;
  ExtractorError writeErr;
  bool writeOk = extractor.write(chrOutPath, nesDoc, expOpts, &writeErr);
  QVERIFY2(writeOk, qPrintable(writeErr.toString()));
  QVERIFY(QFile::exists(chrOutPath));
  QVERIFY(QFileInfo(chrOutPath).size() % 16 == 0); // Must be multiple of 16-byte tiles

  // 8. SNES ROM Aladdin (F).smc auto-probe and extraction test
  QString aladdinPath = QStringLiteral("sample/Aladdin (F).smc");
  if (QFile::exists(aladdinPath)) {
    SpriteDocument aladdinDoc;
    ExtractorError aladdinErr;
    bool aladdinOk = extractor.read(aladdinPath, aladdinDoc, &aladdinErr);
    QVERIFY2(aladdinOk, qPrintable(aladdinErr.toString()));
    QVERIFY(!aladdinDoc.atlas().isNull());
    QVERIFY(aladdinDoc.atlas().width() > 0);
    QVERIFY(aladdinDoc.frameCount() >= 10);
    qDebug() << "Aladdin (F).smc auto-extracted:"
             << aladdinDoc.frameCount() << "sprites,"
             << "atlas:" << aladdinDoc.atlas().size();

    // 9. Pattern-aware decoding & palette detection unit tests
    QFile aladdinFile(aladdinPath);
    if (aladdinFile.open(QIODevice::ReadOnly)) {
      QByteArray rom = aladdinFile.readAll();
      aladdinFile.close();

      // Scan ROM palettes
      QList<DetectedPalette> palettes = RomProbe::scanPalettes(rom, RetroFormat::Snes4bpp, 32);
      QVERIFY(!palettes.isEmpty());
      QVERIFY(palettes.first().colors.size() == 16);

      // Verify BGR555 conversion
      quint16 redWord = 0x001F; // Full red
      QRgb redRgb = RetroTileDecoder::snesBgr555ToRgb(redWord, false);
      QCOMPARE(qRed(redRgb), 255);
      QCOMPARE(qGreen(redRgb), 0);
      QCOMPARE(qBlue(redRgb), 0);

      // Verify TilePattern decoding on 4 tiles (16x16 sprite)
      const uchar *raw = reinterpret_cast<const uchar*>(rom.constData());
      QImage sheetLinear = RetroTileDecoder::decodeTileSheet(
          raw + 0x4F800, 32 * 4, RetroFormat::Snes4bpp, 16, 4,
          palettes.first().colors, true, TilePattern::Linear);
      QCOMPARE(sheetLinear.size(), QSize(128, 8)); // 16 tiles wide * 8 px

      QImage sheet2x2 = RetroTileDecoder::decodeTileSheet(
          raw + 0x4F800, 32 * 4, RetroFormat::Snes4bpp, 16, 4,
          palettes.first().colors, true, TilePattern::Sprite16x16_2x2);
      QCOMPARE(sheet2x2.size(), QSize(128, 16)); // 2x2 sprite: 16px high!
    }
  }

  // 10. SNES 3bpp format decoding test
  QByteArray snes3bppTile(24, 0);
  // Plane 0 line 0: 0xFF, Plane 1 line 0: 0x00, Plane 2 line 0: 0xFF -> color = 1 | 4 = 5
  snes3bppTile.data()[0] = static_cast<char>(0xFF);
  snes3bppTile.data()[16] = static_cast<char>(0xFF);
  QVector<QRgb> pal3bpp = RetroTileDecoder::defaultPreviewPalette(RetroFormat::Snes3bpp);
  QRgb dest3bpp[64] = {0};
  RetroTileDecoder::decodeTile8x8(reinterpret_cast<const uchar*>(snes3bppTile.constData()),
                                 RetroFormat::Snes3bpp, dest3bpp, 8, pal3bpp, false);
  QCOMPARE(dest3bpp[0], pal3bpp[5]);
}

int main(int argc, char *argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);
  TestExtractors tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_extractors.moc"
