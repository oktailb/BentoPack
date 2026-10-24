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

#include "commands/filtercommands.h"
#include "image/pixelhash.h"

ApplyFilterCommand::ApplyFilterCommand(SpriteDocument *doc,
                                       const QString &filterTitle,
                                       const QImage &oldAtlas,
                                       const QList<QImage> &oldFrames,
                                       const QList<SpriteBox> &oldBoxes,
                                       const QMap<QString, SpriteAnimation> &oldAnimations,
                                       const QImage &newAtlas,
                                       const QList<QImage> &newFrames,
                                       const QList<SpriteBox> &newBoxes,
                                       const QMap<QString, SpriteAnimation> &newAnimations,
                                       const QList<SpriteLayer> &oldLayers,
                                       const QMap<int, QList<SpriteCel>> &oldFrameCels,
                                       const QMap<QString, SkinProfile> &oldSkinProfiles,
                                       QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_oldFrames(oldFrames)
    , m_oldBoxes(oldBoxes)
    , m_oldAnimations(oldAnimations)
    , m_newFrames(newFrames)
    , m_newBoxes(newBoxes)
    , m_newAnimations(newAnimations)
    , m_layersBackup(oldLayers)
    , m_frameCelsBackup(oldFrameCels)
    , m_skinProfilesBackup(oldSkinProfiles)
{
    setText(filterTitle.isEmpty() ? QStringLiteral("Apply Filter") : filterTitle);

    // Differential dirty-rect optimization for high-res (4K/8K/16K) atlases
    if (!oldAtlas.isNull() && !newAtlas.isNull() && oldAtlas.size() == newAtlas.size()) {
        m_atlasDirtyRect = PixelHash::computeDirtyRect(oldAtlas, newAtlas);
        const qint64 totalPixels = static_cast<qint64>(oldAtlas.width()) * oldAtlas.height();
        const qint64 dirtyPixels = static_cast<qint64>(m_atlasDirtyRect.width()) * m_atlasDirtyRect.height();

        if (m_atlasDirtyRect.isEmpty()) {
            // Atlas is completely untouched by the filter
            m_isFullAtlasReplacement = false;
        } else if (dirtyPixels >= totalPixels * 0.90) {
            // Over 90% changed: full atlas snapshot
            m_isFullAtlasReplacement = true;
            m_oldAtlasFallback = oldAtlas;
            m_newAtlasFallback = newAtlas;
        } else {
            // Sub-rectangle changed: store only differential dirty bounding box patches
            m_isFullAtlasReplacement = false;
            m_oldAtlasPatch = oldAtlas.copy(m_atlasDirtyRect);
            m_newAtlasPatch = newAtlas.copy(m_atlasDirtyRect);
        }
    } else {
        m_isFullAtlasReplacement = true;
        m_oldAtlasFallback = oldAtlas;
        m_newAtlasFallback = newAtlas;
    }

    if (m_doc) {
        if (m_layersBackup.isEmpty() && m_doc->hasLayers()) {
            m_layersBackup = m_doc->layers();
        }
        if (m_frameCelsBackup.isEmpty()) {
            for (int i = 0; i < m_doc->frameCount(); ++i) {
                if (m_doc->hasFrameCels(i)) {
                    m_frameCelsBackup[i] = m_doc->frameCels(i);
                }
            }
        }
        if (m_skinProfilesBackup.isEmpty() && m_doc->hasSkinProfiles()) {
            m_skinProfilesBackup = m_doc->skinProfiles();
        }
    }
}

void ApplyFilterCommand::redo()
{
    if (!m_doc) return;
    if (m_isFullAtlasReplacement) {
        if (!m_newAtlasFallback.isNull()) {
            m_doc->setAtlas(m_newAtlasFallback);
        }
    } else if (!m_atlasDirtyRect.isEmpty() && !m_newAtlasPatch.isNull()) {
        m_doc->patchAtlasDirect(m_atlasDirtyRect, m_newAtlasPatch);
    }
    m_doc->setFrames(m_newFrames, m_newBoxes);
    if (!m_newAnimations.isEmpty() || !m_oldAnimations.isEmpty()) {
        m_doc->setAnimations(m_newAnimations);
    }
}

void ApplyFilterCommand::undo()
{
    if (!m_doc) return;
    if (m_isFullAtlasReplacement) {
        if (!m_oldAtlasFallback.isNull()) {
            m_doc->setAtlas(m_oldAtlasFallback);
        }
    } else if (!m_atlasDirtyRect.isEmpty() && !m_oldAtlasPatch.isNull()) {
        m_doc->patchAtlasDirect(m_atlasDirtyRect, m_oldAtlasPatch);
    }
    m_doc->setFrames(m_oldFrames, m_oldBoxes);
    if (!m_layersBackup.isEmpty()) {
        m_doc->setLayers(m_layersBackup);
        for (auto it = m_frameCelsBackup.begin(); it != m_frameCelsBackup.end(); ++it) {
            m_doc->setFrameCels(it.key(), it.value());
        }
    }
    if (!m_skinProfilesBackup.isEmpty()) {
        m_doc->setSkinProfiles(m_skinProfilesBackup);
    }
    if (!m_newAnimations.isEmpty() || !m_oldAnimations.isEmpty()) {
        m_doc->setAnimations(m_oldAnimations);
    }
}

SetAuxiliaryMapCommand::SetAuxiliaryMapCommand(SpriteDocument *doc,
                                               MaterialMapType mapType,
                                               const QList<QImage> &newFrames,
                                               const QString &commandName,
                                               QUndoCommand *parent)
    : QUndoCommand(commandName.isEmpty() ? QObject::tr("Set Auxiliary Map") : commandName, parent)
    , m_doc(doc)
    , m_mapType(mapType)
    , m_newFrames(newFrames)
{
    if (m_doc) {
        m_hadOldFrames = m_doc->hasAuxiliaryMap(m_mapType);
        m_oldFrames = m_doc->auxiliaryFrames(m_mapType);
    }
}

void SetAuxiliaryMapCommand::redo()
{
    if (!m_doc) return;
    m_doc->setAuxiliaryFrames(m_mapType, m_newFrames);
}

void SetAuxiliaryMapCommand::undo()
{
    if (!m_doc) return;
    m_doc->setAuxiliaryFrames(m_mapType, m_oldFrames);
}

#include <QDateTime>

AddMaterialLayerCommand::AddMaterialLayerCommand(SpriteDocument *doc,
                                                 MaterialMapType mapType,
                                                 const QList<QImage> &materialFrames,
                                                 const QString &commandName,
                                                 QUndoCommand *parent)
    : QUndoCommand(commandName.isEmpty()
                   ? (mapType == MaterialMapType::Normal
                      ? QObject::tr("Create Normal Layer ")
                      : QObject::tr("Create Emissive Layer "))
                   : commandName, parent)
    , m_doc(doc)
    , m_mapType(mapType)
    , m_materialFrames(materialFrames)
{
    if (m_doc) {
        m_oldLayers = m_doc->layers();
        for (int i = 0; i < m_doc->frameCount(); ++i) {
            if (m_doc->hasFrameCels(i)) {
                m_oldCels[i] = m_doc->frameCels(i);
            }
        }
        m_hadOldAux = m_doc->hasAuxiliaryMap(m_mapType);
        m_oldAuxFrames = m_doc->auxiliaryFrames(m_mapType);
    }
}

void AddMaterialLayerCommand::redo()
{
    if (!m_doc) return;

    QString defaultLayerName = (m_mapType == MaterialMapType::Normal)
        ? QStringLiteral("Normal (_n)")
        : QStringLiteral("Emissive (_e)");

    // If document currently has no layers, initialize base layer from existing frames
    if (!m_doc->hasLayers() && m_doc->frameCount() > 0) {
        SpriteLayer baseLayer;
        baseLayer.id = QStringLiteral("layer_albedo");
        baseLayer.name = QStringLiteral("Albedo");
        baseLayer.visible = true;
        baseLayer.locked = false;
        baseLayer.opacity = 255;
        baseLayer.zOrder = 0;
        baseLayer.blendMode = QPainter::CompositionMode_SourceOver;
        m_doc->addLayer(baseLayer);

        for (int i = 0; i < m_doc->frameCount(); ++i) {
            SpriteCel baseCel;
            baseCel.layerIndex = 0;
            baseCel.layerId = baseLayer.id;
            baseCel.opacity = 255;
            baseCel.image = m_doc->frame(i);
            m_doc->setCel(i, 0, baseCel);
        }
    }

    // Check if a layer with this name already exists
    int targetLayerIdx = -1;
    for (int l = 0; l < m_doc->layerCount(); ++l) {
        if (m_doc->layer(l).name == defaultLayerName) {
            targetLayerIdx = l;
            break;
        }
    }

    if (targetLayerIdx < 0) {
        SpriteLayer matLayer;
        matLayer.id = QStringLiteral("mat_%1_%2").arg(static_cast<int>(m_mapType)).arg(QDateTime::currentMSecsSinceEpoch());
        matLayer.name = defaultLayerName;
        matLayer.visible = true;
        matLayer.locked = false;
        matLayer.opacity = 255;
        matLayer.zOrder = m_doc->layers().isEmpty() ? 10 : (m_doc->layers().last().zOrder + 10);
        matLayer.blendMode = (m_mapType == MaterialMapType::Emissive)
            ? QPainter::CompositionMode_Plus
            : QPainter::CompositionMode_SourceOver;
        m_doc->addLayer(matLayer);
        targetLayerIdx = m_doc->layerCount() - 1;
    }

    QString targetLayerId = m_doc->layer(targetLayerIdx).id;

    // Populate cels for each frame
    for (int i = 0; i < m_doc->frameCount(); ++i) {
        SpriteCel cel;
        cel.layerIndex = targetLayerIdx;
        cel.layerId = targetLayerId;
        cel.opacity = 255;
        if (i < m_materialFrames.size()) {
            cel.image = m_materialFrames.at(i);
        } else if (m_mapType == MaterialMapType::Normal) {
            QSize sz = m_doc->frame(i).size();
            if (sz.isEmpty()) sz = QSize(32, 32);
            QImage neutral(sz, QImage::Format_ARGB32);
            neutral.fill(qRgba(128, 128, 255, 0));
            cel.image = neutral;
        } else {
            QSize sz = m_doc->frame(i).size();
            if (sz.isEmpty()) sz = QSize(32, 32);
            QImage blank(sz, QImage::Format_ARGB32);
            blank.fill(Qt::transparent);
            cel.image = blank;
        }
        m_doc->setCel(i, targetLayerIdx, cel);
    }

    // Synchronize auxiliary frames for multi-atlas export
    m_doc->setAuxiliaryFrames(m_mapType, m_materialFrames);
    m_doc->recompositeAllFrames();
}

void AddMaterialLayerCommand::undo()
{
    if (!m_doc) return;

    if (!m_oldLayers.isEmpty()) {
        m_doc->setLayers(m_oldLayers);
        for (int i = 0; i < m_doc->frameCount(); ++i) {
            if (m_oldCels.contains(i)) {
                m_doc->setFrameCels(i, m_oldCels[i]);
            } else {
                m_doc->setFrameCels(i, {});
            }
        }
    } else {
        m_doc->setLayers({});
        for (int i = 0; i < m_doc->frameCount(); ++i) {
            m_doc->setFrameCels(i, {});
        }
    }

    if (m_hadOldAux) {
        m_doc->setAuxiliaryFrames(m_mapType, m_oldAuxFrames);
    } else {
        m_doc->setAuxiliaryFrames(m_mapType, {});
    }

    m_doc->recompositeAllFrames();
}
