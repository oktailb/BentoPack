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

#ifndef UNREALEXTRACTOR_H
#define UNREALEXTRACTOR_H

#include "extractor/extractor.h"

/**
 * @brief Exporter for Unreal Engine 5 Paper2D / PaperZD Sprite Atlas.
 *
 * Produces companion PNG atlas texture and a JSON descriptor with RenderGeometry
 * and CollisionGeometry for direct sprite and flipbook generation in Unreal Engine.
 */
#include <QtPlugin>

class UnrealExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit UnrealExtractor(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("unreal"); }
    QString displayName() const override { return tr("Unreal Engine Paper2D (*.paper2d.json)"); }
    QString description() const override { return tr("Exports Unreal Engine Paper2D sprites with tight RenderGeometry."); }
    QStringList supportedExtensions() const override { return { QStringLiteral("paper2d.json"), QStringLiteral("json") }; }
    Capabilities capabilities() const override { return CanImport | CanExport | SupportsAnimations | SupportsAtlasMetadata; }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;
};

#endif // UNREALEXTRACTOR_H
