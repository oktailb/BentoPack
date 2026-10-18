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
#include "image/pixelhash.h"
#include "packer/celdeduplicator.h"
#include "packer/deltapatchslicer.h"
#include "packer/atlaspacker.h"
#include "model/spritedocument.h"

class TestVramDedup : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testPixelHashDeterminism();
    void testPixelHashVisualEquality();
    void testCelHashingAndDeduplication();
    void testDeltaPatchDirtyBoundingBox();
    void testDeltaPatchPerfectReconstruction();
    void testAtlasPackerFastHashDeduplication();
    void testAtlasPackerDeltaPatchPacking();
    void cleanupTestCase();
};

void TestVramDedup::initTestCase()
{
}

void TestVramDedup::cleanupTestCase()
{
}

void TestVramDedup::testPixelHashDeterminism()
{
    QImage imgA(32, 32, QImage::Format_ARGB32);
    imgA.fill(qRgba(255, 128, 64, 255));

    QImage imgB(32, 32, QImage::Format_ARGB32);
    imgB.fill(qRgba(255, 128, 64, 255));

    QImage imgDiff(32, 32, QImage::Format_ARGB32);
    imgDiff.fill(qRgba(255, 128, 65, 255)); // 1 single channel unit difference

    quint64 hashA1 = PixelHash::hashImage(imgA);
    quint64 hashA2 = PixelHash::hashImage(imgA);
    quint64 hashB = PixelHash::hashImage(imgB);
    quint64 hashDiff = PixelHash::hashImage(imgDiff);

    QVERIFY(hashA1 != 0);
    QCOMPARE(hashA1, hashA2);
    QCOMPARE(hashA1, hashB);
    QVERIFY(hashA1 != hashDiff);

    QImage nullImg;
    QCOMPARE(PixelHash::hashImage(nullImg), 0ULL);
}

void TestVramDedup::testPixelHashVisualEquality()
{
    QImage base(48, 48, QImage::Format_ARGB32);
    base.fill(Qt::transparent);
    {
        QPainter p(&base);
        p.setBrush(Qt::green);
        p.drawRect(5, 5, 20, 20);
    }

    QImage clone = base.copy();
    QVERIFY(PixelHash::areImagesIdentical(base, clone));

    // Modify a single pixel at (10, 10)
    QImage mutated = base.copy();
    mutated.setPixelColor(10, 10, QColor(0, 254, 0, 255));
    QVERIFY(!PixelHash::areImagesIdentical(base, mutated));

    // Different size
    QImage smaller = base.copy(0, 0, 40, 40);
    QVERIFY(!PixelHash::areImagesIdentical(base, smaller));
}

void TestVramDedup::testCelHashingAndDeduplication()
{
    SpriteDocument doc;
    QList<QImage> frames;
    QList<SpriteBox> boxes;

    QImage sharedBody(32, 32, QImage::Format_ARGB32);
    sharedBody.fill(Qt::blue);

    QImage headFrame0(16, 16, QImage::Format_ARGB32);
    headFrame0.fill(Qt::red);

    QImage headFrame1(16, 16, QImage::Format_ARGB32);
    headFrame1.fill(Qt::yellow);

    for (int i = 0; i < 4; ++i) {
        QImage frame(64, 64, QImage::Format_ARGB32);
        frame.fill(Qt::transparent);
        frames.append(frame);
        boxes.append(SpriteBox(QRect(0, 0, 64, 64)));
    }
    doc.setFrames(frames, boxes);

    // Frame 0: Body + Head0
    SpriteCel celBody0;
    celBody0.layerIndex = 0;
    celBody0.layerId = QStringLiteral("body");
    celBody0.x = 16;
    celBody0.y = 32;
    celBody0.opacity = 255;
    celBody0.image = sharedBody;

    SpriteCel celHead0;
    celHead0.layerIndex = 1;
    celHead0.layerId = QStringLiteral("head");
    celHead0.x = 24;
    celHead0.y = 8;
    celHead0.opacity = 255;
    celHead0.image = headFrame0;

    doc.setFrameCels(0, {celBody0, celHead0});

    // Frame 1: Reused Body + Head1
    SpriteCel celBody1 = celBody0; // Exactly identical linked cel
    SpriteCel celHead1;
    celHead1.layerIndex = 1;
    celHead1.layerId = QStringLiteral("head");
    celHead1.x = 24;
    celHead1.y = 8;
    celHead1.opacity = 255;
    celHead1.image = headFrame1;

    doc.setFrameCels(1, {celBody1, celHead1});

    // Frame 2: Reused Body + Head0 (back to frame 0 head)
    SpriteCel celBody2 = celBody0;
    SpriteCel celHead2 = celHead0;
    doc.setFrameCels(2, {celBody2, celHead2});

    // Frame 3: Reused Body + Head1
    SpriteCel celBody3 = celBody0;
    SpriteCel celHead3 = celHead1;
    doc.setFrameCels(3, {celBody3, celHead3});

    CelDeduplicationResult dedup = CelDeduplicator::analyze(doc);
    QCOMPARE(dedup.totalCelsCount, 8); // 4 frames * 2 cels = 8 cels total
    QCOMPARE(dedup.uniqueCelsCount, 3); // 1 unique body + 2 unique heads = 3 unique cels!

    // Savings: (8 - 3) = 5 redundant cels eliminated
    QVERIFY(dedup.savingsPercentage > 50.0f);

    // Verify mapping
    int bodyUniqueId = dedup.celMapping[qMakePair(0, 0)];
    QCOMPARE(dedup.celMapping[qMakePair(1, 0)], bodyUniqueId);
    QCOMPARE(dedup.celMapping[qMakePair(2, 0)], bodyUniqueId);
    QCOMPARE(dedup.celMapping[qMakePair(3, 0)], bodyUniqueId);

    int head0UniqueId = dedup.celMapping[qMakePair(0, 1)];
    int head1UniqueId = dedup.celMapping[qMakePair(1, 1)];
    QVERIFY(head0UniqueId != head1UniqueId);
    QCOMPARE(dedup.celMapping[qMakePair(2, 1)], head0UniqueId);
    QCOMPARE(dedup.celMapping[qMakePair(3, 1)], head1UniqueId);
}

void TestVramDedup::testDeltaPatchDirtyBoundingBox()
{
    // Base 64x64 frame with background character
    QImage base(64, 64, QImage::Format_ARGB32);
    base.fill(Qt::transparent);
    {
        QPainter p(&base);
        p.fillRect(10, 10, 44, 44, Qt::blue);
    }

    // Identical frame -> should detect duplicate
    DeltaPatch dupPatch = DeltaPatchSlicer::computeDelta(base, base, 1, 0, 0.25f);
    QVERIFY(dupPatch.isDuplicate);
    QVERIFY(!dupPatch.isDelta);

    // Frame with tiny 6x6 modification (e.g. eye blink) at (25, 20)
    // 6 * 6 = 36 pixels out of 4096 = 0.88% area (well <= 25% threshold)
    QImage smallChange = base.copy();
    {
        QPainter p(&smallChange);
        p.fillRect(25, 20, 6, 6, Qt::red);
    }

    DeltaPatch deltaPatch = DeltaPatchSlicer::computeDelta(smallChange, base, 1, 0, 0.25f);
    QVERIFY(deltaPatch.isDelta);
    QVERIFY(!deltaPatch.isDuplicate);
    QCOMPARE(deltaPatch.dirtyRect, QRect(25, 20, 6, 6));
    QCOMPARE(deltaPatch.deltaOffset, QPoint(25, 20));
    QCOMPARE(deltaPatch.patchImage.size(), QSize(6, 6));
    QVERIFY(deltaPatch.deltaAreaRatio < 0.05f);

    // Frame with major 50x50 modification -> area > 25% threshold -> should NOT be a delta
    QImage bigChange = base.copy();
    {
        QPainter p(&bigChange);
        p.fillRect(0, 0, 50, 50, Qt::yellow);
    }
    DeltaPatch fullPatch = DeltaPatchSlicer::computeDelta(bigChange, base, 2, 0, 0.25f);
    QVERIFY(!fullPatch.isDelta);
    QVERIFY(!fullPatch.isDuplicate);
    QCOMPARE(fullPatch.patchImage.size(), QSize(64, 64));
}

void TestVramDedup::testDeltaPatchPerfectReconstruction()
{
    // Create base frame
    QImage base(64, 64, QImage::Format_ARGB32);
    base.fill(qRgba(30, 40, 50, 255));
    for (int y = 15; y < 45; ++y) {
        for (int x = 15; x < 45; ++x) {
            base.setPixelColor(x, y, QColor((x * 7) % 256, (y * 5) % 256, 120, 255));
        }
    }

    // Create frame 1 with a modified eye region at (20, 22, 8, 4)
    QImage frame1 = base.copy();
    for (int y = 22; y < 26; ++y) {
        for (int x = 20; x < 28; ++x) {
            frame1.setPixelColor(x, y, QColor(255, 255, 0, 255));
        }
    }

    // Create frame 2 with clear-to-alpha region at (32, 30, 6, 6)
    QImage frame2 = base.copy();
    for (int y = 30; y < 36; ++y) {
        for (int x = 32; x < 38; ++x) {
            frame2.setPixelColor(x, y, Qt::transparent);
        }
    }

    QList<QImage> animFrames = { base, frame1, frame2 };
    DeltaSlicingResult res = DeltaPatchSlicer::sliceAnimation(animFrames, 0.25f, false);

    QCOMPARE(res.totalFrames, 3);
    QCOMPARE(res.keyFramesCount, 1);
    QCOMPARE(res.deltaFramesCount, 2);
    QVERIFY(res.areaSavingsPercentage > 60.0f); // Massive area saved!

    // Perfectly reconstruct frame1
    QImage rec1 = DeltaPatchSlicer::reconstructFrame(base, res.patches[1], base.size());
    QVERIFY(PixelHash::areImagesIdentical(frame1, rec1));

    // Perfectly reconstruct frame2 (including transparent alpha pixels)
    QImage rec2 = DeltaPatchSlicer::reconstructFrame(base, res.patches[2], base.size());
    QVERIFY(PixelHash::areImagesIdentical(frame2, rec2));
}

void TestVramDedup::testAtlasPackerFastHashDeduplication()
{
    // Generate 40 frames: 4 unique colors repeating 10 times each
    QList<QImage> frames;
    QList<QColor> colors = { Qt::red, Qt::green, Qt::blue, Qt::yellow };

    for (int cycle = 0; cycle < 10; ++cycle) {
        for (const QColor &c : colors) {
            QImage img(16, 16, QImage::Format_ARGB32);
            img.fill(c);
            frames.append(img);
        }
    }
    QCOMPARE(frames.size(), 40);

    AtlasPacker::PackOptions opts;
    opts.algorithm = AtlasPacker::MaxRects;
    opts.deduplicate = true;

    AtlasPackResult res = AtlasPacker::pack(frames, opts);
    QVERIFY(res.success);
    QCOMPARE(res.uniqueFramesCount, 4); // Only 4 unique frames!
    QCOMPARE(res.frameRects.size(), 40);

    // Verify all 10 red frames point to the exact same atlas rectangle
    for (int i = 0; i < 40; i += 4) {
        QCOMPARE(res.frameRects[i], res.frameRects[0]);
    }
}

void TestVramDedup::testAtlasPackerDeltaPatchPacking()
{
    // Base frame
    QImage base(32, 32, QImage::Format_ARGB32);
    base.fill(Qt::darkBlue);

    // Delta frame: only 4x4 pixels changed at (10, 10)
    QImage delta(32, 32, QImage::Format_ARGB32);
    delta.fill(Qt::darkBlue);
    for (int y = 10; y < 14; ++y) {
        for (int x = 10; x < 14; ++x) {
            delta.setPixelColor(x, y, Qt::cyan);
        }
    }

    QList<QImage> frames = { base, delta };

    AtlasPacker::PackOptions opts;
    opts.algorithm = AtlasPacker::MaxRects;
    opts.enableDeltaPatching = true;
    opts.deltaPatchThreshold = 0.25f;

    AtlasPackResult res = AtlasPacker::pack(frames, opts);
    QVERIFY(res.success);
    QVERIFY(res.hasDeltaPatches);
    QCOMPARE(res.deltaOffsets[1], QPoint(10, 10));
    QCOMPARE(res.deltaDirtyRects[1], QRect(10, 10, 4, 4));
    QCOMPARE(res.deltaReferenceIndices[1], 0);

    // Rect 1 on atlas should be the size of the 4x4 patch, not the 32x32 base!
    QCOMPARE(res.frameRects[1].size(), QSize(4, 4));
}

QTEST_MAIN(TestVramDedup)
#include "test_vram_dedup.moc"
