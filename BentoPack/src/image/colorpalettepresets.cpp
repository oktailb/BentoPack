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

#include "image/colorpalettepresets.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QSet>
#include <QMap>
#include <QImage>
#include <QStandardPaths>
#include <QCoreApplication>

static void initPaletteResources()
{
    static bool inited = false;
    if (!inited) {
        Q_INIT_RESOURCE(palettes);
        inited = true;
    }
}

static QList<ColorPalettePresets::PaletteInfo> s_availablePalettes;

QVector<QRgb> ColorPalettePresets::standardPalette()
{
    return {
        // Bento Studio Standard 36-color palette (emergency fallback)
        qRgb(0, 0, 0),       qRgb(33, 33, 33),    qRgb(66, 66, 66),
        qRgb(117, 117, 117), qRgb(189, 189, 189), qRgb(255, 255, 255),
        qRgb(62, 39, 35),    qRgb(93, 64, 55),    qRgb(141, 110, 99),
        qRgb(215, 204, 200), qRgb(255, 235, 238), qRgb(255, 205, 210),
        qRgb(183, 28, 28),   qRgb(229, 57, 53),   qRgb(239, 108, 0),
        qRgb(255, 167, 38),  qRgb(253, 216, 53),  qRgb(255, 245, 157),
        qRgb(27, 94, 32),    qRgb(56, 142, 60),   qRgb(76, 175, 80),
        qRgb(139, 195, 74),  qRgb(205, 220, 57),  qRgb(178, 255, 89),
        qRgb(0, 77, 64),     qRgb(0, 137, 123),   qRgb(0, 188, 212),
        qRgb(3, 169, 244),   qRgb(21, 101, 192),  qRgb(63, 81, 181),
        qRgb(74, 20, 140),   qRgb(123, 31, 162),  qRgb(171, 71, 188),
        qRgb(173, 20, 87),   qRgb(233, 30, 99),   qRgb(244, 143, 177)
    };
}

QString ColorPalettePresets::userPalettesDirectory()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/palettes");
    QDir().mkpath(dir);
    return dir;
}

QStringList ColorPalettePresets::paletteSearchPaths()
{
    QStringList paths;
    paths << QStringLiteral(":/palettes");
    paths << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("palettes"));
    paths << userPalettesDirectory();
    return paths;
}

void ColorPalettePresets::scanPalettes()
{
    initPaletteResources();
    s_availablePalettes.clear();

    QSet<QString> seenIds;
    QStringList nameFilters;
    nameFilters << QStringLiteral("*.gpl") << QStringLiteral("*.hex") << QStringLiteral("*.pal");

    // Scan search paths in order: 1. embedded :/palettes, 2. app dir, 3. user config dir
    for (const QString &dirPath : paletteSearchPaths()) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;

        QFileInfoList files = dir.entryInfoList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo &fi : files) {
            QString id = fi.completeBaseName().toLower();
            QString name;
            QString err;
            QVector<QRgb> colors = loadPaletteFromFile(fi.absoluteFilePath(), &err, &name);
            if (colors.isEmpty()) continue;

            if (seenIds.contains(id)) {
                // User or local app directory override of an existing palette
                for (auto &existing : s_availablePalettes) {
                    if (existing.id.toLower() == id) {
                        existing.colors = colors;
                        existing.filePath = fi.absoluteFilePath();
                        if (!name.isEmpty()) existing.name = name;
                        break;
                    }
                }
                continue;
            }

            PaletteInfo info;
            info.id = id;
            info.name = name.isEmpty() ? fi.completeBaseName() : name;
            info.filePath = fi.absoluteFilePath();
            info.colors = colors;
            info.isBuiltIn = fi.absoluteFilePath().startsWith(QStringLiteral(":/"));
            s_availablePalettes.append(info);
            seenIds.insert(id);
        }
    }

    // Ensure bento_standard is first if present
    for (int i = 0; i < s_availablePalettes.size(); ++i) {
        if (s_availablePalettes.at(i).id == QStringLiteral("bento_standard")) {
            if (i > 0) s_availablePalettes.move(i, 0);
            break;
        }
    }

    // Ultimate emergency fallback if resource system or directory scan had an issue
    if (s_availablePalettes.isEmpty()) {
        PaletteInfo standardInfo;
        standardInfo.id = QStringLiteral("bento_standard");
        standardInfo.name = QStringLiteral("Bento Standard (36)");
        standardInfo.isBuiltIn = true;
        standardInfo.colors = standardPalette();
        s_availablePalettes.append(standardInfo);
    }
}

QList<ColorPalettePresets::PaletteInfo> ColorPalettePresets::availablePalettes()
{
    if (s_availablePalettes.isEmpty()) {
        scanPalettes();
    }
    return s_availablePalettes;
}

QVector<QRgb> ColorPalettePresets::getPaletteById(const QString &id)
{
    QString lowerId = id.toLower();
    for (const auto &p : availablePalettes()) {
        if (p.id.toLower() == lowerId) {
            return p.colors;
        }
    }

    if (lowerId == QLatin1String("bento_standard") || lowerId == QLatin1String("standard")) {
        return standardPalette();
    }
    return {};
}

QVector<QRgb> ColorPalettePresets::getPaletteByIndex(int index)
{
    const auto list = availablePalettes();
    if (index >= 0 && index < list.size()) {
        return list.at(index).colors;
    }
    return standardPalette();
}

QVector<QRgb> ColorPalettePresets::getPresetPalette(Preset preset)
{
    Q_UNUSED(preset);
    return standardPalette();
}



QVector<QRgb> ColorPalettePresets::loadPaletteFromFile(const QString &filePath, QString *outError, QString *outName)
{
    QVector<QRgb> result;
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    // 1. Image formats (.png, .bmp, .jpg)
    if (ext == QLatin1String("png") || ext == QLatin1String("bmp") || ext == QLatin1String("jpg")) {
        QImage img(filePath);
        if (img.isNull()) {
            if (outError) *outError = QCoreApplication::translate("ColorPalettePresets", "Failed to decode image.");
            return result;
        }
        if (outName) {
            *outName = fi.completeBaseName();
        }
        QSet<QRgb> uniqueColors;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                QRgb p = img.pixel(x, y);
                if (qAlpha(p) > 128) {
                    uniqueColors.insert(qRgb(qRed(p), qGreen(p), qBlue(p)));
                }
            }
        }
        result = uniqueColors.values().toVector();
        return result;
    }

    // 2. Text-based formats (.hex, .gpl, .pal)
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (outError) *outError = QCoreApplication::translate("ColorPalettePresets", "Cannot open file for reading.");
        return result;
    }

    QTextStream in(&file);
    static const QRegularExpression hexRegex(QStringLiteral("^[#]?[0-9a-fA-F]{6}$"));
    static const QRegularExpression rgbRegex(QStringLiteral("^\\s*(\\d{1,3})\\s+(\\d{1,3})\\s+(\\d{1,3})"));

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1String("GIMP"), Qt::CaseInsensitive) || line.startsWith(QLatin1String("JASC"), Qt::CaseInsensitive)) {
            continue;
        }

        // Header: "Name: <Palette Name>"
        if (line.startsWith(QLatin1String("Name:"), Qt::CaseInsensitive)) {
            if (outName && outName->isEmpty()) {
                *outName = line.mid(5).trimmed();
            }
            continue;
        }
        if (line.startsWith(QLatin1String("Columns:"), Qt::CaseInsensitive)) {
            continue;
        }

        // Check for .hex format (e.g. #FF00AA or FF00AA)
        if (hexRegex.match(line).hasMatch()) {
            QString h = line.startsWith(QLatin1Char('#')) ? line.mid(1) : line;
            bool ok = false;
            uint val = h.toUInt(&ok, 16);
            if (ok) {
                result.append(qRgb((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF));
            }
            continue;
        }

        // Comments
        if (line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';'))) {
            if (outName && outName->isEmpty()) {
                QString c = line.mid(1).trimmed();
                if (c.startsWith(QLatin1String("Name:"), Qt::CaseInsensitive)) {
                    *outName = c.mid(5).trimmed();
                }
            }
            continue;
        }

        // Check for RGB triplet (e.g. "255 128 0" as in .gpl or .pal)
        auto match = rgbRegex.match(line);
        if (match.hasMatch()) {
            int r = match.captured(1).toInt();
            int g = match.captured(2).toInt();
            int b = match.captured(3).toInt();
            if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                result.append(qRgb(r, g, b));
            }
        }
    }

    if (outName && outName->isEmpty()) {
        *outName = fi.completeBaseName();
    }

    if (result.isEmpty() && outError) {
        *outError = QCoreApplication::translate("ColorPalettePresets", "No valid colors found in palette file.");
    }
    return result;
}
