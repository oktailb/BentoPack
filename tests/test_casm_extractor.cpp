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

#include <QTest>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QFile>

#include "casm_generator.h"
#include "casm_extractor.h"
#include "casm_options.h"
#include "model/spritedocument.h"

class TestCAsmExtractor : public QObject
{
    Q_OBJECT

private slots:
    void testGameBoyTileEncoding();
    void testNESTileEncoding();
    void testSega4bppTileEncoding();
    void testAmigaBitplaneEncoding();
    void testC64MulticolorSpriteEncoding();
    void testZXSpectrumEncoding();
    void testCodeGenerationCHeader();
    void testCodeGenerationAssemblyVASM();
    void testCodeGenerationAssemblyRGBDS();
    void testHardwareScanlineLinter();
    void testExtractorWriteEndToEnd();
};

void TestCAsmExtractor::testGameBoyTileEncoding()
{
    // Create an 8x8 image with 4 distinct gray shades
    QImage img(8, 8, QImage::Format_ARGB32);
    img.fill(qRgb(255, 255, 255)); // White (Color 0 in GB)
    for (int x = 0; x < 8; ++x) {
        img.setPixel(x, 0, qRgb(0, 0, 0)); // Black (Color 3 in GB)
    }

    QByteArray bytes = Retro::CAsmGenerator::encodeGameBoyTiles(img);
    QCOMPARE(bytes.size(), 16); // 8x8 tile = 16 bytes

    // For row 0: 8 black pixels (color index 3 = bit 0 & bit 1 set)
    // Both low bitplane and high bitplane should be 0xFF
    QCOMPARE(static_cast<uint8_t>(bytes.at(0)), 0xFF);
    QCOMPARE(static_cast<uint8_t>(bytes.at(1)), 0xFF);

    // Row 1: white pixels (color index 0)
    QCOMPARE(static_cast<uint8_t>(bytes.at(2)), 0x00);
    QCOMPARE(static_cast<uint8_t>(bytes.at(3)), 0x00);
}

void TestCAsmExtractor::testNESTileEncoding()
{
    QImage img(8, 8, QImage::Format_ARGB32);
    img.fill(qRgb(0, 0, 0)); // Color 0 in NES
    for (int x = 0; x < 8; ++x) {
        img.setPixel(x, 0, qRgb(85, 85, 85)); // Color 1 in NES (bit 0 set, bit 1 clear)
    }

    QByteArray bytes = Retro::CAsmGenerator::encodeNESTiles(img);
    QCOMPARE(bytes.size(), 16);

    // Plane 0 row 0 is at offset 0
    QCOMPARE(static_cast<uint8_t>(bytes.at(0)), 0xFF);
    // Plane 1 row 0 is at offset 8
    QCOMPARE(static_cast<uint8_t>(bytes.at(8)), 0x00);
}

void TestCAsmExtractor::testSega4bppTileEncoding()
{
    QImage img(8, 8, QImage::Format_ARGB32);
    img.fill(qRgb(0, 0, 0));

    QByteArray bytes = Retro::CAsmGenerator::encodeSega4bppTiles(img);
    QCOMPARE(bytes.size(), 32); // 8x8 tile 4bpp = 32 bytes
}

void TestCAsmExtractor::testAmigaBitplaneEncoding()
{
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(qRgb(0, 0, 0));

    // 4 bitplanes, 16x16 pixels
    // Row width in bytes = (16+15)/16 * 2 = 2 bytes
    // 4 planes * 16 rows * 2 bytes = 128 bytes
    QByteArray bytes = Retro::CAsmGenerator::encodeAmigaBitplanes(img, 4, false);
    QCOMPARE(bytes.size(), 128);

    QByteArray bytesInterleaved = Retro::CAsmGenerator::encodeAmigaBitplanes(img, 4, true);
    QCOMPARE(bytesInterleaved.size(), 128);
}

void TestCAsmExtractor::testC64MulticolorSpriteEncoding()
{
    QImage img(24, 21, QImage::Format_ARGB32);
    img.fill(qRgb(0, 0, 0));

    QByteArray bytes = Retro::CAsmGenerator::encodeC64MulticolorSprite(img);
    QCOMPARE(bytes.size(), 64); // VIC-II sprite block = 64 bytes
}

void TestCAsmExtractor::testZXSpectrumEncoding()
{
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(qRgb(255, 255, 255));

    QByteArray bytes = Retro::CAsmGenerator::encodeZXSpectrum(img);
    // 16x16: Bitmap = 2 bytes/row * 16 rows = 32 bytes.
    // Attributes = 2x2 cells = 4 bytes.
    // Total = 36 bytes.
    QCOMPARE(bytes.size(), 36);
}

void TestCAsmExtractor::testCodeGenerationCHeader()
{
    SpriteDocument doc;
    QImage frame1(16, 16, QImage::Format_ARGB32);
    frame1.fill(qRgb(255, 0, 0));
    doc.addFrame(frame1);

    Retro::CAsmConfig config;
    config.applyPreset(Retro::System_MegaDrive_SGDK);

    QString code = Retro::CAsmGenerator::generate(doc, config, QStringLiteral("hero"));
    QVERIFY(code.contains(QStringLiteral("#ifndef HERO_H")));
    QVERIFY(code.contains(QStringLiteral("hero_palette")));
    QVERIFY(code.contains(QStringLiteral("hero_frame_00")));
    QVERIFY(code.contains(QStringLiteral("hero_frame_desc_t")));
    QVERIFY(code.contains(QStringLiteral("0x")));
}

void TestCAsmExtractor::testCodeGenerationAssemblyVASM()
{
    SpriteDocument doc;
    QImage frame1(16, 16, QImage::Format_ARGB32);
    frame1.fill(qRgb(0, 255, 0));
    doc.addFrame(frame1);

    Retro::CAsmConfig config;
    config.applyPreset(Retro::System_Amiga_OCS);

    QString code = Retro::CAsmGenerator::generate(doc, config, QStringLiteral("bob"));
    QVERIFY(code.contains(QStringLiteral("bob_frame_00:")));
    QVERIFY(code.contains(QStringLiteral("dc.b $")));
    QVERIFY(code.contains(QStringLiteral("Amiga OCS/ECS")));
}

void TestCAsmExtractor::testCodeGenerationAssemblyRGBDS()
{
    SpriteDocument doc;
    QImage frame1(8, 8, QImage::Format_ARGB32);
    frame1.fill(qRgb(128, 128, 128));
    doc.addFrame(frame1);

    Retro::CAsmConfig config;
    config.applyPreset(Retro::System_GameBoy_GBDK);
    config.language = Retro::Lang_ASM_RGBDS;
    config.numberFormat = Retro::Num_Hex_Dollar;

    QString code = Retro::CAsmGenerator::generate(doc, config, QStringLiteral("gb_sprite"));
    QVERIFY(code.contains(QStringLiteral("gb_sprite_frame_00:")));
    QVERIFY(code.contains(QStringLiteral("db $")));
}

void TestCAsmExtractor::testHardwareScanlineLinter()
{
    SpriteDocument doc;
    // Frame width 72 exceeds 64px (8 sprites on NES / GB)
    QImage frameLarge(72, 16, QImage::Format_ARGB32);
    frameLarge.fill(qRgb(255, 255, 255));
    doc.addFrame(frameLarge);

    Retro::CAsmConfig config;
    config.applyPreset(Retro::System_NES_cc65);

    QStringList warnings = Retro::CAsmGenerator::checkScanlineWarnings(doc, config);
    QVERIFY(!warnings.isEmpty());
    QVERIFY(warnings.first().contains(QStringLiteral("exceeds 64 px")));
}

void TestCAsmExtractor::testExtractorWriteEndToEnd()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    SpriteDocument doc;
    QImage frame(16, 16, QImage::Format_ARGB32);
    frame.fill(qRgb(0, 0, 255));
    doc.addFrame(frame);

    CAsmExtractor extractor;
    QCOMPARE(extractor.id(), QStringLiteral("casm_extractor"));
    QVERIFY(extractor.capabilities().testFlag(Extractor::CanExport));

    QString outHeader = tempDir.filePath(QStringLiteral("hero.h"));
    ExportOptions opts;
    opts.extraParams[QStringLiteral("interactive")] = false; // Disable blocking modal in test

    ExtractorError err;
    bool ok = extractor.write(outHeader, doc, opts, &err);
    QVERIFY2(ok, qPrintable(err.toString()));

    QFile file(outHeader);
    QVERIFY(file.exists());
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("hero_frame_00")));
}

QTEST_MAIN(TestCAsmExtractor)
#include "test_casm_extractor.moc"
