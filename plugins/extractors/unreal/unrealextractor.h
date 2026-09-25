// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

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
