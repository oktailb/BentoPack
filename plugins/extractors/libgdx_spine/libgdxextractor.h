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

#ifndef LIBGDXEXTRACTOR_H
#define LIBGDXEXTRACTOR_H

#include <QtPlugin>
#include "extractor/extractor.h"

/**
 * @brief Extractor plugin for LibGDX and Spine 2D TextureAtlas (.atlas) format.
 */
class LibGdxExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit LibGdxExtractor(QObject *parent = nullptr);
    ~LibGdxExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("libgdx_spine_extractor"); }
    QString displayName() const override { return QStringLiteral("LibGDX / Spine Atlas"); }
    QString description() const override { return QStringLiteral("LibGDX and Spine 2D .atlas texture descriptor with companion image."); }
    QVersionNumber version() const override { return QVersionNumber(1, 0, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("atlas"), QStringLiteral("atlas.txt") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAnimations | SupportsAtlasMetadata;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

private:
    struct AtlasRegion {
        QString name;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int origWidth = 0;
        int origHeight = 0;
        int offsetX = 0;
        int offsetY = 0;
        int index = -1;
        bool rotate = false;
        int degrees = 0;
    };

    struct AtlasPage {
        QString textureFile;
        int width = 0;
        int height = 0;
        QString format = QStringLiteral("RGBA8888");
        QString filter = QStringLiteral("Linear,Linear");
        QString repeat = QStringLiteral("none");
        QList<AtlasRegion> regions;
    };

    bool parseAtlasText(const QString &content, QList<AtlasPage> &pages, QString *errorMsg);
};

#endif // LIBGDXEXTRACTOR_H
