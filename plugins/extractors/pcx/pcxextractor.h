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

#ifndef PCXEXTRACTOR_H
#define PCXEXTRACTOR_H

#include <QtPlugin>
#include <QImage>
#include <QByteArray>
#include "extractor/extractor.h"

/**
 * @brief Extractor plugin for ZSoft PCX image files (.pcx).
 * Supports 8-bit RLE indexed with 256-color VGA palette at EOF and 24-bit 3-plane RGB.
 */
class PcxExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit PcxExtractor(QObject *parent = nullptr);
    ~PcxExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("pcx_extractor"); }
    QString displayName() const override { return QStringLiteral("ZSoft PCX Image"); }
    QString description() const override {
        return QStringLiteral("ZSoft Paintbrush PCX format (.pcx) with 8-bit RLE (256 colors) and 24-bit RGB.");
    }
    QVersionNumber version() const override { return QVersionNumber(1, 0, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("pcx") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

    // Direct codec utilities
    static bool decodePcx(const QByteArray &data, QImage &outImage, QString *errorMessage = nullptr);
    static bool encodePcx(const QImage &image, QByteArray &outData, QString *errorMessage = nullptr);
};

#endif // PCXEXTRACTOR_H
