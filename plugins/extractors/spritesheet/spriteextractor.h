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

#ifndef SPRITEEXTRACTOR_H
#define SPRITEEXTRACTOR_H

#include "extractor/extractor.h"

#include <QtPlugin>

/**
 * @brief Extractor plugin for static sprite sheets (PNG, JPG, JPEG, BMP).
 */
class SpriteExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit SpriteExtractor(QObject *parent = nullptr);
    ~SpriteExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("sprite_extractor"); }
    QString displayName() const override { return QStringLiteral("Sprite Sheet"); }
    QString description() const override { return QStringLiteral("Static sprite sheet with automatic alpha edge detection."); }
    QVersionNumber version() const override { return QVersionNumber(1, 1, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("png"), QStringLiteral("webp"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("bmp"), QStringLiteral("ktx2"), QStringLiteral("basis") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAtlasMetadata;
    }

    bool canDecode(const QString &filePath) const override;

    // Core Codec API
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

    // Segmentation engine on arbitrary image
    bool extractFromImage(const QImage &image, SpriteDocument &outDoc, const SpriteSheetOptions &options = SpriteSheetOptions());
    bool extractFromImage(const QImage &image, SpriteDocument &outDoc, int alphaThreshold, int verticalTolerance);
    bool extractToImages(const QImage &sourceImage,
                         QList<QImage> &outFrames,
                         QList<SpriteBox> &outBoxes,
                         const SpriteSheetOptions &options = SpriteSheetOptions());

    // Options configuration
    void setOptions(const SpriteSheetOptions &options) { m_options = options; }
    const SpriteSheetOptions& options() const { return m_options; }
    SpriteSheetOptions& options() { return m_options; }
    void setSmartCropEnabled(bool enabled) { m_options.smartCrop = enabled; }
    void setOverlapThreshold(double threshold) { m_options.overlapThreshold = threshold; }

private:
    SpriteSheetOptions m_options;
};

#endif // SPRITEEXTRACTOR_H
