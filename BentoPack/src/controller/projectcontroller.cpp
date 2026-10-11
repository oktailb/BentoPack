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

#include "include/controller/projectcontroller.h"
#include "include/model/spritedocument.h"
#include "include/extractor/extractorregistry.h"
#include "include/image/spritedetector.h"
#include "include/config/appconfig.h"
#include "include/project/sessionmanager.h"
#include "include/project/projectmanager.h"
#include "packer/vramtexturecompressor.h"
#include "include/commands/commands.h"
#include <QUndoStack>
#include <QUuid>
#include <QPainter>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QHash>
#include <QtConcurrent>
#include <cmath>
#include <vector>
#include <QCoreApplication>

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
        connect(m_document, &SpriteDocument::atlasRegionChanged, this, [this](const QRect &) {
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
    if (m_document && !m_document->projectName().isEmpty() && m_document->projectName() != QStringLiteral("untitled")) {
        return m_document->projectName();
    }
    return QCoreApplication::translate("MainWindow", "KEY_UNTITLED_PROJECT");
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
    m_undoCommitHistory.clear();
    m_undoCommands.clear();
    m_currentProjectPath.clear();
    m_currentFilePath.clear();

    if (m_sessionManager) {
        m_sessionManager->startNewSession();
        ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir());
        m_sessionManager->gitCommit(tr("New project"));
        m_undoCommitHistory.insert(0, m_sessionManager->gitHeadCommitHash());
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
    if (m_document) {
        m_document->setFilePath(m_currentFilePath);
    }
    addRecentProject(m_currentProjectPath);

    if (m_undoStack) {
        m_undoStack->clear();
    }
    m_lastUndoIndex = 0;
    m_undoCommitHistory.clear();
    m_undoCommands.clear();
    if (m_sessionManager) {
        m_undoCommitHistory.insert(0, m_sessionManager->gitHeadCommitHash());
    }

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

    if (!targetPath.endsWith(QStringLiteral(".bento"), Qt::CaseInsensitive)) {
        targetPath += QStringLiteral(".bento");
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

    // Pack workspace to .bento atomically
    if (!m_sessionManager->saveSessionToBento(targetPath, errorMsg)) {
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
    QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
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

    QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
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
    QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
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

    if (filePath.endsWith(QStringLiteral(".bento"), Qt::CaseInsensitive)) {
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
    if (m_document) {
        m_document->setFilePath(filePath);
    }
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
    m_undoCommitHistory.clear();
    m_undoCommands.clear();
    if (m_sessionManager) {
        m_undoCommitHistory.insert(0, m_sessionManager->gitHeadCommitHash());
    }

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

    // If it's a native project (.bento)
    if (ext == QStringLiteral("bento")) {
        openProject(filePath);
        return;
    }

    m_isProcessing = true;
    emit processingStarted();
    emit statusMessage(tr("Loading %1...").arg(fi.fileName()));
    emit progressChanged(20);

    QFuture<AsyncExtractionResult> future = QtConcurrent::run([filePath]() -> AsyncExtractionResult {
        AsyncExtractionResult result;
        result.type = AsyncExtractionResult::JobOpen;
        result.filePath = filePath;

        SpriteDocument tempDoc;
        QString err;
        if (!decodeFileToDocument(filePath, tempDoc, &err)) {
            result.success = false;
            result.errorMessage = err;
            return result;
        }

        result.atlas = tempDoc.atlas();
        result.frameImages = tempDoc.frames();
        result.boxes = tempDoc.boxes();
        result.layers = tempDoc.layers();
        for (int f = 0; f < tempDoc.frameCount(); ++f) {
            if (tempDoc.hasFrameCels(f)) {
                result.frameCels[f] = tempDoc.frameCels(f);
            }
        }
        result.animations = tempDoc.animations();
        result.skinProfiles = tempDoc.skinProfiles();
        result.success = true;
        return result;
    });

    m_watcher.setFuture(future);
}

bool ProjectController::decodeFileToDocument(const QString &filePath, SpriteDocument &outDoc, QString *errorMsg)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        QString err = tr("File does not exist: %1").arg(filePath);
        if (errorMsg) *errorMsg = err;
        return false;
    }

    Extractor *extractor = ExtractorRegistry::instance().findDecoder(filePath);
    if (!extractor) {
        QString loadErr;
        QImage image = VramTextureCompressor::loadAtlasImage(filePath, &loadErr);
        if (image.isNull()) {
            QString err = tr("No suitable codec found for file: %1").arg(filePath);
            if (!loadErr.isEmpty()) err += QStringLiteral(" (%1)").arg(loadErr);
            if (errorMsg) *errorMsg = err;
            return false;
        }
        SpriteDetectionOptions detOpts;
        QList<QImage> frameImages;
        QList<SpriteBox> boxes;
        SpriteDetector::detectToImages(image, frameImages, boxes, detOpts);
        outDoc.setAtlas((image.format() == QImage::Format_ARGB32) ? image : image.convertToFormat(QImage::Format_ARGB32));
        outDoc.setFrames(frameImages, boxes);
        outDoc.setFilePath(filePath);
        return true;
    }

    ExtractorError err;
    if (!extractor->read(filePath, outDoc, &err)) {
        QString fullError = err.toString();
        if (errorMsg) *errorMsg = fullError;
        return false;
    }
    outDoc.setFilePath(filePath);
    return true;
}

bool ProjectController::replaceExistingAtlasWithMedia(const QString &filePath, QString *errorMsg)
{
    if (!m_document) {
        if (errorMsg) *errorMsg = tr("No active SpriteDocument.");
        return false;
    }

    SpriteDocument incomingDoc;
    if (!decodeFileToDocument(filePath, incomingDoc, errorMsg)) {
        return false;
    }

    QImage newAtlas = incomingDoc.atlas();
    if (newAtlas.isNull()) {
        if (errorMsg) *errorMsg = tr("Decoded atlas image is null.");
        return false;
    }

    QList<QImage> newFrames;
    QList<SpriteBox> newBoxes;
    QList<SpriteLayer> newLayers;
    QMap<int, QList<SpriteCel>> newFrameCels;
    QMap<QString, SpriteAnimation> newAnimations;
    QMap<QString, SkinProfile> newSkinProfiles;

    const bool incomingHasStructure = (incomingDoc.hasLayers() || (incomingDoc.frameCount() > 1 && incomingDoc.frameCount() == incomingDoc.boxes().size()));

    if (incomingHasStructure || m_document->frameCount() == 0) {
        // Multi-frame or structured project incoming: take its frames and boxes
        newFrames = incomingDoc.frames();
        newBoxes = incomingDoc.boxes();
        newLayers = incomingDoc.layers();
        for (int f = 0; f < incomingDoc.frameCount(); ++f) {
            if (incomingDoc.hasFrameCels(f)) {
                newFrameCels[f] = incomingDoc.frameCels(f);
            }
        }
        newAnimations = incomingDoc.animations();
        newSkinProfiles = incomingDoc.skinProfiles();
    } else {
        // Flat atlas texture replacement on existing project:
        // PRESERVE existing bounding boxes, pivots, and animations!
        newBoxes = m_document->boxes();
        newAnimations = m_document->animations();
        newSkinProfiles = m_document->skinProfiles();
        newLayers = m_document->layers();

        if (!m_document->atlas().isNull() && m_document->atlas().size() != newAtlas.size() &&
            m_document->atlas().width() > 0 && m_document->atlas().height() > 0) {
            double scaleX = static_cast<double>(newAtlas.width()) / m_document->atlas().width();
            double scaleY = static_cast<double>(newAtlas.height()) / m_document->atlas().height();
            for (auto &b : newBoxes) {
                b.rect = QRect(qRound(b.rect.x() * scaleX), qRound(b.rect.y() * scaleY),
                               qRound(b.rect.width() * scaleX), qRound(b.rect.height() * scaleY));
                b.pivot = QPoint(qRound(b.pivot.x() * scaleX), qRound(b.pivot.y() * scaleY));
            }
        }

        newFrames.reserve(newBoxes.size());
        for (int f = 0; f < newBoxes.size(); ++f) {
            QRect r = newBoxes[f].rect.intersected(newAtlas.rect());
            if (!r.isEmpty()) {
                newFrames.append(newAtlas.copy(r));
            } else {
                newFrames.append(QImage());
            }
        }

        if (m_document->hasLayers()) {
            for (int f = 0; f < newBoxes.size(); ++f) {
                QList<SpriteCel> cels = m_document->frameCels(f);
                if (!cels.isEmpty()) {
                    cels[0].image = (f < newFrames.size()) ? newFrames[f] : QImage();
                    newFrameCels[f] = cels;
                }
            }
        }
    }

    if (m_undoStack) {
        m_undoStack->push(new ReplaceAtlasMediaCommand(m_document,
                                                       newAtlas,
                                                       newFrames,
                                                       newBoxes,
                                                       newLayers,
                                                       newFrameCels,
                                                       newAnimations,
                                                       newSkinProfiles));
    } else {
        m_document->setAtlas(newAtlas);
        m_document->setFrames(newFrames, newBoxes);
        m_document->setLayers(newLayers);
        for (auto it = newFrameCels.begin(); it != newFrameCels.end(); ++it) {
            m_document->setFrameCels(it.key(), it.value());
        }
        if (!newAnimations.isEmpty()) {
            m_document->setAnimations(newAnimations);
        }
        if (!newSkinProfiles.isEmpty()) {
            m_document->setSkinProfiles(newSkinProfiles);
        }
    }

    setProjectModified(true);
    if (m_sessionManager && m_sessionManager->hasActiveSession()) {
        ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir());
        m_sessionManager->gitCommit(tr("Replace atlas with %1").arg(QFileInfo(filePath).fileName()));
    }

    addRecentFile(filePath);
    emit statusMessage(tr("Replaced atlas with %1 successfully.").arg(QFileInfo(filePath).fileName()));
    emit fileLoaded(filePath);
    return true;
}

static QImage compositeFrameFromCels(const QSize &frameSize,
                                     const QList<SpriteLayer> &layers,
                                     const QList<SpriteCel> &cels)
{
    if (frameSize.isEmpty()) return QImage();

    QImage composite(frameSize, QImage::Format_ARGB32_Premultiplied);
    composite.fill(Qt::transparent);

    QPainter painter(&composite);

    QList<int> sortedLayerIndices;
    sortedLayerIndices.reserve(layers.size());
    for (int i = 0; i < layers.size(); ++i) sortedLayerIndices.append(i);
    std::stable_sort(sortedLayerIndices.begin(), sortedLayerIndices.end(), [&layers](int a, int b) {
        return layers[a].zOrder < layers[b].zOrder;
    });

    for (int lIdx : sortedLayerIndices) {
        const SpriteLayer &lyr = layers.at(lIdx);
        if (!lyr.visible) continue;

        SpriteCel currentCel;
        bool found = false;
        for (const auto &c : cels) {
            if (c.layerIndex == lIdx || (!lyr.id.isEmpty() && c.layerId == lyr.id)) {
                currentCel = c;
                found = true;
                break;
            }
        }
        if (!found || currentCel.isNull()) continue;

        painter.setCompositionMode(lyr.blendMode);
        double layerOp = (lyr.opacity / 255.0) * (currentCel.opacity / 255.0);
        painter.setOpacity(std::clamp(layerOp, 0.0, 1.0));
        painter.drawImage(QPoint(currentCel.x, currentCel.y), currentCel.image);
    }
    painter.end();
    return composite.convertToFormat(QImage::Format_ARGB32);
}

static QImage compositeAtlasFromFrames(const QSize &atlasSize,
                                       const QList<QImage> &frames,
                                       const QList<SpriteBox> &boxes)
{
    if (atlasSize.isEmpty() || frames.isEmpty()) return QImage();

    QImage newAtlas(atlasSize, QImage::Format_ARGB32_Premultiplied);
    newAtlas.fill(Qt::transparent);

    QPainter p(&newAtlas);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (int i = 0; i < frames.size(); ++i) {
        if (i < boxes.size() && !boxes[i].rect.isEmpty()) {
            const QImage &img = frames[i];
            if (!img.isNull()) {
                p.drawImage(boxes[i].rect.topLeft(), img);
            }
        }
    }
    p.end();
    return newAtlas.convertToFormat(QImage::Format_ARGB32);
}

bool ProjectController::addMediaAsNewLayers(const QString &filePath, QString *errorMsg, bool asCommonBackground)
{
    if (!m_document) {
        if (errorMsg) *errorMsg = tr("No active SpriteDocument.");
        return false;
    }

    SpriteDocument incomingDoc;
    Extractor *extractor = ExtractorRegistry::instance().findDecoder(filePath);
    if (extractor) {
        ExtractorError exErr;
        if (!extractor->read(filePath, incomingDoc, &exErr)) {
            if (errorMsg) *errorMsg = exErr.toString();
            return false;
        }
    } else {
        QString loadErr;
        QImage img = VramTextureCompressor::loadAtlasImage(filePath, &loadErr);
        if (img.isNull()) {
            if (errorMsg) *errorMsg = tr("Failed to load image: %1").arg(loadErr.isEmpty() ? filePath : loadErr);
            return false;
        }
        incomingDoc.setAtlas((img.format() == QImage::Format_ARGB32) ? img : img.convertToFormat(QImage::Format_ARGB32));
        incomingDoc.setFrames({incomingDoc.atlas()}, {SpriteBox(QRect(0, 0, img.width(), img.height()))});
    }

    // 1. Prepare layers and existing cels
    QList<SpriteLayer> combinedLayers = m_document->layers();
    QMap<int, QList<SpriteCel>> combinedCels;
    for (int f = 0; f < m_document->frameCount(); ++f) {
        if (m_document->hasFrameCels(f)) {
            combinedCels[f] = m_document->frameCels(f);
        }
    }

    // If current document had no layers, promote existing content to a "Base" layer first
    if (combinedLayers.isEmpty() && m_document->frameCount() > 0) {
        SpriteLayer baseLayer;
        baseLayer.id = QStringLiteral("layer_base");
        baseLayer.name = tr("Base");
        baseLayer.visible = true;
        baseLayer.locked = false;
        baseLayer.opacity = 255;
        baseLayer.zOrder = 0;
        combinedLayers.append(baseLayer);

        for (int f = 0; f < m_document->frameCount(); ++f) {
            SpriteCel baseCel;
            baseCel.layerIndex = 0;
            baseCel.layerId = baseLayer.id;
            baseCel.image = m_document->frame(f);
            baseCel.x = 0;
            baseCel.y = 0;
            baseCel.opacity = 255;
            combinedCels[f].append(baseCel);
        }
    }

    // 2. Add incoming layers
    if (asCommonBackground) {
        // Option 4: Common background placed behind existing sprite layers
        SpriteLayer bgLayer;
        bgLayer.id = QStringLiteral("layer_bg_%1").arg(QUuid::createUuid().toString(QUuid::Id128).left(8));
        bgLayer.name = tr("Background: %1").arg(QFileInfo(filePath).completeBaseName());
        bgLayer.visible = true;
        bgLayer.locked = false;
        bgLayer.opacity = 255;
        bgLayer.zOrder = 0;

        for (auto &lyr : combinedLayers) {
            lyr.zOrder += 1;
        }
        combinedLayers.prepend(bgLayer);

        for (auto it = combinedCels.begin(); it != combinedCels.end(); ++it) {
            for (auto &cel : it.value()) {
                cel.layerIndex += 1;
            }
        }

        QImage srcBg = !incomingDoc.atlas().isNull() ? incomingDoc.atlas() : (incomingDoc.frameCount() > 0 ? incomingDoc.frame(0) : QImage());
        QMap<quint64, QImage> scaledBgCache;

        for (int f = 0; f < m_document->frameCount(); ++f) {
            QSize fSize = (f < m_document->boxes().size() && !m_document->box(f).rect.isEmpty())
                              ? m_document->box(f).rect.size()
                              : (f < m_document->frames().size() ? m_document->frame(f).size() : QSize(32, 32));
            quint64 sizeKey = (static_cast<quint64>(fSize.width()) << 32) | static_cast<quint32>(fSize.height());

            if (!scaledBgCache.contains(sizeKey)) {
                QImage scaled = srcBg.scaled(fSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                if (scaled.size() != fSize) {
                    int ox = (scaled.width() - fSize.width()) / 2;
                    int oy = (scaled.height() - fSize.height()) / 2;
                    scaled = scaled.copy(ox, oy, fSize.width(), fSize.height());
                }
                scaledBgCache[sizeKey] = scaled.convertToFormat(QImage::Format_ARGB32);
            }

            SpriteCel bgCel;
            bgCel.layerIndex = 0;
            bgCel.layerId = bgLayer.id;
            bgCel.opacity = 255;
            bgCel.x = 0;
            bgCel.y = 0;
            bgCel.image = scaledBgCache.value(sizeKey);
            combinedCels[f].prepend(bgCel);
        }
    } else if (incomingDoc.hasLayers()) {
        // Multi-layer incoming document (e.g. Aseprite)
        for (int l = 0; l < incomingDoc.layers().size(); ++l) {
            SpriteLayer incLayer = incomingDoc.layer(l);
            SpriteLayer newLayer = incLayer;
            newLayer.id = QStringLiteral("layer_%1_%2").arg(QFileInfo(filePath).baseName()).arg(incLayer.id.isEmpty() ? QString::number(l) : incLayer.id);
            newLayer.zOrder = combinedLayers.size();
            int newLayerIdx = combinedLayers.size();
            combinedLayers.append(newLayer);

            const bool sameFrameCount = (incomingDoc.frameCount() == m_document->frameCount() && incomingDoc.frameCount() > 0);

            for (int f = 0; f < m_document->frameCount(); ++f) {
                if (sameFrameCount && incomingDoc.hasFrameCels(f)) {
                    for (const SpriteCel &c : incomingDoc.frameCels(f)) {
                        if (c.layerIndex == l || (!incLayer.id.isEmpty() && c.layerId == incLayer.id)) {
                            SpriteCel celCopy = c;
                            celCopy.layerIndex = newLayerIdx;
                            celCopy.layerId = newLayer.id;
                            combinedCels[f].append(celCopy);
                            break;
                        }
                    }
                } else {
                    SpriteCel emptyCel;
                    emptyCel.layerIndex = newLayerIdx;
                    emptyCel.layerId = newLayer.id;
                    emptyCel.opacity = 255;
                    emptyCel.x = 0;
                    emptyCel.y = 0;
                    emptyCel.image = QImage(m_document->frame(f).size(), QImage::Format_ARGB32_Premultiplied);
                    emptyCel.image.fill(Qt::transparent);
                    combinedCels[f].append(emptyCel);
                }
            }
        }
    } else {
        // Option 3: Whole-Atlas Layer Workflow
        SpriteLayer newLayer;
        newLayer.id = QStringLiteral("layer_%1_%2").arg(QFileInfo(filePath).baseName()).arg(QUuid::createUuid().toString(QUuid::Id128).left(8));
        newLayer.name = QFileInfo(filePath).completeBaseName();
        newLayer.visible = true;
        newLayer.locked = false;
        newLayer.opacity = 255;
        newLayer.zOrder = combinedLayers.size();
        int newLayerIdx = combinedLayers.size();
        combinedLayers.append(newLayer);

        QImage incomingAtlas = incomingDoc.atlas();
        if (incomingAtlas.isNull() && incomingDoc.frameCount() > 0) {
            incomingAtlas = incomingDoc.frame(0);
        }

        QSize targetAtlasSize = m_document->atlas().isNull() ? QSize() : m_document->atlas().size();
        if (targetAtlasSize.isValid() && !targetAtlasSize.isEmpty() && !incomingAtlas.isNull() && incomingAtlas.size() != targetAtlasSize) {
            incomingAtlas = incomingAtlas.scaled(targetAtlasSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        for (int f = 0; f < m_document->frameCount(); ++f) {
            SpriteCel cel;
            cel.layerIndex = newLayerIdx;
            cel.layerId = newLayer.id;
            cel.opacity = 255;
            cel.x = 0;
            cel.y = 0;

            QRect r = m_document->box(f).rect;
            if (!r.isEmpty() && !incomingAtlas.isNull()) {
                QRect clipped = r.intersected(incomingAtlas.rect());
                if (!clipped.isEmpty()) {
                    if (clipped.size() == r.size()) {
                        cel.image = incomingAtlas.copy(clipped);
                    } else {
                        cel.image = QImage(r.size(), QImage::Format_ARGB32_Premultiplied);
                        cel.image.fill(Qt::transparent);
                        QPainter p(&cel.image);
                        p.drawImage(clipped.topLeft() - r.topLeft(), incomingAtlas.copy(clipped));
                        p.end();
                    }
                } else {
                    cel.image = QImage(r.size(), QImage::Format_ARGB32_Premultiplied);
                    cel.image.fill(Qt::transparent);
                }
            } else if (f < incomingDoc.frameCount()) {
                cel.image = incomingDoc.frame(f);
            } else {
                cel.image = QImage(m_document->frame(f).size(), QImage::Format_ARGB32_Premultiplied);
                cel.image.fill(Qt::transparent);
            }

            combinedCels[f].append(cel);
        }
    }

    QList<QImage> compositedFrames;
    compositedFrames.reserve(m_document->frameCount());
    for (int f = 0; f < m_document->frameCount(); ++f) {
        QSize fSize = (f < m_document->boxes().size() && !m_document->box(f).rect.isEmpty())
                          ? m_document->box(f).rect.size()
                          : (f < m_document->frames().size() ? m_document->frame(f).size() : QSize(32, 32));
        QList<SpriteCel> cels = combinedCels.value(f);
        compositedFrames.append(compositeFrameFromCels(fSize, combinedLayers, cels));
    }
    QImage compositedAtlas = compositeAtlasFromFrames(m_document->atlas().size(), compositedFrames, m_document->boxes());

    if (m_undoStack) {
        m_undoStack->push(new AddMediaLayersCommand(m_document, combinedLayers, combinedCels, QFileInfo(filePath).fileName(), compositedFrames, compositedAtlas));
    } else {
        m_document->setLayers(combinedLayers);
        for (auto it = combinedCels.begin(); it != combinedCels.end(); ++it) {
            m_document->setFrameCels(it.key(), it.value());
        }
        m_document->setCompositedFrames(compositedFrames);
        m_document->setAtlas(compositedAtlas);
    }

    setProjectModified(true);
    if (m_sessionManager && m_sessionManager->hasActiveSession()) {
        ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir());
        m_sessionManager->gitCommit(tr("Add layers from %1").arg(QFileInfo(filePath).fileName()));
    }

    addRecentFile(filePath);
    emit statusMessage(asCommonBackground
        ? tr("Added common background from %1 successfully.").arg(QFileInfo(filePath).fileName())
        : tr("Added layers from %1 successfully.").arg(QFileInfo(filePath).fileName()));
    emit fileLoaded(filePath);
    return true;
}

void ProjectController::replaceExistingAtlasWithMediaAsync(const QString &filePath)
{
    if (m_isProcessing) return;

    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        QString err = tr("File does not exist: %1").arg(filePath);
        emit fileLoadError(filePath, err);
        return;
    }

    if (!m_document) {
        emit fileLoadError(filePath, tr("No active SpriteDocument."));
        return;
    }

    QFileInfo fi(filePath);
    m_isProcessing = true;
    emit processingStarted();
    emit statusMessage(tr("Replacing atlas with %1...").arg(fi.fileName()));
    emit progressChanged(20);

    QList<SpriteBox> docBoxes = m_document->boxes();
    QSize docAtlasSize = m_document->atlas().size();
    QMap<QString, SpriteAnimation> docAnims = m_document->animations();
    QMap<QString, SkinProfile> docSkins = m_document->skinProfiles();
    QList<SpriteLayer> docLayers = m_document->layers();
    QMap<int, QList<SpriteCel>> docCels;
    for (int f = 0; f < m_document->frameCount(); ++f) {
        if (m_document->hasFrameCels(f)) {
            docCels[f] = m_document->frameCels(f);
        }
    }

    QFuture<AsyncExtractionResult> future = QtConcurrent::run([filePath, docBoxes, docAtlasSize, docAnims, docSkins, docLayers, docCels]() -> AsyncExtractionResult {
        AsyncExtractionResult result;
        result.type = AsyncExtractionResult::JobReplaceAtlas;
        result.filePath = filePath;

        SpriteDocument tempDoc;
        Extractor *extractor = ExtractorRegistry::instance().findDecoder(filePath);
        if (extractor) {
            ExtractorError exErr;
            if (!extractor->read(filePath, tempDoc, &exErr)) {
                result.success = false;
                result.errorMessage = exErr.toString();
                return result;
            }
        } else {
            QString loadErr;
            QImage img = VramTextureCompressor::loadAtlasImage(filePath, &loadErr);
            if (img.isNull()) {
                result.success = false;
                result.errorMessage = QObject::tr("Failed to load image: %1").arg(loadErr.isEmpty() ? filePath : loadErr);
                return result;
            }
            tempDoc.setAtlas((img.format() == QImage::Format_ARGB32) ? img : img.convertToFormat(QImage::Format_ARGB32));
        }

        if (tempDoc.hasLayers()) {
            result.atlas = tempDoc.atlas();
            result.frameImages = tempDoc.frames();
            result.boxes = tempDoc.boxes();
            result.layers = tempDoc.layers();
            for (int f = 0; f < tempDoc.frameCount(); ++f) {
                if (tempDoc.hasFrameCels(f)) {
                    result.frameCels[f] = tempDoc.frameCels(f);
                }
            }
            result.animations = tempDoc.animations();
            result.skinProfiles = tempDoc.skinProfiles();
        } else {
            // Flat atlas replacement: preserve existing bounding boxes, pivots, and animations
            QImage newAtlas = tempDoc.atlas();
            QList<SpriteBox> newBoxes = docBoxes;
            QList<QImage> newFrames;
            QMap<int, QList<SpriteCel>> newFrameCels;

            if (!docAtlasSize.isEmpty() && docAtlasSize != newAtlas.size() &&
                docAtlasSize.width() > 0 && docAtlasSize.height() > 0) {
                double scaleX = static_cast<double>(newAtlas.width()) / docAtlasSize.width();
                double scaleY = static_cast<double>(newAtlas.height()) / docAtlasSize.height();
                for (auto &b : newBoxes) {
                    b.rect = QRect(qRound(b.rect.x() * scaleX), qRound(b.rect.y() * scaleY),
                                   qRound(b.rect.width() * scaleX), qRound(b.rect.height() * scaleY));
                    b.pivot = QPoint(qRound(b.pivot.x() * scaleX), qRound(b.pivot.y() * scaleY));
                }
            }

            newFrames.reserve(newBoxes.size());
            for (int f = 0; f < newBoxes.size(); ++f) {
                QRect r = newBoxes[f].rect.intersected(newAtlas.rect());
                if (!r.isEmpty()) {
                    newFrames.append(newAtlas.copy(r));
                } else {
                    newFrames.append(QImage());
                }
            }

            if (!docLayers.isEmpty()) {
                for (int f = 0; f < newBoxes.size(); ++f) {
                    QList<SpriteCel> cels = docCels.value(f);
                    if (!cels.isEmpty()) {
                        cels[0].image = (f < newFrames.size()) ? newFrames[f] : QImage();
                        newFrameCels[f] = cels;
                    }
                }
            }

            result.atlas = newAtlas;
            result.frameImages = newFrames;
            result.boxes = newBoxes;
            result.layers = docLayers;
            result.frameCels = newFrameCels;
            result.animations = docAnims;
            result.skinProfiles = docSkins;
        }

        result.success = true;
        return result;
    });

    m_watcher.setFuture(future);
}

void ProjectController::addMediaAsNewLayersAsync(const QString &filePath, bool asCommonBackground)
{
    if (m_isProcessing) return;

    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        QString err = tr("File does not exist: %1").arg(filePath);
        emit fileLoadError(filePath, err);
        return;
    }

    if (!m_document) {
        emit fileLoadError(filePath, tr("No active SpriteDocument."));
        return;
    }

    QFileInfo fi(filePath);
    m_isProcessing = true;
    emit processingStarted();
    emit statusMessage(asCommonBackground
        ? tr("Adding common background from %1...").arg(fi.fileName())
        : tr("Importing layers from %1...").arg(fi.fileName()));
    emit progressChanged(20);

    QList<SpriteBox> docBoxes = m_document->boxes();
    QList<QImage> docFrames = m_document->frames();
    QList<SpriteLayer> docLayers = m_document->layers();
    QImage docAtlas = m_document->atlas();
    int frameCount = m_document->frameCount();
    QMap<int, QList<SpriteCel>> docCels;
    for (int f = 0; f < frameCount; ++f) {
        if (m_document->hasFrameCels(f)) {
            docCels[f] = m_document->frameCels(f);
        }
    }

    QFuture<AsyncExtractionResult> future = QtConcurrent::run([filePath, asCommonBackground, docBoxes, docFrames, docLayers, docAtlas, frameCount, docCels]() -> AsyncExtractionResult {
        AsyncExtractionResult result;
        result.type = asCommonBackground ? AsyncExtractionResult::JobAddCommonBackground : AsyncExtractionResult::JobAddLayers;
        result.filePath = filePath;

        SpriteDocument incomingDoc;
        Extractor *extractor = ExtractorRegistry::instance().findDecoder(filePath);
        if (extractor) {
            ExtractorError exErr;
            if (!extractor->read(filePath, incomingDoc, &exErr)) {
                result.success = false;
                result.errorMessage = exErr.toString();
                return result;
            }
        } else {
            QString loadErr;
            QImage img = VramTextureCompressor::loadAtlasImage(filePath, &loadErr);
            if (img.isNull()) {
                result.success = false;
                result.errorMessage = QObject::tr("Failed to load image: %1").arg(loadErr.isEmpty() ? filePath : loadErr);
                return result;
            }
            incomingDoc.setAtlas((img.format() == QImage::Format_ARGB32) ? img : img.convertToFormat(QImage::Format_ARGB32));
            incomingDoc.setFrames({incomingDoc.atlas()}, {SpriteBox(QRect(0, 0, img.width(), img.height()))});
        }

        QList<SpriteLayer> combinedLayers = docLayers;
        QMap<int, QList<SpriteCel>> combinedCels = docCels;

        if (combinedLayers.isEmpty() && frameCount > 0) {
            SpriteLayer baseLayer;
            baseLayer.id = QStringLiteral("layer_base");
            baseLayer.name = QObject::tr("Base");
            baseLayer.visible = true;
            baseLayer.locked = false;
            baseLayer.opacity = 255;
            baseLayer.zOrder = 0;
            combinedLayers.append(baseLayer);

            for (int f = 0; f < frameCount; ++f) {
                SpriteCel baseCel;
                baseCel.layerIndex = 0;
                baseCel.layerId = baseLayer.id;
                baseCel.image = (f < docFrames.size()) ? docFrames[f] : QImage();
                baseCel.x = 0;
                baseCel.y = 0;
                baseCel.opacity = 255;
                combinedCels[f].append(baseCel);
            }
        }

        if (asCommonBackground) {
            SpriteLayer bgLayer;
            bgLayer.id = QStringLiteral("layer_bg_%1").arg(QUuid::createUuid().toString(QUuid::Id128).left(8));
            bgLayer.name = QObject::tr("Background: %1").arg(QFileInfo(filePath).completeBaseName());
            bgLayer.visible = true;
            bgLayer.locked = false;
            bgLayer.opacity = 255;
            bgLayer.zOrder = 0;

            for (auto &lyr : combinedLayers) {
                lyr.zOrder += 1;
            }
            combinedLayers.prepend(bgLayer);

            for (auto it = combinedCels.begin(); it != combinedCels.end(); ++it) {
                for (auto &cel : it.value()) {
                    cel.layerIndex += 1;
                }
            }

            QImage srcBg = !incomingDoc.atlas().isNull() ? incomingDoc.atlas() : (incomingDoc.frameCount() > 0 ? incomingDoc.frame(0) : QImage());
            QMap<quint64, QImage> scaledBgCache;

            for (int f = 0; f < frameCount; ++f) {
                QSize fSize = (f < docBoxes.size() && !docBoxes[f].rect.isEmpty())
                                  ? docBoxes[f].rect.size()
                                  : (f < docFrames.size() ? docFrames[f].size() : QSize(32, 32));
                quint64 sizeKey = (static_cast<quint64>(fSize.width()) << 32) | static_cast<quint32>(fSize.height());

                if (!scaledBgCache.contains(sizeKey)) {
                    QImage scaled = srcBg.scaled(fSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                    if (scaled.size() != fSize) {
                        int ox = (scaled.width() - fSize.width()) / 2;
                        int oy = (scaled.height() - fSize.height()) / 2;
                        scaled = scaled.copy(ox, oy, fSize.width(), fSize.height());
                    }
                    scaledBgCache[sizeKey] = scaled.convertToFormat(QImage::Format_ARGB32);
                }

                SpriteCel bgCel;
                bgCel.layerIndex = 0;
                bgCel.layerId = bgLayer.id;
                bgCel.opacity = 255;
                bgCel.x = 0;
                bgCel.y = 0;
                bgCel.image = scaledBgCache.value(sizeKey);
                combinedCels[f].prepend(bgCel);
            }
        } else if (incomingDoc.hasLayers()) {
            for (int l = 0; l < incomingDoc.layers().size(); ++l) {
                SpriteLayer incLayer = incomingDoc.layer(l);
                SpriteLayer newLayer = incLayer;
                newLayer.id = QStringLiteral("layer_%1_%2").arg(QFileInfo(filePath).baseName()).arg(incLayer.id.isEmpty() ? QString::number(l) : incLayer.id);
                newLayer.zOrder = combinedLayers.size();
                int newLayerIdx = combinedLayers.size();
                combinedLayers.append(newLayer);

                const bool sameFrameCount = (incomingDoc.frameCount() == frameCount && incomingDoc.frameCount() > 0);

                for (int f = 0; f < frameCount; ++f) {
                    if (sameFrameCount && incomingDoc.hasFrameCels(f)) {
                        for (const SpriteCel &c : incomingDoc.frameCels(f)) {
                            if (c.layerIndex == l || (!incLayer.id.isEmpty() && c.layerId == incLayer.id)) {
                                SpriteCel celCopy = c;
                                celCopy.layerIndex = newLayerIdx;
                                celCopy.layerId = newLayer.id;
                                combinedCels[f].append(celCopy);
                                break;
                            }
                        }
                    } else {
                        SpriteCel emptyCel;
                        emptyCel.layerIndex = newLayerIdx;
                        emptyCel.layerId = newLayer.id;
                        emptyCel.opacity = 255;
                        emptyCel.x = 0;
                        emptyCel.y = 0;
                        emptyCel.image = QImage((f < docFrames.size()) ? docFrames[f].size() : QSize(32, 32), QImage::Format_ARGB32_Premultiplied);
                        emptyCel.image.fill(Qt::transparent);
                        combinedCels[f].append(emptyCel);
                    }
                }
            }
        } else {
            SpriteLayer newLayer;
            newLayer.id = QStringLiteral("layer_%1_%2").arg(QFileInfo(filePath).baseName()).arg(QUuid::createUuid().toString(QUuid::Id128).left(8));
            newLayer.name = QFileInfo(filePath).completeBaseName();
            newLayer.visible = true;
            newLayer.locked = false;
            newLayer.opacity = 255;
            newLayer.zOrder = combinedLayers.size();
            int newLayerIdx = combinedLayers.size();
            combinedLayers.append(newLayer);

            QImage incomingAtlas = incomingDoc.atlas();
            if (incomingAtlas.isNull() && incomingDoc.frameCount() > 0) {
                incomingAtlas = incomingDoc.frame(0);
            }

            QSize targetAtlasSize = docAtlas.isNull() ? QSize() : docAtlas.size();
            if (targetAtlasSize.isValid() && !targetAtlasSize.isEmpty() && !incomingAtlas.isNull() && incomingAtlas.size() != targetAtlasSize) {
                incomingAtlas = incomingAtlas.scaled(targetAtlasSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }

            for (int f = 0; f < frameCount; ++f) {
                SpriteCel cel;
                cel.layerIndex = newLayerIdx;
                cel.layerId = newLayer.id;
                cel.opacity = 255;
                cel.x = 0;
                cel.y = 0;

                QRect r = (f < docBoxes.size()) ? docBoxes[f].rect : QRect();
                if (!r.isEmpty() && !incomingAtlas.isNull()) {
                    QRect clipped = r.intersected(incomingAtlas.rect());
                    if (!clipped.isEmpty()) {
                        if (clipped.size() == r.size()) {
                            cel.image = incomingAtlas.copy(clipped);
                        } else {
                            cel.image = QImage(r.size(), QImage::Format_ARGB32_Premultiplied);
                            cel.image.fill(Qt::transparent);
                            QPainter p(&cel.image);
                            p.drawImage(clipped.topLeft() - r.topLeft(), incomingAtlas.copy(clipped));
                            p.end();
                        }
                    } else {
                        cel.image = QImage(r.size(), QImage::Format_ARGB32_Premultiplied);
                        cel.image.fill(Qt::transparent);
                    }
                } else if (f < incomingDoc.frameCount()) {
                    cel.image = incomingDoc.frame(f);
                } else {
                    cel.image = QImage((f < docFrames.size()) ? docFrames[f].size() : QSize(32, 32), QImage::Format_ARGB32_Premultiplied);
                    cel.image.fill(Qt::transparent);
                }

                combinedCels[f].append(cel);
            }
        }

        // Pre-composite frames and atlas in worker thread
        QList<QImage> compositedFrames;
        compositedFrames.reserve(frameCount);
        for (int f = 0; f < frameCount; ++f) {
            QSize fSize = (f < docBoxes.size() && !docBoxes[f].rect.isEmpty())
                              ? docBoxes[f].rect.size()
                              : ((f < docFrames.size() && !docFrames[f].isNull()) ? docFrames[f].size() : QSize(32, 32));
            QList<SpriteCel> cels = combinedCels.value(f);
            compositedFrames.append(compositeFrameFromCels(fSize, combinedLayers, cels));
        }

        QImage compositedAtlas = compositeAtlasFromFrames(docAtlas.size(), compositedFrames, docBoxes);

        result.layers = combinedLayers;
        result.frameCels = combinedCels;
        result.frameImages = compositedFrames;
        result.atlas = compositedAtlas;
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
    QFileInfo fi(filePath);
    if (filePath.trimmed().isEmpty() || fi.completeBaseName().trimmed().isEmpty() || fi.isDir()) {
        QString err = tr("Export file path must have a valid file name: %1").arg(filePath);
        if (errorMsg) *errorMsg = err;
        return false;
    }

    if (!m_document || m_document->isEmpty()) {
        QString err = tr("Document is empty.");
        if (errorMsg) *errorMsg = err;
        return false;
    }

    Extractor *extractor = nullptr;
    if (options.format == FORMAT_UNITY) {
        extractor = ExtractorRegistry::instance().findExtractorById(QStringLiteral("unity"));
    } else if (options.format == FORMAT_GODOT) {
        extractor = ExtractorRegistry::instance().findExtractorById(QStringLiteral("godot"));
    }
    if (!extractor) {
        extractor = ExtractorRegistry::instance().findEncoder(filePath);
    }
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
    if (m_document) {
        m_document->setFilePath(filePath);
    }
    addRecentFile(filePath);
    emit statusMessage(tr("Saved %1 successfully.").arg(QFileInfo(filePath).fileName()));
    emit fileSaved(filePath);
    return true;
}

QStringList ProjectController::recentFiles() const
{
    QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
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

    QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
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
    QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
    settings.remove(QStringLiteral("recentFiles"));
    emit recentFilesChanged(QStringList());
}

QRgb ProjectController::detectDominantBackgroundColor(const QImage &srcImage, int minAlpha)
{
    if (srcImage.isNull()) return 0;
    if (minAlpha < 0) {
        minAlpha = AppConfig::instance().project().backgroundMinAlpha;
    }

    QImage image = (srcImage.format() == QImage::Format_ARGB32)
        ? srcImage
        : srcImage.convertToFormat(QImage::Format_ARGB32);

    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) return 0;

    std::vector<const QRgb*> constScanLines(h);
    for (int y = 0; y < h; ++y) {
        constScanLines[y] = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    }

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
    return backgroundColor;
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

    const QRgb backgroundColor = detectDominantBackgroundColor(srcImage, minAlpha);
    if (qAlpha(backgroundColor) < minAlpha) return image;

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
    if (cleanedImage.isNull()) return false;

    m_document->setAtlas(cleanedImage);

    if (!detectSpritesAndRefresh(alphaThreshold, verticalTolerance, smartCrop, overlapThreshold)) {
        return false;
    }

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

bool ProjectController::detectSpritesAndRefresh(int alphaThreshold,
                                                int verticalTolerance,
                                                bool smartCrop,
                                                double overlapThreshold,
                                                int minSliceSize)
{
    if (!m_document || m_document->atlas().isNull()) return false;

    emit statusMessage(tr("Detecting sprites..."));

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
    opts.minSliceSize = minSliceSize > 0 ? minSliceSize : AppConfig::instance().atlas().minSliceSize;

    if (!SpriteDetector::detectToImages(m_document->atlas(), frameImages, boxes, opts)) {
        return false;
    }

    m_document->setFrames(frameImages, boxes);

    emit spritesDetected();
    emit statusMessage(tr("%1 sprite(s) detected.").arg(boxes.size()));
    return true;
}

void ProjectController::detectSpritesAndRefreshAsync(int alphaThreshold,
                                                     int verticalTolerance,
                                                     bool smartCrop,
                                                     double overlapThreshold,
                                                     int minSliceSize)
{
    if (m_isProcessing || !m_document || m_document->atlas().isNull()) return;

    m_isProcessing = true;
    emit processingStarted();
    emit statusMessage(tr("Detecting sprites in background..."));
    emit progressChanged(20);

    QImage atlasCopy = m_document->atlas();
    const int actualAlpha = (alphaThreshold < 0) ? AppConfig::instance().atlas().defaultAlphaThreshold : alphaThreshold;
    const int actualMinSize = minSliceSize > 0 ? minSliceSize : AppConfig::instance().atlas().minSliceSize;

    QFuture<AsyncExtractionResult> future = QtConcurrent::run([atlasCopy, actualAlpha, verticalTolerance, smartCrop, overlapThreshold, actualMinSize]() -> AsyncExtractionResult {
        AsyncExtractionResult result;
        result.type = AsyncExtractionResult::JobDetectSprites;

        SpriteDetectionOptions opts;
        opts.alphaThreshold = actualAlpha;
        opts.verticalTolerance = verticalTolerance;
        opts.smartCrop = smartCrop;
        opts.overlapThreshold = overlapThreshold;
        opts.minSliceSize = actualMinSize;

        if (!SpriteDetector::detectToImages(atlasCopy, result.frameImages, result.boxes, opts)) {
            result.success = false;
            result.errorMessage = QObject::tr("Failed to segment sprite frames.");
            return result;
        }

        result.atlas = atlasCopy;
        result.success = true;
        return result;
    });

    m_watcher.setFuture(future);
}

void ProjectController::onAsyncJobFinished()
{
    AsyncExtractionResult res = m_watcher.result();

    if (!res.success) {
        m_isProcessing = false;
        emit processingFinished();
        emit progressChanged(0);
        if (res.type == AsyncExtractionResult::JobOpen) {
            emit fileLoadError(res.filePath, res.errorMessage);
        } else {
            emit statusMessage(res.errorMessage);
        }
        return;
    }

    if (res.type == AsyncExtractionResult::JobOpen) {
        if (m_document) {
            m_document->setAtlas(res.atlas);
            m_document->setFrames(res.frameImages, res.boxes);
            m_document->setLayers(res.layers);
            for (auto it = res.frameCels.begin(); it != res.frameCels.end(); ++it) {
                m_document->setFrameCels(it.key(), it.value());
            }
            if (!res.animations.isEmpty()) {
                m_document->setAnimations(res.animations);
            }
            if (!res.skinProfiles.isEmpty()) {
                m_document->setSkinProfiles(res.skinProfiles);
            }
            m_document->setFilePath(res.filePath);
        }

        m_currentFilePath = res.filePath;
        m_currentProjectPath.clear();
        addRecentFile(res.filePath);
        if (m_sessionManager) {
            m_sessionManager->startNewSession();
            ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir());
            m_sessionManager->gitCommit(tr("Open %1").arg(QFileInfo(res.filePath).fileName()));
        }
        if (m_undoStack) {
            m_undoStack->clear();
        }
        m_lastUndoIndex = 0;
        m_undoCommitHistory.clear();
        m_undoCommands.clear();
        if (m_sessionManager) {
            m_undoCommitHistory.insert(0, m_sessionManager->gitHeadCommitHash());
        }
        setProjectModified(false);
        emit statusMessage(tr("Loaded %1 successfully.").arg(QFileInfo(res.filePath).fileName()));
        emit progressChanged(100);
        emit fileLoaded(res.filePath);
    } else if (res.type == AsyncExtractionResult::JobReplaceAtlas) {
        if (m_document) {
            if (m_undoStack) {
                m_undoStack->push(new ReplaceAtlasMediaCommand(m_document,
                                                               res.atlas,
                                                               res.frameImages,
                                                               res.boxes,
                                                               res.layers,
                                                               res.frameCels,
                                                               res.animations,
                                                               res.skinProfiles));
            } else {
                m_document->setAtlas(res.atlas);
                m_document->setFrames(res.frameImages, res.boxes);
                m_document->setLayers(res.layers);
                for (auto it = res.frameCels.begin(); it != res.frameCels.end(); ++it) {
                    m_document->setFrameCels(it.key(), it.value());
                }
                if (!res.animations.isEmpty()) {
                    m_document->setAnimations(res.animations);
                }
                if (!res.skinProfiles.isEmpty()) {
                    m_document->setSkinProfiles(res.skinProfiles);
                }
            }
            setProjectModified(true);
            if (m_sessionManager && m_sessionManager->hasActiveSession()) {
                ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir());
                m_sessionManager->gitCommit(tr("Replace atlas with %1").arg(QFileInfo(res.filePath).fileName()));
            }
            addRecentFile(res.filePath);
            emit statusMessage(tr("Replaced atlas with %1 successfully.").arg(QFileInfo(res.filePath).fileName()));
            emit progressChanged(100);
            emit fileLoaded(res.filePath);
        }
    } else if (res.type == AsyncExtractionResult::JobAddLayers || res.type == AsyncExtractionResult::JobAddCommonBackground) {
        if (m_document) {
            const bool asCommonBackground = (res.type == AsyncExtractionResult::JobAddCommonBackground);
            if (m_undoStack) {
                m_undoStack->push(new AddMediaLayersCommand(m_document,
                                                            res.layers,
                                                            res.frameCels,
                                                            QFileInfo(res.filePath).fileName(),
                                                            res.frameImages,
                                                            res.atlas));
            } else {
                m_document->setLayers(res.layers);
                for (auto it = res.frameCels.begin(); it != res.frameCels.end(); ++it) {
                    m_document->setFrameCels(it.key(), it.value());
                }
                if (!res.frameImages.isEmpty()) {
                    m_document->setCompositedFrames(res.frameImages);
                } else {
                    m_document->recompositeAllFrames();
                }
                if (!res.atlas.isNull()) {
                    m_document->setAtlas(res.atlas);
                } else {
                    m_document->recompositeAtlas();
                }
            }

            setProjectModified(true);
            if (m_sessionManager && m_sessionManager->hasActiveSession()) {
                ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir());
                m_sessionManager->gitCommit(tr("Add layers from %1").arg(QFileInfo(res.filePath).fileName()));
            }

            addRecentFile(res.filePath);
            emit statusMessage(asCommonBackground
                ? tr("Added common background from %1 successfully.").arg(QFileInfo(res.filePath).fileName())
                : tr("Added layers from %1 successfully.").arg(QFileInfo(res.filePath).fileName()));
            emit progressChanged(100);
            emit fileLoaded(res.filePath);
        }
    } else if (res.type == AsyncExtractionResult::JobRemoveBackground) {
        if (m_document) {
            m_document->setAtlas(res.atlas);
            m_document->setFrames(res.frameImages, res.boxes);
            m_document->setFilePath(res.filePath);
        }
        emit statusMessage(tr("Background removed successfully."));
        emit progressChanged(100);
        emit backgroundRemoved();
    } else if (res.type == AsyncExtractionResult::JobDetectSprites) {
        if (m_document) {
            m_document->setAtlas(res.atlas);
            m_document->setFrames(res.frameImages, res.boxes);
            m_document->setFilePath(res.filePath);
        }
        emit statusMessage(tr("%1 sprite(s) detected successfully.").arg(res.frameImages.size()));
        emit progressChanged(100);
        emit spritesDetected();
    }

    m_isProcessing = false;
    emit processingFinished();
}

void ProjectController::onUndoStackIndexChanged(int idx)
{
    if (m_isProjectLoading) return;
    if (!m_sessionManager || !m_sessionManager->hasActiveSession()) return;
    if (!m_document || m_document->isEmpty()) return;

    if (m_undoStack && m_undoStack->count() == 0) {
        m_lastUndoIndex = 0;
        m_undoCommands.clear();
        return;
    }

    if (idx < m_lastUndoIndex) {
        // User performed an UNDO: step back in git history without creating a new commit
        m_lastUndoIndex = idx;
        if (m_undoCommitHistory.contains(idx)) {
            const QString targetHash = m_undoCommitHistory.value(idx);
            if (!targetHash.isEmpty()) {
                m_sessionManager->gitCheckout(targetHash);
            }
        }
        emit projectHistoryChanged();
        return;
    }

    if (idx > m_lastUndoIndex) {
        const QUndoCommand *currentCmd = m_undoStack ? m_undoStack->command(idx - 1) : nullptr;
        bool isRedoOfSameCommand = (currentCmd != nullptr) && (m_undoCommands.value(idx, nullptr) == currentCmd);

        // Check if this is a true REDO of an already known command & commit
        if (isRedoOfSameCommand && m_undoCommitHistory.contains(idx)) {
            m_lastUndoIndex = idx;
            const QString targetHash = m_undoCommitHistory.value(idx);
            if (!targetHash.isEmpty()) {
                m_sessionManager->gitCheckout(targetHash);
            }
            emit projectHistoryChanged();
            return;
        }

        // Otherwise, this is a brand new command!
        // Prune stale in-memory undo mapping after current index
        auto it = m_undoCommitHistory.begin();
        while (it != m_undoCommitHistory.end()) {
            if (it.key() >= idx) {
                it = m_undoCommitHistory.erase(it);
            } else {
                ++it;
            }
        }

        auto itCmd = m_undoCommands.begin();
        while (itCmd != m_undoCommands.end()) {
            if (itCmd.key() >= idx) {
                itCmd = m_undoCommands.erase(itCmd);
            } else {
                ++itCmd;
            }
        }

        // Save project JSON to scratch session directory
        ProjectManager::saveProjectToSessionDir(*m_document, m_sessionManager->currentSessionDir());

        QString commitMsg = currentCmd ? currentCmd->text() : tr("Action executed");
        if (commitMsg.trimmed().isEmpty()) {
            commitMsg = tr("Project modified");
        }

        m_lastUndoIndex = idx;
        m_sessionManager->gitCommit(commitMsg);
        m_undoCommitHistory.insert(idx, m_sessionManager->gitHeadCommitHash());
        m_undoCommands.insert(idx, currentCmd);
        emit projectHistoryChanged();
        return;
    }
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
    m_undoCommitHistory.clear();
    m_undoCommands.clear();
    m_undoCommitHistory.insert(0, targetHash);
    m_isProjectLoading = false;

    emit m_document->documentReset();
    emit projectHistoryChanged();
    emit statusMessage(tr("Checked out revision %1.").arg(targetHash.left(7)));
    return true;
}

bool ProjectController::canUndoGit() const
{
    if (!m_sessionManager || !m_sessionManager->hasActiveSession()) return false;
    QString headHash = m_sessionManager->gitHeadCommitHash();
    if (headHash.isEmpty()) return false;
    return !m_sessionManager->gitParentCommitHash(headHash).isEmpty();
}

bool ProjectController::canRedoGit() const
{
    if (!m_sessionManager || !m_sessionManager->hasActiveSession()) return false;
    QString headHash = m_sessionManager->gitHeadCommitHash();
    if (headHash.isEmpty()) return false;
    return !m_sessionManager->gitChildrenOf(headHash).isEmpty();
}

bool ProjectController::hasMultipleRedoBranches() const
{
    if (!m_sessionManager || !m_sessionManager->hasActiveSession()) return false;
    QString headHash = m_sessionManager->gitHeadCommitHash();
    if (headHash.isEmpty()) return false;
    return m_sessionManager->gitChildrenOf(headHash).size() > 1;
}

QList<GitCommitInfo> ProjectController::redoBranches() const
{
    if (!m_sessionManager || !m_sessionManager->hasActiveSession()) return {};
    QString headHash = m_sessionManager->gitHeadCommitHash();
    if (headHash.isEmpty()) return {};
    return m_sessionManager->gitChildrenOf(headHash);
}

bool ProjectController::undoGit()
{
    if (!m_sessionManager || !m_sessionManager->hasActiveSession()) return false;
    QString headHash = m_sessionManager->gitHeadCommitHash();
    if (headHash.isEmpty()) return false;

    QString parentHash = m_sessionManager->gitParentCommitHash(headHash);
    if (parentHash.isEmpty()) return false;

    QString err;
    return checkoutRevision(parentHash, &err);
}

bool ProjectController::redoGit(const QString &targetCommitHash)
{
    if (!m_sessionManager || !m_sessionManager->hasActiveSession()) return false;
    QString headHash = m_sessionManager->gitHeadCommitHash();
    if (headHash.isEmpty()) return false;

    QString commitToCheckout = targetCommitHash;
    if (commitToCheckout.isEmpty()) {
        QList<GitCommitInfo> children = m_sessionManager->gitChildrenOf(headHash);
        if (children.isEmpty()) return false;
        commitToCheckout = children.first().hash;
    }

    QString err;
    return checkoutRevision(commitToCheckout, &err);
}



