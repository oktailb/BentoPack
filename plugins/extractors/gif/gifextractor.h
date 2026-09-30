// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

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
                              ExtractorError *error);
};

#endif // GIFEXTRACTOR_H
