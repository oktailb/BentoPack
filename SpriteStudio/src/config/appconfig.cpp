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
            foundName = QStringLiteral("SpriteStudio");
        }
    }

    if (foundEmail.isEmpty()) {
        foundEmail = qEnvironmentVariable("GIT_AUTHOR_EMAIL");
        if (foundEmail.isEmpty()) {
            foundEmail = QStringLiteral("spritestudio@local");
        }
    }

    if (name) *name = foundName;
    if (email) *email = foundEmail;
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
    // Windows: %APPDATA%/SpriteStudio/spritestudio_config.json
    // Linux:   ~/.config/SpriteStudio/spritestudio_config.json
    // macOS:   ~/Library/Application Support/SpriteStudio/spritestudio_config.json
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QDir::homePath() + QStringLiteral("/.spritestudio");
    }
    return QDir(configDir).filePath(QStringLiteral("spritestudio_config.json"));
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
    }

    // 5. Git section
    if (root.contains(QStringLiteral("git")) && root.value(QStringLiteral("git")).isObject()) {
        QJsonObject gitObj = root.value(QStringLiteral("git")).toObject();
        m_git.authorName = gitObj.value(QStringLiteral("author_name")).toString(m_git.authorName);
        m_git.authorEmail = gitObj.value(QStringLiteral("author_email")).toString(m_git.authorEmail);
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
    root[QStringLiteral("project")] = projObj;

    // 5. Git
    QJsonObject gitObj;
    gitObj[QStringLiteral("author_name")] = m_git.authorName;
    gitObj[QStringLiteral("author_email")] = m_git.authorEmail;
    root[QStringLiteral("git")] = gitObj;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[AppConfig] Unable to open configuration file for writing:" << path;
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}
