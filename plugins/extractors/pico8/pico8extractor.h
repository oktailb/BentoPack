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

#ifndef PICO8EXTRACTOR_H
#define PICO8EXTRACTOR_H

#include <QtPlugin>
#include <QImage>
#include <QVector>
#include <QRgb>
#include "extractor/extractor.h"

/**
 * @brief Extractor plugin for PICO-8 fantasy console cartridges (.p8).
 * Parses and serializes the 128x128 4bpp sprite sheet (__gfx__) and sprite flags (__gff__).
 */
class Pico8Extractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit Pico8Extractor(QObject *parent = nullptr);
    ~Pico8Extractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("pico8_extractor"); }
    QString displayName() const override { return QStringLiteral("PICO-8 Cartridge"); }
    QString description() const override {
        return QStringLiteral("PICO-8 fantasy console cartridge format (.p8) sprite sheet (__gfx__) and sprite flags (__gff__).");
    }
    QVersionNumber version() const override { return QVersionNumber(1, 0, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("p8") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

    // PICO-8 Palette Utilities
    static const QVector<QRgb>& pico8Palette();
    static int findNearestPico8Color(QRgb rgb);
    static bool parseGfx(const QStringList &gfxLines, QImage &outImage);
    static QStringList formatGfx(const QImage &image);
    static QVector<quint8> parseGff(const QStringList &gffLines);
    static QStringList formatGff(const QVector<quint8> &flags);
};

#endif // PICO8EXTRACTOR_H
