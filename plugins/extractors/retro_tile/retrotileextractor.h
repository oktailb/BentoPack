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

#ifndef RETROTILEEXTRACTOR_H
#define RETROTILEEXTRACTOR_H

#include <QtPlugin>
#include <QImage>
#include <QByteArray>
#include "extractor/extractor.h"
#include "retrotiledecoder.h"
#include "romprobe.h"

/**
 * @brief Extractor plugin for 8/16/32-bit retro console ROMs, VRAM dumps, and raw CHR streams.
 * Supports NES, Game Boy, SNES, Sega Mega Drive / Master System, Neo-Geo, and PSX TIM.
 */
class RetroTileExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit RetroTileExtractor(QObject *parent = nullptr);
    ~RetroTileExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("retro_tile_extractor"); }
    QString displayName() const override { return QStringLiteral("Retro Console ROM & VRAM Tile Ripper"); }
    QString description() const override {
        return QStringLiteral("Universal tile ripper & VRAM explorer for 8/16/32-bit consoles (NES, Game Boy, SNES, Genesis, SMS, Neo-Geo, PSX TIM) with automatic CHR detection and auto-slicing.");
    }
    QVersionNumber version() const override { return QVersionNumber(1, 0, 0); }
    QStringList supportedExtensions() const override {
        return {
            QStringLiteral("nes"), QStringLiteral("chr"),
            QStringLiteral("gb"), QStringLiteral("gbc"),
            QStringLiteral("sfc"), QStringLiteral("smc"),
            QStringLiteral("md"), QStringLiteral("gen"), QStringLiteral("smd"),
            QStringLiteral("sms"), QStringLiteral("gg"),
            QStringLiteral("tim"), QStringLiteral("vram"),
            QStringLiteral("c1"), QStringLiteral("c2"),
            QStringLiteral("bin")
        };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

    // Raw tile encoders for homebrew export
    static bool encodeTiles(const QImage &image, RetroFormat format, QByteArray &outData, QString *errorMsg = nullptr);
};

#endif // RETROTILEEXTRACTOR_H
