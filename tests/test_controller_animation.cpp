#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QUndoStack>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>

#include "model/spritedocument.h"
#include "controller/animationcontroller.h"
#include "animation/animationplayer.h"
#include "commands/commands.h"
#include "config/appconfig.h"
#include "widgets/timelinefilmstripwidget.h"
#include "widgets/filmstriplistwidget.h"
#include "project/projectmanager.h"

class TestControllerAnimation : public QObject
{
    Q_OBJECT

private slots:
    void testAnimationControllerPlayback();
    void testAnimationControllerCreateAnimation();
    void testAnimationControllerReverseAnimation();
    void testAnimationControllerRemoveAnimation();
    void testAnimationControllerCurrentSelection();
    void testAnimationControllerAutoPlay();
    void testAnimationPlayerPingPongAndOnce();
    void testAnimationDuplicationAndRename();
    void testTimelineReorderFrames();
    void testTimelineFilmstripWidgetDragAndDrop();
    void testAnimationLoopModePersistence();
    void testTransportSpeedTimingStability();
    void testAnimationControllerPivotAlignment();
    void testAnimationControllerReticleToggle();
    void testAnimationPreviewZoomAndFit();
    void testAnimationPreviewPivotInteraction();
    void testAnimationPolygonMasking();
};

void TestControllerAnimation::testAnimationControllerPlayback()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);
    animCtrl.player()->setSequence({0, 1}, 12);

    QCOMPARE(animCtrl.fps(), 12);
    animCtrl.setFps(24);
    QCOMPARE(animCtrl.fps(), 24);

    QSignalSpy spyState(&animCtrl, &AnimationController::playbackStateChanged);

    QVERIFY(!animCtrl.isPlaying());
    animCtrl.play();
    QVERIFY(animCtrl.isPlaying());
    QCOMPARE(spyState.count(), 1);

    animCtrl.pause();
    QVERIFY(!animCtrl.isPlaying());

    animCtrl.togglePlayPause();
    QVERIFY(animCtrl.isPlaying());
    animCtrl.togglePlayPause();
    QVERIFY(!animCtrl.isPlaying());
}

void TestControllerAnimation::testAnimationControllerCreateAnimation()
{
    SpriteDocument doc;
    // Add 4 dummy frames
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::black);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(0, 16, 16, 16));
    doc.addSlice(QRect(16, 16, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    QSignalSpy spyAnim(&animCtrl, &AnimationController::currentAnimationChanged);

    animCtrl.createAnimation(QStringLiteral("walk"), {0, 1, 2}, 15);

    QVERIFY(doc.hasAnimation(QStringLiteral("walk")));
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 2}));
    QCOMPARE(doc.animation(QStringLiteral("walk")).fps, 15);
    QCOMPARE(animCtrl.currentAnimationName(), QStringLiteral("walk"));

    // Undo should remove animation
    undoStack.undo();
    QVERIFY(!doc.hasAnimation(QStringLiteral("walk")));

    // Redo should restore animation
    undoStack.redo();
    QVERIFY(doc.hasAnimation(QStringLiteral("walk")));
}

void TestControllerAnimation::testAnimationControllerReverseAnimation()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(0, 16, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    animCtrl.createAnimation(QStringLiteral("attack"), {0, 1, 2}, 10);
    QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices, (QList<int>{0, 1, 2}));

    animCtrl.reverseAnimationOrder();
    QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices, (QList<int>{2, 1, 0}));

    undoStack.undo();
    QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices, (QList<int>{0, 1, 2}));
}

void TestControllerAnimation::testAnimationControllerRemoveAnimation()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    animCtrl.createAnimation(QStringLiteral("idle"), {0}, 8);
    QVERIFY(doc.hasAnimation(QStringLiteral("idle")));

    animCtrl.removeAnimation(QStringLiteral("idle"));
    QVERIFY(!doc.hasAnimation(QStringLiteral("idle")));

    undoStack.undo();
    QVERIFY(doc.hasAnimation(QStringLiteral("idle")));
}

void TestControllerAnimation::testAnimationControllerCurrentSelection()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));

    AnimationController animCtrl(&doc);

    QVERIFY(!animCtrl.hasCurrentAnimation());

    animCtrl.updateCurrentAnimation({0, 1});
    QVERIFY(animCtrl.hasCurrentAnimation());
    QCOMPARE(animCtrl.currentAnimationName(), QStringLiteral("current"));

    animCtrl.removeCurrentAnimation();
    QVERIFY(!animCtrl.hasCurrentAnimation());
}

void TestControllerAnimation::testAnimationControllerAutoPlay()
{
    SpriteDocument doc;
    QImage img(48, 48, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(32, 0, 16, 16));

    AnimationController animCtrl(&doc);

    // Auto-play default should be true
    QVERIFY(AppConfig::instance().animation().autoPlayOnSelection);

    // 1. Selecting 2 frames automatically starts playback
    animCtrl.updateCurrentAnimation({0, 1});
    QVERIFY(animCtrl.isPlaying());

    // 2. Selecting 1 frame pauses playback
    animCtrl.updateCurrentAnimation({0});
    QVERIFY(!animCtrl.isPlaying());

    // 3. Starting manual play then changing selection preserves playback
    animCtrl.play();
    QVERIFY(animCtrl.isPlaying());
    animCtrl.updateCurrentAnimation({1, 2});
    QVERIFY(animCtrl.isPlaying());
}

void TestControllerAnimation::testAnimationPlayerPingPongAndOnce()
{
    AnimationPlayer player;
    player.setSequence({10, 20, 30}, 10);
    player.setLoopMode(AnimationPlayer::PingPong);
    QCOMPARE(player.loopMode(), AnimationPlayer::PingPong);

    // Initial frame is index 0 (frame 10)
    QCOMPARE(player.currentSequenceIndex(), 0);
    QCOMPARE(player.currentGlobalFrameIndex(), 10);

    // Step forward: 0 -> 1 -> 2
    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 1);
    QCOMPARE(player.currentGlobalFrameIndex(), 20);

    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 2);
    QCOMPARE(player.currentGlobalFrameIndex(), 30);

    // Next step in PingPong bounces back: 2 -> 1 -> 0 -> 1 ...
    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 1);
    QCOMPARE(player.currentGlobalFrameIndex(), 20);

    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 0);
    QCOMPARE(player.currentGlobalFrameIndex(), 10);

    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 1);

    // Navigation tests
    player.lastFrame();
    QCOMPARE(player.currentSequenceIndex(), 2);
    player.firstFrame();
    QCOMPARE(player.currentSequenceIndex(), 0);
    player.seek(1);
    QCOMPARE(player.currentSequenceIndex(), 1);

    // Test LoopMode::Once
    player.setLoopMode(AnimationPlayer::Once);
    player.firstFrame();
    player.play();
    QVERIFY(player.isPlaying());

    // Advance to last frame
    player.stepForward(); // 1
    QCOMPARE(player.currentSequenceIndex(), 1);
    player.stepForward(); // 2 (last frame)
    QCOMPARE(player.currentSequenceIndex(), 2);

    // In Once mode, advancing past the end stops playback and stays on last frame
    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 2);
    QVERIFY(!player.isPlaying());
}

void TestControllerAnimation::testAnimationDuplicationAndRename()
{
    SpriteDocument doc;
    QImage img(48, 48, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(32, 0, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    animCtrl.createAnimation(QStringLiteral("run"), {0, 1, 2}, 14, SpriteAnimation::PingPong);
    QVERIFY(doc.hasAnimation(QStringLiteral("run")));
    QCOMPARE(doc.animation(QStringLiteral("run")).loopMode, SpriteAnimation::PingPong);

    // Duplicate animation
    animCtrl.duplicateAnimation(QStringLiteral("run"));
    QString copyName = QStringLiteral("run_copy");
    QVERIFY(doc.hasAnimation(copyName));
    QCOMPARE(doc.animation(copyName).frameIndices, (QList<int>{0, 1, 2}));
    QCOMPARE(doc.animation(copyName).fps, 14);
    QCOMPARE(doc.animation(copyName).loopMode, SpriteAnimation::PingPong);

    // Undo duplication
    undoStack.undo();
    QVERIFY(!doc.hasAnimation(copyName));

    // Redo duplication
    undoStack.redo();
    QVERIFY(doc.hasAnimation(copyName));

    // Rename animation
    animCtrl.renameAnimation(copyName, QStringLiteral("run_fast"));
    QVERIFY(!doc.hasAnimation(copyName));
    QVERIFY(doc.hasAnimation(QStringLiteral("run_fast")));
    QCOMPARE(doc.animation(QStringLiteral("run_fast")).fps, 14);

    // Undo rename
    undoStack.undo();
    QVERIFY(doc.hasAnimation(copyName));
    QVERIFY(!doc.hasAnimation(QStringLiteral("run_fast")));

    // Redo rename
    undoStack.redo();
    QVERIFY(!doc.hasAnimation(copyName));
    QVERIFY(doc.hasAnimation(QStringLiteral("run_fast")));
}

void TestControllerAnimation::testTimelineReorderFrames()
{
    SpriteDocument doc;
    QImage img(64, 64, QImage::Format_ARGB32);
    doc.setAtlas(img);
    for (int i = 0; i < 4; ++i) {
        doc.addSlice(QRect(i * 16, 0, 16, 16));
    }

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    animCtrl.createAnimation(QStringLiteral("combo"), {0, 1, 2, 3}, 12);
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{0, 1, 2, 3}));

    // Reorder sequence
    animCtrl.reorderAnimationFrames(QStringLiteral("combo"), {3, 0, 2, 1});
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{3, 0, 2, 1}));

    // Undo reorder
    undoStack.undo();
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{0, 1, 2, 3}));

    // Redo reorder
    undoStack.redo();
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{3, 0, 2, 1}));

    // Add and remove frame from animation
    animCtrl.addFrameToAnimation(QStringLiteral("combo"), 0);
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{3, 0, 2, 1, 0}));

    animCtrl.removeFrameFromAnimation(QStringLiteral("combo"), 4);
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{3, 0, 2, 1}));

    // Batch add frames to animation
    animCtrl.addFramesToAnimation(QStringLiteral("combo"), {0, 2});
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{3, 0, 2, 1, 0, 2}));
}

void TestControllerAnimation::testTimelineFilmstripWidgetDragAndDrop()
{
    SpriteDocument doc;
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    doc.setAtlas(img);
    for (int i = 0; i < 4; ++i) {
        doc.addSlice(QRect(i * 16, 0, 16, 16));
    }

    QUndoStack undoStack;
    TimelineFilmstripWidget timeline;
    AnimationController animCtrl(&doc, &undoStack);
    animCtrl.attachTimelineWidget(&timeline);

    animCtrl.createAnimation(QStringLiteral("walk"), {0, 1, 2, 3}, 12);
    animCtrl.selectAnimation(QStringLiteral("walk"));

    FilmstripListWidget *list = timeline.listWidget();
    QVERIFY(list != nullptr);
    QCOMPARE(list->count(), 4);

    // Verify initial sequential items
    for (int i = 0; i < 4; ++i) {
        QListWidgetItem *it = list->item(i);
        QVERIFY(it != nullptr);
        QCOMPARE(it->data(Qt::UserRole).toInt(), i);
    }

    // Test calculateDropIndex bounds
    QCOMPARE(list->calculateDropIndex(QPoint(-100, 20)), 0);
    QCOMPARE(list->calculateDropIndex(QPoint(10000, 20)), 4);

    // Simulate drag-and-drop: move item from row 0 to drop after row 2 (insertion index 3 -> targetRow 2)
    // Resulting list should be [1, 2, 0, 3]
    QSignalSpy spyReordered(&timeline, &TimelineFilmstripWidget::sequenceReordered);
    QSignalSpy spyMoved(list, &FilmstripListWidget::itemMoved);

    list->executeItemMove(0, 3);

    QCOMPARE(spyMoved.count(), 1);
    QCOMPARE(spyReordered.count(), 1);
    QCOMPARE(list->count(), 4);

    // Verify order in Filmstrip widget
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), 1);
    QCOMPARE(list->item(1)->data(Qt::UserRole).toInt(), 2);
    QCOMPARE(list->item(2)->data(Qt::UserRole).toInt(), 0);
    QCOMPARE(list->item(3)->data(Qt::UserRole).toInt(), 3);

    // Verify order in document animation model
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{1, 2, 0, 3}));

    // Test Undo: list items and model should revert to [0, 1, 2, 3] without frame loss
    undoStack.undo();
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 2, 3}));
    QCOMPARE(list->count(), 4);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), 0);
    QCOMPARE(list->item(1)->data(Qt::UserRole).toInt(), 1);
    QCOMPARE(list->item(2)->data(Qt::UserRole).toInt(), 2);
    QCOMPARE(list->item(3)->data(Qt::UserRole).toInt(), 3);

    // Test Redo: list items and model should revert to [1, 2, 0, 3]
    undoStack.redo();
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{1, 2, 0, 3}));
    QCOMPARE(list->count(), 4);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), 1);
    QCOMPARE(list->item(1)->data(Qt::UserRole).toInt(), 2);
    QCOMPARE(list->item(2)->data(Qt::UserRole).toInt(), 0);
    QCOMPARE(list->item(3)->data(Qt::UserRole).toInt(), 3);

    // Test moving backward: move item at row 3 to index 0 -> [3, 1, 2, 0]
    list->executeItemMove(3, 0);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{3, 1, 2, 0}));
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), 3);
    QCOMPARE(list->item(1)->data(Qt::UserRole).toInt(), 1);
    QCOMPARE(list->item(2)->data(Qt::UserRole).toInt(), 2);
    QCOMPARE(list->item(3)->data(Qt::UserRole).toInt(), 0);
}

void TestControllerAnimation::testAnimationLoopModePersistence()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));

    doc.addAnimation(QStringLiteral("loop_anim"), {0, 1}, 10, SpriteAnimation::Loop);
    doc.addAnimation(QStringLiteral("pingpong_anim"), {0, 1}, 12, SpriteAnimation::PingPong);
    doc.addAnimation(QStringLiteral("once_anim"), {0, 1}, 8, SpriteAnimation::Once);

    QByteArray json = ProjectManager::serializeDocumentToJson(doc, QStringLiteral("atlas.png"), 1.0, QPointF(0, 0));

    SpriteDocument restoredDoc;
    double zoom = 1.0;
    QPointF pan;
    QString errMsg;
    bool ok = ProjectManager::deserializeJsonToDocument(json, restoredDoc, QStringLiteral("."), &zoom, &pan, &errMsg);
    QVERIFY2(ok, qPrintable(errMsg));

    QVERIFY(restoredDoc.hasAnimation(QStringLiteral("loop_anim")));
    QCOMPARE(restoredDoc.animation(QStringLiteral("loop_anim")).loopMode, SpriteAnimation::Loop);

    QVERIFY(restoredDoc.hasAnimation(QStringLiteral("pingpong_anim")));
    QCOMPARE(restoredDoc.animation(QStringLiteral("pingpong_anim")).loopMode, SpriteAnimation::PingPong);

    QVERIFY(restoredDoc.hasAnimation(QStringLiteral("once_anim")));
    QCOMPARE(restoredDoc.animation(QStringLiteral("once_anim")).loopMode, SpriteAnimation::Once);
}

void TestControllerAnimation::testTransportSpeedTimingStability()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));

    AnimationPlayer player;
    AnimationController animCtrl(&doc, nullptr, &player);

    QSlider scrubber(Qt::Horizontal);
    QLabel frameIndicator;
    animCtrl.attachScrubberSlider(&scrubber, &frameIndicator);

    QLabel fpsLabel;
    QSpinBox fpsSpinBox;
    QLabel timingLabel;

    fpsLabel.setFixedWidth(28);
    fpsSpinBox.setFixedWidth(55);
    timingLabel.setFixedWidth(120);
    timingLabel.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Initial widths
    QCOMPARE(fpsLabel.width(), 28);
    QCOMPARE(fpsSpinBox.width(), 55);
    QCOMPARE(timingLabel.width(), 120);

    // Connect FPS changes to update timingLabel exactly as MainWindow does
    connect(&animCtrl, &AnimationController::fpsChanged, [&](int fps) {
        fpsSpinBox.blockSignals(true);
        fpsSpinBox.setValue(fps);
        fpsSpinBox.blockSignals(false);
        timingLabel.setText(" -> Timing: " + QString::number(1000.0 / static_cast<double>(fps), 'g', 4) + "ms");
    });

    animCtrl.createAnimation(QStringLiteral("walk"), {0}, 10);

    // Test clicking across various FPS values within valid minFps..maxFps (1..60)
    const QList<int> testFpsList = {10, 9, 8, 12, 24, 30, 60, 1};
    for (int fpsVal : testFpsList) {
        animCtrl.setFps(fpsVal);
        QCOMPARE(fpsSpinBox.value(), fpsVal);

        // Verify the timing text matches expected ms
        QString expectedMs = QString::number(1000.0 / static_cast<double>(fpsVal), 'g', 4) + "ms";
        QVERIFY(timingLabel.text().contains(expectedMs));

        // CRITICAL: The widths of the widgets MUST remain strictly invariant
        QCOMPARE(fpsLabel.width(), 28);
        QCOMPARE(fpsSpinBox.width(), 55);
        QCOMPARE(timingLabel.width(), 120);
    }
}

void TestControllerAnimation::testAnimationControllerPivotAlignment()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);
    doc.setAtlas(atlas);

    // Frame 0: 20x20, default pivot (10, 20)
    int idx0 = doc.addSlice(QRect(0, 0, 20, 20));
    // Frame 1: 40x60, default pivot (20, 60)
    int idx1 = doc.addSlice(QRect(20, 0, 40, 60));

    doc.setAnimation(QStringLiteral("walk"), {idx0, idx1}, 12, true);

    QGraphicsView view;
    AnimationController animCtrl(&doc, nullptr, nullptr, nullptr, &view);
    animCtrl.selectAnimation(QStringLiteral("walk"));

    // Check envelope
    QRect env = doc.computeAnimationEnvelope(QStringLiteral("walk"));
    QCOMPARE(env.x(), 20);
    QCOMPARE(env.y(), 60);
    QCOMPARE(env.width(), 40);
    QCOMPARE(env.height(), 60);

    // Seek to sequence index 0 (Frame 0)
    animCtrl.seek(0);
    QGraphicsScene *scene = animCtrl.previewScene();
    QVERIFY(scene != nullptr);
    QList<QGraphicsItem*> items = scene->items();
    QGraphicsPixmapItem *pixItem = nullptr;
    for (QGraphicsItem *it : items) {
        pixItem = dynamic_cast<QGraphicsPixmapItem*>(it);
        if (pixItem) break;
    }
    QVERIFY(pixItem != nullptr);
    // Frame 0 pos in scene: (originX - px0, originY - py0) = (20 - 10, 60 - 20) = (10, 40)
    QCOMPARE(pixItem->pos(), QPointF(10, 40));
    // Absolute pivot in scene = pos + local pivot = (20, 60)
    QCOMPARE(pixItem->pos() + doc.boxPivot(idx0), QPointF(20, 60));

    // Seek to sequence index 1 (Frame 1)
    animCtrl.seek(1);
    // Frame 1 pos in scene: (originX - px1, originY - py1) = (20 - 20, 60 - 60) = (0, 0)
    QCOMPARE(pixItem->pos(), QPointF(0, 0));
    // Absolute pivot in scene = pos + local pivot = (20, 60)
    QCOMPARE(pixItem->pos() + doc.boxPivot(idx1), QPointF(20, 60));
}

void TestControllerAnimation::testAnimationControllerReticleToggle()
{
    SpriteDocument doc;
    QImage atlas(50, 50, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);
    int idx = doc.addSlice(QRect(0, 0, 30, 30));
    doc.setAnimation(QStringLiteral("anim"), {idx}, 12, true);

    QGraphicsView view;
    AnimationController animCtrl(&doc, nullptr, nullptr, nullptr, &view);
    animCtrl.selectAnimation(QStringLiteral("anim"));

    QCOMPARE(animCtrl.showPivotReticle(), false);

    QSignalSpy spyReticle(&animCtrl, &AnimationController::showPivotReticleChanged);
    animCtrl.setShowPivotReticle(true);
    QCOMPARE(spyReticle.count(), 1);
    QCOMPARE(animCtrl.showPivotReticle(), true);

    animCtrl.setShowPivotReticle(false);
    QCOMPARE(spyReticle.count(), 2);
    QCOMPARE(animCtrl.showPivotReticle(), false);
}

void TestControllerAnimation::testAnimationPreviewZoomAndFit()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::blue);
    doc.setAtlas(atlas);
    int idx = doc.addSlice(QRect(0, 0, 40, 40));
    doc.setAnimation(QStringLiteral("run"), {idx}, 10, true);

    QGraphicsView view;
    view.resize(300, 300);
    view.show();

    AnimationController animCtrl(&doc, nullptr, nullptr, nullptr, &view);
    animCtrl.selectAnimation(QStringLiteral("run"));

    QSignalSpy spyZoom(&animCtrl, &AnimationController::zoomChanged);

    // Initial zoom
    QVERIFY(animCtrl.zoomFactor() > 0.0);

    // Zoom in
    double zBefore = animCtrl.zoomFactor();
    animCtrl.zoomIn();
    QVERIFY(animCtrl.zoomFactor() > zBefore);
    QVERIFY(!spyZoom.isEmpty());

    // Zoom out
    animCtrl.zoomOut();
    QCOMPARE(animCtrl.zoomFactor(), zBefore);

    // Reset zoom
    animCtrl.resetZoom();
    QCOMPARE(animCtrl.zoomFactor(), 1.0);

    // Fit in view
    animCtrl.fitInView();
    QVERIFY(animCtrl.zoomFactor() > 0.0);
}

void TestControllerAnimation::testAnimationPreviewPivotInteraction()
{
    SpriteDocument doc;
    QUndoStack undoStack;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::yellow);
    doc.setAtlas(atlas);
    int idx = doc.addSlice(QRect(0, 0, 50, 50));
    doc.setAnimation(QStringLiteral("idle"), {idx}, 8, true);

    QGraphicsView view;
    view.resize(400, 400);
    view.show();

    AnimationController animCtrl(&doc, &undoStack, nullptr, nullptr, &view);
    animCtrl.selectAnimation(QStringLiteral("idle"));
    animCtrl.setShowPivotReticle(true);

    // Default pivot is (25, 50)
    QCOMPARE(doc.boxPivot(idx), QPoint(25, 50));

    QSignalSpy spyDrag(&animCtrl, &AnimationController::pivotDragged);
    QSignalSpy spyFinish(&animCtrl, &AnimationController::pivotDragFinished);

    // Simulate Shift+Click at position (150, 150) in viewport to place pivot directly
    QPoint clickPos(150, 150);

    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPointF(clickPos), QPointF(clickPos), QPointF(clickPos), Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
    QApplication::sendEvent(view.viewport(), &pressEvent);

    QCOMPARE(spyDrag.count(), 1);

    // Release mouse
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPointF(clickPos), QPointF(clickPos), QPointF(clickPos), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &releaseEvent);

    QCOMPARE(spyFinish.count(), 1);
    QVERIFY(doc.box(idx).hasCustomPivot);

    // Undo restores default pivot
    QVERIFY(undoStack.canUndo());
    undoStack.undo();
    QCOMPARE(doc.box(idx).hasCustomPivot, false);
    QCOMPARE(doc.boxPivot(idx), QPoint(25, 50));

    // Redo restores dragged pivot
    QVERIFY(undoStack.canRedo());
    undoStack.redo();
    QCOMPARE(doc.box(idx).hasCustomPivot, true);
}

void TestControllerAnimation::testAnimationPolygonMasking()
{
    SpriteDocument doc;
    QImage img(40, 40, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.fillRect(0, 0, 20, 20, QColor(0, 255, 0, 255)); // Sprite content
        p.fillRect(30, 30, 10, 10, QColor(255, 0, 0, 255)); // Neighbor trace
    }

    SpriteBox box(QRect(0, 0, 40, 40));
    box.hasPolygonMesh = true;
    box.polygon << QPointF(0, 0) << QPointF(20, 0) << QPointF(20, 20) << QPointF(0, 20);
    box.triangles = { 0, 1, 2, 0, 2, 3 };
    doc.addFrame(img, box);

    doc.setAnimation(QStringLiteral("walk"), { 0 }, 12, true);

    QGraphicsView view;
    AnimationController animCtrl(&doc, nullptr, nullptr, nullptr, &view);
    animCtrl.selectAnimation(QStringLiteral("walk"));
    animCtrl.seek(0);

    QGraphicsScene *scene = animCtrl.previewScene();
    QVERIFY(scene != nullptr);

    QGraphicsPixmapItem *pixItem = nullptr;
    for (QGraphicsItem *it : scene->items()) {
        pixItem = dynamic_cast<QGraphicsPixmapItem*>(it);
        if (pixItem) break;
    }
    QVERIFY(pixItem != nullptr);

    QImage rendered = pixItem->pixmap().toImage();
    // Sprite pixels inside polygon are preserved
    QCOMPARE(rendered.pixelColor(5, 5), QColor(0, 255, 0, 255));
    // Neighbor trace outside polygon is transparent (alpha == 0)
    QCOMPARE(rendered.pixelColor(35, 35).alpha(), 0);
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestControllerAnimation tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_controller_animation.moc"
