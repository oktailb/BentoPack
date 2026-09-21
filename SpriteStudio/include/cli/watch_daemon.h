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

#ifndef WATCH_DAEMON_H
#define WATCH_DAEMON_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QLockFile>
#include <memory>
#include <functional>
#include "cli/cliparser.h"
#include "spritestudiocore_export.h"

namespace SpriteStudioCli {

/**
 * @brief Manages background directory surveillance, debouncing,
 *        self-trigger protection, and per-directory lock isolation for spritestudio-cli.
 */
class SPRITESTUDIO_CORE_EXPORT WatchDaemon : public QObject
{
    Q_OBJECT

public:
    explicit WatchDaemon(QObject *parent = nullptr);
    ~WatchDaemon() override;

    /**
     * @brief Attempts to acquire an exclusive lock on the target directory.
     * @param directory The primary directory to be guarded.
     * @param conflictingPid Output parameter populated with the conflicting PID if locked.
     * @return true if lock was acquired successfully, false if another instance holds it.
     */
    bool acquireDirectoryLock(const QString &directory, qint64 &conflictingPid);

    /**
     * @brief Releases the directory lock.
     */
    void releaseDirectoryLock();

    /**
     * @brief Starts watching the input paths and connects the debounced re-pack callback.
     * @param inputPaths Directories and files to watch.
     * @param targetSheetPath Path to output sprite sheet (excluded from triggers).
     * @param targetDataPath Path to output metadata file (excluded from triggers).
     * @param repackCallback Lambda or function executing the actual atlas generation.
     * @param debounceMs Debounce delay in milliseconds (default: 300).
     * @param daemonMode Whether to run detached/redirected to log file.
     * @return CliResult status.
     */
    CliResult startWatching(const QStringList &inputPaths,
                            const QString &targetSheetPath,
                            const QString &targetDataPath,
                            std::function<CliResult()> repackCallback,
                            int debounceMs = 300,
                            bool daemonMode = false);

    /**
     * @brief Signals an active watch daemon on the given directory to terminate.
     * @param directory Directory containing the lockfile.
     */
    static CliResult stopWatch(const QString &directory);

    /**
     * @brief Checks if a daemon is actively running for the given directory.
     */
    static bool isDirectoryLocked(const QString &directory, qint64 &pid);

signals:
    void repackCompleted(bool success);

private slots:
    void onDirectoryChanged(const QString &path);
    void onFileChanged(const QString &path);
    void onDebounceTimeout();

private:
    void addWatchPathRecursively(const QString &path);
    bool isIgnoredPath(const QString &path) const;

    QFileSystemWatcher m_watcher;
    QTimer m_debounceTimer;
    std::unique_ptr<QLockFile> m_lockFile;
    QString m_lockedDirectory;

    QString m_targetSheetCanonical;
    QString m_targetDataCanonical;
    std::function<CliResult()> m_repackCallback;

    int m_debounceMs = 300;
    bool m_daemonMode = false;
    bool m_isRepacking = false;
};

} // namespace SpriteStudioCli

#endif // WATCH_DAEMON_H
