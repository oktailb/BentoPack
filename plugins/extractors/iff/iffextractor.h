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

#ifndef IFFEXTRACTOR_H
#define IFFEXTRACTOR_H

#include <QtPlugin>
#include <QImage>
#include <QByteArray>
#include <QVector>
#include <QRgb>
#include "extractor/extractor.h"

/**
 * @brief Extractor plugin for Commodore Amiga / Electronic Arts IFF ILBM and PBM images (.iff, .ilbm, .lbm).
 * Supports Deluxe Paint standard, planar bitplanes (1 to 8 bpp, 24 bpp), Extra Half-Brite (EHB),
 * HAM6/HAM8 modes, and ByteRun1 RLE compression.
 */
class IffExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit IffExtractor(QObject *parent = nullptr);
    ~IffExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("iff_extractor"); }
    QString displayName() const override { return QStringLiteral("Amiga IFF / ILBM Deluxe Paint"); }
    QString description() const override {
        return QStringLiteral("Commodore Amiga IFF / ILBM format (.iff, .ilbm, .lbm) with planar bitplanes (1-8 bpp, 24 bpp), EHB (Extra Half-Brite), HAM6/HAM8, and ByteRun1 RLE.");
    }
    QVersionNumber version() const override { return QVersionNumber(1, 0, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("iff"), QStringLiteral("ilbm"), QStringLiteral("lbm") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

    // Direct codec utilities
    static bool decodeIff(const QByteArray &data, QImage &outImage, QString *errorMessage = nullptr);
    static bool encodeIff(const QImage &image, QByteArray &outData, QString *errorMessage = nullptr);

    // ByteRun1 (PackBits) RLE helpers
    static QByteArray byteRun1Compress(const uchar *src, int srcLen);
    static bool byteRun1Decompress(const uchar *src, int srcLen, uchar *dst, int dstLen);
};

#endif // IFFEXTRACTOR_H
