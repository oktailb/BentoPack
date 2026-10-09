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

#include "packer/multiatlaspacker.h"
#include "model/spritedocument.h"
#include <QPainter>
#include <QRegularExpression>

MaterialMapType MultiAtlasPacker::detectMapType(const QString &name)
{
    QString clean = name.trimmed().toLower();
    if (clean.isEmpty()) {
        return MaterialMapType::Albedo;
    }

    // Normal Map conventions: _n, _normal, _norm, -n, .n
    if (clean.endsWith(QStringLiteral("_n")) ||
        clean.endsWith(QStringLiteral("_normal")) ||
        clean.endsWith(QStringLiteral("_norm")) ||
        clean.endsWith(QStringLiteral("-n")) ||
        clean.endsWith(QStringLiteral(".n")) ||
        clean == QStringLiteral("n") ||
        clean == QStringLiteral("normal") ||
        clean == QStringLiteral("norm")) {
        return MaterialMapType::Normal;
    }

    // Emissive Map conventions: _e, _emissive, _emit, _glow, -e, .e
    if (clean.endsWith(QStringLiteral("_e")) ||
        clean.endsWith(QStringLiteral("_emissive")) ||
        clean.endsWith(QStringLiteral("_emit")) ||
        clean.endsWith(QStringLiteral("_glow")) ||
        clean.endsWith(QStringLiteral("-e")) ||
        clean.endsWith(QStringLiteral(".e")) ||
        clean == QStringLiteral("e") ||
        clean == QStringLiteral("emissive") ||
        clean == QStringLiteral("emit") ||
        clean == QStringLiteral("glow")) {
        return MaterialMapType::Emissive;
    }

    // Specular / Roughness Map conventions: _s, _specular, _spec, _roughness, _rough, _metal, -s, .s
    if (clean.endsWith(QStringLiteral("_s")) ||
        clean.endsWith(QStringLiteral("_specular")) ||
        clean.endsWith(QStringLiteral("_spec")) ||
        clean.endsWith(QStringLiteral("_roughness")) ||
        clean.endsWith(QStringLiteral("_rough")) ||
        clean.endsWith(QStringLiteral("_metal")) ||
        clean.endsWith(QStringLiteral("-s")) ||
        clean.endsWith(QStringLiteral(".s")) ||
        clean == QStringLiteral("s") ||
        clean == QStringLiteral("specular") ||
        clean == QStringLiteral("spec") ||
        clean == QStringLiteral("roughness") ||
        clean == QStringLiteral("rough")) {
        return MaterialMapType::Specular;
    }

    return MaterialMapType::Albedo;
}

QString MultiAtlasPacker::mapTypeSuffix(MaterialMapType type)
{
    switch (type) {
    case MaterialMapType::Normal:   return QStringLiteral("_n");
    case MaterialMapType::Emissive: return QStringLiteral("_e");
    case MaterialMapType::Specular: return QStringLiteral("_s");
    case MaterialMapType::Albedo:
    default:
        return QString();
    }
}

QString MultiAtlasPacker::mapTypeName(MaterialMapType type)
{
    switch (type) {
    case MaterialMapType::Normal:   return QStringLiteral("Normal");
    case MaterialMapType::Emissive: return QStringLiteral("Emissive");
    case MaterialMapType::Specular: return QStringLiteral("Specular");
    case MaterialMapType::Albedo:
    default:
        return QStringLiteral("Albedo");
    }
}

QColor MultiAtlasPacker::neutralColor(MaterialMapType type)
{
    switch (type) {
    case MaterialMapType::Normal:
        // Industry standard tangent-space flat normal vector (0, 0, 1) -> RGBA(128, 128, 255, 0)
        return QColor(128, 128, 255, 0);
    case MaterialMapType::Emissive:
    case MaterialMapType::Specular:
    case MaterialMapType::Albedo:
    default:
        return QColor(0, 0, 0, 0);
    }
}

QImage MultiAtlasPacker::flipNormalMapY(const QImage &normalMap)
{
    if (normalMap.isNull()) {
        return normalMap;
    }

    QImage flipped = normalMap.convertToFormat(QImage::Format_ARGB32);
    const int h = flipped.height();
    const int w = flipped.width();

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(flipped.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb pixel = line[x];
            int a = qAlpha(pixel);
            if (a > 0) {
                int r = qRed(pixel);
                int g = 255 - qGreen(pixel);
                int b = qBlue(pixel);
                line[x] = qRgba(r, g, b, a);
            }
        }
    }

    return flipped;
}

QImage MultiAtlasPacker::generateSlaveAtlas(const QList<QImage> &slaveFrames,
                                            const AtlasPackResult &masterResult,
                                            MaterialMapType mapType,
                                            int extrude,
                                            bool normalMapYFlip)
{
    if (masterResult.dimensions.isEmpty() || slaveFrames.isEmpty() || masterResult.frameRects.isEmpty()) {
        return QImage();
    }

    QImage slaveAtlas(masterResult.dimensions, QImage::Format_ARGB32_Premultiplied);
    slaveAtlas.fill(neutralColor(mapType));

    QPainter painter(&slaveAtlas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    // Track populated regions to prevent redundant redraws during deduplication
    QSet<int> renderedIndices;

    for (int i = 0; i < masterResult.frameRects.size(); ++i) {
        int srcIdx = i;
        if (!masterResult.duplicateMapping.isEmpty() && i < masterResult.duplicateMapping.size()) {
            srcIdx = masterResult.duplicateMapping.at(i);
        }

        if (renderedIndices.contains(srcIdx)) {
            continue;
        }
        renderedIndices.insert(srcIdx);

        if (srcIdx >= 0 && srcIdx < slaveFrames.size()) {
            const QImage &frameImg = slaveFrames.at(srcIdx);
            const QRect &targetRect = masterResult.frameRects.at(i);

            if (!frameImg.isNull() && targetRect.isValid()) {
                painter.drawImage(targetRect, frameImg);

                if (extrude > 0) {
                    AtlasPacker::applyExtrusion(slaveAtlas, targetRect, frameImg, extrude);
                }
            }
        }
    }
    painter.end();

    if (mapType == MaterialMapType::Normal && normalMapYFlip) {
        slaveAtlas = flipNormalMapY(slaveAtlas);
    }

    return slaveAtlas;
}

MultiAtlasPackResult MultiAtlasPacker::pack(const SpriteDocument &doc,
                                            const AtlasPacker::PackOptions &options,
                                            bool normalMapYFlip)
{
    MultiAtlasPackResult result;

    QList<QImage> albedoFrames = doc.framesForMap(MaterialMapType::Albedo);
    if (albedoFrames.isEmpty()) {
        albedoFrames = doc.frames();
    }

    if (albedoFrames.isEmpty()) {
        result.success = false;
        return result;
    }

    QList<QPolygonF> docPolygons;
    docPolygons.reserve(doc.frameCount());
    for (int i = 0; i < doc.frameCount(); ++i) {
        docPolygons.append(doc.box(i).hasPolygonMesh ? doc.box(i).polygon : QPolygonF());
    }

    // 1. Pack Master Atlas (Albedo)
    if (options.algorithm == AtlasPacker::KeepLayout && !doc.atlas().isNull()) {
        result.master.atlas = doc.atlas();
        result.master.frameRects.reserve(doc.boxes().size());
        for (const SpriteBox &box : doc.boxes()) {
            result.master.frameRects.append(box.rect);
        }
        result.master.dimensions = doc.atlas().size();
        result.master.uniqueFramesCount = doc.frameCount();
        result.master.success = true;
    } else {
        result.master = AtlasPacker::pack(albedoFrames, options, docPolygons);
    }

    result.success = result.master.success;
    if (!result.success) {
        return result;
    }

    // 2. Generate Slave Atlases for each active material map
    const MaterialMapType auxTypes[] = {
        MaterialMapType::Normal,
        MaterialMapType::Emissive,
        MaterialMapType::Specular
    };

    for (MaterialMapType type : auxTypes) {
        if (doc.hasAuxiliaryMap(type)) {
            QList<QImage> slaveFrames = doc.framesForMap(type);
            if (!slaveFrames.isEmpty()) {
                QImage slaveAtlas = generateSlaveAtlas(slaveFrames, result.master, type, options.extrude, normalMapYFlip);
                if (!slaveAtlas.isNull()) {
                    result.auxiliaryAtlases.insert(type, slaveAtlas);
                    result.maps.insert(type, slaveAtlas);
                }
            }
        }
    }

    return result;
}
