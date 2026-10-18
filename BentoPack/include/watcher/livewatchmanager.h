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

#ifndef LIVEWATCHMANAGER_H
#define LIVEWATCHMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QSettings>
#include "extractor/export.h"
#include "bentopackcore_export.h"

enum class WatchSourceType {
    SingleFile = 0,
    Directory = 1
};

enum class WatchState {
    Idle = 0,       // Stopped / Not watching
    Watching = 1,   // Active surveillance
    Compiling = 2,  // Change detected, reloading and exporting in background
    Success = 3,    // Last reload and export succeeded
    Error = 4       // Error occurred
};

struct BENTOPACK_CORE_EXPORT LiveWatchConfig {
    WatchSourceType sourceType = WatchSourceType::SingleFile;
    QString sourcePath;
    QString fileFilters = QStringLiteral("*.ase;*.aseprite;*.png");
    int debounceMs = 250;

    bool autoExport = true;
    QString outputDirectory;
    QString outputBaseName;
    QString engineFormatId = QStringLiteral("godot"); // Extractor plugin ID (e.g. "godot", "unity", "unreal", "json")

    ExportOptions exportOptions;

    // Computed path preview
    QString computedExportPath() const;

    // Serialization
    void save(QSettings &settings) const;
    void load(QSettings &settings);

    // Factory with sensible defaults
    static LiveWatchConfig createDefault(const QString &sourceFilePath = QString());
};

class BENTOPACK_CORE_EXPORT LiveWatchManager : public QObject {
    Q_OBJECT

public:
    explicit LiveWatchManager(QObject *parent = nullptr);
    ~LiveWatchManager() override;

    bool startWatching(const LiveWatchConfig &config, QString *errorMsg = nullptr);
    void stopWatching();
    bool isWatching() const { return m_isWatching; }

    WatchState state() const { return m_state; }
    QString lastStatusMessage() const { return m_lastMessage; }
    const LiveWatchConfig& config() const { return m_config; }

    void updateConfig(const LiveWatchConfig &config);
    void triggerManualReload();

    // Verification that file is not being written to or locked
    static bool verifyFileReady(const QString &filePath, int maxRetries = 6, int retryDelayMs = 50);

signals:
    void stateChanged(WatchState state, const QString &message);
    void fileReloaded(const QString &filePath);
    void exportCompleted(bool success, const QString &exportPath, const QString &message);

private slots:
    void onDirectoryChanged(const QString &path);
    void onFileChanged(const QString &path);
    void onDebounceTimeout();

private:
    void setState(WatchState state, const QString &msg = QString());
    void executeReloadAndExport(const QString &targetFile);
    bool isIgnoredFile(const QString &path) const;
    void registerPathsRecursively(const QString &path);

    LiveWatchConfig m_config;
    bool m_isWatching = false;
    WatchState m_state = WatchState::Idle;
    QString m_lastMessage;

    QFileSystemWatcher m_watcher;
    QTimer m_debounceTimer;
    QString m_pendingChangedFile;
    bool m_isProcessing = false;
    QString m_targetExportCanonical;
};

#endif // LIVEWATCHMANAGER_H
