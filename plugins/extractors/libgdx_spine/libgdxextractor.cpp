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

#include "libgdxextractor.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QPainter>
#include <QRegularExpression>
#include <QMap>
#include <algorithm>

LibGdxExtractor::LibGdxExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool LibGdxExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    if (ext != QStringLiteral("atlas") && !filePath.endsWith(QStringLiteral(".atlas.txt"), Qt::CaseInsensitive)) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    // Inspect first few lines for typical LibGDX/Spine header keys
    QString head = QString::fromUtf8(file.read(2048));
    file.close();

    return head.contains(QStringLiteral("format:"), Qt::CaseInsensitive)
        || head.contains(QStringLiteral("filter:"), Qt::CaseInsensitive)
        || head.contains(QStringLiteral("xy:"), Qt::CaseInsensitive)
        || head.contains(QStringLiteral("size:"), Qt::CaseInsensitive);
}

bool LibGdxExtractor::parseAtlasText(const QString &content, QList<AtlasPage> &pages, QString *errorMsg)
{
    QStringList lines = content.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")));
    if (lines.isEmpty()) {
        if (errorMsg) *errorMsg = tr("Atlas file is empty.");
        return false;
    }

    AtlasPage currentPage;
    AtlasRegion currentRegion;
    bool inRegion = false;
    bool pageHasTexture = false;

    auto finishRegion = [&]() {
        if (inRegion && !currentRegion.name.isEmpty()) {
            if (currentRegion.origWidth <= 0) currentRegion.origWidth = currentRegion.width;
            if (currentRegion.origHeight <= 0) currentRegion.origHeight = currentRegion.height;
            currentPage.regions.append(currentRegion);
            currentRegion = AtlasRegion();
            inRegion = false;
        }
    };

    auto finishPage = [&]() {
        finishRegion();
        if (pageHasTexture) {
            pages.append(currentPage);
            currentPage = AtlasPage();
            pageHasTexture = false;
        }
    };

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        QString trimmed = line.trimmed();

        if (trimmed.isEmpty()) {
            // Empty line separates pages in standard LibGDX atlas format
            finishRegion();
            if (pageHasTexture && !currentPage.regions.isEmpty()) {
                finishPage();
            }
            continue;
        }

        // In LibGDX / Spine format, properties have a colon (e.g. size: 64,64 or xy: 10,20),
        // whereas texture filenames and region names never contain colons.
        int colonIdx = trimmed.indexOf(QLatin1Char(':'));
        bool isProperty = (colonIdx != -1);

        if (!isProperty) {
            // Check if this is a new page texture line or a region name
            if (!pageHasTexture) {
                // First non-property line is the texture filename
                currentPage.textureFile = trimmed;
                pageHasTexture = true;
            } else {
                // Non-property line after page header is a region name
                finishRegion();
                currentRegion.name = trimmed;
                inRegion = true;
            }
        } else {
            // Property line key: value
            QString key = trimmed.left(colonIdx).trimmed().toLower();
            QString val = trimmed.mid(colonIdx + 1).trimmed();

            if (!inRegion) {
                // Page-level property
                if (key == QStringLiteral("size")) {
                    QStringList parts = val.split(QLatin1Char(','));
                    if (parts.size() >= 2) {
                        currentPage.width = parts[0].trimmed().toInt();
                        currentPage.height = parts[1].trimmed().toInt();
                    }
                } else if (key == QStringLiteral("format")) {
                    currentPage.format = val;
                } else if (key == QStringLiteral("filter")) {
                    currentPage.filter = val;
                } else if (key == QStringLiteral("repeat")) {
                    currentPage.repeat = val;
                }
            } else {
                // Region-level property
                if (key == QStringLiteral("rotate")) {
                    if (val.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
                        currentRegion.rotate = true;
                        currentRegion.degrees = 90;
                    } else if (val.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
                        currentRegion.rotate = false;
                        currentRegion.degrees = 0;
                    } else {
                        currentRegion.degrees = val.toInt();
                        currentRegion.rotate = (currentRegion.degrees != 0);
                    }
                } else if (key == QStringLiteral("xy")) {
                    QStringList parts = val.split(QLatin1Char(','));
                    if (parts.size() >= 2) {
                        currentRegion.x = parts[0].trimmed().toInt();
                        currentRegion.y = parts[1].trimmed().toInt();
                    }
                } else if (key == QStringLiteral("size")) {
                    QStringList parts = val.split(QLatin1Char(','));
                    if (parts.size() >= 2) {
                        currentRegion.width = parts[0].trimmed().toInt();
                        currentRegion.height = parts[1].trimmed().toInt();
                    }
                } else if (key == QStringLiteral("orig")) {
                    QStringList parts = val.split(QLatin1Char(','));
                    if (parts.size() >= 2) {
                        currentRegion.origWidth = parts[0].trimmed().toInt();
                        currentRegion.origHeight = parts[1].trimmed().toInt();
                    }
                } else if (key == QStringLiteral("offset")) {
                    QStringList parts = val.split(QLatin1Char(','));
                    if (parts.size() >= 2) {
                        currentRegion.offsetX = parts[0].trimmed().toInt();
                        currentRegion.offsetY = parts[1].trimmed().toInt();
                    }
                } else if (key == QStringLiteral("index")) {
                    currentRegion.index = val.toInt();
                } else if (key == QStringLiteral("bounds")) {
                    // Spine newer format: bounds: x,y,w,h
                    QStringList parts = val.split(QLatin1Char(','));
                    if (parts.size() >= 4) {
                        currentRegion.x = parts[0].trimmed().toInt();
                        currentRegion.y = parts[1].trimmed().toInt();
                        currentRegion.width = parts[2].trimmed().toInt();
                        currentRegion.height = parts[3].trimmed().toInt();
                    }
                } else if (key == QStringLiteral("offsets")) {
                    // Spine newer format: offsets: ox,oy,w,h
                    QStringList parts = val.split(QLatin1Char(','));
                    if (parts.size() >= 4) {
                        currentRegion.offsetX = parts[0].trimmed().toInt();
                        currentRegion.offsetY = parts[1].trimmed().toInt();
                        currentRegion.origWidth = parts[2].trimmed().toInt();
                        currentRegion.origHeight = parts[3].trimmed().toInt();
                    }
                }
            }
        }
    }

    finishPage();
    return !pages.isEmpty();
}

bool LibGdxExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    setStatusMessage(tr("Reading LibGDX/Spine Atlas..."));
    setProgress(10);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Cannot open atlas file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    QList<AtlasPage> pages;
    QString parseErr;
    if (!parseAtlasText(content, pages, &parseErr) || pages.isEmpty()) {
        if (error) {
            error->code = ExtractorError::ParsingFailed;
            error->message = parseErr.isEmpty() ? tr("Failed to parse atlas file.") : parseErr;
            error->filePath = filePath;
        }
        return false;
    }

    QDir baseDir = QFileInfo(filePath).dir();
    outDoc.clear();
    outDoc.setFilePath(filePath);

    // Primary page
    const AtlasPage &page = pages.first();

    // Locate companion atlas image
    QString imagePath = baseDir.filePath(page.textureFile);
    if (!QFile::exists(imagePath)) {
        // Fallback: search same base name with common extensions
        QString baseName = QFileInfo(filePath).completeBaseName();
        for (const QString &ext : { QStringLiteral("png"), QStringLiteral("webp"), QStringLiteral("jpg") }) {
            QString cand = baseDir.filePath(baseName + QStringLiteral(".") + ext);
            if (QFile::exists(cand)) {
                imagePath = cand;
                break;
            }
        }
    }

    QImage atlasImage;
    if (!atlasImage.load(imagePath)) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Failed to load companion atlas image: %1").arg(imagePath);
            error->filePath = imagePath;
        }
        return false;
    }

    outDoc.setAtlas(atlasImage);

    // Maps to group animations: animName -> QMap<index, globalFrameIndex>
    QMap<QString, QMap<int, int>> indexedAnimations;
    QMap<QString, QList<int>> sequenceAnimations;

    setProgress(40);
    int totalRegions = page.regions.size();

    for (int i = 0; i < totalRegions; ++i) {
        const AtlasRegion &reg = page.regions.at(i);

        QRect cropRect(reg.x, reg.y, reg.width, reg.height);
        // Clamp to image bounds
        cropRect = cropRect.intersected(atlasImage.rect());

        QImage frameImg;
        if (cropRect.isValid() && !cropRect.isEmpty()) {
            frameImg = atlasImage.copy(cropRect);
            if (reg.rotate) {
                // If rotated 90 degrees in atlas, rotate back for standard display
                QTransform rot;
                rot.rotate(reg.degrees != 0 ? -reg.degrees : -90);
                frameImg = frameImg.transformed(rot);
            }
        } else {
            frameImg = QImage(qMax(1, reg.width), qMax(1, reg.height), QImage::Format_ARGB32_Premultiplied);
            frameImg.fill(Qt::transparent);
        }

        SpriteBox box(QRect(reg.x, reg.y, reg.width, reg.height));
        box.index = i;
        if (reg.offsetX != 0 || reg.offsetY != 0) {
            box.hasCustomPivot = true;
            box.pivot = QPoint(reg.offsetX, reg.offsetY);
        }

        int frameIndex = outDoc.frameCount();
        outDoc.addFrame(frameImg, box);

        // Animation grouping logic
        if (reg.index >= 0) {
            indexedAnimations[reg.name].insert(reg.index, frameIndex);
        } else {
            // Check for numbered suffix like name_0, name_1 or name01
            QRegularExpression suffixRegex(QStringLiteral("^(.+?)[_\\s]?(\\d+)$"));
            auto match = suffixRegex.match(reg.name);
            if (match.hasMatch()) {
                QString base = match.captured(1);
                sequenceAnimations[base].append(frameIndex);
            }
        }

        setProgress(40 + (i * 45 / qMax(1, totalRegions)));
    }

    // Register indexed animations
    for (auto it = indexedAnimations.begin(); it != indexedAnimations.end(); ++it) {
        const QString &animName = it.key();
        QList<int> sortedFrames = it.value().values();
        if (sortedFrames.size() > 1) {
            outDoc.addAnimation(animName, sortedFrames, 12, SpriteAnimation::Loop);
        }
    }

    // Register sequence animations if any
    for (auto it = sequenceAnimations.begin(); it != sequenceAnimations.end(); ++it) {
        const QString &animName = it.key();
        const QList<int> &frames = it.value();
        if (frames.size() > 1 && !outDoc.hasAnimation(animName)) {
            outDoc.addAnimation(animName, frames, 12, SpriteAnimation::Loop);
        }
    }

    setProgress(100);
    setStatusMessage(tr("LibGDX/Spine Atlas loaded successfully."));
    if (error) error->code = ExtractorError::NoError;
    return true;
}

bool LibGdxExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);
    setStatusMessage(tr("Exporting LibGDX/Spine Atlas..."));
    setProgress(10);

    if (inDoc.frameCount() == 0 && inDoc.atlas().isNull()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("Document contains no sprites to export.");
            error->filePath = filePath;
        }
        return false;
    }

    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QString baseName = fi.completeBaseName();
    QString textureFileName = baseName + QStringLiteral(".png");
    QString textureFilePath = dir.filePath(textureFileName);

    // Save companion image
    QImage atlasImg = inDoc.atlas();
    if (atlasImg.isNull()) {
        // If atlas is not precomposed, compose from frames bounding rect
        int maxW = 1024, maxH = 1024;
        for (int i = 0; i < inDoc.frameCount(); ++i) {
            const SpriteBox &b = inDoc.box(i);
            maxW = qMax(maxW, b.rect.right() + 1);
            maxH = qMax(maxH, b.rect.bottom() + 1);
        }
        atlasImg = QImage(maxW, maxH, QImage::Format_ARGB32_Premultiplied);
        atlasImg.fill(Qt::transparent);
        QPainter p(&atlasImg);
        for (int i = 0; i < inDoc.frameCount(); ++i) {
            p.drawImage(inDoc.box(i).rect.topLeft(), inDoc.frame(i));
        }
        p.end();
    }

    if (!atlasImg.save(textureFilePath, "PNG")) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to save companion atlas image: %1").arg(textureFilePath);
            error->filePath = textureFilePath;
        }
        return false;
    }

    setProgress(50);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot create output atlas file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QTextStream out(&file);

    // 1. Page Header
    out << textureFileName << "\n";
    out << "size: " << atlasImg.width() << "," << atlasImg.height() << "\n";
    out << "format: RGBA8888\n";
    out << "filter: Linear,Linear\n";
    out << "repeat: none\n";

    // 2. Build map of frame -> (animation name, animation frame index)
    QMap<int, QPair<QString, int>> frameAnimInfo;
    for (auto it = inDoc.animations().begin(); it != inDoc.animations().end(); ++it) {
        const QString &animName = it.key();
        const QList<int> &fIndices = it.value().frameIndices;
        for (int seq = 0; seq < fIndices.size(); ++seq) {
            int frameIdx = fIndices.at(seq);
            if (!frameAnimInfo.contains(frameIdx)) {
                frameAnimInfo.insert(frameIdx, qMakePair(animName, seq));
            }
        }
    }

    // 3. Write Regions
    int count = inDoc.frameCount();
    for (int i = 0; i < count; ++i) {
        const SpriteBox &box = inDoc.box(i);
        QString regionName;
        int animIndex = -1;

        if (frameAnimInfo.contains(i)) {
            regionName = frameAnimInfo[i].first;
            animIndex = frameAnimInfo[i].second;
        } else {
            regionName = QStringLiteral("sprite_%1").arg(i, 3, 10, QLatin1Char('0'));
            animIndex = -1;
        }

        out << regionName << "\n";
        out << "  rotate: false\n";
        out << "  xy: " << box.rect.x() << ", " << box.rect.y() << "\n";
        out << "  size: " << box.rect.width() << ", " << box.rect.height() << "\n";
        out << "  orig: " << box.rect.width() << ", " << box.rect.height() << "\n";
        out << "  offset: " << (box.hasCustomPivot ? box.pivot.x() : 0) << ", "
                           << (box.hasCustomPivot ? box.pivot.y() : 0) << "\n";
        out << "  index: " << animIndex << "\n";
    }

    file.close();
    setProgress(100);
    setStatusMessage(tr("Exported LibGDX/Spine Atlas successfully."));
    if (error) error->code = ExtractorError::NoError;
    return true;
}
