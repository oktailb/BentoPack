/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QObject>
#include <QString>
#include <QColor>
#include "bentopackcore_export.h"

/**
 * @brief Configuration settings for the atlas view, zooming, slicing, and nudging.
 */
struct SPRITESTUDIO_CORE_EXPORT AtlasConfig
{
    double zoomMin = 0.1;
    double zoomMax = 10.0;
    double zoomStep = 1.15;
    int    minSliceSize = 3;
    int    defaultAlphaThreshold = 1;
    int    defaultVerticalTolerance = 0;
    int    nudgeStepSmall = 1;
    int    nudgeStepLarge = 10;
    int    fitViewPadding = 20;
};

/**
 * @brief Visual styling settings (box colors, handles, marquee preview).
 */
struct VisualConfig
{
    double handleSize = 8.0;
    double handleMargin = 16.0;
    QColor selectedBoxColor = QColor(255, 200, 0);          // Gold
    QColor unselectedBoxColor = QColor(0, 180, 255, 180);    // Cyan outline
    QColor selectedBoxFillColor = QColor(0, 160, 255, 60);   // Translucent blue fill
    QColor hoveredBoxFillColor = QColor(0, 180, 255, 30);    // Light translucent blue
    QColor marqueeColor = QColor(0, 120, 215);               // Blue dashed marquee
    QColor newSlicePreviewColor = QColor(0, 220, 100);       // Green dashed slice preview
};

/**
 * @brief Configuration settings for animation playback and FPS boundaries.
 */
struct AnimationConfig
{
    int defaultFps = 12;
    int minFps = 1;
    int maxFps = 60;
    bool autoPlayOnSelection = true;
};

/**
 * @brief Configuration settings for project management, recent files, and image filters.
 */
struct SPRITESTUDIO_CORE_EXPORT ProjectConfig
{
    int maxRecentFiles = 10;
    int backgroundRemovalTolerance = 10;
    int backgroundMinAlpha = 10;
    int undoLimit = 50;
};

/**
 * @brief Configuration settings for Git version control and author identity.
 */
struct SPRITESTUDIO_CORE_EXPORT GitConfig
{
    QString authorName;
    QString authorEmail;

    static void detectSystemIdentity(QString *name, QString *email);
};

/**
 * @brief General application settings (language, locale, startup).
 */
struct SPRITESTUDIO_CORE_EXPORT GeneralConfig
{
    QString language = QStringLiteral("system"); // "system", "fr_FR", "en_US", "ja_JA"
    bool checkUpdatesOnStartup = true;
    bool reopenLastProject = false;
};

/**
 * @brief Default settings for export and VRAM texture compression.
 */
struct SPRITESTUDIO_CORE_EXPORT ExportConfig
{
    QString defaultFormatId;            // Dynamic Extractor plugin ID (e.g. "godot_extractor", "json_extractor")
    int defaultTextureFormatIndex = 0;   // 0 = PNG, 1 = KTX2 UASTC, 2 = KTX2 ETC1S
    int defaultAlgorithmIndex = 0;       // 0 = Keep layout, 1 = MaxRects BSSF, etc.
    bool defaultZstd = true;
    int defaultZstdLevel = 9;
};

/**
 * @brief Central configuration manager for BentoPack.
 *
 * Persists and loads user and default application settings to/from a structured
 * JSON file (bentopack_config.json). Designed with full fail-safe resilience:
 * if the JSON file is missing, corrupt, or contains invalid keys, safe defaults
 * are automatically preserved without throwing exceptions or crashing.
 */
class SPRITESTUDIO_CORE_EXPORT AppConfig : public QObject
{
    Q_OBJECT

public:
    static AppConfig& instance();

    // Accessors
    const GeneralConfig& general() const { return m_general; }
    GeneralConfig& general() { return m_general; }

    const AtlasConfig& atlas() const { return m_atlas; }
    AtlasConfig& atlas() { return m_atlas; }

    const VisualConfig& visuals() const { return m_visuals; }
    VisualConfig& visuals() { return m_visuals; }

    const AnimationConfig& animation() const { return m_animation; }
    AnimationConfig& animation() { return m_animation; }

    const ProjectConfig& project() const { return m_project; }
    ProjectConfig& project() { return m_project; }

    const GitConfig& git() const { return m_git; }
    GitConfig& git() { return m_git; }

    const ExportConfig& exportSettings() const { return m_export; }
    ExportConfig& exportSettings() { return m_export; }

    // File operations
    bool load(const QString &filePath = QString());
    bool save(const QString &filePath = QString()) const;
    void resetToDefaults();

    QString configFilePath() const;
    void setConfigFilePath(const QString &path);

signals:
    void configChanged();

private:
    AppConfig();
    ~AppConfig() override = default;
    Q_DISABLE_COPY(AppConfig)

    QString resolveDefaultConfigPath() const;

    GeneralConfig   m_general;
    AtlasConfig     m_atlas;
    VisualConfig    m_visuals;
    AnimationConfig m_animation;
    ProjectConfig   m_project;
    GitConfig       m_git;
    ExportConfig    m_export;

    mutable QString m_customConfigPath;
};

#endif // APPCONFIG_H
