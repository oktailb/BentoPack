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

#include "cli/watch_daemon.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>
#include <QTextStream>
#include <QElapsedTimer>
#include <QFile>

#if defined(Q_OS_WIN)
#include <windows.h>
#else
#include <signal.h>
#endif

namespace SpriteStudioCli {

WatchDaemon::WatchDaemon(QObject *parent)
    : QObject(parent)
{
    m_debounceTimer.setSingleShot(true);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &WatchDaemon::onDirectoryChanged);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &WatchDaemon::onFileChanged);
    connect(&m_debounceTimer, &QTimer::timeout, this, &WatchDaemon::onDebounceTimeout);
}

WatchDaemon::~WatchDaemon()
{
    releaseDirectoryLock();
}

bool WatchDaemon::acquireDirectoryLock(const QString &directory, qint64 &conflictingPid)
{
    conflictingPid = 0;
    QDir dir(directory);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QString lockPath = dir.filePath(QStringLiteral(".spritestudio-watch.lock"));
    m_lockFile = std::make_unique<QLockFile>(lockPath);
    m_lockFile->setStaleLockTime(30000); // 30 seconds stale lock threshold

    if (!m_lockFile->tryLock(150)) {
        QString host, app;
        m_lockFile->getLockInfo(&conflictingPid, &host, &app);
        return false;
    }

    m_lockedDirectory = dir.absolutePath();

    // Store PID file for quick querying and stop command
    QString pidPath = dir.filePath(QStringLiteral(".spritestudio-watch.pid"));
    QFile pidFile(pidPath);
    if (pidFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&pidFile);
        out << QCoreApplication::applicationPid() << "\n";
        pidFile.close();
    }

    return true;
}

void WatchDaemon::releaseDirectoryLock()
{
    if (m_lockFile && m_lockFile->isLocked()) {
        m_lockFile->unlock();
    }
    m_lockFile.reset();

    if (!m_lockedDirectory.isEmpty()) {
        QDir dir(m_lockedDirectory);
        dir.remove(QStringLiteral(".spritestudio-watch.pid"));
        dir.remove(QStringLiteral(".spritestudio-watch.lock"));
        m_lockedDirectory.clear();
    }
}

bool WatchDaemon::isDirectoryLocked(const QString &directory, qint64 &pid)
{
    pid = 0;
    QDir dir(directory);
    QString lockPath = dir.filePath(QStringLiteral(".spritestudio-watch.lock"));
    QString pidPath = dir.filePath(QStringLiteral(".spritestudio-watch.pid"));

    QLockFile testLock(lockPath);
    QString host, app;
    if (testLock.getLockInfo(&pid, &host, &app)) {
        return true;
    }

    if (QFile::exists(pidPath)) {
        QFile f(pidPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            in >> pid;
            return pid > 0;
        }
    }
    return false;
}

CliResult WatchDaemon::stopWatch(const QString &directory)
{
    QDir dir(directory);
    QString pidPath = dir.filePath(QStringLiteral(".spritestudio-watch.pid"));
    if (!QFile::exists(pidPath)) {
        return CliResult::error(ExitFileNotFound,
            QStringLiteral("No active watch daemon PID file found in directory: %1").arg(dir.absolutePath()));
    }

    QFile pidFile(pidPath);
    qint64 targetPid = 0;
    if (pidFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&pidFile);
        in >> targetPid;
        pidFile.close();
    }

    if (targetPid <= 0) {
        return CliResult::error(ExitIoError, QStringLiteral("Invalid PID in %1").arg(pidPath));
    }

    bool killed = false;
#if defined(Q_OS_WIN)
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(targetPid));
    if (hProcess != NULL) {
        killed = TerminateProcess(hProcess, 0) != 0;
        CloseHandle(hProcess);
    }
#else
    killed = (kill(static_cast<pid_t>(targetPid), SIGTERM) == 0);
#endif

    dir.remove(QStringLiteral(".spritestudio-watch.pid"));
    dir.remove(QStringLiteral(".spritestudio-watch.lock"));

    if (killed) {
        return CliResult::success(QStringLiteral("Successfully terminated watch daemon (PID %1) for %2")
            .arg(targetPid).arg(dir.absolutePath()));
    } else {
        return CliResult::success(QStringLiteral("Watch daemon process %1 was not active. Cleaned up lock files.")
            .arg(targetPid));
    }
}

bool WatchDaemon::isIgnoredPath(const QString &path) const
{
    if (path.isEmpty()) return true;

    QFileInfo fi(path);
    QString cleanPath = fi.canonicalFilePath();
    if (cleanPath.isEmpty()) {
        cleanPath = fi.absoluteFilePath();
    }

    // Ignore target output files to prevent infinite feedback loops!
    if (!m_targetSheetCanonical.isEmpty() && cleanPath == m_targetSheetCanonical) {
        return true;
    }
    if (!m_targetDataCanonical.isEmpty() && cleanPath == m_targetDataCanonical) {
        return true;
    }

    // Ignore locks, pids and temporary editor files
    QString fn = fi.fileName();
    if (fn.startsWith(QStringLiteral(".spritestudio-watch")) ||
        fn.startsWith(QStringLiteral(".")) ||
        fn.endsWith(QStringLiteral("~")) ||
        fn.endsWith(QStringLiteral(".tmp")) ||
        fn.endsWith(QStringLiteral(".swp"))) {
        return true;
    }

    return false;
}

void WatchDaemon::addWatchPathRecursively(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists()) return;

    if (fi.isDir()) {
        QString absDir = fi.canonicalFilePath();
        if (absDir.isEmpty()) absDir = fi.absoluteFilePath();

        if (!m_watcher.directories().contains(absDir)) {
            m_watcher.addPath(absDir);
        }

        // Add subdirectories and PNG/JPG images
        QDirIterator it(absDir, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString sub = it.next();
            QFileInfo sfi(sub);
            if (sfi.isDir()) {
                QString c = sfi.canonicalFilePath();
                if (!c.isEmpty() && !m_watcher.directories().contains(c) && !sfi.fileName().startsWith(QStringLiteral("."))) {
                    m_watcher.addPath(c);
                }
            } else if (sfi.isFile()) {
                QString ext = sfi.suffix().toLower();
                if (ext == QStringLiteral("png") || ext == QStringLiteral("jpg") ||
                    ext == QStringLiteral("jpeg") || ext == QStringLiteral("gif") ||
                    ext == QStringLiteral("ssp") || ext == QStringLiteral("json")) {
                    QString c = sfi.canonicalFilePath();
                    if (!c.isEmpty() && !m_watcher.files().contains(c)) {
                        m_watcher.addPath(c);
                    }
                }
            }
        }
    } else if (fi.isFile()) {
        QString absFile = fi.canonicalFilePath();
        if (absFile.isEmpty()) absFile = fi.absoluteFilePath();
        if (!m_watcher.files().contains(absFile)) {
            m_watcher.addPath(absFile);
        }
    }
}

CliResult WatchDaemon::startWatching(const QStringList &inputPaths,
                                     const QString &targetSheetPath,
                                     const QString &targetDataPath,
                                     std::function<CliResult()> repackCallback,
                                     int debounceMs,
                                     bool daemonMode)
{
    if (inputPaths.isEmpty()) {
        return CliResult::error(ExitFileNotFound, QStringLiteral("No input paths provided for watch mode."));
    }

    m_repackCallback = std::move(repackCallback);
    m_debounceMs = (debounceMs > 0) ? debounceMs : 300;
    m_debounceTimer.setInterval(m_debounceMs);
    m_daemonMode = daemonMode;

    // Determine primary directory to lock
    QString primaryDir;
    QFileInfo firstFi(inputPaths.first());
    if (firstFi.isDir()) {
        primaryDir = firstFi.canonicalFilePath();
    } else {
        primaryDir = firstFi.dir().canonicalPath();
    }
    if (primaryDir.isEmpty()) {
        primaryDir = QDir::currentPath();
    }

    // Attempt exclusive directory lock
    qint64 conflictingPid = 0;
    if (!acquireDirectoryLock(primaryDir, conflictingPid)) {
        return CliResult::error(ExitLockConflict,
            QStringLiteral("Lock conflict: Another spritestudio-cli watch daemon is already running for directory: %1 (PID: %2).")
            .arg(primaryDir).arg(conflictingPid));
    }

    // Save canonical paths of targets for self-trigger suppression
    if (!targetSheetPath.isEmpty()) {
        m_targetSheetCanonical = QFileInfo(targetSheetPath).canonicalFilePath();
    }
    if (!targetDataPath.isEmpty()) {
        m_targetDataCanonical = QFileInfo(targetDataPath).canonicalFilePath();
    }

    // Populate paths into QFileSystemWatcher
    for (const QString &p : inputPaths) {
        addWatchPathRecursively(p);
    }

    QTextStream out(stdout);
    out << QStringLiteral("[WATCH] Daemon initialized on %1 (PID: %2)\n")
           .arg(primaryDir).arg(QCoreApplication::applicationPid());
    out << QStringLiteral("[WATCH] Monitoring %1 directories and %2 files (debounce: %3 ms)\n")
           .arg(m_watcher.directories().size()).arg(m_watcher.files().size()).arg(m_debounceMs);
    out.flush();

    // Execute initial pack
    out << QStringLiteral("[WATCH] Performing initial atlas generation...\n");
    out.flush();
    CliResult initRes = m_repackCallback();
    if (initRes.exitCode == ExitSuccess) {
        out << QStringLiteral("[WATCH] Initial pack complete: %1\n").arg(initRes.message);
    } else {
        out << QStringLiteral("[WATCH] Warning: Initial pack returned code %1: %2\n")
               .arg(initRes.exitCode).arg(initRes.message);
    }
    out.flush();

    // Re-resolve canonical output targets now that files exist
    if (!targetSheetPath.isEmpty()) {
        m_targetSheetCanonical = QFileInfo(targetSheetPath).canonicalFilePath();
    }
    if (!targetDataPath.isEmpty()) {
        m_targetDataCanonical = QFileInfo(targetDataPath).canonicalFilePath();
    }

    return CliResult::success();
}

void WatchDaemon::onDirectoryChanged(const QString &path)
{
    if (m_isRepacking || isIgnoredPath(path)) return;

    // Scan directory for new subdirectories or new images to watch
    addWatchPathRecursively(path);

    m_debounceTimer.start();
}

void WatchDaemon::onFileChanged(const QString &path)
{
    if (m_isRepacking || isIgnoredPath(path)) return;

    // Qt filesystem watcher can remove file on atomic rewrite, re-add it
    if (QFile::exists(path) && !m_watcher.files().contains(path)) {
        m_watcher.addPath(path);
    }

    m_debounceTimer.start();
}

void WatchDaemon::onDebounceTimeout()
{
    if (m_isRepacking) return;
    m_isRepacking = true;

    QElapsedTimer timer;
    timer.start();

    QTextStream out(stdout);
    out << QStringLiteral("[WATCH] Change detected. Re-packing atlas...\n");
    out.flush();

    CliResult res;
    if (m_repackCallback) {
        res = m_repackCallback();
    }

    qint64 elapsed = timer.elapsed();
    if (res.exitCode == ExitSuccess) {
        out << QStringLiteral("[WATCH] Re-pack complete in %1 ms. %2\n").arg(elapsed).arg(res.message);
    } else {
        out << QStringLiteral("[WATCH] Re-pack failed in %1 ms: %2\n").arg(elapsed).arg(res.message);
    }
    out.flush();

    m_isRepacking = false;
    emit repackCompleted(res.exitCode == ExitSuccess);
}

} // namespace SpriteStudioCli
