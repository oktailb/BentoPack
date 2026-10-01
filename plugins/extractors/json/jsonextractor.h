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

#ifndef JSONEXTRACTOR_H
#define JSONEXTRACTOR_H

#include <QtPlugin>
#include "extractor/extractor.h"

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
