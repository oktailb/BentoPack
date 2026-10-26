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

#ifndef CASM_EXTRACTOR_H
#define CASM_EXTRACTOR_H

#include "extractor/extractor.h"
#include <QtPlugin>

class CAsmExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit CAsmExtractor(QObject *parent = nullptr);
    ~CAsmExtractor() override = default;

    QString id() const override { return QStringLiteral("casm_extractor"); }
    QString displayName() const override { return QStringLiteral("C / C++ & Assembly Retro Source (.h / .c / .asm)"); }
    QString description() const override {
        return QStringLiteral("Exports sprite sheets, animations, and palettes as native C/C++ or assembly code for 8-bit, 16-bit, and 32-bit retro architectures.");
    }
    QVersionNumber version() const override { return QVersionNumber(1, 0, 0); }

    QStringList supportedExtensions() const override {
        return { QStringLiteral("h"), QStringLiteral("c"), QStringLiteral("asm"), QStringLiteral("s"), QStringLiteral("inc") };
    }

    Capabilities capabilities() const override {
        return CanExport | SupportsAnimations | SupportsAtlasMetadata;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

    QWidget* createSettingsWidget(QWidget *parent = nullptr) override;
};

#endif // CASM_EXTRACTOR_H
