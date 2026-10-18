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

#include "watcher/livewatchmanager.h"
#include "controller/projectcontroller.h"
#include "extractor/extractorregistry.h"
#include "model/spritedocument.h"
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QTime>
#include <QThread>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <cstring>

QString LiveWatchConfig::computedExportPath() const
{
    QString dir = outputDirectory.trimmed();
    if (dir.isEmpty() && !sourcePath.isEmpty()) {
        QFileInfo fi(sourcePath);
        dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    }
    if (dir.isEmpty()) {
        dir = QDir::currentPath();
    }

    QString base = outputBaseName.trimmed();
    if (base.isEmpty() && !sourcePath.isEmpty()) {
        QFileInfo fi(sourcePath);
        base = fi.completeBaseName();
    }
    if (base.isEmpty()) {
        base = QStringLiteral("atlas");
    }

    // Determine extension based on engineFormatId
    QString ext = QStringLiteral(".tres");
    QString fmtId = engineFormatId.toLower();
    if (fmtId.contains(QStringLiteral("godot"))) {
        ext = QStringLiteral(".tres");
    } else if (fmtId.contains(QStringLiteral("unity"))) {
        ext = QStringLiteral(".prefab");
    } else if (fmtId.contains(QStringLiteral("unreal"))) {
        ext = QStringLiteral(".paper2d.json");
    } else if (fmtId.contains(QStringLiteral("json")) || fmtId.contains(QStringLiteral("spritesheet")) || fmtId.contains(QStringLiteral("texturepacker"))) {
        ext = QStringLiteral(".json");
    } else if (fmtId.contains(QStringLiteral("spine")) || fmtId.contains(QStringLiteral("libgdx"))) {
        ext = QStringLiteral(".atlas");
    } else {
        Extractor *extractor = ExtractorRegistry::instance().findExtractorById(engineFormatId);
        if (extractor && !extractor->supportedExtensions().isEmpty()) {
            ext = extractor->supportedExtensions().first();
            if (!ext.startsWith('.')) ext.prepend('.');
        }
    }

    return QDir(dir).filePath(base + ext);
}

void LiveWatchConfig::save(QSettings &settings) const
{
    settings.beginGroup(QStringLiteral("LiveWatch"));
    settings.setValue(QStringLiteral("sourceType"), static_cast<int>(sourceType));
    settings.setValue(QStringLiteral("sourcePath"), sourcePath);
    settings.setValue(QStringLiteral("fileFilters"), fileFilters);
    settings.setValue(QStringLiteral("debounceMs"), debounceMs);
    settings.setValue(QStringLiteral("autoExport"), autoExport);
    settings.setValue(QStringLiteral("outputDirectory"), outputDirectory);
    settings.setValue(QStringLiteral("outputBaseName"), outputBaseName);
    settings.setValue(QStringLiteral("engineFormatId"), engineFormatId);
    settings.setValue(QStringLiteral("textureFormat"), static_cast<int>(exportOptions.textureFormat));
    settings.setValue(QStringLiteral("enablePolygonMesh"), exportOptions.packOptions.algorithm == AtlasPacker::TightPolygon);
    settings.setValue(QStringLiteral("modularCelDedup"), exportOptions.packOptions.enableCelDeduplication);
    settings.setValue(QStringLiteral("deltaPatchSlicing"), exportOptions.packOptions.enableDeltaPatching);
    settings.setValue(QStringLiteral("deduplicateFrames"), exportOptions.packOptions.deduplicate);
    settings.setValue(QStringLiteral("padding"), exportOptions.packOptions.padding);
    settings.setValue(QStringLiteral("powerOfTwo"), exportOptions.packOptions.powerOfTwo);
    settings.setValue(QStringLiteral("trimSprites"), exportOptions.trimSprites);
    settings.endGroup();
}

void LiveWatchConfig::load(QSettings &settings)
{
    settings.beginGroup(QStringLiteral("LiveWatch"));
    sourceType = static_cast<WatchSourceType>(settings.value(QStringLiteral("sourceType"), 0).toInt());
    sourcePath = settings.value(QStringLiteral("sourcePath")).toString();
    fileFilters = settings.value(QStringLiteral("fileFilters"), QStringLiteral("*.ase;*.aseprite;*.png")).toString();
    debounceMs = settings.value(QStringLiteral("debounceMs"), 250).toInt();
    autoExport = settings.value(QStringLiteral("autoExport"), true).toBool();
    outputDirectory = settings.value(QStringLiteral("outputDirectory")).toString();
    outputBaseName = settings.value(QStringLiteral("outputBaseName")).toString();
    engineFormatId = settings.value(QStringLiteral("engineFormatId"), QStringLiteral("godot")).toString();

    int texFmt = settings.value(QStringLiteral("textureFormat"), 0).toInt();
    exportOptions.textureFormat = static_cast<TextureFormat>(texFmt);
    bool polygonMesh = settings.value(QStringLiteral("enablePolygonMesh"), false).toBool();
    exportOptions.packOptions.algorithm = polygonMesh ? AtlasPacker::TightPolygon : AtlasPacker::MaxRects;
    exportOptions.packOptions.enableCelDeduplication = settings.value(QStringLiteral("modularCelDedup"), true).toBool();
    exportOptions.packOptions.enableDeltaPatching = settings.value(QStringLiteral("deltaPatchSlicing"), false).toBool();
    exportOptions.packOptions.deduplicate = settings.value(QStringLiteral("deduplicateFrames"), true).toBool();
    exportOptions.packOptions.padding = settings.value(QStringLiteral("padding"), 2).toInt();
    exportOptions.packOptions.powerOfTwo = settings.value(QStringLiteral("powerOfTwo"), true).toBool();
    exportOptions.trimSprites = settings.value(QStringLiteral("trimSprites"), false).toBool();
    settings.endGroup();
}

LiveWatchConfig LiveWatchConfig::createDefault(const QString &sourceFilePath)
{
    LiveWatchConfig cfg;
    cfg.sourceType = WatchSourceType::SingleFile;
    cfg.sourcePath = sourceFilePath;
    cfg.debounceMs = 250;
    cfg.autoExport = true;
    cfg.fileFilters = QStringLiteral("*.ase;*.aseprite;*.png");
    cfg.engineFormatId = QStringLiteral("godot");

    cfg.exportOptions.format = FORMAT_GODOT;
    cfg.exportOptions.textureFormat = TEXTURE_FORMAT_PNG;
    cfg.exportOptions.packOptions.algorithm = AtlasPacker::MaxRects;
    cfg.exportOptions.packOptions.enableCelDeduplication = true;
    cfg.exportOptions.packOptions.enableDeltaPatching = false;
    cfg.exportOptions.packOptions.deduplicate = true;
    cfg.exportOptions.packOptions.powerOfTwo = true;
    cfg.exportOptions.packOptions.padding = 2;
    cfg.exportOptions.trimSprites = false;

    if (!sourceFilePath.isEmpty()) {
        QFileInfo fi(sourceFilePath);
        cfg.outputDirectory = fi.absolutePath();
        cfg.outputBaseName = fi.completeBaseName();
    }

    return cfg;
}

// ============================================================================
// LiveWatchManager
// ============================================================================

LiveWatchManager::LiveWatchManager(QObject *parent)
    : QObject(parent)
{
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(250);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &LiveWatchManager::onDirectoryChanged);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &LiveWatchManager::onFileChanged);
    connect(&m_debounceTimer, &QTimer::timeout, this, &LiveWatchManager::onDebounceTimeout);
}

LiveWatchManager::~LiveWatchManager()
{
    stopWatching();
}

bool LiveWatchManager::startWatching(const LiveWatchConfig &config, QString *errorMsg)
{
    stopWatching();

    m_config = config;
    if (m_config.sourcePath.trimmed().isEmpty()) {
        QString err = tr("No source path specified for watch mode.");
        if (errorMsg) *errorMsg = err;
        setState(WatchState::Error, err);
        return false;
    }

    QFileInfo fi(m_config.sourcePath);
    if (!fi.exists()) {
        QString err = tr("Source file or directory does not exist: %1").arg(m_config.sourcePath);
        if (errorMsg) *errorMsg = err;
        setState(WatchState::Error, err);
        return false;
    }

    m_debounceTimer.setInterval(std::max(100, m_config.debounceMs));

    // Store canonical target export path to prevent recursive trigger loops
    QString targetExport = m_config.computedExportPath();
    if (!targetExport.isEmpty()) {
        m_targetExportCanonical = QFileInfo(targetExport).canonicalFilePath();
        if (m_targetExportCanonical.isEmpty()) {
            m_targetExportCanonical = QFileInfo(targetExport).absoluteFilePath();
        }
    }

    // Register paths
    registerPathsRecursively(m_config.sourcePath);

    m_isWatching = true;
    QString activeMsg = tr("Watching %1").arg(fi.fileName());
    setState(WatchState::Watching, activeMsg);

    return true;
}

void LiveWatchManager::stopWatching()
{
    if (!m_watcher.files().isEmpty()) {
        m_watcher.removePaths(m_watcher.files());
    }
    if (!m_watcher.directories().isEmpty()) {
        m_watcher.removePaths(m_watcher.directories());
    }

    m_debounceTimer.stop();
    m_pendingChangedFile.clear();
    m_isWatching = false;
    setState(WatchState::Idle, tr("Idle"));
}

void LiveWatchManager::updateConfig(const LiveWatchConfig &config)
{
    bool wasActive = m_isWatching;
    if (wasActive) {
        startWatching(config);
    } else {
        m_config = config;
    }
}

void LiveWatchManager::triggerManualReload()
{
    QString target = m_config.sourcePath;
    if (target.isEmpty()) return;

    QFileInfo fi(target);
    if (fi.isDir()) {
        QDir dir(target);
        QFileInfoList list = dir.entryInfoList({QStringLiteral("*.ase"), QStringLiteral("*.aseprite"), QStringLiteral("*.png")}, QDir::Files, QDir::Time);
        if (!list.isEmpty()) {
            target = list.first().absoluteFilePath();
        } else {
            setState(WatchState::Error, tr("No valid sprite files found in watched directory."));
            return;
        }
    }

    executeReloadAndExport(target);
}

bool LiveWatchManager::verifyFileReady(const QString &filePath, int maxRetries, int retryDelayMs)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) return false;

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        QFileInfo fi(filePath);
        if (fi.size() > 0) {
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly)) {
                QString suffix = fi.suffix().toLower();
                if (suffix == QStringLiteral("ase") || suffix == QStringLiteral("aseprite")) {
                    if (file.size() >= 12) {
                        QByteArray header = file.read(12);
                        if (header.size() >= 6) {
                            quint16 magic = *reinterpret_cast<const quint16*>(header.constData() + 4);
                            if (magic == 0xA5E0) {
                                file.close();
                                return true;
                            }
                        }
                    }
                } else if (suffix == QStringLiteral("png")) {
                    if (file.size() >= 8) {
                        QByteArray sig = file.read(8);
                        const char pngSig[8] = {'\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n'};
                        if (std::memcmp(sig.constData(), pngSig, 8) == 0) {
                            file.close();
                            return true;
                        }
                    }
                } else {
                    file.close();
                    return true;
                }
                file.close();
            }
        }
        if (attempt < maxRetries - 1) {
            QThread::msleep(retryDelayMs);
        }
    }
    return false;
}

void LiveWatchManager::onDirectoryChanged(const QString &path)
{
    if (m_isProcessing || isIgnoredFile(path)) return;

    registerPathsRecursively(path);
    m_debounceTimer.start();
}

void LiveWatchManager::onFileChanged(const QString &path)
{
    if (m_isProcessing || isIgnoredFile(path)) return;

    // File re-add protection (many editors do atomic rename/replacement)
    if (QFile::exists(path) && !m_watcher.files().contains(path)) {
        m_watcher.addPath(path);
    }

    m_pendingChangedFile = path;
    m_debounceTimer.start();
}

void LiveWatchManager::onDebounceTimeout()
{
    if (m_isProcessing) return;

    QString target = m_pendingChangedFile;
    m_pendingChangedFile.clear();

    if (target.isEmpty() || !QFile::exists(target)) {
        target = m_config.sourcePath;
        QFileInfo fi(target);
        if (fi.isDir()) {
            QDir dir(target);
            QFileInfoList list = dir.entryInfoList({QStringLiteral("*.ase"), QStringLiteral("*.aseprite"), QStringLiteral("*.png")}, QDir::Files, QDir::Time);
            if (!list.isEmpty()) {
                target = list.first().absoluteFilePath();
            } else {
                return;
            }
        }
    }

    executeReloadAndExport(target);
}

void LiveWatchManager::executeReloadAndExport(const QString &targetFile)
{
    if (m_isProcessing) return;
    m_isProcessing = true;

    setState(WatchState::Compiling, tr("Recompiling in background..."));

    QString sourceFile = targetFile;
    LiveWatchConfig cfg = m_config;
    QString exportPath = cfg.computedExportPath();

    auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, [this, watcher, sourceFile, exportPath]() {
        QPair<bool, QString> result = watcher->result();
        watcher->deleteLater();
        m_isProcessing = false;

        if (result.first) {
            QString successMsg = tr("Up to date (%1)").arg(QTime::currentTime().toString("HH:mm:ss"));
            setState(WatchState::Success, successMsg);
            emit fileReloaded(sourceFile);
            emit exportCompleted(true, exportPath, successMsg);
        } else {
            setState(WatchState::Error, result.second);
            emit exportCompleted(false, exportPath, result.second);
        }
    });

    QFuture<QPair<bool, QString>> future = QtConcurrent::run([sourceFile, exportPath, cfg]() -> QPair<bool, QString> {
        // Step 1: verify file readiness and stability
        if (!LiveWatchManager::verifyFileReady(sourceFile)) {
            return qMakePair(false, tr("File not found or locked by another application: %1").arg(sourceFile));
        }

        // Step 2: decode file into a SpriteDocument
        SpriteDocument tempDoc;
        QString decodeErr;
        if (!ProjectController::decodeFileToDocument(sourceFile, tempDoc, &decodeErr)) {
            return qMakePair(false, decodeErr.isEmpty() ? tr("Failed to decode file: %1").arg(sourceFile) : decodeErr);
        }

        // Step 3: if autoExport is enabled, write engine descriptor and texture
        if (cfg.autoExport && !exportPath.isEmpty()) {
            QDir().mkpath(QFileInfo(exportPath).absolutePath());
            Extractor *extractor = ExtractorRegistry::instance().findExtractorById(cfg.engineFormatId);
            if (!extractor) {
                extractor = ExtractorRegistry::instance().findEncoder(exportPath);
            }
            if (!extractor) {
                return qMakePair(false, tr("No exporter found for target format: %1").arg(exportPath));
            }

            ExtractorError err;
            if (!extractor->write(exportPath, tempDoc, cfg.exportOptions, &err)) {
                return qMakePair(false, err.toString());
            }
        }

        return qMakePair(true, QString());
    });

    watcher->setFuture(future);
}

bool LiveWatchManager::isIgnoredFile(const QString &path) const
{
    if (path.isEmpty()) return true;

    QFileInfo fi(path);
    QString cleanPath = fi.canonicalFilePath();
    if (cleanPath.isEmpty()) cleanPath = fi.absoluteFilePath();

    // Prevent recursive loop with output target
    if (!m_targetExportCanonical.isEmpty()) {
        if (cleanPath == m_targetExportCanonical) return true;
        // Companion textures (.png, .ktx2)
        QFileInfo expFi(m_targetExportCanonical);
        QString expBase = expFi.absolutePath() + '/' + expFi.completeBaseName();
        if (cleanPath.startsWith(expBase)) return true;
    }

    // Ignore temporary, hidden, swap and lock files
    QString fn = fi.fileName();
    if (fn.startsWith(QStringLiteral(".")) ||
        fn.startsWith(QStringLiteral("~")) ||
        fn.endsWith(QStringLiteral("~")) ||
        fn.endsWith(QStringLiteral(".tmp")) ||
        fn.endsWith(QStringLiteral(".swp")) ||
        fn.startsWith(QStringLiteral(".bentopack-watch"))) {
        return true;
    }

    return false;
}

void LiveWatchManager::registerPathsRecursively(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists()) return;

    if (fi.isDir()) {
        QString absDir = fi.canonicalFilePath();
        if (absDir.isEmpty()) absDir = fi.absoluteFilePath();

        if (!m_watcher.directories().contains(absDir)) {
            m_watcher.addPath(absDir);
        }

        // Parse configured extensions
        QStringList filters = m_config.fileFilters.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (QString &f : filters) {
            f = f.trimmed().remove(QStringLiteral("*.")).remove(QLatin1Char('*')).remove(QLatin1Char('.')).toLower();
        }
        if (filters.isEmpty()) {
            filters = {QStringLiteral("ase"), QStringLiteral("aseprite"), QStringLiteral("png")};
        }

        QDirIterator it(absDir, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString sub = it.next();
            QFileInfo sfi(sub);
            if (sfi.isDir()) {
                QString c = sfi.canonicalFilePath();
                if (!c.isEmpty() && !m_watcher.directories().contains(c) && !sfi.fileName().startsWith('.')) {
                    m_watcher.addPath(c);
                }
            } else if (sfi.isFile() && !isIgnoredFile(sub)) {
                QString sfx = sfi.suffix().toLower();
                if (filters.contains(sfx)) {
                    QString c = sfi.canonicalFilePath();
                    if (!c.isEmpty() && !m_watcher.files().contains(c)) {
                        m_watcher.addPath(c);
                    }
                }
            }
        }
    } else if (fi.isFile() && !isIgnoredFile(path)) {
        QString absFile = fi.canonicalFilePath();
        if (absFile.isEmpty()) absFile = fi.absoluteFilePath();
        if (!m_watcher.files().contains(absFile)) {
            m_watcher.addPath(absFile);
        }
        // Also watch parent directory to detect file recreation/atomic writes
        QString parentDir = fi.dir().canonicalPath();
        if (!parentDir.isEmpty() && !m_watcher.directories().contains(parentDir)) {
            m_watcher.addPath(parentDir);
        }
    }
}

void LiveWatchManager::setState(WatchState state, const QString &msg)
{
    m_state = state;
    m_lastMessage = msg;
    emit stateChanged(state, msg);
}
