/**
 * Copyright (c) 2026 Vincent LECOQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include "snessavestateloader.h"
#include "snespreset.h"
#include "snesromanalyzer.h"
#include "image/spritedetector.h"
#include "snes_extractor.h"

class TestSnesExtractor : public QObject
{
    Q_OBJECT

private slots:
    void testAddressCalculations();
    void testBgr555ColorConversion();
    void testRawCgramDump();
    void testRawVramDump();
    void testSnes9xSnapshotParsing();
    void testBuiltInPresets();
    void testPresetSerialization();
    void testExtractorCanDecode();
    void testExtractorReadWriteRoundtrip();
    void testPaletteScanner();
    void testEntropyAnd4bppConfidence();
    void testDecompressors();
    void testSpriteDetectorConstraints();
};

void TestSnesExtractor::testAddressCalculations()
{
    // Copier header check (fileSize % 1024 == 512)
    QVERIFY(!SnesAddress::hasCopierHeader(1024 * 1024)); // Clean 1MB ROM
    QVERIFY(SnesAddress::hasCopierHeader(1024 * 1024 + 512)); // Headered 1MB ROM
    QVERIFY(!SnesAddress::hasCopierHeader(512)); // Small file under limit

    // LoROM translation: Bank $00:$8000 -> PC 0
    QCOMPARE(SnesAddress::snesToPcLoRom(0x00, 0x8000, false), 0);
    // LoROM Bank $01:$8000 -> PC 0x8000
    QCOMPARE(SnesAddress::snesToPcLoRom(0x01, 0x8000, false), 0x8000);
    // With 512-byte header
    QCOMPARE(SnesAddress::snesToPcLoRom(0x01, 0x8000, true), 0x8000 + 512);

    // HiROM translation: Bank $C0:$0000 -> PC 0
    QCOMPARE(SnesAddress::snesToPcHiRom(0xC0, 0x0000, false), 0);
    // HiROM Bank $C1:$0000 -> PC 0x10000
    QCOMPARE(SnesAddress::snesToPcHiRom(0xC1, 0x0000, false), 0x10000);
    // With 512-byte header
    QCOMPARE(SnesAddress::snesToPcHiRom(0xC1, 0x0000, true), 0x10000 + 512);
}

void TestSnesExtractor::testBgr555ColorConversion()
{
    // Black: 0x0000
    QRgb black = SnesSavestateData::bgr555ToRgb(0x0000);
    QCOMPARE(qRed(black), 0);
    QCOMPARE(qGreen(black), 0);
    QCOMPARE(qBlue(black), 0);

    // White: 0x7FFF
    QRgb white = SnesSavestateData::bgr555ToRgb(0x7FFF);
    QCOMPARE(qRed(white), 255);
    QCOMPARE(qGreen(white), 255);
    QCOMPARE(qBlue(white), 255);

    // Pure Red: 0x001F (5 bits red)
    QRgb red = SnesSavestateData::bgr555ToRgb(0x001F);
    QCOMPARE(qRed(red), 255);
    QCOMPARE(qGreen(red), 0);
    QCOMPARE(qBlue(red), 0);

    // Pure Green: 0x03E0 (5 bits green)
    QRgb green = SnesSavestateData::bgr555ToRgb(0x03E0);
    QCOMPARE(qRed(green), 0);
    QCOMPARE(qGreen(green), 255);
    QCOMPARE(qBlue(green), 0);

    // Pure Blue: 0x7C00 (5 bits blue)
    QRgb blue = SnesSavestateData::bgr555ToRgb(0x7C00);
    QCOMPARE(qRed(blue), 0);
    QCOMPARE(qGreen(blue), 0);
    QCOMPARE(qBlue(blue), 255);
}

void TestSnesExtractor::testRawCgramDump()
{
    // Create synthetic 512-byte CGRAM dump (16 palettes of 16 colors)
    QByteArray cgram(512, 0);
    // Fill palette 8 (OBJ 0) with distinct colors
    uchar *p = reinterpret_cast<uchar*>(cgram.data()) + (8 * 32);
    // Color 0: black (transparent)
    qToLittleEndian<quint16>(0x0000, p);
    // Color 1: bright red
    qToLittleEndian<quint16>(0x001F, p + 2);
    // Color 2: bright green
    qToLittleEndian<quint16>(0x03E0, p + 4);

    SnesSavestateData data = SnesSavestateLoader::loadFromData(cgram, QStringLiteral("test.cgram"));
    QVERIFY(data.isValid);
    QCOMPARE(data.cgram.size(), 512);

    QVector<QRgb> pal8 = data.getPalette(8, true);
    QCOMPARE(pal8.size(), 16);
    QCOMPARE(qAlpha(pal8[0]), 0); // Transparent color 0
    QCOMPARE(qRed(pal8[1]), 255);
    QCOMPARE(qGreen(pal8[1]), 0);
    QCOMPARE(qGreen(pal8[2]), 255);
}

void TestSnesExtractor::testRawVramDump()
{
    QByteArray vram(65536, 0x55);
    SnesSavestateData data = SnesSavestateLoader::loadFromData(vram, QStringLiteral("test.vram"));
    QVERIFY(data.isValid);
    QCOMPARE(data.vram.size(), 65536);
}

void TestSnesExtractor::testSnes9xSnapshotParsing()
{
    // Build mock Snes9x snapshot with tagged chunks
    QByteArray snap;
    snap.append("#!s9x:0002\n", 11);

    // Add VRAM block: Tag 'V','R','A','M' + length (65536 big endian) + 65536 bytes
    snap.append("VRAM");
    quint32 vramLen = 65536;
    snap.append(reinterpret_cast<const char*>(&vramLen), 4);
    // Ensure length was formatted or write directly
    snap.append(QByteArray(65536, 0x2A));

    // Add CRA (CGRAM) block: Tag 'C','R','A',' ' + 512 bytes
    snap.append("CRA ");
    snap.append(QByteArray(512, 0x1F));

    SnesSavestateData data = SnesSavestateLoader::loadFromData(snap, QStringLiteral("game.000"));
    QVERIFY(data.isValid);
    QCOMPARE(data.vram.size(), 65536);
    QCOMPARE(data.vram.at(0), static_cast<char>(0x2A));
    QCOMPARE(data.cgram.size(), 512);
}

void TestSnesExtractor::testBuiltInPresets()
{
    QList<SnesPreset> presets = SnesPresetManager::builtInPresets();
    QVERIFY(presets.size() >= 7);

    // Verify Zelda Link preset
    bool foundZelda = false;
    for (const auto &p : presets) {
        if (p.gameName.contains(QStringLiteral("Zelda"), Qt::CaseInsensitive)) {
            foundZelda = true;
            QCOMPARE(p.tileOffset, 0x080000);
            QCOMPARE(p.tileLength, 28672);
            QCOMPARE(p.bpp, 4);
            QCOMPARE(p.tilesPerRow, 16);
            QCOMPARE(p.paletteOffset, 0x0DD308);
            break;
        }
    }
    QVERIFY(foundZelda);
}

void TestSnesExtractor::testPresetSerialization()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString jsonPath = tempDir.filePath(QStringLiteral("custom_preset.json"));

    SnesPreset p;
    p.title = QStringLiteral("Boss Sprites");
    p.gameName = QStringLiteral("Secret Project");
    p.category = QStringLiteral("Enemies");
    p.tileOffset = 0x120000;
    p.tileLength = 65536;
    p.bpp = 4;
    p.pattern = TilePattern::Sprite16x16_VramStride16;
    p.tilesPerRow = 16;
    p.paletteOffset = 0x12FF00;
    p.defaultPaletteIndex = 9;
    p.description = QStringLiteral("Large 32x32 boss sprite frames");

    QVERIFY(SnesPresetManager::saveToFile(jsonPath, p));

    SnesPreset loaded = SnesPresetManager::loadFromFile(jsonPath);
    QVERIFY(loaded.isValid());
    QCOMPARE(loaded.title, p.title);
    QCOMPARE(loaded.gameName, p.gameName);
    QCOMPARE(loaded.tileOffset, p.tileOffset);
    QCOMPARE(loaded.tileLength, p.tileLength);
    QCOMPARE(loaded.pattern, TilePattern::Sprite16x16_VramStride16);
    QCOMPARE(loaded.paletteOffset, p.paletteOffset);
}

void TestSnesExtractor::testExtractorCanDecode()
{
    SnesExtractor extractor;
    QVERIFY(extractor.canDecode(QStringLiteral("game.sfc")));
    QVERIFY(extractor.canDecode(QStringLiteral("game.smc")));
    QVERIFY(extractor.canDecode(QStringLiteral("state.000")));
    QVERIFY(extractor.canDecode(QStringLiteral("state.s96")));
    QVERIFY(extractor.canDecode(QStringLiteral("state.zst")));
    QVERIFY(extractor.canDecode(QStringLiteral("preset.stk")));
    QVERIFY(!extractor.canDecode(QStringLiteral("document.png")));
}

void TestSnesExtractor::testExtractorReadWriteRoundtrip()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // 1. Create a 16x16 test image with 4 distinct colored 8x8 tiles
    QImage testAtlas(16, 16, QImage::Format_ARGB32);
    testAtlas.fill(Qt::transparent);

    // Draw non-empty pixels
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            testAtlas.setPixel(x, y, qRgb(255, 0, 0)); // Red in tile 0
            testAtlas.setPixel(x + 8, y, qRgb(0, 255, 0)); // Green in tile 1
            testAtlas.setPixel(x, y + 8, qRgb(0, 0, 255)); // Blue in tile 2
            testAtlas.setPixel(x + 8, y + 8, qRgb(255, 255, 0)); // Yellow in tile 3
        }
    }

    SpriteDocument doc;
    doc.setAtlas(testAtlas);
    SpriteBox box(QRect(0, 0, 16, 16));
    doc.setFrames({testAtlas}, {box});

    // 2. Export to SNES 4bpp binary file
    QString snesFile = tempDir.filePath(QStringLiteral("export_test.sfc"));
    SnesExtractor extractor;
    ExportOptions opt;
    ExtractorError err;
    QVERIFY(extractor.write(snesFile, doc, opt, &err));

    // Verify file exists and has exactly 4 tiles * 32 bytes = 128 bytes
    QFile f(snesFile);
    QVERIFY(f.exists());
    QCOMPARE(f.size(), 128);

    // 3. Read back in headless mode
    SpriteDocument importedDoc;
    QVERIFY(extractor.read(snesFile, importedDoc, &err));
    QVERIFY(!importedDoc.atlas().isNull());
    QCOMPARE(importedDoc.frameCount(), 1);
}

void TestSnesExtractor::testPaletteScanner()
{
    // Build a mock ROM containing a valid SNES BGR555 palette ramp
    QByteArray mockRom(256, 0);

    // Write a 32-byte palette at offset 64 with a ramp
    // Color 0 = 0x0000 (Black / Transparent backdrop)
    // Colors 1..15 = rising shades of red/orange (R 2..30, G 1..15, B 0)
    for (int i = 0; i < 16; ++i) {
        quint16 bgr555 = 0;
        if (i > 0) {
            int r = std::min(31, i * 2);
            int g = std::min(31, i);
            int b = 0;
            bgr555 = r | (g << 5) | (b << 10);
        }
        mockRom[64 + i * 2] = static_cast<char>(bgr555 & 0xFF);
        mockRom[64 + i * 2 + 1] = static_cast<char>((bgr555 >> 8) & 0xFF);
    }

    QList<SnesScoredPalette> palettes = SnesPaletteScanner::scanRomPalettes(mockRom, 16, 0.30);
    QVERIFY(!palettes.isEmpty());
    bool foundOffset64 = false;
    for (const auto &p : palettes) {
        if (p.offset == 64) {
            foundOffset64 = true;
            QVERIFY(p.score > 0.40);
            QCOMPARE(p.colors.size(), 16);
            break;
        }
    }
    QVERIFY(foundOffset64);

    // Verify harmonic presets exist
    QStringList names = SnesPaletteScanner::harmonicPresetNames();
    QVERIFY(names.size() >= 8);
    QVector<QRgb> preset0 = SnesPaletteScanner::getHarmonicPresetPalette(0);
    QCOMPARE(preset0.size(), 16);
}

void TestSnesExtractor::testEntropyAnd4bppConfidence()
{
    // Uniform data has 0 entropy
    QByteArray uniformData(1024, 0x55);
    double hZero = SnesRomAnalyzer::calculateShannonEntropy(reinterpret_cast<const uchar*>(uniformData.constData()), uniformData.size());
    QCOMPARE(hZero, 0.0);

    // High variety data has higher entropy
    QByteArray rampData(256, 0);
    for (int i = 0; i < 256; ++i) rampData[i] = static_cast<char>(i);
    double hRamp = SnesRomAnalyzer::calculateShannonEntropy(reinterpret_cast<const uchar*>(rampData.constData()), rampData.size());
    QVERIFY(hRamp > 7.9);

    // 4bpp confidence evaluation
    double confUniform = SnesRomAnalyzer::evaluate4bppConfidence(reinterpret_cast<const uchar*>(uniformData.constData()), uniformData.size());
    QVERIFY(confUniform < 0.2);
}

void TestSnesExtractor::testDecompressors()
{
    // Test RLE decompressor
    // Format: flag byte (0x00..0x7F = literal (N+1) bytes; 0x80..0xFF = repeat byte ((flag & 0x7F) + 1) times)
    QByteArray rleSrc;
    rleSrc.append(static_cast<char>(0x02)); // Literal 3 bytes
    rleSrc.append('A');
    rleSrc.append('B');
    rleSrc.append('C');
    rleSrc.append(static_cast<char>(0x84)); // Repeat 5 times
    rleSrc.append('X');

    QByteArray rleOut;
    QVERIFY(SnesRomAnalyzer::decompressRle(reinterpret_cast<const uchar*>(rleSrc.constData()), rleSrc.size(), rleOut, 8));
    QCOMPARE(rleOut.size(), 8);
    QCOMPARE(rleOut.left(3), QByteArray("ABC"));
    QCOMPARE(rleOut.mid(3, 5), QByteArray("XXXXX"));
}

void TestSnesExtractor::testSpriteDetectorConstraints()
{
    // Create an image with two 16x16 squares that are placed side by side (touching or overlapping)
    QImage testImg(64, 64, QImage::Format_ARGB32);
    testImg.fill(Qt::transparent);

    // Draw sprite A: (8, 8) to (23, 23) (16x16)
    for (int y = 8; y < 24; ++y) {
        for (int x = 8; x < 24; ++x) {
            testImg.setPixel(x, y, qRgb(255, 0, 0));
        }
    }

    // Draw sprite B touching sprite A: (24, 8) to (39, 23) (16x16)
    for (int y = 8; y < 24; ++y) {
        for (int x = 24; x < 40; ++x) {
            testImg.setPixel(x, y, qRgb(0, 255, 0));
        }
    }

    // Detection with max dimensions 16x16 and non-overlapping constraint
    SpriteDetectionOptions opts;
    opts.maxWidth = 16;
    opts.maxHeight = 16;
    opts.disallowOverlaps = true;
    opts.minSliceSize = 4;

    QList<SpriteBox> boxes;
    QVERIFY(SpriteDetector::detectBoxes(testImg, boxes, opts));

    // Because each box is constrained to 16x16, the connected 32x16 block should be split into 2 boxes of 16x16
    QCOMPARE(boxes.size(), 2);
    for (const auto &box : boxes) {
        QVERIFY(box.rect.width() <= 16);
        QVERIFY(box.rect.height() <= 16);
    }
    // Verify no overlap
    QVERIFY(!boxes[0].rect.intersects(boxes[1].rect));
}

QTEST_MAIN(TestSnesExtractor)
#include "test_snes_extractor.moc"
