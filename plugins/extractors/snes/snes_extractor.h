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

#ifndef SNES_EXTRACTOR_H
#define SNES_EXTRACTOR_H

#include <QtPlugin>
#include <QImage>
#include <QByteArray>
#include "extractor/extractor.h"
#include "extractor/retrotiledecoder.h"
#include "extractor/snessavestateloader.h"
#include "extractor/snespreset.h"

/**
 * @brief Specialized Extractor plugin for Super Nintendo / Super Famicom (SNES).
 * Supports cartridge ROMs (LoROM/HiROM), emulator savestates (Snes9x, ZSNES),
 * raw 64KB VRAM / 512B CGRAM dumps, and classic game presets.
 */
class SnesExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit SnesExtractor(QObject *parent = nullptr);
    ~SnesExtractor() override = default;

    QString id() const override { return QStringLiteral("snes_extractor"); }
    QString displayName() const override { return QStringLiteral("Super Nintendo / Super Famicom (SNES) Ripper"); }
    QString description() const override {
        return QStringLiteral("Specialized SNES sprite ripper supporting LoROM/HiROM addressing, Snes9x & ZSNES savestates, raw VRAM/CGRAM dumps, classic game presets, and multi-format tile patterns.");
    }
    QVersionNumber version() const override { return QVersionNumber(1, 0, 0); }
    QStringList supportedExtensions() const override {
        return {
            QStringLiteral("sfc"), QStringLiteral("smc"), QStringLiteral("swc"), QStringLiteral("fig"),
            QStringLiteral("000"), QStringLiteral("001"), QStringLiteral("002"), QStringLiteral("003"),
            QStringLiteral("004"), QStringLiteral("005"), QStringLiteral("006"), QStringLiteral("007"),
            QStringLiteral("008"), QStringLiteral("009"), QStringLiteral("s96"),
            QStringLiteral("zst"), QStringLiteral("zs1"), QStringLiteral("zs2"), QStringLiteral("zs3"),
            QStringLiteral("mss"), QStringLiteral("stk")
        };
    }
    Capabilities capabilities() const override {
        return CanImport | CanExport;
    }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;

    bool hasInteractiveDialog() const override { return true; }
    bool showInteractiveDialog(const QString &filePath, SpriteDocument &outDoc, QWidget *parent = nullptr) override;
};

#endif // SNES_EXTRACTOR_H
