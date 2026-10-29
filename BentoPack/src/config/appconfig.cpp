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

#include "config/appconfig.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDebug>

#if defined(HAVE_LIBGIT2) && __has_include(<git2.h>)
#define APPCONFIG_HAS_LIBGIT2 1
#include <git2.h>
#if !defined(LIBGIT2_VER_MAJOR) || (LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR < 28)
#define git_buf_dispose git_buf_free
#endif
#endif

void GitConfig::detectSystemIdentity(QString *name, QString *email)
{
    QString foundName;
    QString foundEmail;

#ifdef APPCONFIG_HAS_LIBGIT2
    git_libgit2_init();
    git_config *cfg = nullptr;
    if (git_config_open_default(&cfg) == 0) {
        git_buf nameBuf = {nullptr, 0, 0};
        if (git_config_get_string_buf(&nameBuf, cfg, "user.name") == 0 && nameBuf.ptr) {
            foundName = QString::fromUtf8(nameBuf.ptr).trimmed();
        }
        git_buf_dispose(&nameBuf);

        git_buf emailBuf = {nullptr, 0, 0};
        if (git_config_get_string_buf(&emailBuf, cfg, "user.email") == 0 && emailBuf.ptr) {
            foundEmail = QString::fromUtf8(emailBuf.ptr).trimmed();
        }
        git_buf_dispose(&emailBuf);

        git_config_free(cfg);
    }
#endif

    if (foundName.isEmpty()) {
        foundName = qEnvironmentVariable("GIT_AUTHOR_NAME");
        if (foundName.isEmpty()) {
            foundName = qEnvironmentVariable("USERNAME");
        }
        if (foundName.isEmpty()) {
            foundName = qEnvironmentVariable("USER");
        }
        if (foundName.isEmpty()) {
            foundName = QStringLiteral("BentoPack");
        }
    }

    if (foundEmail.isEmpty()) {
        foundEmail = qEnvironmentVariable("GIT_AUTHOR_EMAIL");
        if (foundEmail.isEmpty()) {
            foundEmail = QStringLiteral("bentopack@local");
        }
    }

    if (name) *name = foundName;
    if (email) *email = foundEmail;
}

#include "extractor/retroprofileregistry.h"

void RetroLinterConfig::applyPreset(const QString &profileId, RetroLinterConfig &cfg)
{
    cfg.profileId = profileId;
    if (profileId == QStringLiteral("custom") || profileId.isEmpty()) {
        cfg.profileIndex = 0;
        return;
    }
    if (RetroProfileRegistry::instance().hasProfile(profileId)) {
        auto p = RetroProfileRegistry::instance().profile(profileId);
        cfg.scanlineSpriteWidthLimit = p.scanlineSpriteWidthLimit;
        cfg.maxVramTiles = p.maxVramTiles;
        cfg.tileSize = p.tileSize;
        cfg.maxSpritesPerScanline = p.maxSpritesPerScanline;
        cfg.maxColorsPerSprite = p.maxColorsPerSprite;
    }
}

void RetroLinterConfig::applyPreset(int presetIndex, RetroLinterConfig &cfg)
{
    cfg.profileIndex = presetIndex;
    if (presetIndex > 0 && RetroProfileRegistry::instance().hasLegacyIndex(presetIndex)) {
        auto p = RetroProfileRegistry::instance().profileByLegacyIndex(presetIndex);
        cfg.profileId = p.id;
        applyPreset(p.id, cfg);
    }
}

AppConfig& AppConfig::instance()
{
    static AppConfig cfg;
    return cfg;
}

AppConfig::AppConfig()
{
    // Attempt to load existing config on construction or create default
    load();
}

QString AppConfig::resolveDefaultConfigPath() const
{
    // Preferred persistent location:
    // Windows: %APPDATA%/BentoPack/bentopack_config.json
    // Linux:   ~/.config/BentoPack/bentopack_config.json
    // macOS:   ~/Library/Application Support/BentoPack/bentopack_config.json
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QDir::homePath() + QStringLiteral("/.bentopack");
    }
    return QDir(configDir).filePath(QStringLiteral("bentopack_config.json"));
}

QString AppConfig::configFilePath() const
{
    if (!m_customConfigPath.isEmpty()) {
        return m_customConfigPath;
    }
    return resolveDefaultConfigPath();
}

void AppConfig::setConfigFilePath(const QString &path)
{
    m_customConfigPath = path;
}

void AppConfig::resetToDefaults()
{
    m_general = GeneralConfig();
    m_atlas = AtlasConfig();
    m_visuals = VisualConfig();
    m_animation = AnimationConfig();
    m_project = ProjectConfig();
    m_git = GitConfig();
    m_export = ExportConfig();
    m_retro = RetroLinterConfig();
    emit configChanged();
}

bool AppConfig::load(const QString &filePath)
{
    QString path = filePath.isEmpty() ? configFilePath() : filePath;

    if (!QFile::exists(path)) {
        // Automatically save initial default config file so user can inspect and edit it
        save(path);
        return true;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[AppConfig] Unable to open configuration file for reading:" << path;
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[AppConfig] Corrupted or invalid JSON in" << path << ":"
                   << parseError.errorString() << "- Safely retaining current defaults.";
        return false;
    }

    QJsonObject root = doc.object();

    // 0. General section
    if (root.contains(QStringLiteral("general")) && root.value(QStringLiteral("general")).isObject()) {
        QJsonObject genObj = root.value(QStringLiteral("general")).toObject();
        m_general.language = genObj.value(QStringLiteral("language")).toString(m_general.language);
        m_general.checkUpdatesOnStartup = genObj.value(QStringLiteral("check_updates_on_startup")).toBool(m_general.checkUpdatesOnStartup);
        m_general.reopenLastProject = genObj.value(QStringLiteral("reopen_last_project")).toBool(m_general.reopenLastProject);
    }

    // 1. Atlas section
    if (root.contains(QStringLiteral("atlas")) && root.value(QStringLiteral("atlas")).isObject()) {
        QJsonObject atlasObj = root.value(QStringLiteral("atlas")).toObject();
        m_atlas.zoomMin = atlasObj.value(QStringLiteral("zoom_min")).toDouble(m_atlas.zoomMin);
        m_atlas.zoomMax = atlasObj.value(QStringLiteral("zoom_max")).toDouble(m_atlas.zoomMax);
        m_atlas.zoomStep = atlasObj.value(QStringLiteral("zoom_step")).toDouble(m_atlas.zoomStep);
        m_atlas.minSliceSize = atlasObj.value(QStringLiteral("min_slice_size")).toInt(m_atlas.minSliceSize);
        m_atlas.defaultAlphaThreshold = atlasObj.value(QStringLiteral("default_alpha_threshold")).toInt(m_atlas.defaultAlphaThreshold);
        m_atlas.defaultVerticalTolerance = atlasObj.value(QStringLiteral("default_vertical_tolerance")).toInt(m_atlas.defaultVerticalTolerance);
        m_atlas.nudgeStepSmall = atlasObj.value(QStringLiteral("nudge_step_small")).toInt(m_atlas.nudgeStepSmall);
        m_atlas.nudgeStepLarge = atlasObj.value(QStringLiteral("nudge_step_large")).toInt(m_atlas.nudgeStepLarge);
        m_atlas.fitViewPadding = atlasObj.value(QStringLiteral("fit_view_padding")).toInt(m_atlas.fitViewPadding);
        m_atlas.showPolygonMeshByDefault = atlasObj.value(QStringLiteral("show_polygon_mesh_by_default")).toBool(m_atlas.showPolygonMeshByDefault);
    }

    // 2. Visuals section
    if (root.contains(QStringLiteral("visuals")) && root.value(QStringLiteral("visuals")).isObject()) {
        QJsonObject visObj = root.value(QStringLiteral("visuals")).toObject();
        m_visuals.handleSize = visObj.value(QStringLiteral("handle_size")).toDouble(m_visuals.handleSize);
        m_visuals.handleMargin = visObj.value(QStringLiteral("handle_margin")).toDouble(m_visuals.handleMargin);

        if (visObj.contains(QStringLiteral("selected_box_color"))) {
            m_visuals.selectedBoxColor = QColor(visObj.value(QStringLiteral("selected_box_color")).toString());
        }
        if (visObj.contains(QStringLiteral("unselected_box_color"))) {
            m_visuals.unselectedBoxColor = QColor(visObj.value(QStringLiteral("unselected_box_color")).toString());
        }
        if (visObj.contains(QStringLiteral("selected_box_fill_color"))) {
            m_visuals.selectedBoxFillColor = QColor(visObj.value(QStringLiteral("selected_box_fill_color")).toString());
        }
        if (visObj.contains(QStringLiteral("hovered_box_fill_color"))) {
            m_visuals.hoveredBoxFillColor = QColor(visObj.value(QStringLiteral("hovered_box_fill_color")).toString());
        }
        if (visObj.contains(QStringLiteral("marquee_color"))) {
            m_visuals.marqueeColor = QColor(visObj.value(QStringLiteral("marquee_color")).toString());
        }
        if (visObj.contains(QStringLiteral("new_slice_preview_color"))) {
            m_visuals.newSlicePreviewColor = QColor(visObj.value(QStringLiteral("new_slice_preview_color")).toString());
        }
        if (visObj.contains(QStringLiteral("mesh_wireframe_color"))) {
            m_visuals.meshWireframeColor = QColor(visObj.value(QStringLiteral("mesh_wireframe_color")).toString());
        }
    }

    // 3. Animation section
    if (root.contains(QStringLiteral("animation")) && root.value(QStringLiteral("animation")).isObject()) {
        QJsonObject animObj = root.value(QStringLiteral("animation")).toObject();
        m_animation.defaultFps = animObj.value(QStringLiteral("default_fps")).toInt(m_animation.defaultFps);
        m_animation.minFps = animObj.value(QStringLiteral("min_fps")).toInt(m_animation.minFps);
        m_animation.maxFps = animObj.value(QStringLiteral("max_fps")).toInt(m_animation.maxFps);
        m_animation.autoPlayOnSelection = animObj.value(QStringLiteral("auto_play_on_selection")).toBool(m_animation.autoPlayOnSelection);
    }

    // 4. Project section
    if (root.contains(QStringLiteral("project")) && root.value(QStringLiteral("project")).isObject()) {
        QJsonObject projObj = root.value(QStringLiteral("project")).toObject();
        m_project.maxRecentFiles = projObj.value(QStringLiteral("max_recent_files")).toInt(m_project.maxRecentFiles);
        m_project.backgroundRemovalTolerance = projObj.value(QStringLiteral("background_removal_tolerance")).toInt(m_project.backgroundRemovalTolerance);
        m_project.backgroundMinAlpha = projObj.value(QStringLiteral("background_min_alpha")).toInt(m_project.backgroundMinAlpha);
        m_project.undoLimit = projObj.value(QStringLiteral("undo_limit")).toInt(m_project.undoLimit);
        m_project.autoSaveSession = projObj.value(QStringLiteral("auto_save_session")).toBool(m_project.autoSaveSession);
    }

    // 5. Git section
    if (root.contains(QStringLiteral("git")) && root.value(QStringLiteral("git")).isObject()) {
        QJsonObject gitObj = root.value(QStringLiteral("git")).toObject();
        m_git.authorName = gitObj.value(QStringLiteral("author_name")).toString(m_git.authorName);
        m_git.authorEmail = gitObj.value(QStringLiteral("author_email")).toString(m_git.authorEmail);
    }

    // 6. Export section
    if (root.contains(QStringLiteral("export")) && root.value(QStringLiteral("export")).isObject()) {
        QJsonObject expObj = root.value(QStringLiteral("export")).toObject();
        m_export.defaultFormatId = expObj.value(QStringLiteral("default_format_id")).toString(m_export.defaultFormatId);
        m_export.defaultTextureFormatIndex = expObj.value(QStringLiteral("default_texture_format_index")).toInt(m_export.defaultTextureFormatIndex);
        m_export.defaultAlgorithmIndex = expObj.value(QStringLiteral("default_algorithm_index")).toInt(m_export.defaultAlgorithmIndex);
        m_export.defaultZstd = expObj.value(QStringLiteral("default_zstd")).toBool(m_export.defaultZstd);
        m_export.defaultZstdLevel = expObj.value(QStringLiteral("default_zstd_level")).toInt(m_export.defaultZstdLevel);
        m_export.defaultPadding = expObj.value(QStringLiteral("default_padding")).toInt(m_export.defaultPadding);
        m_export.defaultBorderPadding = expObj.value(QStringLiteral("default_border_padding")).toInt(m_export.defaultBorderPadding);
        m_export.defaultExtrude = expObj.value(QStringLiteral("default_extrude")).toInt(m_export.defaultExtrude);
        m_export.defaultMaxWidth = expObj.value(QStringLiteral("default_max_width")).toInt(m_export.defaultMaxWidth);
        m_export.defaultMaxHeight = expObj.value(QStringLiteral("default_max_height")).toInt(m_export.defaultMaxHeight);
        m_export.defaultPowerOfTwo = expObj.value(QStringLiteral("default_power_of_two")).toBool(m_export.defaultPowerOfTwo);
        m_export.defaultForceSquare = expObj.value(QStringLiteral("default_force_square")).toBool(m_export.defaultForceSquare);
        m_export.defaultDeduplicate = expObj.value(QStringLiteral("default_deduplicate")).toBool(m_export.defaultDeduplicate);
        m_export.defaultTrimSprites = expObj.value(QStringLiteral("default_trim_sprites")).toBool(m_export.defaultTrimSprites);
        m_export.defaultNormalMapYFlip = expObj.value(QStringLiteral("default_normal_map_y_flip")).toBool(m_export.defaultNormalMapYFlip);
    }

    // 7. Retro & Hardware Constraints section
    if (root.contains(QStringLiteral("retro_linter")) && root.value(QStringLiteral("retro_linter")).isObject()) {
        QJsonObject retroObj = root.value(QStringLiteral("retro_linter")).toObject();
        m_retro.enabled = retroObj.value(QStringLiteral("enabled")).toBool(m_retro.enabled);
        m_retro.profileIndex = retroObj.value(QStringLiteral("profile_index")).toInt(m_retro.profileIndex);
        m_retro.profileId = retroObj.value(QStringLiteral("profile_id")).toString(m_retro.profileId);
        if (m_retro.profileId.isEmpty() && m_retro.profileIndex > 0) {
            if (RetroProfileRegistry::instance().hasLegacyIndex(m_retro.profileIndex)) {
                m_retro.profileId = RetroProfileRegistry::instance().profileByLegacyIndex(m_retro.profileIndex).id;
            }
        }
        m_retro.scanlineSpriteWidthLimit = retroObj.value(QStringLiteral("scanline_sprite_width_limit")).toInt(m_retro.scanlineSpriteWidthLimit);
        m_retro.maxVramTiles = retroObj.value(QStringLiteral("max_vram_tiles")).toInt(m_retro.maxVramTiles);
        m_retro.tileSize = retroObj.value(QStringLiteral("tile_size")).toInt(m_retro.tileSize);
        m_retro.guideLineThickness = retroObj.value(QStringLiteral("guide_line_thickness")).toInt(m_retro.guideLineThickness);
        m_retro.maxSpritesPerScanline = retroObj.value(QStringLiteral("max_sprites_per_scanline")).toInt(m_retro.maxSpritesPerScanline);
        m_retro.maxColorsPerSprite = retroObj.value(QStringLiteral("max_colors_per_sprite")).toInt(m_retro.maxColorsPerSprite);
        m_retro.showScanlineGuide = retroObj.value(QStringLiteral("show_scanline_guide")).toBool(m_retro.showScanlineGuide);
        m_retro.showTileGrid = retroObj.value(QStringLiteral("show_tile_grid")).toBool(m_retro.showTileGrid);
        m_retro.showTimelineBadges = retroObj.value(QStringLiteral("show_timeline_badges")).toBool(m_retro.showTimelineBadges);
        m_retro.showVramGauge = retroObj.value(QStringLiteral("show_vram_gauge")).toBool(m_retro.showVramGauge);
        m_retro.showAtlasGuides = retroObj.value(QStringLiteral("show_atlas_guides")).toBool(m_retro.showAtlasGuides);
    }

    emit configChanged();
    return true;
}

bool AppConfig::save(const QString &filePath) const
{
    QString path = filePath.isEmpty() ? configFilePath() : filePath;

    QFileInfo fi(path);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;

    // 0. General
    QJsonObject genObj;
    genObj[QStringLiteral("language")] = m_general.language;
    genObj[QStringLiteral("check_updates_on_startup")] = m_general.checkUpdatesOnStartup;
    genObj[QStringLiteral("reopen_last_project")] = m_general.reopenLastProject;
    root[QStringLiteral("general")] = genObj;

    // 1. Atlas
    QJsonObject atlasObj;
    atlasObj[QStringLiteral("zoom_min")] = m_atlas.zoomMin;
    atlasObj[QStringLiteral("zoom_max")] = m_atlas.zoomMax;
    atlasObj[QStringLiteral("zoom_step")] = m_atlas.zoomStep;
    atlasObj[QStringLiteral("min_slice_size")] = m_atlas.minSliceSize;
    atlasObj[QStringLiteral("default_alpha_threshold")] = m_atlas.defaultAlphaThreshold;
    atlasObj[QStringLiteral("default_vertical_tolerance")] = m_atlas.defaultVerticalTolerance;
    atlasObj[QStringLiteral("nudge_step_small")] = m_atlas.nudgeStepSmall;
    atlasObj[QStringLiteral("nudge_step_large")] = m_atlas.nudgeStepLarge;
    atlasObj[QStringLiteral("fit_view_padding")] = m_atlas.fitViewPadding;
    atlasObj[QStringLiteral("show_polygon_mesh_by_default")] = m_atlas.showPolygonMeshByDefault;
    root[QStringLiteral("atlas")] = atlasObj;

    // 2. Visuals
    QJsonObject visObj;
    visObj[QStringLiteral("handle_size")] = m_visuals.handleSize;
    visObj[QStringLiteral("handle_margin")] = m_visuals.handleMargin;
    visObj[QStringLiteral("selected_box_color")] = m_visuals.selectedBoxColor.name(QColor::HexRgb);
    visObj[QStringLiteral("unselected_box_color")] = m_visuals.unselectedBoxColor.name(QColor::HexArgb);
    visObj[QStringLiteral("selected_box_fill_color")] = m_visuals.selectedBoxFillColor.name(QColor::HexArgb);
    visObj[QStringLiteral("hovered_box_fill_color")] = m_visuals.hoveredBoxFillColor.name(QColor::HexArgb);
    visObj[QStringLiteral("marquee_color")] = m_visuals.marqueeColor.name(QColor::HexRgb);
    visObj[QStringLiteral("new_slice_preview_color")] = m_visuals.newSlicePreviewColor.name(QColor::HexRgb);
    visObj[QStringLiteral("mesh_wireframe_color")] = m_visuals.meshWireframeColor.name(QColor::HexArgb);
    root[QStringLiteral("visuals")] = visObj;

    // 3. Animation
    QJsonObject animObj;
    animObj[QStringLiteral("default_fps")] = m_animation.defaultFps;
    animObj[QStringLiteral("min_fps")] = m_animation.minFps;
    animObj[QStringLiteral("max_fps")] = m_animation.maxFps;
    animObj[QStringLiteral("auto_play_on_selection")] = m_animation.autoPlayOnSelection;
    root[QStringLiteral("animation")] = animObj;

    // 4. Project
    QJsonObject projObj;
    projObj[QStringLiteral("max_recent_files")] = m_project.maxRecentFiles;
    projObj[QStringLiteral("background_removal_tolerance")] = m_project.backgroundRemovalTolerance;
    projObj[QStringLiteral("background_min_alpha")] = m_project.backgroundMinAlpha;
    projObj[QStringLiteral("undo_limit")] = m_project.undoLimit;
    projObj[QStringLiteral("auto_save_session")] = m_project.autoSaveSession;
    root[QStringLiteral("project")] = projObj;

    // 5. Git
    QJsonObject gitObj;
    gitObj[QStringLiteral("author_name")] = m_git.authorName;
    gitObj[QStringLiteral("author_email")] = m_git.authorEmail;
    root[QStringLiteral("git")] = gitObj;

    // 6. Export
    QJsonObject expObj;
    expObj[QStringLiteral("default_format_id")] = m_export.defaultFormatId;
    expObj[QStringLiteral("default_texture_format_index")] = m_export.defaultTextureFormatIndex;
    expObj[QStringLiteral("default_algorithm_index")] = m_export.defaultAlgorithmIndex;
    expObj[QStringLiteral("default_zstd")] = m_export.defaultZstd;
    expObj[QStringLiteral("default_zstd_level")] = m_export.defaultZstdLevel;
    expObj[QStringLiteral("default_padding")] = m_export.defaultPadding;
    expObj[QStringLiteral("default_border_padding")] = m_export.defaultBorderPadding;
    expObj[QStringLiteral("default_extrude")] = m_export.defaultExtrude;
    expObj[QStringLiteral("default_max_width")] = m_export.defaultMaxWidth;
    expObj[QStringLiteral("default_max_height")] = m_export.defaultMaxHeight;
    expObj[QStringLiteral("default_power_of_two")] = m_export.defaultPowerOfTwo;
    expObj[QStringLiteral("default_force_square")] = m_export.defaultForceSquare;
    expObj[QStringLiteral("default_deduplicate")] = m_export.defaultDeduplicate;
    expObj[QStringLiteral("default_trim_sprites")] = m_export.defaultTrimSprites;
    expObj[QStringLiteral("default_normal_map_y_flip")] = m_export.defaultNormalMapYFlip;
    root[QStringLiteral("export")] = expObj;

    // 7. Retro & Hardware Constraints
    QJsonObject retroObj;
    retroObj[QStringLiteral("enabled")] = m_retro.enabled;
    retroObj[QStringLiteral("profile_index")] = m_retro.profileIndex;
    retroObj[QStringLiteral("profile_id")] = m_retro.profileId;
    retroObj[QStringLiteral("scanline_sprite_width_limit")] = m_retro.scanlineSpriteWidthLimit;
    retroObj[QStringLiteral("max_vram_tiles")] = m_retro.maxVramTiles;
    retroObj[QStringLiteral("tile_size")] = m_retro.tileSize;
    retroObj[QStringLiteral("guide_line_thickness")] = m_retro.guideLineThickness;
    retroObj[QStringLiteral("max_sprites_per_scanline")] = m_retro.maxSpritesPerScanline;
    retroObj[QStringLiteral("max_colors_per_sprite")] = m_retro.maxColorsPerSprite;
    retroObj[QStringLiteral("show_scanline_guide")] = m_retro.showScanlineGuide;
    retroObj[QStringLiteral("show_tile_grid")] = m_retro.showTileGrid;
    retroObj[QStringLiteral("show_timeline_badges")] = m_retro.showTimelineBadges;
    retroObj[QStringLiteral("show_vram_gauge")] = m_retro.showVramGauge;
    retroObj[QStringLiteral("show_atlas_guides")] = m_retro.showAtlasGuides;
    root[QStringLiteral("retro_linter")] = retroObj;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[AppConfig] Unable to open configuration file for writing:" << path;
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    emit const_cast<AppConfig*>(this)->configChanged();
    return true;
}
