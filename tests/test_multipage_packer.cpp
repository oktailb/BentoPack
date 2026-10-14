#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include "packer/multipagepacker.h"
#include "model/spritedocument.h"

class TestMultiPagePacker : public QObject
{
    Q_OBJECT

private slots:
    void testSinglePageFitting();
    void testOverflowSpanningMultiplePages();
    void testSemanticAnimationCohesion();
    void testMultiAtlasAuxiliaryMapsSynchronization();
    void testHugeAnimationSequentialSplitting();
    void testEmptyDocument();

private:
    QImage createColorImage(int width, int height, const QColor &color);
};

QImage TestMultiPagePacker::createColorImage(int width, int height, const QColor &color)
{
    QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
    img.fill(color);
    return img;
}

void TestMultiPagePacker::testSinglePageFitting()
{
    // 4 small frames of 32x32 into a 128x128 atlas
    QList<QImage> frames;
    for (int i = 0; i < 4; ++i) {
        frames.append(createColorImage(32, 32, QColor(i * 50, 100, 200)));
    }

    AtlasPacker::PackOptions options;
    options.maxWidth = 128;
    options.maxHeight = 128;
    options.padding = 2;
    options.algorithm = AtlasPacker::MaxRects;

    QMap<QString, SpriteAnimation> anims;
    MultiPagePackResult result = MultiPagePacker::packFrames(frames, anims, options);

    QVERIFY(result.success);
    QCOMPARE(result.totalPages(), 1);
    QVERIFY(!result.isMultiPage());
    QCOMPARE(result.framePageIndices.size(), 4);
    for (int p : result.framePageIndices) {
        QCOMPARE(p, 0);
    }
    QCOMPARE(result.pages[0].dimensions.width() <= 128, true);
    QCOMPARE(result.pages[0].dimensions.height() <= 128, true);
}

void TestMultiPagePacker::testOverflowSpanningMultiplePages()
{
    // 10 frames of 64x64 into a 128x128 atlas.
    // In a 128x128 area with padding, at most 4 frames of 64x64 can fit per page.
    // Therefore 10 frames MUST be spanned across at least 3 pages.
    QList<QImage> frames;
    for (int i = 0; i < 10; ++i) {
        frames.append(createColorImage(60, 60, QColor(i * 20, 150, 100)));
    }

    AtlasPacker::PackOptions options;
    options.maxWidth = 128;
    options.maxHeight = 128;
    options.padding = 2;
    options.algorithm = AtlasPacker::MaxRects;

    QMap<QString, SpriteAnimation> anims;
    MultiPagePackResult result = MultiPagePacker::packFrames(frames, anims, options);

    QVERIFY(result.success);
    QVERIFY(result.totalPages() >= 3);
    QVERIFY(result.isMultiPage());
    QCOMPARE(result.framePageIndices.size(), 10);

    for (int i = 0; i < 10; ++i) {
        int page = result.pageForFrame(i);
        QVERIFY(page >= 0 && page < result.totalPages());
        QRect r = result.rectForFrame(i);
        QVERIFY(r.width() >= 60 && r.height() >= 60);
    }
}

void TestMultiPagePacker::testSemanticAnimationCohesion()
{
    // 2 distinct animations:
    // "walk": frames 0, 1 (two 60x60 frames)
    // "attack": frames 2, 3 (two 60x60 frames)
    // Total frames = 4.
    // If maxWidth = 120 and maxHeight = 64:
    // A page of 120x64 can fit EXACTLY two 60x60 frames side by side!
    // But it CANNOT fit 4 frames.
    // Semantic cohesion must place frames {0, 1} on Page A and frames {2, 3} on Page B!
    QList<QImage> frames;
    for (int i = 0; i < 4; ++i) {
        frames.append(createColorImage(58, 58, QColor(50, i * 60, 200)));
    }

    QMap<QString, SpriteAnimation> anims;
    SpriteAnimation walk;
    walk.name = QStringLiteral("walk");
    walk.frameIndices = {0, 1};
    anims.insert(walk.name, walk);

    SpriteAnimation attack;
    attack.name = QStringLiteral("attack");
    attack.frameIndices = {2, 3};
    anims.insert(attack.name, attack);

    AtlasPacker::PackOptions options;
    options.maxWidth = 128;
    options.maxHeight = 64;
    options.padding = 2;
    options.algorithm = AtlasPacker::MaxRects;

    MultiPagePackResult result = MultiPagePacker::packFrames(frames, anims, options);

    QVERIFY(result.success);
    QCOMPARE(result.totalPages(), 2);

    // Verify cohesion: walk frames (0 and 1) must share the same page
    int walkPage0 = result.pageForFrame(0);
    int walkPage1 = result.pageForFrame(1);
    QCOMPARE(walkPage0, walkPage1);

    // Verify cohesion: attack frames (2 and 3) must share the same page
    int atkPage2 = result.pageForFrame(2);
    int atkPage3 = result.pageForFrame(3);
    QCOMPARE(atkPage2, atkPage3);

    // And the two animations must be on different pages
    QVERIFY(walkPage0 != atkPage2);
}

void TestMultiPagePacker::testMultiAtlasAuxiliaryMapsSynchronization()
{
    SpriteDocument doc;
    QList<QImage> frames;
    QList<SpriteBox> boxes;

    // Add 8 frames of 60x60 with normal maps into a 128x128 limit (spans multiple pages)
    for (int i = 0; i < 8; ++i) {
        QImage diff = createColorImage(60, 60, Qt::yellow);
        frames.append(diff);
        boxes.append(SpriteBox(QRect(0, 0, 60, 60)));
    }
    doc.setFrames(frames, boxes);

    // Add normal maps for each frame
    QList<QImage> normalFrames;
    for (int i = 0; i < 8; ++i) {
        normalFrames.append(createColorImage(60, 60, QColor(128, 128, 255)));
    }
    doc.setAuxiliaryFrames(MaterialMapType::Normal, normalFrames);
    QVERIFY(doc.hasAnyAuxiliaryMaps());

    AtlasPacker::PackOptions options;
    options.maxWidth = 128;
    options.maxHeight = 128;
    options.padding = 2;
    options.algorithm = AtlasPacker::MaxRects;

    MultiPagePackResult result = MultiPagePacker::pack(doc, options, true /* exportMaterialMaps */, false);

    QVERIFY(result.success);
    QVERIFY(result.totalPages() >= 2);

    for (int p = 0; p < result.totalPages(); ++p) {
        const AtlasPage &page = result.pages[p];
        QVERIFY(page.hasAuxiliaryMap(MaterialMapType::Normal));
        QCOMPARE(page.auxiliaryAtlases.value(MaterialMapType::Normal).size(), page.dimensions);
    }
}

void TestMultiPagePacker::testHugeAnimationSequentialSplitting()
{
    // A single huge animation of 16 frames (60x60 each) that exceeds any single 128x128 page
    QList<QImage> frames;
    for (int i = 0; i < 16; ++i) {
        frames.append(createColorImage(60, 60, Qt::red));
    }

    QMap<QString, SpriteAnimation> anims;
    SpriteAnimation hugeAnim;
    hugeAnim.name = QStringLiteral("huge_boss_attack");
    for (int i = 0; i < 16; ++i) {
        hugeAnim.frameIndices.append(i);
    }
    anims.insert(hugeAnim.name, hugeAnim);

    AtlasPacker::PackOptions options;
    options.maxWidth = 128;
    options.maxHeight = 128;
    options.padding = 2;
    options.algorithm = AtlasPacker::MaxRects;

    MultiPagePackResult result = MultiPagePacker::packFrames(frames, anims, options);

    QVERIFY(result.success);
    QVERIFY(result.totalPages() >= 4);

    // Sequential integrity check: page numbers should be monotonically non-decreasing
    for (int i = 1; i < 16; ++i) {
        QVERIFY(result.pageForFrame(i) >= result.pageForFrame(i - 1));
    }
}

void TestMultiPagePacker::testEmptyDocument()
{
    SpriteDocument doc;
    AtlasPacker::PackOptions options;
    MultiPagePackResult result = MultiPagePacker::pack(doc, options);
    QVERIFY(!result.success);
    QCOMPARE(result.totalPages(), 0);
}

QTEST_MAIN(TestMultiPagePacker)
#include "test_multipage_packer.moc"
