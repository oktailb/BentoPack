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

#ifndef ASEPRITEEXTRACTOR_H
#define ASEPRITEEXTRACTOR_H

#include <QtPlugin>
#include <QVector>
#include <QColor>
#include "extractor/extractor.h"

/**
 * @brief Extractor plugin for native Aseprite project files (.ase, .aseprite).
 *
 * Directly decodes native Aseprite binary chunks:
 * - Color modes: 32bpp RGBA, 16bpp Grayscale, 8bpp Indexed (with custom palette)
 * - Compressed image cels (zlib/deflate)
 * - Multi-layer compositing with opacity and visibility
 * - Frame durations and named animation tags with loop modes (Forward, Reverse, Ping-pong)
 * - Slices and bounding boxes
 */
class AsepriteExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit AsepriteExtractor(QObject *parent = nullptr);
    ~AsepriteExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("aseprite_extractor"); }
    QString displayName() const override { return QStringLiteral("Aseprite Native Binary"); }
    QString description() const override { return QStringLiteral("Native Aseprite project files (.ase, .aseprite) with layers, cels, animation tags, and palette."); }
    QVersionNumber version() const override { return QVersionNumber(1, 0, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("ase"), QStringLiteral("aseprite") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAnimations | SupportsAtlasMetadata;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

private:
    struct AseLayer {
        quint16 flags = 0;
        quint16 type = 0; // 0=normal, 1=group, 2=tilemap
        quint16 childLevel = 0;
        quint16 blendMode = 0;
        quint8  opacity = 255;
        QString name;

        bool isVisible() const { return (flags & 1) != 0; }
    };

    struct AseCel {
        quint16 layerIndex = 0;
        qint16  x = 0;
        qint16  y = 0;
        quint8  opacity = 255;
        quint16 celType = 0; // 0=raw, 1=linked, 2=compressed
        quint16 width = 0;
        quint16 height = 0;
        quint16 linkedFrame = 0;
        QImage  image;
    };

    struct AseTag {
        quint16 fromFrame = 0;
        quint16 toFrame = 0;
        quint8  loopDirection = 0; // 0=forward, 1=reverse, 2=ping-pong
        quint16 repeatCount = 0;
        QString name;
    };

    struct AseSlice {
        QString name;
        QRect   bounds;
        QPoint  pivot;
        bool    hasPivot = false;
    };

    static QByteArray decompressZlib(const char *data, int compressedSize, quint32 expectedUncompressedSize);
    static QImage decodePixelsToImage(const QByteArray &rawPixels, int width, int height,
                                     quint16 colorDepth, const QVector<QRgb> &palette,
                                     quint8 transparentIndex);
};

#endif // ASEPRITEEXTRACTOR_H
