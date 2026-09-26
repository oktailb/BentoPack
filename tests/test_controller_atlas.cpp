#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QUndoStack>
#include <QContextMenuEvent>
#include <QTranslator>
#include <QMainWindow>
#include <QDockWidget>
#include <QSettings>
#include <QTemporaryDir>
#include <QPainter>
#include <QLibraryInfo>
#include <QCoreApplication>
#include <QDebug>
#include <QApplication>

#include "model/spritedocument.h"
#include "controller/atlasviewcontroller.h"
#include "controller/animationcontroller.h"
#include "controller/projectcontroller.h"
#include "atlasboxitem.h"
#include "commands/commands.h"
#include "animation/animationplayer.h"
#include "widgets/timelinefilmstripwidget.h"

class TestControllerAtlas : public QObject
{
    Q_OBJECT

private slots:
    void testAtlasViewControllerToolMode();
    void testAtlasViewControllerZoom();
    void testAtlasViewControllerBoxSync();
    void testAtlasViewControllerSelection();
    void testAtlasViewControllerNudge();
    void testAtlasViewControllerTrimAndMerge();
    void testAtlasViewControllerErasePixels();
    void testAtlasViewControllerMarqueeSelection();
    void testControllerCrossSyncNoRecursion();
    void testAtlasViewControllerContextMenuSignals();
    void testAtlasViewControllerMultiSelectAndDelete();
    void testAtlasViewControllerMouseCenteredZoom();
    void testI18nKeyTranslations();
    void testAtlasBoxItemHandleCosmeticSize();
    void testAtlasBoxItemPivotDrag();
    void testAtlasViewControllerGroupDrag();
    void testAtlasViewControllerContinuousSlice();
    void testDockStatePersistence();
    void testSelectionOrderPreserved();
};

void TestControllerAtlas::testAtlasViewControllerToolMode()
{
    QGraphicsView view;
    SpriteDocument doc;
    AtlasViewController atlasCtrl(&view, &doc);

    QSignalSpy spyTool(&atlasCtrl, &AtlasViewController::toolModeChanged);

    QCOMPARE(atlasCtrl.toolMode(), AtlasViewController::ToolSelect);

    atlasCtrl.setToolMode(AtlasViewController::ToolAddSlice);
    QCOMPARE(atlasCtrl.toolMode(), AtlasViewController::ToolAddSlice);
    QCOMPARE(spyTool.count(), 1);

    atlasCtrl.setToolMode(AtlasViewController::ToolSelect);
    QCOMPARE(atlasCtrl.toolMode(), AtlasViewController::ToolSelect);
    QCOMPARE(spyTool.count(), 2);
}

void TestControllerAtlas::testAtlasViewControllerZoom()
{
    QGraphicsView view;
    SpriteDocument doc;
    AtlasViewController atlasCtrl(&view, &doc);

    QSignalSpy spyZoom(&atlasCtrl, &AtlasViewController::zoomChanged);

    atlasCtrl.setZoomFactor(2.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 2.0);
    QCOMPARE(spyZoom.count(), 1);

    // Test clamping limits: min 0.1, max 10.0
    atlasCtrl.setZoomFactor(0.01);
    QCOMPARE(atlasCtrl.zoomFactor(), 0.1);

    atlasCtrl.setZoomFactor(50.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 10.0);

    atlasCtrl.setZoomFactor(1.0);
    atlasCtrl.zoomIn(2.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 2.0);
    atlasCtrl.zoomOut(2.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 1.0);
}

void TestControllerAtlas::testAtlasViewControllerBoxSync()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    AtlasViewController atlasCtrl(&view, &doc);

    QCOMPARE(atlasCtrl.boxCount(), 0);

    doc.addSlice(QRect(10, 10, 20, 20));
    doc.addSlice(QRect(40, 10, 20, 20));
    doc.addSlice(QRect(70, 10, 20, 20));

    // framesChanged signal triggers syncAtlasBoxes
    QCOMPARE(atlasCtrl.boxCount(), 3);
}

void TestControllerAtlas::testAtlasViewControllerSelection()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 20, 20));
    doc.addSlice(QRect(20, 0, 20, 20));
    doc.addSlice(QRect(40, 0, 20, 20));

    AtlasViewController atlasCtrl(&view, &doc);

    QSignalSpy spySel(&atlasCtrl, &AtlasViewController::selectionChanged);

    atlasCtrl.setSelectedBoxIndices({1});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), QList<int>{1});
    QCOMPARE(spySel.count(), 1);

    atlasCtrl.selectAll();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{0, 1, 2}));

    atlasCtrl.invertSelection();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), QList<int>());

    atlasCtrl.setSelectedBoxIndices({0});
    atlasCtrl.invertSelection();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{1, 2}));

    atlasCtrl.clearSelection();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), QList<int>());
}

void TestControllerAtlas::testAtlasViewControllerNudge()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20));

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    atlasCtrl.setSelectedBoxIndices({0});

    // Nudge right 5px, down 3px
    atlasCtrl.nudgeSelectedBoxes(5, 3);
    QCOMPARE(doc.box(0).rect, QRect(15, 13, 20, 20));

    // Undo should return box to original pos
    undoStack.undo();
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));

    // Redo should apply nudge again
    undoStack.redo();
    QCOMPARE(doc.box(0).rect, QRect(15, 13, 20, 20));
}

void TestControllerAtlas::testAtlasViewControllerTrimAndMerge()
{
    QGraphicsView view;
    SpriteDocument doc;
    // Create an atlas with an opaque 10x10 area inside a 30x30 bounding box
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);

    QPainter p(&atlas);
    p.fillRect(5, 5, 10, 10, Qt::red);
    p.fillRect(50, 50, 20, 20, Qt::blue);
    p.end();

    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 30, 30));    // Box 0: includes the 10x10 red area at (5,5)
    doc.addSlice(QRect(40, 40, 40, 40));  // Box 1: includes the 20x20 blue area at (50,50)

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    // Test Trim on Box 0
    atlasCtrl.setSelectedBoxIndices({0});
    atlasCtrl.trimSelectedSlice(1);
    QCOMPARE(doc.box(0).rect, QRect(5, 5, 10, 10));

    undoStack.undo();
    QCOMPARE(doc.box(0).rect, QRect(0, 0, 30, 30));

    // Test Merge Box 0 and Box 1
    atlasCtrl.setSelectedBoxIndices({0, 1});
    atlasCtrl.mergeSelectedSlices();
    QCOMPARE(doc.frameCount(), 1);

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 2);
}

void TestControllerAtlas::testAtlasViewControllerErasePixels()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);

    // Paint an opaque red rectangle at (10, 10, 20, 20)
    QPainter p(&atlas);
    p.fillRect(10, 10, 20, 20, Qt::red);
    p.end();

    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20)); // Frame 0

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    // Select box 0 and erase its pixels
    atlasCtrl.setSelectedBoxIndices({0});
    atlasCtrl.eraseSelectedSlicesPixels();

    // 1. Frame count is now 0
    QCOMPARE(doc.frameCount(), 0);

    // 2. Pixels inside the rect on atlas are now transparent
    QRgb pixelInside = doc.atlas().pixel(15, 15);
    QCOMPARE(qAlpha(pixelInside), 0);

    // 3. Pixel outside the rect remains white
    QRgb pixelOutside = doc.atlas().pixel(5, 5);
    QCOMPARE(qAlpha(pixelOutside), 255);

    // 4. Undo restores frame and original red pixels
    undoStack.undo();
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));
    QRgb restoredPixel = doc.atlas().pixel(15, 15);
    QCOMPARE(qAlpha(restoredPixel), 255);
    QCOMPARE(qRed(restoredPixel), 255);

    // 5. Redo erases pixels again
    undoStack.redo();
    QCOMPARE(doc.frameCount(), 0);
    QCOMPARE(qAlpha(doc.atlas().pixel(15, 15)), 0);
}

void TestControllerAtlas::testAtlasViewControllerMarqueeSelection()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(200, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // 4 boxes horizontally
    doc.addSlice(QRect(0, 0, 20, 20));    // 0
    doc.addSlice(QRect(30, 0, 20, 20));   // 1
    doc.addSlice(QRect(60, 0, 20, 20));   // 2
    doc.addSlice(QRect(90, 0, 20, 20));   // 3

    AtlasViewController atlasCtrl(&view, &doc);

    // Initial marquee selecting box 0 and 1
    atlasCtrl.startMarqueeSelection(QPointF(5, 5), Qt::NoModifier);
    atlasCtrl.updateMarqueeSelection(QPointF(45, 15));
    atlasCtrl.endMarqueeSelection();

    QList<int> expected01 = {0, 1};
    QCOMPARE(atlasCtrl.selectedBoxIndices(), expected01);

    // Additive marquee: adds box 2 and 3
    atlasCtrl.startMarqueeSelection(QPointF(55, 5), Qt::ControlModifier);
    atlasCtrl.updateMarqueeSelection(QPointF(95, 15));
    atlasCtrl.endMarqueeSelection();

    QList<int> expectedAll = {0, 1, 2, 3};
    QCOMPARE(atlasCtrl.selectedBoxIndices(), expectedAll);

    // Subtractive marquee: removes box 0
    atlasCtrl.startMarqueeSelection(QPointF(0, 0), Qt::ShiftModifier);
    atlasCtrl.updateMarqueeSelection(QPointF(25, 25));
    atlasCtrl.endMarqueeSelection();

    QList<int> expectedRest = {1, 2, 3};
    QCOMPARE(atlasCtrl.selectedBoxIndices(), expectedRest);
}

void TestControllerAtlas::testControllerCrossSyncNoRecursion()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(200, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    doc.addSlice(QRect(0, 0, 20, 20));
    doc.addSlice(QRect(30, 0, 20, 20));
    doc.addSlice(QRect(60, 0, 20, 20));
    doc.addSlice(QRect(90, 0, 20, 20));

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);
    AnimationController animCtrl(&doc, &undoStack);

    // Wire them identically to MainWindow::setupControllers()
    QObject::connect(&atlasCtrl, &AtlasViewController::selectionChanged,
                     &animCtrl, &AnimationController::updateCurrentAnimation);

    QObject::connect(&animCtrl, &AnimationController::framesSelectedInAnimation,
                     &atlasCtrl, [&atlasCtrl](const QList<int> &indices) {
        if (atlasCtrl.selectedBoxIndices() != indices) {
            atlasCtrl.setSelectedBoxIndices(indices);
        }
    });

    // 1. Selecting on atlas updates animation without infinite recursion
    atlasCtrl.setSelectedBoxIndices({0, 1});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{0, 1}));
    QVERIFY(doc.hasAnimation(QStringLiteral("current")));
    QCOMPARE(doc.animation(QStringLiteral("current")).frameIndices, (QList<int>{0, 1}));

    // 2. Selecting an explicit animation updates atlas selection
    animCtrl.createAnimation(QStringLiteral("run"), {2, 3}, 12);
    animCtrl.selectAnimation(QStringLiteral("run"));
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{2, 3}));

    // 3. Modifying atlas selection again does not disrupt animation list
    atlasCtrl.setSelectedBoxIndices({1});
    QCOMPARE(doc.animation(QStringLiteral("current")).frameIndices, (QList<int>{1}));
}

void TestControllerAtlas::testAtlasViewControllerContextMenuSignals()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20));

    AtlasViewController atlasCtrl(&view, &doc);
    QSignalSpy spyAtlasMenu(&atlasCtrl, &AtlasViewController::atlasContextMenuRequested);

    // Context menu event on empty space (e.g. 80, 80)
    QContextMenuEvent emptyEvent(QContextMenuEvent::Mouse, QPoint(80, 80), view.viewport()->mapToGlobal(QPoint(80, 80)));
    QCoreApplication::sendEvent(view.viewport(), &emptyEvent);

    QCOMPARE(spyAtlasMenu.count(), 1);
    QCOMPARE(spyAtlasMenu.takeFirst().at(0).toPoint(), QPoint(80, 80));
}

void TestControllerAtlas::testAtlasViewControllerMultiSelectAndDelete()
{
    QGraphicsView view;
    SpriteDocument doc;
    QUndoStack undoStack;

    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    // Draw distinct colors in 5 regions
    for (int i = 0; i < 5; ++i) {
        QRect r(i * 20, 0, 15, 15);
        for (int y = r.top(); y <= r.bottom(); ++y) {
            QRgb *line = reinterpret_cast<QRgb*>(atlas.scanLine(y));
            for (int x = r.left(); x <= r.right(); ++x) {
                line[x] = qRgba(50 * (i + 1), 0, 0, 255);
            }
        }
    }
    doc.setAtlas(atlas);

    for (int i = 0; i < 5; ++i) {
        doc.addSlice(QRect(i * 20, 0, 15, 15));
    }
    QCOMPARE(doc.frameCount(), 5);

    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    // 1. Multi-selection does not collapse
    atlasCtrl.setSelectedBoxIndices({1, 3});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{1, 3}));
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{1, 3}));

    // 2. Delete selected slices (slices 1 and 3)
    QSignalSpy framesChangedSpy(&doc, &SpriteDocument::framesChanged);
    atlasCtrl.deleteSelectedSlices();

    QCOMPARE(doc.frameCount(), 3);
    QVERIFY(framesChangedSpy.count() >= 1);
    // Remaining boxes should be original 0, 2, 4 (now at 0, 1, 2)
    QCOMPARE(doc.box(0).rect, QRect(0, 0, 15, 15));
    QCOMPARE(doc.box(1).rect, QRect(40, 0, 15, 15));
    QCOMPARE(doc.box(2).rect, QRect(80, 0, 15, 15));

    // 3. Undo restores all 5 frames
    undoStack.undo();
    QCOMPARE(doc.frameCount(), 5);
    QCOMPARE(doc.box(1).rect, QRect(20, 0, 15, 15));
    QCOMPARE(doc.box(3).rect, QRect(60, 0, 15, 15));

    // 4. Redo deletes them again
    undoStack.redo();
    QCOMPARE(doc.frameCount(), 3);

    // 5. Erase pixels for slices 0 and 2 (which correspond to original 0 and 4)
    atlasCtrl.setSelectedBoxIndices({0, 2});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{0, 2}));

    QSignalSpy atlasChangedSpy(&doc, &SpriteDocument::atlasChanged);
    atlasCtrl.eraseSelectedSlicesPixels();

    // Now only 1 frame remains (original 2, which was at index 1)
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(40, 0, 15, 15));
    QVERIFY(atlasChangedSpy.count() >= 1);

    // Verify erased pixels are transparent
    QCOMPARE(qAlpha(doc.atlas().pixel(5, 5)), 0);
    QCOMPARE(qAlpha(doc.atlas().pixel(85, 5)), 0);
    // Non-erased slice still has opaque pixels
    QCOMPARE(qAlpha(doc.atlas().pixel(45, 5)), 255);

    // 6. Undo restores both pixels and frames
    undoStack.undo();
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(qAlpha(doc.atlas().pixel(5, 5)), 255);
    QCOMPARE(qAlpha(doc.atlas().pixel(85, 5)), 255);
}

void TestControllerAtlas::testAtlasViewControllerMouseCenteredZoom()
{
    QGraphicsView view;
    view.resize(800, 600);
    view.show();
    QCoreApplication::processEvents();

    SpriteDocument doc;
    QImage atlas(1000, 1000, QImage::Format_ARGB32_Premultiplied);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    AtlasViewController atlasCtrl(&view, &doc);
    atlasCtrl.setZoomFactor(1.0);
    QCoreApplication::processEvents();

    // Zoom centered on a specific viewport point (e.g., 300, 250)
    QPointF mousePos(300.0, 250.0);
    QPointF sceneBefore = view.mapToScene(mousePos.toPoint());

    // Zoom in by factor 1.5
    atlasCtrl.zoomAt(mousePos, 1.5);
    QCOMPARE(atlasCtrl.zoomFactor(), 1.5);

    QPointF sceneAfter = view.mapToScene(mousePos.toPoint());
    qDebug() << "sceneBefore:" << sceneBefore << "sceneAfter:" << sceneAfter
             << "diff:" << (sceneAfter - sceneBefore);
    // The scene point mapped to the cursor must remain stationary (within 4 pixels tolerance for offscreen scrollbars)
    QVERIFY(qAbs(sceneAfter.x() - sceneBefore.x()) <= 4.0);
    QVERIFY(qAbs(sceneAfter.y() - sceneBefore.y()) <= 4.0);

    // Zoom out by factor 0.8 at another point
    QPointF mousePos2(150.0, 120.0);
    QPointF sceneBefore2 = view.mapToScene(mousePos2.toPoint());
    atlasCtrl.zoomAt(mousePos2, 0.8);
    QPointF sceneAfter2 = view.mapToScene(mousePos2.toPoint());
    QVERIFY(qAbs(sceneAfter2.x() - sceneBefore2.x()) <= 4.0);
    QVERIFY(qAbs(sceneAfter2.y() - sceneBefore2.y()) <= 4.0);

    // Check zoom limits with zoomAt
    atlasCtrl.setZoomFactor(10.0);
    atlasCtrl.zoomAt(mousePos, 1.5);
    QCOMPARE(atlasCtrl.zoomFactor(), 10.0);

    atlasCtrl.setZoomFactor(0.1);
    atlasCtrl.zoomAt(mousePos, 0.5);
    QCOMPARE(atlasCtrl.zoomFactor(), 0.1);
}

void TestControllerAtlas::testI18nKeyTranslations()
{
#ifdef QM_DIR
    auto loadCatalog = [](QTranslator &translator, const QString &baseName) -> bool {
        QString qmName = baseName.endsWith(QStringLiteral(".qm")) ? baseName : (baseName + QStringLiteral(".qm"));
        QString baseWithoutExt = baseName;
        if (baseWithoutExt.endsWith(QStringLiteral(".qm"))) {
            baseWithoutExt.chop(3);
        }

        QStringList candidateDirs = {
#ifdef QM_DIR
            QStringLiteral(QM_DIR),
            QStringLiteral(QM_DIR) + QStringLiteral("/.qm"),
            QStringLiteral(QM_DIR) + QStringLiteral("/i18n"),
#endif
            QCoreApplication::applicationDirPath(),
            QCoreApplication::applicationDirPath() + QStringLiteral("/i18n"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/../BentoPack"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/../BentoPack/.qm"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/../BentoPack/i18n"),
            QStringLiteral(":/i18n"),
            QStringLiteral(":/i18n/"),
            QLibraryInfo::path(QLibraryInfo::TranslationsPath)
        };

        for (const QString &dir : candidateDirs) {
            if (dir.isEmpty()) continue;
            if (translator.load(qmName, dir)) return true;
            if (translator.load(baseWithoutExt, dir)) return true;
            if (QFile::exists(dir + QStringLiteral("/") + qmName) && translator.load(dir + QStringLiteral("/") + qmName)) return true;
        }
        return false;
    };

    // 1. Test French translation
    QTranslator frTranslator;
    bool frLoaded = loadCatalog(frTranslator, QStringLiteral("bentopack_fr_FR.qm"));
    if (!frLoaded) {
        QSKIP("Translation catalog bentopack_fr_FR.qm not found in build tree or resources on this platform.");
    }

    {
        QCoreApplication::installTranslator(&frTranslator);

        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILE"), QStringLiteral("Fichier"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_VIEW"), QStringLiteral("Affichage"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOLBAR_MAIN"), QStringLiteral("Barre d'outils principale"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_DOCK_PREVIEW"), QStringLiteral("Aperçu de l'animation"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_OPEN"), QStringLiteral("&Ouvrir"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_SAVE"), QStringLiteral("&Enregistrer"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOL_SELECT"), QStringLiteral("Sélectionner"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_CREATE_ANIM"), QStringLiteral("Créer une animation depuis la sélection"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_ADD_TO_ANIM"), QStringLiteral("Ajouter à l'animation"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_SELECT_ALL"), QStringLiteral("Tout sélectionner"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_TITLE"), QStringLiteral("À propos"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_PRICING"), QStringLiteral("Tarifs & Licences"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_PLUGINS"), QStringLiteral("Plugins & Statut"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_PRICING_ACTIVE_EDITION"), QStringLiteral("[ÉDITION ACTIVE]"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_PRICING_TARGET_LABEL"), QStringLiteral("Pour qui :"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_TITLE"), QStringLiteral("Préférences"));
        QCOMPARE(QCoreApplication::translate("TimelineFilmstripWidget", "KEY_TIMELINE_ADD_SELECTION"), QStringLiteral("+ Ajouter la sélection"));
        QCOMPARE(QCoreApplication::translate("GitHistoryDock", "KEY_GIT_BTN_RESTORE"), QStringLiteral("Restaurer cette révision"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_UNTITLED_PROJECT"), QStringLiteral("Projet sans titre"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_LANG_HINT"), QStringLiteral("Les modifications de langue s'appliquent immédiatement."));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILTERS"), QStringLiteral("&Filtres"));
        QCOMPARE(QCoreApplication::translate("BackgroundRemovalDialog", "Background Removal"), QStringLiteral("Suppression d'arrière-plan"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Live Preview"), QStringLiteral("Aperçu en direct"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Auto-detect Sprite Boxes"), QStringLiteral("Détection auto des boîtes"));
        QCOMPARE(QCoreApplication::translate("ColorAdjustFilter", "Color Adjustment (HSV & Contrast)..."), QStringLiteral("Ajustement des Couleurs (HSV & Contraste)..."));
        QCOMPARE(QCoreApplication::translate("PixelRescaleFilter", "Pixel Art Rescale..."), QStringLiteral("Redimensionnement Pixel Art..."));
        QCOMPARE(QCoreApplication::translate("RetroPaletteFilter", "Retro Palette & Dithering..."), QStringLiteral("Palette Rétro & Tramage (Dithering)..."));
        QCOMPARE(QCoreApplication::translate("FilterRegistry", "Geometry & Transform"), QStringLiteral("Géométrie & Transformations"));
        QCOMPARE(QCoreApplication::translate("AtlasPackingFilter", "Atlas Bin-Packing (MaxRects)..."), QStringLiteral("Empaquetage d'Atlas (MaxRects)..."));
        QCOMPARE(QCoreApplication::translate("AtlasPackingDialog", "Atlas Bin-Packing (MaxRects)"), QStringLiteral("Empaquetage d'Atlas (MaxRects)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Export Atlas & Animations"), QStringLiteral("Exporter l'Atlas & les Animations"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Keep Current Layout (WYSIWYG — As Displayed)"), QStringLiteral("Conserver l'agencement actuel (WYSIWYG — Tel quel)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "MaxRects (Best Short Side Fit — Recommended)"), QStringLiteral("MaxRects (Best Short Side Fit — Recommandé)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Force Power of Two (2^n)"), QStringLiteral("Forcer la Puissance de Deux (2^n)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Packing Efficiency: Preserved as-is (WYSIWYG)"), QStringLiteral("Efficacité d'empaquetage : Conservée telle quelle (WYSIWYG)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Cancel"), QStringLiteral("Annuler"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Tight Mesh & 2D Polygon Packing"), QStringLiteral("Maillage polygonal 2D & Découpage serré"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Approximation Tolerance (ε):"), QStringLiteral("Tolérance d'approximation (ε) :"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Apply to Selection"), QStringLiteral("Appliquer à la sélection"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pixel Editor — BentoPack"), QStringLiteral("Éditeur de pixels — BentoPack"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pencil (1px continuous Bresenham) [P]"), QStringLiteral("Crayon (Bresenham 1px continu) [P]"));

        // Standard modal buttons (OK, Cancel, Discard, Save, Apply...)
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "OK"), QStringLiteral("OK"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QStringLiteral("Annuler"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Discard"), QStringLiteral("Ne pas enregistrer"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Save"), QStringLiteral("Enregistrer"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Apply"), QStringLiteral("Appliquer"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Cancel"), QStringLiteral("Annuler"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Cancel"), QStringLiteral("Annuler"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Apply"), QStringLiteral("Appliquer"));

        QCoreApplication::removeTranslator(&frTranslator);
    }

    // 2. Test English translation
    QTranslator enTranslator;
    bool enLoaded = loadCatalog(enTranslator, QStringLiteral("bentopack_en_US.qm"));
    if (!enLoaded) {
        QSKIP("Translation catalog bentopack_en_US.qm not found in build tree or resources on this platform.");
    }

    {
        QCoreApplication::installTranslator(&enTranslator);

        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILE"), QStringLiteral("File"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_VIEW"), QStringLiteral("View"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOLBAR_MAIN"), QStringLiteral("Main Toolbar"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_DOCK_PREVIEW"), QStringLiteral("Animation Preview"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_OPEN"), QStringLiteral("&Open"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_SAVE"), QStringLiteral("&Save"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOL_SELECT"), QStringLiteral("Select & Edit"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_CREATE_ANIM"), QStringLiteral("Create animation from selection"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_ADD_TO_ANIM"), QStringLiteral("Add to Animation"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_SELECT_ALL"), QStringLiteral("Select All"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_TITLE"), QStringLiteral("About"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_PRICING"), QStringLiteral("Pricing & Licensing"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_PLUGINS"), QStringLiteral("Plugins & Status"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_PRICING_ACTIVE_EDITION"), QStringLiteral("[ACTIVE EDITION]"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_PRICING_TARGET_LABEL"), QStringLiteral("Target:"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_TITLE"), QStringLiteral("Preferences"));
        QCOMPARE(QCoreApplication::translate("TimelineFilmstripWidget", "KEY_TIMELINE_ADD_SELECTION"), QStringLiteral("+ Add Selection"));
        QCOMPARE(QCoreApplication::translate("GitHistoryDock", "KEY_GIT_BTN_RESTORE"), QStringLiteral("Restore this revision"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_UNTITLED_PROJECT"), QStringLiteral("Untitled Project"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_LANG_HINT"), QStringLiteral("Language changes are applied immediately."));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILTERS"), QStringLiteral("&Filters"));
        QCOMPARE(QCoreApplication::translate("BackgroundRemovalDialog", "Background Removal"), QStringLiteral("Background Removal"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Live Preview"), QStringLiteral("Live Preview"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Auto-detect Sprite Boxes"), QStringLiteral("Auto-detect Sprite Boxes"));
        QCOMPARE(QCoreApplication::translate("ColorAdjustFilter", "Color Adjustment (HSV & Contrast)..."), QStringLiteral("Color Adjustment (HSV & Contrast)..."));
        QCOMPARE(QCoreApplication::translate("PixelRescaleFilter", "Pixel Art Rescale..."), QStringLiteral("Pixel Art Rescale..."));
        QCOMPARE(QCoreApplication::translate("RetroPaletteFilter", "Retro Palette & Dithering..."), QStringLiteral("Retro Palette & Dithering..."));
        QCOMPARE(QCoreApplication::translate("FilterRegistry", "Geometry & Transform"), QStringLiteral("Geometry & Transform"));
        QCOMPARE(QCoreApplication::translate("AtlasPackingFilter", "Atlas Bin-Packing (MaxRects)..."), QStringLiteral("Atlas Bin-Packing (MaxRects)..."));
        QCOMPARE(QCoreApplication::translate("AtlasPackingDialog", "Atlas Bin-Packing (MaxRects)"), QStringLiteral("Atlas Bin-Packing (MaxRects)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Export Atlas & Animations"), QStringLiteral("Export Atlas & Animations"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Keep Current Layout (WYSIWYG — As Displayed)"), QStringLiteral("Keep Current Layout (WYSIWYG — As Displayed)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "MaxRects (Best Short Side Fit — Recommended)"), QStringLiteral("MaxRects (Best Short Side Fit — Recommended)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Force Power of Two (2^n)"), QStringLiteral("Force Power of Two (2^n)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Packing Efficiency: Preserved as-is (WYSIWYG)"), QStringLiteral("Packing Efficiency: Preserved as-is (WYSIWYG)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Cancel"), QStringLiteral("Cancel"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Tight Mesh & 2D Polygon Packing"), QStringLiteral("Tight Mesh & 2D Polygon Packing"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Approximation Tolerance (ε):"), QStringLiteral("Approximation Tolerance (ε):"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Apply to Selection"), QStringLiteral("Apply to Selection"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pixel Editor — BentoPack"), QStringLiteral("Pixel Editor — BentoPack"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pencil (1px continuous Bresenham) [P]"), QStringLiteral("Pencil (1px continuous Bresenham) [P]"));

        // Standard modal buttons (OK, Cancel, Discard, Save, Apply...)
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "OK"), QStringLiteral("OK"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QStringLiteral("Cancel"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Discard"), QStringLiteral("Discard"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Save"), QStringLiteral("Save"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Apply"), QStringLiteral("Apply"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Cancel"), QStringLiteral("Cancel"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Cancel"), QStringLiteral("Cancel"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Apply"), QStringLiteral("Apply"));

        QCoreApplication::removeTranslator(&enTranslator);
    }

    // 3. Test Japanese translation
    QTranslator jaTranslator;
    bool jaLoaded = loadCatalog(jaTranslator, QStringLiteral("bentopack_ja_JA.qm"));
    if (!jaLoaded) {
        QSKIP("Translation catalog bentopack_ja_JA.qm not found in build tree or resources on this platform.");
    }

    {
        QCoreApplication::installTranslator(&jaTranslator);

        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILE"), QStringLiteral("ファイル"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_VIEW"), QStringLiteral("表示(&V)"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOLBAR_MAIN"), QStringLiteral("メインツールバー"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_DOCK_PREVIEW"), QStringLiteral("アニメーションプレビュー"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_OPEN"), QStringLiteral("開く(&O)"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_SAVE"), QStringLiteral("保存(&S)"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOL_SELECT"), QStringLiteral("選択・編集"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_CREATE_ANIM"), QStringLiteral("選択範囲からアニメーションを作成する"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_ADD_TO_ANIM"), QStringLiteral("アニメーションに追加"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_SELECT_ALL"), QStringLiteral("すべて選択"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_TITLE"), QStringLiteral("情報"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_PRICING"), QStringLiteral("価格とライセンス"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_PLUGINS"), QStringLiteral("プラグインと状態"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_PRICING_ACTIVE_EDITION"), QStringLiteral("[有効なエディション]"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_PRICING_TARGET_LABEL"), QStringLiteral("対象:"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_TITLE"), QStringLiteral("設定"));
        QCOMPARE(QCoreApplication::translate("TimelineFilmstripWidget", "KEY_TIMELINE_ADD_SELECTION"), QStringLiteral("+ 選択を追加"));
        QCOMPARE(QCoreApplication::translate("GitHistoryDock", "KEY_GIT_BTN_RESTORE"), QStringLiteral("このリビジョンを復元"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_UNTITLED_PROJECT"), QStringLiteral("無題のプロジェクト"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_LANG_HINT"), QStringLiteral("言語の変更は即座に適用されます。"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILTERS"), QStringLiteral("フィルター(&F)"));
        QCOMPARE(QCoreApplication::translate("BackgroundRemovalDialog", "Background Removal"), QStringLiteral("背景の削除"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Live Preview"), QStringLiteral("リアルタイムプレビュー"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Auto-detect Sprite Boxes"), QStringLiteral("スプライト枠の自動検出"));
        QCOMPARE(QCoreApplication::translate("ColorAdjustFilter", "Color Adjustment (HSV & Contrast)..."), QStringLiteral("カラー調整 (HSV・コントラスト)..."));
        QCOMPARE(QCoreApplication::translate("PixelRescaleFilter", "Pixel Art Rescale..."), QStringLiteral("ピクセルアートリサイズ..."));
        QCOMPARE(QCoreApplication::translate("RetroPaletteFilter", "Retro Palette & Dithering..."), QStringLiteral("レトロパレット＆ディザリング..."));
        QCOMPARE(QCoreApplication::translate("FilterRegistry", "Geometry & Transform"), QStringLiteral("ジオメトリと変形"));
        QCOMPARE(QCoreApplication::translate("AtlasPackingFilter", "Atlas Bin-Packing (MaxRects)..."), QStringLiteral("アトラスビンパッキング (MaxRects)..."));
        QCOMPARE(QCoreApplication::translate("AtlasPackingDialog", "Atlas Bin-Packing (MaxRects)"), QStringLiteral("アトラスビンパッキング (MaxRects)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Export Atlas & Animations"), QStringLiteral("アトラスとアニメーションのエクスポート"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Keep Current Layout (WYSIWYG — As Displayed)"), QStringLiteral("現在の配置を維持 (WYSIWYG — 表示通り)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "MaxRects (Best Short Side Fit — Recommended)"), QStringLiteral("MaxRects (Best Short Side Fit — 推奨)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Force Power of Two (2^n)"), QStringLiteral("2の累乗サイズに強制 (2^n)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Packing Efficiency: Preserved as-is (WYSIWYG)"), QStringLiteral("充填効率：そのまま維持 (WYSIWYG)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Cancel"), QStringLiteral("キャンセル"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Tight Mesh & 2D Polygon Packing"), QStringLiteral("2Dポリゴンメッシュ＆タイトパッキング"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Approximation Tolerance (ε):"), QStringLiteral("近似許容値 (ε)："));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Apply to Selection"), QStringLiteral("選択範囲に適用"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Target Frame %1: Rectangle mode (no mesh applied)"), QStringLiteral("対象フレーム %1：矩形モード（メッシュ未適用）"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pixel Editor — BentoPack"), QStringLiteral("ピクセルエディタ — BentoPack"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pencil (1px continuous Bresenham) [P]"), QStringLiteral("鉛筆（1px連続ブレゼンハム）[P]"));

        // Standard modal buttons (OK, Cancel, Discard, Save, Apply...)
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "OK"), QStringLiteral("OK"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QStringLiteral("キャンセル"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Discard"), QStringLiteral("破棄"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Save"), QStringLiteral("保存"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Apply"), QStringLiteral("適用"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Cancel"), QStringLiteral("キャンセル"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Cancel"), QStringLiteral("キャンセル"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Apply"), QStringLiteral("適用"));

        QCoreApplication::removeTranslator(&jaTranslator);
    }

    // 4. Test ProjectController::currentProjectName() returns translated title
    {
        SpriteDocument doc;
        QUndoStack undo;
        ProjectController pc(&doc, &undo);

        if (frLoaded) {
            QCoreApplication::installTranslator(&frTranslator);
            QCOMPARE(pc.currentProjectName(), QStringLiteral("Projet sans titre"));
            QCoreApplication::removeTranslator(&frTranslator);
        }

        if (enLoaded) {
            QCoreApplication::installTranslator(&enTranslator);
            QCOMPARE(pc.currentProjectName(), QStringLiteral("Untitled Project"));
            QCoreApplication::removeTranslator(&enTranslator);
        }

        if (jaLoaded) {
            QCoreApplication::installTranslator(&jaTranslator);
            QCOMPARE(pc.currentProjectName(), QStringLiteral("無題のプロジェクト"));
            QCoreApplication::removeTranslator(&jaTranslator);
        }
    }

    // 5. Test Untranslated Key Fallback
    // Missing keys must clearly show visually that they are keys and not final text
    QString untranslated = QCoreApplication::translate("MainWindow", "KEY_UNKNOWN_FEATURE");
    QCOMPARE(untranslated, QStringLiteral("KEY_UNKNOWN_FEATURE"));
    QVERIFY(untranslated.startsWith(QStringLiteral("KEY_")));
#endif
}

void TestControllerAtlas::testAtlasBoxItemHandleCosmeticSize()
{
    // Test on micro-sprite (16x16) and normal sprite (64x64)
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.show();

    AtlasBoxItem item16(0, QRect(0, 0, 16, 16), QRect(0, 0, 256, 256));
    scene.addItem(&item16);
    item16.setSelectedBox(true);

    // 1. Test at 1x zoom (scale = 1.0)
    view.resetTransform();
    double size1x = item16.currentHandleSize();
    // 16 * 0.35 = 5.6, so size is capped to 5.6 to prevent overlapping handles on small sprites
    QVERIFY(size1x <= 5.6);
    QVERIFY(size1x >= 1.0);

    // 2. Test at 4x zoom (scale = 4.0)
    view.scale(4.0, 4.0);
    double size4x = item16.currentHandleSize();
    // In scene coords, size4x should be 8.0 / 4.0 = 2.0
    QCOMPARE(size4x, 2.0);

    // 3. Test at 16x zoom (scale = 16.0)
    view.resetTransform();
    view.scale(16.0, 16.0);
    double size16x = item16.currentHandleSize();
    // In scene coords, size16x should be 8.0 / 16.0 = 0.5
    QCOMPARE(size16x, 0.5);

    // 4. Test boundingRect and shape at 16x
    QRectF br = item16.boundingRect();
    QVERIFY(br.contains(item16.boxRect()));
    QPainterPath sp = item16.shape();
    QVERIFY(!sp.isEmpty());
}

void TestControllerAtlas::testAtlasBoxItemPivotDrag()
{
    QRect bounds(0, 0, 200, 200);
    AtlasBoxItem item(0, QRect(10, 10, 50, 50), bounds);

    // Initial default pivot: (25, 50)
    QCOMPARE(item.boxPivot(), QPoint(25, 50));
    QCOMPARE(item.hasCustomPivot(), false);

    // Set custom pivot
    item.setBoxPivot(QPoint(15, 30), true);
    QCOMPARE(item.boxPivot(), QPoint(15, 30));
    QCOMPARE(item.hasCustomPivot(), true);
}

void TestControllerAtlas::testAtlasViewControllerGroupDrag()
{
    QGraphicsView view;
    SpriteDocument doc;
    QUndoStack undoStack;
    AtlasViewController controller(&view, &doc, &undoStack);

    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // Add 3 frames
    doc.addSlice(QRect(10, 10, 20, 20));
    doc.addSlice(QRect(40, 10, 20, 20));
    doc.addSlice(QRect(70, 10, 20, 20));
    QCOMPARE(doc.frameCount(), 3);

    // Select all 3 frames
    controller.setSelectedBoxIndices({0, 1, 2});
    QCOMPARE(controller.selectedBoxIndices().size(), 3);

    // Move group by (15, 25)
    controller.moveSelectedBoxes(15, 25);

    QCOMPARE(doc.box(0).rect, QRect(25, 35, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(55, 35, 20, 20));
    QCOMPARE(doc.box(2).rect, QRect(85, 35, 20, 20));

    // Test Undo
    QVERIFY(undoStack.canUndo());
    undoStack.undo();

    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(40, 10, 20, 20));
    QCOMPARE(doc.box(2).rect, QRect(70, 10, 20, 20));

    // Test Redo
    QVERIFY(undoStack.canRedo());
    undoStack.redo();

    QCOMPARE(doc.box(0).rect, QRect(25, 35, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(55, 35, 20, 20));
    QCOMPARE(doc.box(2).rect, QRect(85, 35, 20, 20));

    // Test Boundary Clamping: try to move beyond atlas width (200)
    controller.moveSelectedBoxes(500, 0);
    QVERIFY(doc.box(2).rect.right() <= atlas.rect().right());
    QVERIFY(doc.box(0).rect.left() >= atlas.rect().left());
}

void TestControllerAtlas::testAtlasViewControllerContinuousSlice()
{
    QGraphicsView view;
    view.resize(400, 400);
    view.show();

    SpriteDocument doc;
    QUndoStack undoStack;
    AtlasViewController controller(&view, &doc, &undoStack);

    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // Set ToolAddSlice mode
    controller.setToolMode(AtlasViewController::ToolAddSlice);
    QCOMPARE(controller.toolMode(), AtlasViewController::ToolAddSlice);

    // 1. Draw without Shift -> auto-switches to ToolSelect
    QPoint p1 = view.mapFromScene(QPointF(20, 20));
    QPoint p2 = view.mapFromScene(QPointF(80, 80));

    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, p1);
    QTest::mouseMove(view.viewport(), p2);
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, p2);

    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(controller.toolMode(), AtlasViewController::ToolSelect);

    // 2. Draw WITH Shift -> remains in ToolAddSlice mode for rapid chaining
    controller.setToolMode(AtlasViewController::ToolAddSlice);

    QPoint p3 = view.mapFromScene(QPointF(100, 100));
    QPoint p4 = view.mapFromScene(QPointF(160, 160));

    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::ShiftModifier, p3);
    QTest::mouseMove(view.viewport(), p4);
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::ShiftModifier, p4);

    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(controller.toolMode(), AtlasViewController::ToolAddSlice);
}

void TestControllerAtlas::testDockStatePersistence()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString iniPath = tempDir.filePath(QStringLiteral("dock_settings.ini"));
    QSettings settings(iniPath, QSettings::IniFormat);
    settings.clear();

    // 1. Setup main window with two named dock widgets
    QMainWindow mw;
    mw.setObjectName(QStringLiteral("MainWindow"));

    QDockWidget *dock1 = new QDockWidget(QStringLiteral("Preview"), &mw);
    dock1->setObjectName(QStringLiteral("dockPreview"));
    mw.addDockWidget(Qt::RightDockWidgetArea, dock1);

    QDockWidget *dock2 = new QDockWidget(QStringLiteral("Timeline"), &mw);
    dock2->setObjectName(QStringLiteral("dockTimeline"));
    mw.addDockWidget(Qt::BottomDockWidgetArea, dock2);

    mw.resize(800, 600);
    mw.show();
    dock1->show();
    dock2->show();

    // Save initial state
    QByteArray initialState = mw.saveState();
    settings.setValue(QStringLiteral("mainWindow/windowState"), initialState);
    settings.sync();
    QVERIFY(!initialState.isEmpty());

    // 2. Modify layout: hide dock1, move dock2 to top
    dock1->hide();
    mw.addDockWidget(Qt::TopDockWidgetArea, dock2);
    QVERIFY(dock1->isHidden());
    QCOMPARE(mw.dockWidgetArea(dock2), Qt::TopDockWidgetArea);

    // 3. Restore initial state from QSettings
    QByteArray restoredState = settings.value(QStringLiteral("mainWindow/windowState")).toByteArray();
    QCOMPARE(restoredState, initialState);
    bool ok = mw.restoreState(restoredState);
    QVERIFY(ok);

    // Verify dock1 is restored visible and dock2 is back at BottomDockWidgetArea
    QVERIFY(!dock1->isHidden());
    QCOMPARE(mw.dockWidgetArea(dock2), Qt::BottomDockWidgetArea);

    // Clean up test settings
    settings.clear();
}

void TestControllerAtlas::testSelectionOrderPreserved()
{
    // 1. SpriteDocument selection order
    SpriteDocument doc;
    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    for (int i = 0; i < 6; ++i) {
        doc.addSlice(QRect(i * 20, 0, 20, 20));
    }
    QCOMPARE(doc.frameCount(), 6);

    // Explicitly set arbitrary selection order [4, 2, 5, 1]
    QList<int> customOrder = {4, 2, 5, 1};
    doc.setSelectedFrameIndices(customOrder);
    QCOMPARE(doc.selectedFrameIndices(), customOrder);

    // Adding frame 3 appends to selection
    doc.setBoxSelection(3, true);
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{4, 2, 5, 1, 3}));

    // Removing frame 2 preserves the order of remaining elements
    doc.setBoxSelection(2, false);
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{4, 5, 1, 3}));

    // 2. AtlasViewController interactive click selection order
    QGraphicsView view;
    AtlasViewController atlasCtrl(&view, &doc);
    atlasCtrl.setAtlasImage(atlas);
    QCOMPARE(atlasCtrl.boxCount(), 6);

    // Initial click on frame 4
    emit atlasCtrl.boxItems()[4]->boxSelected(4, true, Qt::NoModifier);
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{4}));

    // Ctrl-click on frame 2
    emit atlasCtrl.boxItems()[2]->boxSelected(2, true, Qt::ControlModifier);
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{4, 2}));

    // Ctrl-click on frame 5
    emit atlasCtrl.boxItems()[5]->boxSelected(5, true, Qt::ControlModifier);
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{4, 2, 5}));

    // Ctrl-click on frame 1
    emit atlasCtrl.boxItems()[1]->boxSelected(1, true, Qt::ControlModifier);
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{4, 2, 5, 1}));
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{4, 2, 5, 1}));

    // Ctrl-click to deselect frame 2
    emit atlasCtrl.boxItems()[2]->boxSelected(2, true, Qt::ControlModifier);
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{4, 5, 1}));
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{4, 5, 1}));

    // 3. AnimationController with selection order
    AnimationPlayer player;
    AnimationController animCtrl(&doc, nullptr, &player);

    animCtrl.updateCurrentAnimation(QList<int>{4, 2, 5, 1});
    QVERIFY(doc.hasAnimation(QStringLiteral("current")));
    QCOMPARE(doc.animation(QStringLiteral("current")).frameIndices, (QList<int>{4, 2, 5, 1}));

    animCtrl.createAnimation(QStringLiteral("ordered_anim"), QList<int>{4, 2, 5, 1});
    QVERIFY(doc.hasAnimation(QStringLiteral("ordered_anim")));
    QCOMPARE(doc.animation(QStringLiteral("ordered_anim")).frameIndices, (QList<int>{4, 2, 5, 1}));
}

int main(int argc, char *argv[])
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
    qputenv("QT_FORCE_STDERR_LOGGING", "1");

    QApplication app(argc, argv);
    TestControllerAtlas tc;

    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args << QString::fromLocal8Bit(argv[i]);
    }
    if (!args.contains(QStringLiteral("-o"))) {
        args << QStringLiteral("-o") << QStringLiteral("-,txt");
    }

    int result = QTest::qExec(&tc, args);
    std::fflush(stdout);
    std::fflush(stderr);
    return result;
}

#include "test_controller_atlas.moc"
