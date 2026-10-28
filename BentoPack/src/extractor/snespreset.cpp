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

#include "extractor/snespreset.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>

QList<SnesPreset> SnesPresetManager::builtInPresets()
{
    QList<SnesPreset> list;

    // 1. The Legend of Zelda: A Link to the Past
    {
        SnesPreset p;
        p.title = QStringLiteral("Link Main Sprites (Green/Blue/Red Tunic)");
        p.gameName = QStringLiteral("The Legend of Zelda: A Link to the Past");
        p.category = QStringLiteral("Player Characters");
        p.isHiRom = false;
        p.tileOffset = 0x080000;
        p.tileLength = 28672;
        p.bpp = 4;
        p.pattern = TilePattern::Linear;
        p.tilesPerRow = 16;
        p.paletteOffset = 0x0DD308;
        p.defaultPaletteIndex = 8;
        p.description = QStringLiteral("Complete character motion frames for Link walking, slashing, lifting, swimming and dying.");
        list.append(p);
    }
    {
        SnesPreset p;
        p.title = QStringLiteral("Action Sprites, Shields, Shovel & Book");
        p.gameName = QStringLiteral("The Legend of Zelda: A Link to the Past");
        p.category = QStringLiteral("Items & Weapons");
        p.isHiRom = false;
        p.tileOffset = 0x087000;
        p.tileLength = 16384;
        p.bpp = 4;
        p.pattern = TilePattern::Linear;
        p.tilesPerRow = 16;
        p.paletteOffset = 0x0DD308;
        p.defaultPaletteIndex = 8;
        p.description = QStringLiteral("Weapons, shields, tools and companion objects.");
        list.append(p);
    }

    // 2. Super Mario World
    {
        SnesPreset p;
        p.title = QStringLiteral("Mario & Luigi Sprites");
        p.gameName = QStringLiteral("Super Mario World");
        p.category = QStringLiteral("Player Characters");
        p.isHiRom = false;
        p.tileOffset = 0x0A0000;
        p.tileLength = 32768;
        p.bpp = 4;
        p.pattern = TilePattern::Sprite16x16_VramStride16;
        p.tilesPerRow = 16;
        p.paletteOffset = -1;
        p.defaultPaletteIndex = 8;
        p.description = QStringLiteral("Mario and Luigi walking, jumping, running and cape flying sprites.");
        list.append(p);
    }

    // 3. Super Metroid
    {
        SnesPreset p;
        p.title = QStringLiteral("Samus Aran Power Suit");
        p.gameName = QStringLiteral("Super Metroid");
        p.category = QStringLiteral("Player Characters");
        p.isHiRom = true;
        p.tileOffset = 0x1B0000;
        p.tileLength = 32768;
        p.bpp = 4;
        p.pattern = TilePattern::Sprite16x16_VramStride16;
        p.tilesPerRow = 16;
        p.paletteOffset = -1;
        p.defaultPaletteIndex = 8;
        p.description = QStringLiteral("Samus Aran running, aiming, morph ball and jump animation frames.");
        list.append(p);
    }

    // 4. Chrono Trigger
    {
        SnesPreset p;
        p.title = QStringLiteral("Crono Hero Sprites");
        p.gameName = QStringLiteral("Chrono Trigger");
        p.category = QStringLiteral("Player Characters");
        p.isHiRom = true;
        p.tileOffset = 0x240000;
        p.tileLength = 32768;
        p.bpp = 4;
        p.pattern = TilePattern::Sprite16x16_2x2;
        p.tilesPerRow = 16;
        p.paletteOffset = -1;
        p.defaultPaletteIndex = 8;
        p.description = QStringLiteral("Crono walking, sword slashing, and battle victory poses.");
        list.append(p);
    }

    // 5. Mega Man X
    {
        SnesPreset p;
        p.title = QStringLiteral("Mega Man X Character Sprites");
        p.gameName = QStringLiteral("Mega Man X");
        p.category = QStringLiteral("Player Characters");
        p.isHiRom = false;
        p.tileOffset = 0x050000;
        p.tileLength = 24576;
        p.bpp = 4;
        p.pattern = TilePattern::Sprite16x16_2x2;
        p.tilesPerRow = 16;
        p.paletteOffset = -1;
        p.defaultPaletteIndex = 8;
        p.description = QStringLiteral("X running, wall-sliding, dash-jumping and Buster charging.");
        list.append(p);
    }

    // 6. Secret of Evermore
    {
        SnesPreset p;
        p.title = QStringLiteral("Boy Character Sprites");
        p.gameName = QStringLiteral("Secret of Evermore");
        p.category = QStringLiteral("Player Characters");
        p.isHiRom = true;
        p.tileOffset = 0x1A0000;
        p.tileLength = 32768;
        p.bpp = 4;
        p.pattern = TilePattern::Linear;
        p.tilesPerRow = 16;
        p.paletteOffset = -1;
        p.defaultPaletteIndex = 8;
        p.description = QStringLiteral("Main protagonist walking, running and weapon swinging frames.");
        list.append(p);
    }

    // 7. Illusion of Gaia
    {
        SnesPreset p;
        p.title = QStringLiteral("Will Main Sprite");
        p.gameName = QStringLiteral("Illusion of Gaia");
        p.category = QStringLiteral("Player Characters");
        p.isHiRom = false;
        p.tileOffset = 0x0B8000;
        p.tileLength = 16384;
        p.bpp = 4;
        p.pattern = TilePattern::Linear;
        p.tilesPerRow = 16;
        p.paletteOffset = -1;
        p.defaultPaletteIndex = 8;
        p.description = QStringLiteral("Will walking and flute swinging sprite frames.");
        list.append(p);
    }

    return list;
}

SnesPreset SnesPresetManager::loadFromFile(const QString &filePath)
{
    SnesPreset preset;
    QFileInfo fi(filePath);
    if (!fi.exists()) return preset;

    QString ext = fi.suffix().toLower();

    // 1. JSON Format
    if (ext == QStringLiteral("json")) {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                preset.title = obj.value(QStringLiteral("title")).toString();
                preset.gameName = obj.value(QStringLiteral("gameName")).toString();
                preset.category = obj.value(QStringLiteral("category")).toString();
                preset.isHiRom = obj.value(QStringLiteral("isHiRom")).toBool(false);
                preset.tileOffset = obj.value(QStringLiteral("tileOffset")).toInt(0);
                preset.tileLength = obj.value(QStringLiteral("tileLength")).toInt(16384);
                preset.bpp = obj.value(QStringLiteral("bpp")).toInt(4);
                preset.tilesPerRow = obj.value(QStringLiteral("tilesPerRow")).toInt(16);
                preset.paletteOffset = obj.value(QStringLiteral("paletteOffset")).toInt(-1);
                preset.defaultPaletteIndex = obj.value(QStringLiteral("defaultPaletteIndex")).toInt(8);
                preset.description = obj.value(QStringLiteral("description")).toString();

                QString patStr = obj.value(QStringLiteral("pattern")).toString();
                if (patStr == QStringLiteral("2x2")) preset.pattern = TilePattern::Sprite16x16_2x2;
                else if (patStr == QStringLiteral("stride16")) preset.pattern = TilePattern::Sprite16x16_VramStride16;
                else if (patStr == QStringLiteral("stride32")) preset.pattern = TilePattern::Sprite16x16_VramStride32;
                else if (patStr == QStringLiteral("4x4")) preset.pattern = TilePattern::Sprite32x32_4x4;
                else preset.pattern = TilePattern::Linear;
            }
        }
        return preset;
    }

    // 2. SNESTilesKitten INI Format (.stk)
    QSettings ini(filePath, QSettings::IniFormat);
    preset.title = ini.value(QStringLiteral("_/name"), fi.baseName()).toString();
    preset.gameName = ini.value(QStringLiteral("rom/name"), QString()).toString();
    if (preset.gameName.isEmpty()) {
        preset.gameName = preset.title.section(QLatin1Char('-'), 0, 0).trimmed();
    }

    QString romType = ini.value(QStringLiteral("rom/type"), QStringLiteral("LoROM")).toString();
    preset.isHiRom = romType.contains(QStringLiteral("HiROM"), Qt::CaseInsensitive);

    bool ok = false;
    QString pcLocStr = ini.value(QStringLiteral("tiles/pc_location"), QStringLiteral("0")).toString();
    preset.tileOffset = pcLocStr.toInt(&ok, 16);
    if (!ok) preset.tileOffset = pcLocStr.toInt();

    preset.tileLength = ini.value(QStringLiteral("tiles/length"), 16384).toInt();
    preset.bpp = ini.value(QStringLiteral("tiles/bpp"), 4).toInt();
    preset.tilesPerRow = ini.value(QStringLiteral("tiles_arrangement/tiles_per_row"), 16).toInt();

    QString palPcStr = ini.value(QStringLiteral("palette/pc_location"), QStringLiteral("-1")).toString();
    if (!palPcStr.isEmpty() && palPcStr != QStringLiteral("-1")) {
        preset.paletteOffset = palPcStr.toInt(&ok, 16);
        if (!ok) preset.paletteOffset = palPcStr.toInt();
    } else {
        preset.paletteOffset = -1;
    }

    QString patternStr = ini.value(QStringLiteral("tiles/pattern"), QStringLiteral("normal")).toString().toLower();
    if (patternStr.contains(QStringLiteral("stride16"))) {
        preset.pattern = TilePattern::Sprite16x16_VramStride16;
    } else if (patternStr.contains(QStringLiteral("stride32"))) {
        preset.pattern = TilePattern::Sprite16x16_VramStride32;
    } else if (patternStr.contains(QStringLiteral("2x2")) || patternStr.contains(QStringLiteral("seq"))) {
        preset.pattern = TilePattern::Sprite16x16_2x2;
    } else if (patternStr.contains(QStringLiteral("32x32")) || patternStr.contains(QStringLiteral("4x4"))) {
        preset.pattern = TilePattern::Sprite32x32_4x4;
    } else {
        preset.pattern = TilePattern::Linear;
    }

    return preset;
}

QList<SnesPreset> SnesPresetManager::loadFromDirectory(const QString &dirPath)
{
    QList<SnesPreset> list;
    QDir dir(dirPath);
    if (!dir.exists()) return list;

    QStringList filters;
    filters << QStringLiteral("*.stk") << QStringLiteral("*.json");
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    for (const QFileInfo &fi : files) {
        SnesPreset p = loadFromFile(fi.absoluteFilePath());
        if (p.isValid()) {
            list.append(p);
        }
    }
    return list;
}

bool SnesPresetManager::saveToFile(const QString &filePath, const SnesPreset &preset)
{
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;

    QJsonObject obj;
    obj.insert(QStringLiteral("title"), preset.title);
    obj.insert(QStringLiteral("gameName"), preset.gameName);
    obj.insert(QStringLiteral("category"), preset.category);
    obj.insert(QStringLiteral("isHiRom"), preset.isHiRom);
    obj.insert(QStringLiteral("tileOffset"), preset.tileOffset);
    obj.insert(QStringLiteral("tileLength"), preset.tileLength);
    obj.insert(QStringLiteral("bpp"), preset.bpp);
    obj.insert(QStringLiteral("tilesPerRow"), preset.tilesPerRow);
    obj.insert(QStringLiteral("paletteOffset"), preset.paletteOffset);
    obj.insert(QStringLiteral("defaultPaletteIndex"), preset.defaultPaletteIndex);
    obj.insert(QStringLiteral("description"), preset.description);

    QString patStr;
    switch (preset.pattern) {
    case TilePattern::Sprite16x16_2x2: patStr = QStringLiteral("2x2"); break;
    case TilePattern::Sprite16x16_VramStride16: patStr = QStringLiteral("stride16"); break;
    case TilePattern::Sprite16x16_VramStride32: patStr = QStringLiteral("stride32"); break;
    case TilePattern::Sprite32x32_4x4: patStr = QStringLiteral("4x4"); break;
    default: patStr = QStringLiteral("linear"); break;
    }
    obj.insert(QStringLiteral("pattern"), patStr);

    QJsonDocument doc(obj);
    f.write(doc.toJson(QJsonDocument::Indented));
    return true;
}
