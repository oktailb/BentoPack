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

#include "plistextractor.h"
#include "packer/vramtexturecompressor.h"
#include "packer/atlaspacker.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QRegularExpression>
#include <QTransform>
#include <QMap>

PlistExtractor::PlistExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool PlistExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    if (fi.suffix().compare(QStringLiteral("plist"), Qt::CaseInsensitive) != 0) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray head = file.read(1024);
    file.close();

    return head.contains("<!DOCTYPE plist") || head.contains("<plist");
}

QRect PlistExtractor::parseRect(const QString &str)
{
    static const QRegularExpression rx(QStringLiteral("-?\\d+"));
    auto it = rx.globalMatch(str);
    QList<int> vals;
    while (it.hasNext()) {
        vals.append(it.next().captured().toInt());
    }
    if (vals.size() >= 4) {
        return QRect(vals[0], vals[1], vals[2], vals[3]);
    }
    return QRect();
}

QPoint PlistExtractor::parsePoint(const QString &str)
{
    static const QRegularExpression rx(QStringLiteral("-?\\d+"));
    auto it = rx.globalMatch(str);
    QList<int> vals;
    while (it.hasNext()) {
        vals.append(it.next().captured().toInt());
    }
    if (vals.size() >= 2) {
        return QPoint(vals[0], vals[1]);
    }
    return QPoint();
}

QSize PlistExtractor::parseSize(const QString &str)
{
    static const QRegularExpression rx(QStringLiteral("-?\\d+"));
    auto it = rx.globalMatch(str);
    QList<int> vals;
    while (it.hasNext()) {
        vals.append(it.next().captured().toInt());
    }
    if (vals.size() >= 2) {
        return QSize(vals[0], vals[1]);
    }
    return QSize();
}

QString PlistExtractor::formatRect(const QRect &r)
{
    return QStringLiteral("{{%1,%2},{%3,%4}}").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

QString PlistExtractor::formatPoint(const QPoint &p)
{
    return QStringLiteral("{%1,%2}").arg(p.x()).arg(p.y());
}

QString PlistExtractor::formatSize(const QSize &s)
{
    return QStringLiteral("{%1,%2}").arg(s.width()).arg(s.height());
}

static QVariant parsePlistElement(QXmlStreamReader &xml)
{
    QString name = xml.name().toString().toLower();
    if (name == QStringLiteral("string")) {
        return xml.readElementText();
    } else if (name == QStringLiteral("integer")) {
        return xml.readElementText().toInt();
    } else if (name == QStringLiteral("real")) {
        return xml.readElementText().toDouble();
    } else if (name == QStringLiteral("true")) {
        xml.readNext();
        return true;
    } else if (name == QStringLiteral("false")) {
        xml.readNext();
        return false;
    } else if (name == QStringLiteral("dict")) {
        QVariantMap map;
        QString currentKey;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isEndElement() && xml.name() == QStringLiteral("dict")) {
                break;
            }
            if (xml.isStartElement()) {
                if (xml.name() == QStringLiteral("key")) {
                    currentKey = xml.readElementText();
                } else if (!currentKey.isEmpty()) {
                    map.insert(currentKey, parsePlistElement(xml));
                    currentKey.clear();
                } else {
                    xml.skipCurrentElement();
                }
            }
        }
        return map;
    } else if (name == QStringLiteral("array")) {
        QVariantList list;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isEndElement() && xml.name() == QStringLiteral("array")) {
                break;
            }
            if (xml.isStartElement()) {
                list.append(parsePlistElement(xml));
            }
        }
        return list;
    } else {
        xml.skipCurrentElement();
        return QVariant();
    }
}

static QString extractAnimationName(const QString &frameName)
{
    QRegularExpression rx(QStringLiteral("^(.*?)[_\\-\\s](\\d+)(\\.[a-zA-Z0-9]+)?$"));
    auto match = rx.match(frameName);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString();
}

bool PlistExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Cannot open plist file: %1").arg(file.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QXmlStreamReader xml(&file);
    QVariantMap rootMap;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("dict")) {
            rootMap = parsePlistElement(xml).toMap();
            break;
        }
    }

    if (xml.hasError()) {
        if (error) {
            error->code = ExtractorError::ParsingFailed;
            error->message = tr("XML parse error in plist: %1").arg(xml.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QVariantMap framesMap = rootMap.value(QStringLiteral("frames")).toMap();
    QVariantMap metaMap = rootMap.value(QStringLiteral("metadata")).toMap();

    if (framesMap.isEmpty()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("No 'frames' dictionary found in plist.");
            error->filePath = filePath;
        }
        return false;
    }

    // Determine companion atlas texture file
    QFileInfo plistFi(filePath);
    QString imageFileName = metaMap.value(QStringLiteral("realTextureFileName")).toString();
    if (imageFileName.isEmpty()) {
        imageFileName = metaMap.value(QStringLiteral("textureFileName")).toString();
    }

    QString imageFilePath;
    if (!imageFileName.isEmpty()) {
        imageFilePath = plistFi.dir().filePath(imageFileName);
    }

    if (imageFilePath.isEmpty() || !QFile::exists(imageFilePath)) {
        QString candPng = plistFi.dir().filePath(plistFi.completeBaseName() + QStringLiteral(".png"));
        QString candKtx2 = plistFi.dir().filePath(plistFi.completeBaseName() + QStringLiteral(".ktx2"));
        QString candWebp = plistFi.dir().filePath(plistFi.completeBaseName() + QStringLiteral(".webp"));
        if (QFile::exists(candPng)) {
            imageFilePath = candPng;
        } else if (QFile::exists(candKtx2)) {
            imageFilePath = candKtx2;
        } else if (QFile::exists(candWebp)) {
            imageFilePath = candWebp;
        }
    }

    if (!QFile::exists(imageFilePath)) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Associated atlas image file not found for %1").arg(filePath);
            error->filePath = imageFilePath;
        }
        return false;
    }

    QString loadErr;
    QImage atlasImage = VramTextureCompressor::loadAtlasImage(imageFilePath, &loadErr);
    if (atlasImage.isNull()) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Failed to load atlas image: %1 (%2)").arg(imageFilePath, loadErr);
            error->filePath = imageFilePath;
        }
        return false;
    }

    atlasImage = atlasImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    QList<QImage> frames;
    QList<SpriteBox> boxes;
    QMap<QString, QList<int>> animGroups;

    int frameIndex = 0;
    for (auto it = framesMap.constBegin(); it != framesMap.constEnd(); ++it) {
        QString frameName = it.key();
        QVariantMap f = it.value().toMap();

        // Support format 3 (textureRect, textureRotated) and format 2 (frame, rotated)
        QRect rect;
        if (f.contains(QStringLiteral("textureRect"))) {
            rect = parseRect(f.value(QStringLiteral("textureRect")).toString());
        } else if (f.contains(QStringLiteral("frame"))) {
            rect = parseRect(f.value(QStringLiteral("frame")).toString());
        }

        bool rotated = false;
        if (f.contains(QStringLiteral("textureRotated"))) {
            rotated = f.value(QStringLiteral("textureRotated")).toBool();
        } else if (f.contains(QStringLiteral("rotated"))) {
            rotated = f.value(QStringLiteral("rotated")).toBool();
        }

        QSize sourceSize;
        if (f.contains(QStringLiteral("spriteSourceSize"))) {
            sourceSize = parseSize(f.value(QStringLiteral("spriteSourceSize")).toString());
        } else if (f.contains(QStringLiteral("sourceSize"))) {
            sourceSize = parseSize(f.value(QStringLiteral("sourceSize")).toString());
        }

        QPoint offset;
        if (f.contains(QStringLiteral("spriteOffset"))) {
            offset = parsePoint(f.value(QStringLiteral("spriteOffset")).toString());
        } else if (f.contains(QStringLiteral("offset"))) {
            offset = parsePoint(f.value(QStringLiteral("offset")).toString());
        }

        if (rect.isValid() && atlasImage.rect().contains(rect)) {
            QImage cropped = atlasImage.copy(rect);
            if (rotated) {
                // In Cocos2d-x, rotated sprites are rotated clockwise in the atlas.
                // Rotate counter-clockwise 90° to restore original orientation.
                QTransform t;
                t.rotate(-90.0);
                cropped = cropped.transformed(t);
            }

            SpriteBox box;
            box.rect = rect;
            box.index = frameIndex;
            if (offset != QPoint(0, 0)) {
                box.pivot = QPoint(rect.width() / 2 - offset.x(), rect.height() / 2 - offset.y());
                box.hasCustomPivot = true;
            }

            frames.append(cropped);
            boxes.append(box);

            QString anim = extractAnimationName(frameName);
            if (!anim.isEmpty()) {
                animGroups[anim].append(frameIndex);
            }

            frameIndex++;
        }
    }

    outDoc.setAtlas(atlasImage);
    outDoc.setFrames(frames, boxes);

    for (auto it = animGroups.constBegin(); it != animGroups.constEnd(); ++it) {
        outDoc.setAnimation(it.key(), it.value(), 12, true);
    }

    return true;
}

bool PlistExtractor::write(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, ExtractorError *error)
{
    if (doc.frameCount() == 0) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Document has no frames to export.");
            error->filePath = filePath;
        }
        return false;
    }

    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    dir.mkpath(QStringLiteral("."));

    QString baseName = fi.completeBaseName();
    QString pngFileName = baseName + QStringLiteral(".png");
    QString pngFilePath = dir.filePath(pngFileName);

    // Pack or reuse existing atlas
    QImage atlasToSave = doc.atlas();
    QList<QRect> rectsToSave;

    if (atlasToSave.isNull() || doc.boxes().isEmpty()) {
        AtlasPacker::PackOptions packOpts = options.packOptions;
        if (options.padding > 0) packOpts.padding = options.padding;
        auto packResult = AtlasPacker::pack(doc.frames(), packOpts);
        atlasToSave = packResult.atlas;
        rectsToSave = packResult.frameRects;
    } else {
        for (const SpriteBox &b : doc.boxes()) {
            rectsToSave.append(b.rect);
        }
    }

    if (!atlasToSave.save(pngFilePath, "PNG")) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Failed to save companion atlas image: %1").arg(pngFilePath);
            error->filePath = pngFilePath;
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot write plist file: %1").arg(file.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument(QStringLiteral("1.0"));
    xml.writeDTD(QStringLiteral("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"));
    xml.writeStartElement(QStringLiteral("plist"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("dict"));

    // 1. frames dict
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("frames"));
    xml.writeStartElement(QStringLiteral("dict"));

    for (int i = 0; i < rectsToSave.size(); ++i) {
        QRect r = rectsToSave[i];
        QString frameName = QStringLiteral("frame_%1.png").arg(i, 3, 10, QChar('0'));

        xml.writeTextElement(QStringLiteral("key"), frameName);
        xml.writeStartElement(QStringLiteral("dict"));

        // aliases
        xml.writeTextElement(QStringLiteral("key"), QStringLiteral("aliases"));
        xml.writeStartElement(QStringLiteral("array"));
        xml.writeEndElement(); // array

        // spriteOffset
        xml.writeTextElement(QStringLiteral("key"), QStringLiteral("spriteOffset"));
        xml.writeTextElement(QStringLiteral("string"), formatPoint(QPoint(0, 0)));

        // spriteSize
        xml.writeTextElement(QStringLiteral("key"), QStringLiteral("spriteSize"));
        xml.writeTextElement(QStringLiteral("string"), formatSize(r.size()));

        // spriteSourceSize
        xml.writeTextElement(QStringLiteral("key"), QStringLiteral("spriteSourceSize"));
        xml.writeTextElement(QStringLiteral("string"), formatSize(r.size()));

        // textureRect
        xml.writeTextElement(QStringLiteral("key"), QStringLiteral("textureRect"));
        xml.writeTextElement(QStringLiteral("string"), formatRect(r));

        // textureRotated
        xml.writeTextElement(QStringLiteral("key"), QStringLiteral("textureRotated"));
        xml.writeEmptyElement(QStringLiteral("false"));

        xml.writeEndElement(); // frame dict
    }

    xml.writeEndElement(); // frames dict

    // 2. metadata dict
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("metadata"));
    xml.writeStartElement(QStringLiteral("dict"));

    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("format"));
    xml.writeTextElement(QStringLiteral("integer"), QStringLiteral("3"));

    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("pixelFormat"));
    xml.writeTextElement(QStringLiteral("string"), QStringLiteral("RGBA8888"));

    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("premultiplyAlpha"));
    xml.writeEmptyElement(QStringLiteral("false"));

    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("realTextureFileName"));
    xml.writeTextElement(QStringLiteral("string"), pngFileName);

    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("size"));
    xml.writeTextElement(QStringLiteral("string"), formatSize(atlasToSave.size()));

    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("smartupdate"));
    xml.writeTextElement(QStringLiteral("string"), QStringLiteral("$TexturePacker:SmartUpdate:BentoPack$"));

    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("textureFileName"));
    xml.writeTextElement(QStringLiteral("string"), pngFileName);

    xml.writeEndElement(); // metadata dict

    xml.writeEndElement(); // root dict
    xml.writeEndElement(); // plist
    xml.writeEndDocument();

    file.close();
    return true;
}
