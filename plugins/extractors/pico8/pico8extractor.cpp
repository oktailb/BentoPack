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

#include "pico8extractor.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <limits>

Pico8Extractor::Pico8Extractor(QObject *parent)
    : Extractor(parent)
{
}

const QVector<QRgb>& Pico8Extractor::pico8Palette()
{
    static const QVector<QRgb> pal = {
        qRgba(0, 0, 0, 0),        // 0: Black (transparent)
        qRgb(0x1D, 0x2B, 0x53),   // 1: Dark Blue
        qRgb(0x7E, 0x25, 0x53),   // 2: Dark Purple
        qRgb(0x00, 0x87, 0x51),   // 3: Dark Green
        qRgb(0xAB, 0x52, 0x36),   // 4: Brown
        qRgb(0x5F, 0x57, 0x4F),   // 5: Dark Gray
        qRgb(0xC2, 0xC3, 0xC7),   // 6: Light Gray
        qRgb(0xFF, 0xF1, 0xE8),   // 7: White
        qRgb(0xFF, 0x00, 0x4D),   // 8: Red
        qRgb(0xFF, 0xA3, 0x00),   // 9: Orange
        qRgb(0xFF, 0xEC, 0x27),   // 10: Yellow
        qRgb(0x00, 0xE4, 0x36),   // 11: Green
        qRgb(0x29, 0xAD, 0xFF),   // 12: Light Blue
        qRgb(0x83, 0x76, 0x9C),   // 13: Lavender
        qRgb(0xFF, 0x77, 0xA8),   // 14: Pink
        qRgb(0xFF, 0xCC, 0xAA)    // 15: Peach
    };
    return pal;
}

int Pico8Extractor::findNearestPico8Color(QRgb rgb)
{
    if (qAlpha(rgb) < 128) {
        return 0; // Transparent
    }

    static const QVector<QRgb> opaquePal = {
        qRgb(0x00, 0x00, 0x00),   // 0
        qRgb(0x1D, 0x2B, 0x53),   // 1
        qRgb(0x7E, 0x25, 0x53),   // 2
        qRgb(0x00, 0x87, 0x51),   // 3
        qRgb(0xAB, 0x52, 0x36),   // 4
        qRgb(0x5F, 0x57, 0x4F),   // 5
        qRgb(0xC2, 0xC3, 0xC7),   // 6
        qRgb(0xFF, 0xF1, 0xE8),   // 7
        qRgb(0xFF, 0x00, 0x4D),   // 8
        qRgb(0xFF, 0xA3, 0x00),   // 9
        qRgb(0xFF, 0xEC, 0x27),   // 10
        qRgb(0x00, 0xE4, 0x36),   // 11
        qRgb(0x29, 0xAD, 0xFF),   // 12
        qRgb(0x83, 0x76, 0x9C),   // 13
        qRgb(0xFF, 0x77, 0xA8),   // 14
        qRgb(0xFF, 0xCC, 0xAA)    // 15
    };

    int r = qRed(rgb);
    int g = qGreen(rgb);
    int b = qBlue(rgb);

    int bestIdx = 0;
    int bestDist = std::numeric_limits<int>::max();

    for (int i = 0; i < 16; ++i) {
        int dr = r - qRed(opaquePal[i]);
        int dg = g - qGreen(opaquePal[i]);
        int db = b - qBlue(opaquePal[i]);
        int dist = dr * dr + dg * dg + db * db;
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }

    return bestIdx;
}

bool Pico8Extractor::parseGfx(const QStringList &gfxLines, QImage &outImage)
{
    outImage = QImage(128, 128, QImage::Format_ARGB32);
    outImage.fill(Qt::transparent);

    const auto &pal = pico8Palette();

    for (int y = 0; y < 128; ++y) {
        QString line = (y < gfxLines.size()) ? gfxLines[y].trimmed() : QString();
        QRgb *scanLine = reinterpret_cast<QRgb*>(outImage.scanLine(y));
        for (int x = 0; x < 128; ++x) {
            int colIdx = 0;
            if (x < line.length()) {
                QChar ch = line[x];
                colIdx = ch.digitValue();
                if (colIdx < 0 || colIdx > 15) {
                    if (ch >= QChar('a') && ch <= QChar('f')) {
                        colIdx = 10 + (ch.unicode() - 'a');
                    } else if (ch >= QChar('A') && ch <= QChar('F')) {
                        colIdx = 10 + (ch.unicode() - 'A');
                    } else {
                        colIdx = 0;
                    }
                }
            }
            scanLine[x] = pal[colIdx];
        }
    }
    return true;
}

QStringList Pico8Extractor::formatGfx(const QImage &image)
{
    QStringList lines;
    lines.reserve(128);

    QImage scaled = image;
    if (scaled.size() != QSize(128, 128)) {
        scaled = scaled.scaled(128, 128, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }

    static const char hexChars[] = "0123456789abcdef";

    for (int y = 0; y < 128; ++y) {
        QString line;
        line.reserve(128);
        for (int x = 0; x < 128; ++x) {
            QRgb px = scaled.pixel(x, y);
            int idx = findNearestPico8Color(px);
            line.append(QLatin1Char(hexChars[idx & 0x0F]));
        }
        lines.append(line);
    }

    return lines;
}

QVector<quint8> Pico8Extractor::parseGff(const QStringList &gffLines)
{
    QVector<quint8> flags(256, 0);
    QString combined;
    for (const QString &line : gffLines) {
        combined.append(line.trimmed());
    }

    for (int i = 0; i < 256; ++i) {
        if (i * 2 + 1 < combined.length()) {
            bool ok = false;
            quint8 val = static_cast<quint8>(combined.mid(i * 2, 2).toUInt(&ok, 16));
            if (ok) flags[i] = val;
        }
    }
    return flags;
}

QStringList Pico8Extractor::formatGff(const QVector<quint8> &flags)
{
    QStringList lines;
    lines.reserve(32);
    for (int line = 0; line < 32; ++line) {
        QString lineStr;
        lineStr.reserve(64);
        for (int s = 0; s < 32; ++s) {
            int spriteIdx = line * 32 + s;
            quint8 flag = (spriteIdx < flags.size()) ? flags[spriteIdx] : 0;
            lineStr.append(QStringLiteral("%1").arg(flag, 2, 16, QLatin1Char('0')));
        }
        lines.append(lineStr);
    }
    return lines;
}

bool Pico8Extractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    if (!supportedExtensions().contains(fi.suffix().toLower())) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QString firstLine = QString::fromUtf8(file.readLine(256)).trimmed();
    return firstLine.startsWith(QStringLiteral("pico-8 cartridge"), Qt::CaseInsensitive);
}

bool Pico8Extractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Cannot open PICO-8 file: %1").arg(file.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QStringList gfxLines;
    QStringList gffLines;
    enum Section { None, Gfx, Gff, Other };
    Section currentSection = None;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QString trimmed = line.trimmed();
        if (trimmed == QStringLiteral("__gfx__")) {
            currentSection = Gfx;
            continue;
        } else if (trimmed == QStringLiteral("__gff__")) {
            currentSection = Gff;
            continue;
        } else if (trimmed.startsWith(QStringLiteral("__")) && trimmed.endsWith(QStringLiteral("__"))) {
            currentSection = Other;
            continue;
        }

        if (currentSection == Gfx) {
            gfxLines.append(trimmed);
        } else if (currentSection == Gff) {
            gffLines.append(trimmed);
        }
    }
    file.close();

    QImage atlas;
    parseGfx(gfxLines, atlas);
    QVector<quint8> flags = parseGff(gffLines);

    // Extract 8x8 sprite frames (PICO-8 native grid: 16x16 = 256 sprites)
    QList<QImage> frameImages;
    QList<SpriteBox> boxes;

    for (int idx = 0; idx < 256; ++idx) {
        int sx = (idx % 16) * 8;
        int sy = (idx / 16) * 8;
        QRect r(sx, sy, 8, 8);
        QImage spriteImg = atlas.copy(r);

        bool isEmpty = true;
        for (int y = 0; y < 8; ++y) {
            const QRgb *scan = reinterpret_cast<const QRgb*>(spriteImg.constScanLine(y));
            for (int x = 0; x < 8; ++x) {
                if (qAlpha(scan[x]) > 0) {
                    isEmpty = false;
                    break;
                }
            }
            if (!isEmpty) break;
        }

        if (!isEmpty || (idx == 0 && frameImages.isEmpty())) {
            SpriteBox box(r);
            box.index = idx;
            boxes.append(box);
            frameImages.append(spriteImg);
        }
    }

    if (frameImages.isEmpty()) {
        SpriteBox singleBox(QRect(0, 0, 128, 128));
        boxes.append(singleBox);
        frameImages.append(atlas);
    }

    outDoc.setAtlas(atlas);
    outDoc.setFrames(frameImages, boxes);
    outDoc.setFilePath(filePath);
    return true;
}

bool Pico8Extractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);

    if (inDoc.atlas().isNull()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Cannot export PICO-8: Document atlas image is null.");
            error->filePath = filePath;
        }
        return false;
    }

    QMap<QString, QStringList> sections;
    QString currentSection;

    if (QFile::exists(filePath)) {
        QFile existingFile(filePath);
        if (existingFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&existingFile);
            while (!in.atEnd()) {
                QString line = in.readLine();
                QString trimmed = line.trimmed();
                if (trimmed.startsWith(QStringLiteral("__")) && trimmed.endsWith(QStringLiteral("__"))) {
                    currentSection = trimmed;
                    sections[currentSection] = QStringList();
                } else if (!currentSection.isEmpty()) {
                    sections[currentSection].append(line);
                }
            }
        }
    }

    // Format new __gfx__ from atlas
    QStringList gfxLines = formatGfx(inDoc.atlas());
    sections[QStringLiteral("__gfx__")] = gfxLines;

    // Format __gff__ if not present
    if (!sections.contains(QStringLiteral("__gff__"))) {
        QVector<quint8> flags(256, 0);
        sections[QStringLiteral("__gff__")] = formatGff(flags);
    }

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot open file for writing: %1").arg(outFile.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QTextStream out(&outFile);
    out << "pico-8 cartridge // http://www.pico-8.com\n";
    out << "version 41\n";

    const QStringList sectionOrder = {
        QStringLiteral("__lua__"),
        QStringLiteral("__gfx__"),
        QStringLiteral("__gff__"),
        QStringLiteral("__map__"),
        QStringLiteral("__sfx__"),
        QStringLiteral("__music__")
    };

    for (const QString &sec : sectionOrder) {
        if (sections.contains(sec)) {
            out << sec << "\n";
            for (const QString &line : sections[sec]) {
                out << line << "\n";
            }
        } else if (sec == QStringLiteral("__lua__")) {
            out << "__lua__\n";
        }
    }

    return true;
}
