#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QImage>
#include <QUndoStack>

#include "model/spritedocument.h"
#include "controller/projectcontroller.h"
#include "extractor/extractorregistry.h"
#include "license/integrityguard.h"
#include "license/licensemanager.h"

class TestConcurrencyAndSecurity : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Concurrency & Async Safety
    void testAsyncBackgroundRemovalSafety();
    void testAsyncOpenSafety();
    void testInterleavedAsyncOperations();

    // Security & IntegrityGuard
    void testSteganographicWatermarkCommunity();
    void testSteganographicWatermarkTampered();
    void testLayoutSignatureHMAC();
    void testTamperDetectionSimulation();
};

void TestConcurrencyAndSecurity::initTestCase()
{
    QString binPlugins = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));
    ExtractorRegistry::instance().loadPlugins(binPlugins);
    ExtractorRegistry::instance().loadPlugins(QCoreApplication::applicationDirPath());
}

void TestConcurrencyAndSecurity::cleanupTestCase()
{
    BentoPack::IntegrityGuard::setSimulatedTampered(false);
}

void TestConcurrencyAndSecurity::testAsyncBackgroundRemovalSafety()
{
    SpriteDocument doc;
    QImage img(100, 100, QImage::Format_ARGB32);
    img.fill(qRgb(0, 255, 0)); // Pure green background
    // Draw some red content
    for (int y = 20; y < 80; ++y) {
        for (int x = 20; x < 80; ++x) {
            img.setPixel(x, y, qRgb(255, 0, 0));
        }
    }
    doc.setAtlas(img);

    QUndoStack stack;
    ProjectController pc(&doc, &stack);

    QSignalSpy spyStarted(&pc, &ProjectController::processingStarted);
    QSignalSpy spyFinished(&pc, &ProjectController::processingFinished);
    QSignalSpy spyBg(&pc, &ProjectController::backgroundRemoved);

    // Launch async background removal
    pc.removeAtlasBackgroundAndRefreshAsync(10, 5, false, 0.5);

    QCOMPARE(spyStarted.count(), 1);

    // Wait for the async worker to finish without hanging or crashing
    QVERIFY(spyFinished.wait(5000));
    QCOMPARE(spyFinished.count(), 1);
    QCOMPARE(spyBg.count(), 1);

    // Verify background removal succeeded
    QVERIFY(!doc.atlas().isNull());
}

void TestConcurrencyAndSecurity::testAsyncOpenSafety()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString testImgPath = tempDir.filePath(QStringLiteral("test_async.png"));

    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    QVERIFY(img.save(testImgPath));

    SpriteDocument doc;
    QUndoStack stack;
    ProjectController pc(&doc, &stack);

    QSignalSpy spyStarted(&pc, &ProjectController::processingStarted);
    QSignalSpy spyFinished(&pc, &ProjectController::processingFinished);
    QSignalSpy spyLoaded(&pc, &ProjectController::fileLoaded);

    pc.openFileAsync(testImgPath);

    QCOMPARE(spyStarted.count(), 1);
    QVERIFY(spyFinished.wait(5000));
    QCOMPARE(spyFinished.count(), 1);
    QCOMPARE(spyLoaded.count(), 1);
    QCOMPARE(doc.atlas().size(), QSize(64, 64));
}

void TestConcurrencyAndSecurity::testInterleavedAsyncOperations()
{
    // Test that launching an async job while another is potentially queued doesn't deadlock
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::magenta);
    doc.setAtlas(img);

    QUndoStack stack;
    ProjectController pc(&doc, &stack);

    QSignalSpy spyFinished(&pc, &ProjectController::processingFinished);

    pc.removeAtlasBackgroundAndRefreshAsync();
    // Immediate new project reset while worker is in flight
    pc.newProject();

    // Worker completes gracefully
    QVERIFY(spyFinished.wait(5000));
}

void TestConcurrencyAndSecurity::testSteganographicWatermarkCommunity()
{
    BentoPack::IntegrityGuard::setSimulatedTampered(false);

    QImage img(10, 10, QImage::Format_ARGB32);
    img.fill(qRgba(0, 0, 0, 0)); // Alpha == 0
    img.setPixel(5, 5, qRgb(255, 0, 0)); // Opaque pixel

    BentoPack::IntegrityGuard::applySteganographicWatermark(img);

    // Opaque pixel must remain unmodified
    QCOMPARE(img.pixel(5, 5), qRgb(255, 0, 0));

    // Transparent pixel (0, 0) should still have Alpha == 0 but embed watermark magic bytes
    QRgb transparentPix = img.pixel(0, 0);
    QCOMPARE(qAlpha(transparentPix), 0);

    // Check magic bytes in Community mode (non-zero RGB channels)
    if (!BentoPack::IntegrityGuard::isCommercialAuthentic()) {
        QCOMPARE(transparentPix & 0x00FFFFFF, BentoPack::IntegrityGuard::MAGIC_COMMUNITY_ALPHA0 & 0x00FFFFFF);
    }
}

void TestConcurrencyAndSecurity::testSteganographicWatermarkTampered()
{
    BentoPack::IntegrityGuard::setSimulatedTampered(true);

    QImage img(10, 10, QImage::Format_ARGB32);
    img.fill(qRgba(0, 0, 0, 0));

    BentoPack::IntegrityGuard::applySteganographicWatermark(img);

    QRgb transparentPix = img.pixel(0, 0);
    QCOMPARE(qAlpha(transparentPix), 0);
    QCOMPARE(transparentPix & 0x00FFFFFF, BentoPack::IntegrityGuard::MAGIC_TAMPERED_ALPHA0 & 0x00FFFFFF);

    // Reset
    BentoPack::IntegrityGuard::setSimulatedTampered(false);
}

void TestConcurrencyAndSecurity::testLayoutSignatureHMAC()
{
    QString payload = QStringLiteral("{\"atlas\":\"hero.png\",\"frames\":[{\"rect\":[0,0,32,32]}]}");

    QString signature = BentoPack::IntegrityGuard::computeLayoutSignature(payload);
    QVERIFY(!signature.isEmpty());

    // Valid signature verification
    QVERIFY(BentoPack::IntegrityGuard::verifyLayoutSignature(payload, signature));

    // Invalid / forged signatures rejected
    QVERIFY(!BentoPack::IntegrityGuard::verifyLayoutSignature(payload, QStringLiteral("tampered-tamper-detected")));
    QVERIFY(!BentoPack::IntegrityGuard::verifyLayoutSignature(payload, QStringLiteral("comm-forged_signature_xyz")));
    QVERIFY(!BentoPack::IntegrityGuard::verifyLayoutSignature(payload, QString()));

    // When simulated tampered is active, computed signature is recognized as tampered and fails verification
    BentoPack::IntegrityGuard::setSimulatedTampered(true);
    QString tamperedSig = BentoPack::IntegrityGuard::computeLayoutSignature(payload);
    QVERIFY(!BentoPack::IntegrityGuard::verifyLayoutSignature(payload, tamperedSig));
    BentoPack::IntegrityGuard::setSimulatedTampered(false);
}

void TestConcurrencyAndSecurity::testTamperDetectionSimulation()
{
    BentoPack::IntegrityGuard::setSimulatedTampered(false);
    QCOMPARE(BentoPack::IntegrityGuard::isTampered(), false);

    BentoPack::IntegrityGuard::setSimulatedTampered(true);
    QCOMPARE(BentoPack::IntegrityGuard::isTampered(), true);

    BentoPack::IntegrityGuard::setSimulatedTampered(false);
    QCOMPARE(BentoPack::IntegrityGuard::isTampered(), false);
}

QTEST_MAIN(TestConcurrencyAndSecurity)
#include "test_concurrency_and_security.moc"
