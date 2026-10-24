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

#ifndef PLISTEXTRACTOR_H
#define PLISTEXTRACTOR_H

#include <QtPlugin>
#include <QRect>
#include <QPoint>
#include <QSize>
#include "extractor/extractor.h"

/**
 * @brief Extractor plugin for Apple Property List (.plist) sprite sheet atlas descriptors.
 * Supports Cocos2d-x / TexturePacker formats (format 2 and format 3).
 */
class PlistExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit PlistExtractor(QObject *parent = nullptr);
    ~PlistExtractor() override = default;

    // Plugin metadata
    QString id() const override { return QStringLiteral("plist_extractor"); }
    QString displayName() const override { return QStringLiteral("Apple / Cocos2d-x Plist"); }
    QString description() const override {
        return QStringLiteral("Apple Property List (.plist) sprite sheet atlas descriptor (Cocos2d-x, SpriteKit, TexturePacker).");
    }
    QVersionNumber version() const override { return QVersionNumber(1, 0, 0); }
    QStringList supportedExtensions() const override {
        return { QStringLiteral("plist") };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport | SupportsAnimations | SupportsAtlasMetadata;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

    // Helper geometry parsing/formatting
    static QRect parseRect(const QString &str);
    static QPoint parsePoint(const QString &str);
    static QSize parseSize(const QString &str);
    static QString formatRect(const QRect &r);
    static QString formatPoint(const QPoint &p);
    static QString formatSize(const QSize &s);
};

#endif // PLISTEXTRACTOR_H
