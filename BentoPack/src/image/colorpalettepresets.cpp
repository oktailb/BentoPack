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
#include <QTextStream>
#include <QRegularExpression>
#include <QSet>
#include <QImage>
#include <QCoreApplication>

QVector<QRgb> ColorPalettePresets::getPresetPalette(Preset preset)
{
    QVector<QRgb> pal;
    switch (preset) {
    case Standard:
        pal = {
            // Row 1: Grayscale & Neutrals
            qRgb(0, 0, 0),       qRgb(33, 33, 33),    qRgb(66, 66, 66),
            qRgb(117, 117, 117), qRgb(189, 189, 189), qRgb(255, 255, 255),
            // Row 2: Earth & Browns
            qRgb(62, 39, 35),    qRgb(93, 64, 55),    qRgb(141, 110, 99),
            qRgb(215, 204, 200), qRgb(255, 235, 238), qRgb(255, 205, 210),
            // Row 3: Reds & Oranges
            qRgb(183, 28, 28),   qRgb(229, 57, 53),   qRgb(239, 108, 0),
            qRgb(255, 167, 38),  qRgb(253, 216, 53),  qRgb(255, 245, 157),
            // Row 4: Greens
            qRgb(27, 94, 32),    qRgb(56, 142, 60),   qRgb(76, 175, 80),
            qRgb(139, 195, 74),  qRgb(205, 220, 57),  qRgb(178, 255, 89),
            // Row 5: Cyans & Blues
            qRgb(0, 77, 64),     qRgb(0, 137, 123),   qRgb(0, 188, 212),
            qRgb(3, 169, 244),   qRgb(21, 101, 192),  qRgb(63, 81, 181),
            // Row 6: Purples, Magentas & Pinks
            qRgb(74, 20, 140),   qRgb(123, 31, 162),  qRgb(171, 71, 188),
            qRgb(173, 20, 87),   qRgb(233, 30, 99),   qRgb(244, 143, 177)
        };
        break;

    case GameBoyDMG:
        pal = {
            qRgb(15, 56, 15),     // #0f380f Darkest green
            qRgb(48, 98, 48),     // #306230 Dark green
            qRgb(139, 172, 15),   // #8bac0f Light green
            qRgb(155, 188, 15)    // #9bbc0f Lightest green
        };
        break;

    case GameBoyPocket:
        pal = {
            qRgb(0, 0, 0),        // #000000 Black
            qRgb(85, 85, 85),     // #555555 Dark gray
            qRgb(170, 170, 170),  // #aaaaaa Light gray
            qRgb(255, 255, 255)   // #ffffff White
        };
        break;

    case NES:
        pal = {
            qRgb(124,124,124), qRgb(0,0,252),     qRgb(0,0,188),     qRgb(68,40,188),
            qRgb(148,0,132),   qRgb(168,0,32),     qRgb(168,16,0),    qRgb(136,20,0),
            qRgb(80,48,0),     qRgb(0,120,0),      qRgb(0,104,0),     qRgb(0,88,0),
            qRgb(0,64,88),     qRgb(0,0,0),        qRgb(188,188,188), qRgb(0,120,248),
            qRgb(0,88,248),    qRgb(104,68,252),   qRgb(216,0,204),   qRgb(228,0,88),
            qRgb(248,56,0),    qRgb(228,92,16),    qRgb(172,124,0),   qRgb(0,184,0),
            qRgb(0,168,0),     qRgb(0,168,68),     qRgb(0,136,136),   qRgb(248,248,248),
            qRgb(60,188,252),  qRgb(104,136,252),  qRgb(152,120,248), qRgb(248,120,248),
            qRgb(248,88,152),  qRgb(248,120,88),   qRgb(252,160,68),  qRgb(248,184,0),
            qRgb(184,248,24),  qRgb(88,216,84),    qRgb(88,248,152),  qRgb(0,232,216),
            qRgb(120,120,120), qRgb(252,252,252), qRgb(164,228,252), qRgb(184,184,248),
            qRgb(216,184,248), qRgb(248,184,248), qRgb(248,164,192), qRgb(240,208,176),
            qRgb(252,224,168), qRgb(248,216,120), qRgb(216,248,120), qRgb(184,248,184),
            qRgb(184,248,216), qRgb(0,252,252)
        };
        break;

    case SNES:
        pal = {
            qRgb(0, 0, 0),       qRgb(248, 248, 248), qRgb(184, 184, 184), qRgb(104, 104, 104),
            qRgb(248, 56, 0),    qRgb(216, 0, 0),     qRgb(152, 0, 0),     qRgb(248, 120, 88),
            qRgb(248, 160, 0),   qRgb(248, 224, 0),   qRgb(184, 152, 0),   qRgb(104, 72, 0),
            qRgb(0, 216, 0),     qRgb(0, 144, 0),     qRgb(0, 80, 0),      qRgb(120, 248, 88),
            qRgb(0, 184, 216),   qRgb(0, 104, 184),   qRgb(0, 48, 120),    qRgb(120, 216, 248),
            qRgb(88, 88, 248),   qRgb(40, 40, 184),   qRgb(16, 16, 104),   qRgb(160, 160, 248),
            qRgb(216, 0, 184),   qRgb(144, 0, 120),   qRgb(248, 120, 216), qRgb(248, 184, 152),
            qRgb(216, 136, 88),  qRgb(160, 88, 48),   qRgb(96, 48, 16),    qRgb(48, 48, 48)
        };
        break;

    case Pico8:
        pal = {
            qRgb(0, 0, 0),       qRgb(29, 43, 83),    qRgb(126, 37, 83),  qRgb(0, 135, 81),
            qRgb(171, 82, 54),   qRgb(95, 87, 79),    qRgb(194, 195, 199),qRgb(255, 241, 232),
            qRgb(255, 0, 77),    qRgb(255, 163, 0),   qRgb(255, 236, 39), qRgb(0, 228, 54),
            qRgb(41, 173, 255),  qRgb(131, 118, 156), qRgb(255, 119, 168),qRgb(255, 204, 170)
        };
        break;

    case Commodore64:
        pal = {
            qRgb(0, 0, 0),       qRgb(255, 255, 255), qRgb(136, 0, 0),    qRgb(170, 255, 238),
            qRgb(204, 68, 204),  qRgb(0, 204, 85),    qRgb(0, 0, 170),    qRgb(238, 238, 119),
            qRgb(221, 136, 85),  qRgb(102, 68, 0),    qRgb(255, 119, 119),qRgb(51, 51, 51),
            qRgb(119, 119, 119), qRgb(170, 255, 102), qRgb(0, 136, 255),  qRgb(187, 187, 187)
        };
        break;

    case Amiga:
        pal = {
            qRgb(0, 85, 170),   qRgb(255, 255, 255), qRgb(0, 0, 0),       qRgb(255, 136, 0),
            qRgb(0, 0, 170),    qRgb(0, 170, 0),     qRgb(0, 170, 170),   qRgb(170, 0, 0),
            qRgb(170, 0, 170),  qRgb(170, 85, 0),    qRgb(170, 170, 170), qRgb(85, 85, 85),
            qRgb(85, 85, 255),  qRgb(85, 255, 85),   qRgb(85, 255, 255),  qRgb(255, 85, 85),
            qRgb(255, 85, 255), qRgb(255, 255, 85), qRgb(238, 68, 68),  qRgb(68, 170, 238),
            qRgb(34, 102, 34),  qRgb(204, 170, 119), qRgb(136, 102, 68), qRgb(68, 51, 34),
            qRgb(221, 221, 221),qRgb(187, 187, 187),qRgb(153, 153, 153),qRgb(102, 102, 102),
            qRgb(51, 51, 51),   qRgb(255, 204, 153), qRgb(204, 119, 85), qRgb(119, 34, 34)
        };
        break;

    case PCEngine:
        pal = {
            qRgb(0, 0, 0),       qRgb(255, 255, 255), qRgb(182, 182, 182), qRgb(109, 109, 109),
            qRgb(255, 36, 36),   qRgb(218, 0, 0),     qRgb(145, 0, 0),     qRgb(255, 145, 145),
            qRgb(255, 109, 0),   qRgb(255, 182, 0),   qRgb(255, 255, 0),   qRgb(182, 145, 0),
            qRgb(36, 218, 36),   qRgb(0, 182, 0),     qRgb(0, 109, 0),     qRgb(145, 255, 145),
            qRgb(36, 218, 255),  qRgb(0, 145, 218),   qRgb(0, 72, 182),    qRgb(145, 218, 255),
            qRgb(72, 72, 255),   qRgb(36, 36, 182),   qRgb(0, 0, 145),     qRgb(182, 182, 255),
            qRgb(218, 36, 218),  qRgb(145, 0, 145),   qRgb(255, 145, 255), qRgb(255, 182, 145),
            qRgb(218, 145, 72),  qRgb(145, 72, 0),    qRgb(109, 36, 0),    qRgb(36, 36, 36)
        };
        break;

    case CGAMode1:
        pal = {
            qRgb(0, 0, 0),       qRgb(85, 255, 255),  qRgb(255, 85, 255), qRgb(255, 255, 255)
        };
        break;

    case CGAMode2:
        pal = {
            qRgb(0, 0, 0),       qRgb(85, 255, 85),   qRgb(255, 85, 85),  qRgb(255, 255, 85)
        };
        break;

    case Endesga32:
        pal = {
            qRgb(190, 74, 47),   qRgb(215, 118, 67),  qRgb(234, 212, 170),qRgb(228, 166, 114),
            qRgb(184, 111, 80),  qRgb(115, 62, 57),   qRgb(62, 39, 49),   qRgb(162, 38, 51),
            qRgb(228, 59, 68),   qRgb(247, 118, 34),  qRgb(254, 174, 52), qRgb(254, 231, 97),
            qRgb(99, 199, 77),   qRgb(62, 137, 72),   qRgb(38, 92, 66),   qRgb(25, 60, 62),
            qRgb(18, 78, 137),   qRgb(0, 153, 219),   qRgb(44, 232, 245), qRgb(255, 255, 255),
            qRgb(192, 203, 220), qRgb(139, 155, 180), qRgb(90, 105, 136), qRgb(58, 68, 102),
            qRgb(38, 43, 68),    qRgb(24, 20, 37),    qRgb(255, 0, 68),   qRgb(104, 56, 108),
            qRgb(181, 80, 136),  qRgb(246, 117, 122), qRgb(232, 183, 150),qRgb(194, 133, 105)
        };
        break;

    case Custom:
    default:
        break;
    }
    return pal;
}

QString ColorPalettePresets::getPresetName(Preset preset)
{
    switch (preset) {
    case Standard:      return QCoreApplication::translate("ColorPalettePresets", "Bento Standard (36)");
    case GameBoyDMG:    return QCoreApplication::translate("ColorPalettePresets", "Game Boy DMG (4 Greens)");
    case GameBoyPocket: return QCoreApplication::translate("ColorPalettePresets", "Game Boy Pocket (4 Grays)");
    case NES:           return QCoreApplication::translate("ColorPalettePresets", "NES / Famicom (54)");
    case SNES:          return QCoreApplication::translate("ColorPalettePresets", "SNES / 16-bit (32)");
    case Pico8:         return QCoreApplication::translate("ColorPalettePresets", "PICO-8 (16)");
    case Commodore64:   return QCoreApplication::translate("ColorPalettePresets", "Commodore 64 (16)");
    case Amiga:         return QCoreApplication::translate("ColorPalettePresets", "Amiga OCS (32)");
    case PCEngine:      return QCoreApplication::translate("ColorPalettePresets", "NEC PC-Engine (32)");
    case CGAMode1:      return QCoreApplication::translate("ColorPalettePresets", "CGA Mode 1 (4)");
    case CGAMode2:      return QCoreApplication::translate("ColorPalettePresets", "CGA Mode 2 (4)");
    case Endesga32:     return QCoreApplication::translate("ColorPalettePresets", "Endesga 32 (32)");
    case Custom:        return QCoreApplication::translate("ColorPalettePresets", "Custom Imported");
    }
    return QString();
}

QList<ColorPalettePresets::Preset> ColorPalettePresets::allPresets()
{
    return {
        Standard,
        GameBoyDMG,
        GameBoyPocket,
        NES,
        SNES,
        Pico8,
        Commodore64,
        Amiga,
        PCEngine,
        CGAMode1,
        CGAMode2,
        Endesga32
    };
}

QVector<QRgb> ColorPalettePresets::loadPaletteFromFile(const QString &filePath, QString *outError)
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
        if (line.isEmpty() || line.startsWith(QLatin1String("GIMP")) || line.startsWith(QLatin1String("JASC"))) {
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

        if (line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';'))) {
            continue; // Skip comments
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

    if (result.isEmpty() && outError) {
        *outError = QCoreApplication::translate("ColorPalettePresets", "No valid colors found in palette file.");
    }
    return result;
}
