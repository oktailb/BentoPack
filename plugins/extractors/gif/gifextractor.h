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

#ifndef GIFEXTRACTOR_H
#define GIFEXTRACTOR_H

#include "extractor/extractor.h"

#include <QtPlugin>

/**
 * @brief Extractor plugin for Animated GIF sprites.
 */
class GifExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)
public:
    explicit GifExtractor(QObject *parent = nullptr);
    ~GifExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("gif_extractor"); }
    QString displayName() const override { return QStringLiteral("Animated GIF"); }
    QString description() const override { return QStringLiteral("Animated GIF format with frame sequence extraction and export."); }
    QVersionNumber version() const override { return QVersionNumber(1, 2, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("gif") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAnimations;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

private:
    bool writeSingleAnimation(const QString &targetFilePath,
                              const SpriteDocument &doc,
                              const SpriteAnimation &anim,
                              const ExportOptions &options,
                              ExtractorError *error);
};

#endif // GIFEXTRACTOR_H
