// This file is part of the SpriteStudio Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#ifndef JSONEXTRACTOR_H
#define JSONEXTRACTOR_H

#include <QtPlugin>
#include "extractor/extractor.h"
#include "jsonExtractordialog.h"

/**
 * @brief Extractor plugin for JSON sprite databases (TexturePacker, Aseprite).
 */
class JsonExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)
public:
    explicit JsonExtractor(QObject *parent = nullptr);
    ~JsonExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("json_extractor"); }
    QString displayName() const override { return QStringLiteral("JSON Atlas"); }
    QString description() const override { return QStringLiteral("TexturePacker and Aseprite JSON atlas descriptor with image."); }
    QVersionNumber version() const override { return QVersionNumber(1, 1, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("json") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAnimations | SupportsAtlasMetadata;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;
    QWidget* createSettingsWidget(QWidget *parent = nullptr) override;

private:
    QJsonDocument* exportToTexturePacker(const QString &projectName,
                                        const ExportOptions &opts,
                                        const QString &anim,
                                        const QString &format,
                                        const SpriteDocument &doc);
    void extractFromTexturePackerFormat(const QJsonObject &framesObj,
                                       const QImage &atlasImage,
                                       QList<QImage> &frames,
                                       QList<SpriteBox> &boxes,
                                       QMap<QString, QList<int>> &animationFrames);
    void extractFromArrayFormat(const QJsonArray &framesArray,
                                const QImage &atlasImage,
                                QList<QImage> &frames,
                                QList<SpriteBox> &boxes,
                                QMap<QString, QList<int>> &animationFrames);
    void extractAnimationsFromFrameTags(const QJsonArray &frameTagsArray,
                                       QMap<QString, QList<int>> &animationFrames,
                                       int shift);
    QString extractAnimationName(const QString &frameName);
};

#endif // JSONEXTRACTOR_H
