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

#ifndef RETROPROFILEREGISTRY_H
#define RETROPROFILEREGISTRY_H

#include <QString>
#include <QList>
#include <QMap>
#include <QObject>
#include "bentopackcore_export.h"

/**
 * @brief Represents a target retro hardware constraint profile (PPU limits, scanlines, tiles).
 */
struct BENTOPACK_CORE_EXPORT RetroHardwareProfile
{
    QString id;
    QString name;
    QString description;
    int     scanlineSpriteWidthLimit = 64; // Max scanline sprite pixels
    int     maxVramTiles = 256;            // Unique 8x8 tile budget in VRAM
    int     tileSize = 8;                  // 8 for 8x8, 16 for 16x16
    int     maxSpritesPerScanline = 8;     // Hardware sprite count per scanline
    int     maxColorsPerSprite = 16;       // Palette color budget per sprite
    int     legacyIndex = 0;               // Legacy integer preset mapping for backward compatibility
};

/**
 * @brief Dynamic registry for retro hardware profiles.
 *
 * Plugins register their supported console architectures dynamically.
 * BentoPack Core and SettingsDialog do not hardcode any specific hardware profile.
 */
class BENTOPACK_CORE_EXPORT RetroProfileRegistry : public QObject
{
    Q_OBJECT

public:
    static RetroProfileRegistry& instance();

    void registerProfile(const RetroHardwareProfile &profile);
    void unregisterProfile(const QString &id);
    QList<RetroHardwareProfile> profiles() const;
    RetroHardwareProfile profile(const QString &id) const;
    bool hasProfile(const QString &id) const;
    RetroHardwareProfile profileByLegacyIndex(int legacyIndex) const;
    bool hasLegacyIndex(int legacyIndex) const;

signals:
    void profilesChanged();

private:
    explicit RetroProfileRegistry(QObject *parent = nullptr);
    ~RetroProfileRegistry() override = default;

    QMap<QString, RetroHardwareProfile> m_profiles;
};

#endif // RETROPROFILEREGISTRY_H
