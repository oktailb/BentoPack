#include "include/controller/projectcontroller.h"
#include "include/model/spritedocument.h"
#include "include/extractor/extractorregistry.h"
#include "include/extractor/spriteextractor.h"
#include "include/image/spritedetector.h"
#include "include/config/appconfig.h"
#include "include/project/sessionmanager.h"
#include "include/project/projectmanager.h"
#include <QUndoStack>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QHash>
#include <QtConcurrent>
#include <cmath>
#include <vector>

ProjectController::ProjectController(SpriteDocument *document, QUndoStack *undoStack, QObject *parent)
    : QObject(parent)
    , m_document(document)
    , m_undoStack(undoStack)
    , m_sessionManager(std::make_unique<SessionManager>(this))
{
    // Ensure extractor registry is initialized
    ExtractorRegistry::instance();

    connect(&m_watcher, &QFutureWatcher<AsyncExtractionResult>::finished,
            this, &ProjectController::onAsyncJobFinished);

    if (m_document) {
        connect(m_document, &SpriteDocument::framesChanged, this, [this]() {
            setProjectModified(true);
        });
        connect(m_document, &SpriteDocument::atlasChanged, this, [this]() {
            setProjectModified(true);
        });
        connect(m_document, &SpriteDocument::animationsChanged, this, [this]() {
            setProjectModified(true);
        });
    }

    if (m_undoStack) {
        connect(m_undoStack, &QUndoStack::cleanChanged, this, [this](bool clean) {
            setProjectModified(!clean);
        });
        connect(m_undoStack, &QUndoStack::indexChanged, this, &ProjectController::onUndoStackIndexChanged);
    }
}

ProjectController::~ProjectController() = default;

QString ProjectController::currentFilePath() const
{
    return m_currentFilePath;
}

void ProjectController::setCurrentFilePath(const QString &filePath)
{
    m_currentFilePath = filePath;
}

QString ProjectController::currentProjectPath() const
{
    return m_currentProjectPath;
}

QString ProjectController::currentProjectName() const
{
    if (!m_currentProjectPath.isEmpty()) {
        return QFileInfo(m_currentProjectPath).fileName();
    }
    if (m_document && !m_document->projectName().isEmpty()) {
        return m_document->projectName();
    }
    return tr("Untitled Project");
}

void ProjectController::setProjectModified(bool modified)
{
    if (m_isModified != modified) {
        m_isModified = modified;
        emit projectModifiedChanged(m_isModified);
    }
}

bool ProjectController::newProject()
{
    m_isProjectLoading = true;
    if (m_document) {
        m_document->clear();
    }
    if (m_undoStack) {
        m_undoStack->clear();
    }
    m_lastUndoIndex = 0;
    m_currentProjectPath.clear();
    m_currentFilePath.clear();

    if (m_sessionManager) {
        m_sessionManager->startNewSession();
    }

    setProjectModified(false);
    m_isProjectLoading = false;
    emit statusMessage(tr("New project created."));
    emit projectLoaded(QString());
    return true;
}

bool ProjectController::openProject(const QString &sspPath, QString *errorMsg)
{
    m_isProjectLoading = true;
    if (sspPath.isEmpty() || !QFile::exists(sspPath)) {
        QString err = tr("Project file does not exist: %1").arg(sspPath);
        if (errorMsg) *errorMsg = err;
        emit fileLoadError(sspPath, err);
        m_isProjectLoading = false;
        return false;
    }

    if (!m_document) {
        QString err = tr("No active SpriteDocument.");
        if (errorMsg) *errorMsg = err;
        emit fileLoadError(sspPath, err);
        m_isProjectLoading = false;
        return false;
    }

    emit statusMessage(tr("Opening project %1...").arg(QFileInfo(sspPath).fileName()));

    if (!m_sessionManager->openSessionFromSsp(sspPath, errorMsg)) {
        emit fileLoadError(sspPath, errorMsg ? *errorMsg : tr("Failed to open project."));
        m_isProjectLoading = false;
        return false;
    }

    double zoom = 1.0;
    QPointF pan(0, 0);
    if (!ProjectManager::loadProjectFromSessionDir(m_sessionManager->currentSessionDir(), *m_document, &zoom, &pan, errorMsg)) {
        emit fileLoadError(sspPath, errorMsg ? *errorMsg : tr("Failed to deserialize project."));
        m_isProjectLoading = false;
        return false;
    }

    m_currentProjectPath = QFileInfo(sspPath).absoluteFilePath();
    m_currentFilePath = m_currentProjectPath;
    addRecentProject(m_currentProjectPath);

    if (m_undoStack) {
        m_undoStack->clear();
    }
    m_lastUndoIndex = 0;

    setProjectModified(false);
    m_isProjectLoading = false;
    emit statusMessage(tr("Project loaded successfully: %1").arg(QFileInfo(sspPath).fileName()));
    emit projectLoaded(m_currentProjectPath);
    emit fileLoaded(m_currentProjectPath);
    return true;
}

bool ProjectController::saveProject(const QString &sspPath, QString *errorMsg)
{
    QString targetPath = sspPath.isEmpty() ? m_currentProjectPath : sspPath;
    if (targetPath.isEmpty()) {
        QString err = tr("Project file path is empty. Use Save As.");
        if (errorMsg) *errorMsg = err;
        return false;
    }

    if (!targetPath.endsWith(QStringLiteral(".ssp"), Qt::CaseInsensitive)) {
        targetPath += QStringLiteral(".ssp");
    }

    if (!m_document) {
        QString err = tr("No active SpriteDocument.");
        if (errorMsg) *errorMsg = err;
        return false;
    }

    emit statusMessage(tr("Saving project %1...").arg(QFileInfo(targetPath).fileName()));

    if (!m_sessionManager->hasActiveSession()) {
        m_sessionManager->startNewSession(targetPath);
    }

    // Save document state and atlas to session workspace
    if (!ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir(), 1.0, QPointF(0, 0), errorMsg)) {
        return false;
    }

    // Pack workspace to .ssp atomically
    if (!m_sessionManager->saveSessionToSsp(targetPath, errorMsg)) {
        return false;
    }

    m_currentProjectPath = QFileInfo(targetPath).absoluteFilePath();
    m_currentFilePath = m_currentProjectPath;
    addRecentProject(m_currentProjectPath);

    if (m_undoStack) {
        m_undoStack->setClean();
    }

    setProjectModified(false);
    emit statusMessage(tr("Project saved successfully: %1").arg(QFileInfo(targetPath).fileName()));
    emit projectSaved(m_currentProjectPath);
    return true;
}

bool ProjectController::saveProjectAs(const QString &sspPath, QString *errorMsg)
{
    return saveProject(sspPath, errorMsg);
}

bool ProjectController::restoreSession(const QString &sessionDir, QString *errorMsg)
{
    m_isProjectLoading = true;
    if (!m_document) {
        if (errorMsg) *errorMsg = tr("No active SpriteDocument.");
        m_isProjectLoading = false;
        return false;
    }

    emit statusMessage(tr("Restoring session from %1...").arg(QFileInfo(sessionDir).fileName()));

    if (!m_sessionManager->restoreOrphanSession(sessionDir, errorMsg)) {
        m_isProjectLoading = false;
        return false;
    }

    double zoom = 1.0;
    QPointF pan(0, 0);
    if (!ProjectManager::loadProjectFromSessionDir(sessionDir, *m_document, &zoom, &pan, errorMsg)) {
        m_isProjectLoading = false;
        return false;
    }

    m_currentProjectPath = m_sessionManager->currentOriginalFilePath();
    m_currentFilePath = m_currentProjectPath;

    if (m_undoStack) {
        m_undoStack->clear();
    }
    m_lastUndoIndex = 0;

    // Mark as modified so the user can immediately save it
    setProjectModified(true);
    m_isProjectLoading = false;
    emit statusMessage(tr("Session recovered successfully."));
    emit projectLoaded(m_currentProjectPath);
    emit fileLoaded(m_currentProjectPath);
    return true;
}

QStringList ProjectController::recentProjects() const
{
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    QStringList files = settings.value(QStringLiteral("recentProjects")).toStringList();

    QStringList existingFiles;
    for (const QString &f : files) {
        if (QFile::exists(f)) {
            existingFiles.append(f);
        }
    }
    return existingFiles;
}

void ProjectController::addRecentProject(const QString &filePath)
{
    if (filePath.isEmpty()) return;

    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    QStringList files = settings.value(QStringLiteral("recentProjects")).toStringList();
    files.removeAll(filePath);
    files.prepend(filePath);
    int maxFiles = AppConfig::instance().project().maxRecentFiles;
    while (files.size() > maxFiles) {
        files.removeLast();
    }
    settings.setValue(QStringLiteral("recentProjects"), files);
    emit recentProjectsChanged(recentProjects());
}

void ProjectController::clearRecentProjects()
{
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    settings.remove(QStringLiteral("recentProjects"));
    emit recentProjectsChanged(QStringList());
}

bool ProjectController::openFile(const QString &filePath, QString *errorMsg)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        QString err = tr("File does not exist: %1").arg(filePath);
        if (errorMsg) *errorMsg = err;
        emit fileLoadError(filePath, err);
        return false;
    }

    if (filePath.endsWith(QStringLiteral(".ssp"), Qt::CaseInsensitive)) {
        return openProject(filePath, errorMsg);
    }

    Extractor *extractor = ExtractorRegistry::instance().findDecoder(filePath);
    if (!extractor) {
        QString err = tr("No suitable codec found for file: %1").arg(filePath);
        if (errorMsg) *errorMsg = err;
        emit fileLoadError(filePath, err);
        return false;
    }

    if (!m_document) {
        QString err = tr("No active SpriteDocument.");
        if (errorMsg) *errorMsg = err;
        emit fileLoadError(filePath, err);
        return false;
    }

    ExtractorError err;
    emit statusMessage(tr("Loading %1...").arg(QFileInfo(filePath).fileName()));

    if (!extractor->read(filePath, *m_document, &err)) {
        QString fullError = err.toString();
        if (errorMsg) *errorMsg = fullError;
        emit fileLoadError(filePath, fullError);
        return false;
    }

    m_currentFilePath = filePath;
    m_currentProjectPath.clear();
    addRecentFile(filePath);

    m_isProjectLoading = true;
    if (m_sessionManager) {
        m_sessionManager->startNewSession();
        ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir());
        m_sessionManager->gitCommit(tr("Import %1").arg(QFileInfo(filePath).fileName()));
    }

    if (m_undoStack) {
        m_undoStack->clear();
    }
    m_lastUndoIndex = 0;

    setProjectModified(false);
    m_isProjectLoading = false;
    emit statusMessage(tr("Loaded %1 successfully.").arg(QFileInfo(filePath).fileName()));
    emit fileLoaded(filePath);
    return true;
}

void ProjectController::openFileAsync(const QString &filePath)
{
    if (m_isProcessing) return;

    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        QString err = tr("File does not exist: %1").arg(filePath);
        emit fileLoadError(filePath, err);
        return;
    }

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    // If it's a native project (.ssp)
    if (ext == QStringLiteral("ssp")) {
        openProject(filePath);
        return;
    }

    // If it's a non-image file (e.g. JSON or GIF), fallback to synchronous read
    if (ext == QStringLiteral("json") || ext == QStringLiteral("tres") || ext == QStringLiteral("gif")) {
        emit processingStarted();
        openFile(filePath);
        emit processingFinished();
        return;
    }

    Extractor *extractor = ExtractorRegistry::instance().findDecoder(filePath);
    if (!extractor) {
        QString err = tr("No suitable codec found for file: %1").arg(filePath);
        emit fileLoadError(filePath, err);
        return;
    }

    m_isProcessing = true;
    emit processingStarted();
    emit statusMessage(tr("Loading %1 in background...").arg(fi.fileName()));
    emit progressChanged(20);

    QFuture<AsyncExtractionResult> future = QtConcurrent::run([filePath]() -> AsyncExtractionResult {
        AsyncExtractionResult result;
        result.type = AsyncExtractionResult::JobOpen;
        result.filePath = filePath;

        QImage image(filePath);
        if (image.isNull()) {
            result.success = false;
            result.errorMessage = QObject::tr("Failed to decode image from: %1").arg(filePath);
            return result;
        }

        SpriteDetectionOptions detOpts;
        if (!SpriteDetector::detectToImages(image, result.frameImages, result.boxes, detOpts)) {
            result.success = false;
            result.errorMessage = QObject::tr("Failed to segment sprite frames.");
            return result;
        }

        result.atlas = (image.format() == QImage::Format_ARGB32) ? image : image.convertToFormat(QImage::Format_ARGB32);
        result.success = true;
        return result;
    });

    m_watcher.setFuture(future);
}

bool ProjectController::save(const QString &filePath, QString *errorMsg)
{
    return exportData(filePath, ExportOptions{}, errorMsg);
}

bool ProjectController::exportData(const QString &filePath, const ExportOptions &options, QString *errorMsg)
{
    if (filePath.isEmpty()) {
        QString err = tr("File path is empty.");
        if (errorMsg) *errorMsg = err;
        return false;
    }

    if (!m_document || m_document->isEmpty()) {
        QString err = tr("Document is empty.");
        if (errorMsg) *errorMsg = err;
        return false;
    }

    Extractor *extractor = ExtractorRegistry::instance().findEncoder(filePath);
    if (!extractor) {
        QString err = tr("No suitable exporter found for format: %1").arg(filePath);
        if (errorMsg) *errorMsg = err;
        return false;
    }

    ExtractorError err;
    emit statusMessage(tr("Saving %1...").arg(QFileInfo(filePath).fileName()));

    if (!extractor->write(filePath, *m_document, options, &err)) {
        QString fullError = err.toString();
        if (errorMsg) *errorMsg = fullError;
        return false;
    }

    m_currentFilePath = filePath;
    addRecentFile(filePath);
    emit statusMessage(tr("Saved %1 successfully.").arg(QFileInfo(filePath).fileName()));
    emit fileSaved(filePath);
    return true;
}

QStringList ProjectController::recentFiles() const
{
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    QStringList files = settings.value(QStringLiteral("recentFiles")).toStringList();

    QStringList existingFiles;
    for (const QString &f : files) {
        if (QFile::exists(f)) {
            existingFiles.append(f);
        }
    }
    return existingFiles;
}

void ProjectController::addRecentFile(const QString &filePath)
{
    if (filePath.isEmpty()) return;

    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    QStringList files = settings.value(QStringLiteral("recentFiles")).toStringList();
    files.removeAll(filePath);
    files.prepend(filePath);
    int maxFiles = AppConfig::instance().project().maxRecentFiles;
    while (files.size() > maxFiles) {
        files.removeLast();
    }
    settings.setValue(QStringLiteral("recentFiles"), files);
    emit recentFilesChanged(recentFiles());
}

void ProjectController::clearRecentFiles()
{
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    settings.remove(QStringLiteral("recentFiles"));
    emit recentFilesChanged(QStringList());
}

QImage ProjectController::removeBackgroundFromImage(const QImage &srcImage, int tolerance)
{
    if (srcImage.isNull()) return QImage();

    if (tolerance < 0) {
        tolerance = AppConfig::instance().project().backgroundRemovalTolerance;
    }
    const int minAlpha = AppConfig::instance().project().backgroundMinAlpha;

    QImage image = (srcImage.format() == QImage::Format_ARGB32)
        ? srcImage.copy()
        : srcImage.convertToFormat(QImage::Format_ARGB32);

    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) return image;

    std::vector<const QRgb*> constScanLines(h);
    for (int y = 0; y < h; ++y) {
        constScanLines[y] = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    }

    // Find the most frequent color by sampling with O(1) hash map
    QHash<QRgb, int> histogram;
    histogram.reserve(4096);
    int maxCount = 0;
    QRgb backgroundColor = 0;

    for (int y = 0; y < h; y += 2) {
        const QRgb *line = constScanLines[y];
        for (int x = 0; x < w; x += 2) {
            QRgb pixel = line[x];
            if (qAlpha(pixel) < minAlpha) continue;

            int &count = histogram[pixel];
            count++;
            if (count > maxCount) {
                maxCount = count;
                backgroundColor = pixel;
            }
        }
    }

    if (maxCount == 0) return image;

    const int bgR = qRed(backgroundColor);
    const int bgG = qGreen(backgroundColor);
    const int bgB = qBlue(backgroundColor);

    for (int y = 0; y < h; ++y) {
        QRgb *scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb pixel = scanLine[x];
            if (qAlpha(pixel) < minAlpha) continue;

            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            if (std::abs(r - bgR) <= tolerance &&
                std::abs(g - bgG) <= tolerance &&
                std::abs(b - bgB) <= tolerance) {
                scanLine[x] = qRgba(0, 0, 0, 0);
            }
        }
    }

    return image;
}

bool ProjectController::removeAtlasBackgroundAndRefresh(int alphaThreshold,
                                                        int verticalTolerance,
                                                        bool smartCrop,
                                                        double overlapThreshold)
{
    if (!m_document || m_document->atlas().isNull()) return false;

    emit statusMessage(tr("Removing background..."));
    int defaultTol = AppConfig::instance().project().backgroundRemovalTolerance;
    QImage cleanedImage = removeBackgroundFromImage(m_document->atlas(), defaultTol);

    if (alphaThreshold < 0) {
        alphaThreshold = AppConfig::instance().atlas().defaultAlphaThreshold;
    }

    QList<QImage> frameImages;
    QList<SpriteBox> boxes;
    SpriteDetectionOptions opts;
    opts.alphaThreshold = alphaThreshold;
    opts.verticalTolerance = verticalTolerance;
    opts.smartCrop = smartCrop;
    opts.overlapThreshold = overlapThreshold;

    if (!SpriteDetector::detectToImages(cleanedImage, frameImages, boxes, opts)) {
        return false;
    }

    QList<QPixmap> frames;
    frames.reserve(frameImages.size());
    for (const QImage &img : frameImages) {
        frames.append(QPixmap::fromImage(img));
    }

    m_document->setAtlas(cleanedImage);
    m_document->setFrames(frames, boxes);

    emit backgroundRemoved();
    emit statusMessage(tr("Background removed."));
    return true;
}

void ProjectController::removeAtlasBackgroundAndRefreshAsync(int alphaThreshold,
                                                            int verticalTolerance,
                                                            bool smartCrop,
                                                            double overlapThreshold)
{
    if (m_isProcessing || !m_document || m_document->atlas().isNull()) return;

    m_isProcessing = true;
    emit processingStarted();
    emit statusMessage(tr("Removing background in background..."));
    emit progressChanged(20);

    QImage atlasCopy = m_document->atlas();
    const int defaultTol = AppConfig::instance().project().backgroundRemovalTolerance;
    const int actualAlpha = (alphaThreshold < 0) ? AppConfig::instance().atlas().defaultAlphaThreshold : alphaThreshold;

    QFuture<AsyncExtractionResult> future = QtConcurrent::run([atlasCopy, defaultTol, actualAlpha, verticalTolerance, smartCrop, overlapThreshold]() -> AsyncExtractionResult {
        AsyncExtractionResult result;
        result.type = AsyncExtractionResult::JobRemoveBackground;

        QImage cleaned = ProjectController::removeBackgroundFromImage(atlasCopy, defaultTol);
        if (cleaned.isNull()) {
            result.success = false;
            result.errorMessage = QObject::tr("Failed to remove background from atlas.");
            return result;
        }

        SpriteDetectionOptions opts;
        opts.alphaThreshold = actualAlpha;
        opts.verticalTolerance = verticalTolerance;
        opts.smartCrop = smartCrop;
        opts.overlapThreshold = overlapThreshold;

        if (!SpriteDetector::detectToImages(cleaned, result.frameImages, result.boxes, opts)) {
            result.success = false;
            result.errorMessage = QObject::tr("Failed to segment frames after background removal.");
            return result;
        }

        result.atlas = cleaned;
        result.success = true;
        return result;
    });

    m_watcher.setFuture(future);
}

void ProjectController::onAsyncJobFinished()
{
    AsyncExtractionResult res = m_watcher.result();
    m_isProcessing = false;
    emit processingFinished();

    if (!res.success) {
        emit progressChanged(0);
        if (res.type == AsyncExtractionResult::JobOpen) {
            emit fileLoadError(res.filePath, res.errorMessage);
        } else {
            emit statusMessage(res.errorMessage);
        }
        return;
    }

    // Convert QImage frames to QPixmap on GUI thread
    QList<QPixmap> frames;
    frames.reserve(res.frameImages.size());
    for (const QImage &img : res.frameImages) {
        frames.append(QPixmap::fromImage(img));
    }

    if (m_document) {
        m_document->setAtlas(res.atlas);
        m_document->setFrames(frames, res.boxes);
    }

    if (res.type == AsyncExtractionResult::JobOpen) {
        m_currentFilePath = res.filePath;
        m_currentProjectPath.clear();
        addRecentFile(res.filePath);
        if (m_sessionManager) {
            m_sessionManager->startNewSession();
            ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir());
        }
        if (m_undoStack) {
            m_undoStack->clear();
        }
        setProjectModified(false);
        emit statusMessage(tr("Loaded %1 successfully.").arg(QFileInfo(res.filePath).fileName()));
        emit progressChanged(100);
        emit fileLoaded(res.filePath);
    } else if (res.type == AsyncExtractionResult::JobRemoveBackground) {
        emit statusMessage(tr("Background removed successfully."));
        emit progressChanged(100);
        emit backgroundRemoved();
    }
}

void ProjectController::onUndoStackIndexChanged(int idx)
{
    if (m_isProjectLoading) return;
    if (!m_sessionManager || !m_sessionManager->hasActiveSession()) return;
    if (!m_document || m_document->isEmpty()) return;

    // Save project JSON to scratch session directory
    ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir());

    // Determine descriptive action message
    QString commitMsg;
    if (idx > m_lastUndoIndex) {
        const QUndoCommand *cmd = m_undoStack ? m_undoStack->command(idx - 1) : nullptr;
        commitMsg = cmd ? cmd->text() : tr("Action executed");
        if (commitMsg.trimmed().isEmpty()) {
            commitMsg = tr("Project modified");
        }
    } else if (idx < m_lastUndoIndex) {
        const QUndoCommand *cmd = m_undoStack ? m_undoStack->command(idx) : nullptr;
        QString undone = cmd ? cmd->text() : QString();
        commitMsg = tr("Undo: %1").arg(undone.isEmpty() ? tr("Action") : undone);
    } else {
        return;
    }

    m_lastUndoIndex = idx;
    m_sessionManager->gitCommit(commitMsg);
    emit projectHistoryChanged();
}

bool ProjectController::checkoutRevision(const QString &commitHash, QString *errorMsg)
{
    const QString targetHash = commitHash;
    if (!m_sessionManager || !m_sessionManager->hasActiveSession()) {
        if (errorMsg) *errorMsg = tr("No active session workspace.");
        return false;
    }

    if (!m_sessionManager->gitCheckout(targetHash, errorMsg)) {
        return false;
    }

    m_isProjectLoading = true;
    double zoom = 1.0;
    QPointF pan(0, 0);
    if (!ProjectManager::loadProjectFromSessionDir(m_sessionManager->currentSessionDir(), *m_document, &zoom, &pan, errorMsg)) {
        m_isProjectLoading = false;
        return false;
    }

    if (m_undoStack) {
        m_undoStack->clear();
        m_lastUndoIndex = 0;
    }
    m_isProjectLoading = false;

    emit m_document->documentReset();
    emit projectHistoryChanged();
    emit statusMessage(tr("Checked out revision %1.").arg(targetHash.left(7)));
    return true;
}

