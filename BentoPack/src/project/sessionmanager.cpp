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

#include "include/project/sessionmanager.h"
#include "config/appconfig.h"
extern "C" {
#include "zip/miniz.h"
}
#include <QStandardPaths>
#include <QCoreApplication>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDirIterator>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/types.h>
#include <signal.h>
#endif

#ifdef HAVE_LIBGIT2
#include <git2.h>
#endif

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
    , m_pid(QCoreApplication::applicationPid())
{
}

SessionManager::~SessionManager()
{
    if (hasActiveSession()) {
        closeCurrentSession(false);
    }
}

QString SessionManager::sessionsRootPath()
{
    QString tempBase = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return QDir(tempBase).filePath(QStringLiteral("BentoPack/sessions"));
}

void SessionManager::setOriginalFilePath(const QString &path)
{
    m_originalFilePath = path;
    if (hasActiveSession()) {
        writeSessionLock(QStringLiteral("active"));
    }
}

QString SessionManager::sessionAssetsDir() const
{
    if (m_sessionDir.isEmpty()) return QString();
    return QDir(m_sessionDir).filePath(QStringLiteral("assets"));
}

QString SessionManager::sessionProjectJsonPath() const
{
    if (m_sessionDir.isEmpty()) return QString();
    return QDir(m_sessionDir).filePath(QStringLiteral("project.json"));
}

QString SessionManager::sessionAtlasImagePath() const
{
    if (m_sessionDir.isEmpty()) return QString();
    return QDir(sessionAssetsDir()).filePath(QStringLiteral("atlas.png"));
}

void SessionManager::ensureDirectoriesExist()
{
    if (m_sessionDir.isEmpty()) return;
    QDir().mkpath(m_sessionDir);
    QDir().mkpath(sessionAssetsDir());
}

bool SessionManager::startNewSession(const QString &originalFilePath)
{
    if (hasActiveSession()) {
        closeCurrentSession(false);
    }

    m_sessionUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_sessionDir = QDir(sessionsRootPath()).filePath(m_sessionUuid);
    m_originalFilePath = originalFilePath;
    m_sessionCreatedTime = QDateTime::currentDateTimeUtc();

    ensureDirectoriesExist();
    writeSessionLock(QStringLiteral("active"));

#ifdef HAVE_LIBGIT2
    gitInit();
#endif

    emit sessionStarted(m_sessionDir);
    return true;
}

bool SessionManager::openSessionFromBento(const QString &bentoPath, QString *errorMsg)
{
    if (bentoPath.isEmpty() || !QFile::exists(bentoPath)) {
        if (errorMsg) *errorMsg = tr("Project file does not exist: %1").arg(bentoPath);
        return false;
    }

    if (!startNewSession(QFileInfo(bentoPath).absoluteFilePath())) {
        if (errorMsg) *errorMsg = tr("Failed to initialize session directory.");
        return false;
    }

    if (!unpackZip(bentoPath, m_sessionDir, errorMsg)) {
        closeCurrentSession(true);
        return false;
    }

    // Rewrite lock file with updated timestamps
    writeSessionLock(QStringLiteral("active"));

#ifdef HAVE_LIBGIT2
    gitInit();
    // Only commit initial load if the repository has no previous commits (e.g. legacy archive)
    if (gitLog().isEmpty()) {
        gitCommit(QStringLiteral("Initial project load from archive"));
    }
#endif

    return true;
}

bool SessionManager::restoreOrphanSession(const QString &sessionDir, QString *errorMsg)
{
    if (!QDir(sessionDir).exists()) {
        if (errorMsg) *errorMsg = tr("Session directory does not exist: %1").arg(sessionDir);
        return false;
    }

    if (hasActiveSession()) {
        closeCurrentSession(false);
    }

    SessionLockInfo lock = readSessionLock(sessionDir);
    m_sessionDir = sessionDir;
    m_sessionUuid = lock.sessionUuid.isEmpty() ? QFileInfo(sessionDir).fileName() : lock.sessionUuid;
    m_originalFilePath = lock.originalFilePath;
    m_sessionCreatedTime = lock.created.isValid() ? lock.created : QDateTime::currentDateTimeUtc();

    ensureDirectoriesExist();

    // Take over the session lock with our own PID
    writeSessionLock(QStringLiteral("active"));

    emit sessionStarted(m_sessionDir);
    return true;
}

void SessionManager::closeCurrentSession(bool discard)
{
    if (m_sessionDir.isEmpty()) return;

    QString closingDir = m_sessionDir;

    if (!discard) {
        writeSessionLock(QStringLiteral("clean_closed"));
    }

    // Clean up temporary workspace directory on clean exit or discard
    QDir(closingDir).removeRecursively();

    m_sessionDir.clear();
    m_sessionUuid.clear();
    m_originalFilePath.clear();

    emit sessionClosed(closingDir);
}

bool SessionManager::writeSessionLock(const QString &status)
{
    if (m_sessionDir.isEmpty()) return false;

    QJsonObject obj;
    obj[QStringLiteral("pid")] = m_pid;
    obj[QStringLiteral("sessionUuid")] = m_sessionUuid;
    obj[QStringLiteral("originalFilePath")] = m_originalFilePath;
    obj[QStringLiteral("status")] = status;
    obj[QStringLiteral("created")] = m_sessionCreatedTime.toString(Qt::ISODateWithMs);
    obj[QStringLiteral("lastModified")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QFile file(QDir(m_sessionDir).filePath(QStringLiteral(".session_lock")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

SessionLockInfo SessionManager::readSessionLock(const QString &sessionDir)
{
    SessionLockInfo info;
    QFile file(QDir(sessionDir).filePath(QStringLiteral(".session_lock")));
    if (!file.open(QIODevice::ReadOnly)) {
        return info;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return info;

    QJsonObject obj = doc.object();
    info.pid = obj.value(QStringLiteral("pid")).toInteger(0);
    info.sessionUuid = obj.value(QStringLiteral("sessionUuid")).toString();
    info.originalFilePath = obj.value(QStringLiteral("originalFilePath")).toString();
    info.status = obj.value(QStringLiteral("status")).toString();
    info.created = QDateTime::fromString(obj.value(QStringLiteral("created")).toString(), Qt::ISODateWithMs);
    info.lastModified = QDateTime::fromString(obj.value(QStringLiteral("lastModified")).toString(), Qt::ISODateWithMs);
    return info;
}

bool SessionManager::isProcessAlive(qint64 pid)
{
    if (pid <= 0) return false;
    if (pid == QCoreApplication::applicationPid()) return true;

#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == NULL) {
        return false;
    }
    DWORD exitCode = 0;
    bool active = (GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE);
    CloseHandle(process);
    return active;
#else
    return (kill(static_cast<pid_t>(pid), 0) == 0);
#endif
}

QList<OrphanSessionInfo> SessionManager::detectOrphanSessions()
{
    QList<OrphanSessionInfo> result;
    QDir sessionsDir(sessionsRootPath());
    if (!sessionsDir.exists()) return result;

    const QFileInfoList subdirs = sessionsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &sub : subdirs) {
        QString sDir = sub.absoluteFilePath();
        SessionLockInfo lock = readSessionLock(sDir);

        // If no lock file exists, check if project.json exists
        bool hasProjectJson = QFile::exists(QDir(sDir).filePath(QStringLiteral("project.json")));
        if (lock.status.isEmpty() && !hasProjectJson) {
            continue;
        }

        // If status was clean_closed, remove stale folder
        if (lock.status == QStringLiteral("clean_closed")) {
            QDir(sDir).removeRecursively();
            continue;
        }

        // If PID is still active and it's not our process, it's currently used by another running instance
        if (lock.pid > 0 && lock.pid != QCoreApplication::applicationPid() && isProcessAlive(lock.pid)) {
            continue;
        }

        // Process is dead or missing, but session was left active -> Orphan/Crash detected!
        OrphanSessionInfo orphan;
        orphan.sessionDir = sDir;
        orphan.lockInfo = lock;
        orphan.projectName = lock.originalFilePath.isEmpty()
            ? tr("Unsaved Project")
            : QFileInfo(lock.originalFilePath).fileName();
        orphan.lastActivity = lock.lastModified.isValid() ? lock.lastModified : sub.lastModified();

        result.append(orphan);
    }

    return result;
}

bool SessionManager::discardOrphanSession(const QString &sessionDir)
{
    if (sessionDir.isEmpty()) return false;
    return QDir(sessionDir).removeRecursively();
}

static bool addDirectoryToZip(mz_zip_archive &zip, const QDir &dir, const QString &baseDir)
{
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &entry : entries) {
        // Do not pack temporary session lock file into .bento archive
        if (entry.fileName() == QStringLiteral(".session_lock")) {
            continue;
        }

        QString relPath = QDir(baseDir).relativeFilePath(entry.absoluteFilePath());
        relPath.replace(QLatin1Char('\\'), QLatin1Char('/'));

        // Prefix with "bento/" for clean namespacing and backward compatibility
        QString zipEntryPath = QStringLiteral("bento/") + relPath;

        if (entry.isDir()) {
            if (!zipEntryPath.endsWith(QLatin1Char('/'))) {
                zipEntryPath += QLatin1Char('/');
            }
            QByteArray utf8Path = zipEntryPath.toUtf8();
            if (!mz_zip_writer_add_mem(&zip, utf8Path.constData(), nullptr, 0, MZ_DEFAULT_COMPRESSION)) {
                return false;
            }
            if (!addDirectoryToZip(zip, QDir(entry.absoluteFilePath()), baseDir)) {
                return false;
            }
        } else if (entry.isFile()) {
            QFile file(entry.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray data = file.readAll();
                file.close();
                QByteArray utf8Path = zipEntryPath.toUtf8();
                if (!mz_zip_writer_add_mem(&zip, utf8Path.constData(), data.constData(), static_cast<size_t>(data.size()), MZ_DEFAULT_COMPRESSION)) {
                    return false;
                }
            } else {
                return false;
            }
        }
    }
    return true;
}

bool SessionManager::packZip(const QString &sourceDir, const QString &zipFilePath, QString *errorMsg)
{
    QDir src(sourceDir);
    if (!src.exists()) {
        if (errorMsg) *errorMsg = tr("Source directory does not exist: %1").arg(sourceDir);
        return false;
    }

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    QByteArray nativePath = zipFilePath.toUtf8();
    if (!mz_zip_writer_init_file(&zip, nativePath.constData(), 0)) {
        if (errorMsg) *errorMsg = tr("Cannot create ZIP file: %1").arg(zipFilePath);
        return false;
    }

    if (!addDirectoryToZip(zip, src, sourceDir)) {
        mz_zip_writer_end(&zip);
        if (errorMsg) *errorMsg = tr("Error occurred while adding files to ZIP archive: %1").arg(zipFilePath);
        return false;
    }

    if (!mz_zip_writer_finalize_archive(&zip)) {
        mz_zip_writer_end(&zip);
        if (errorMsg) *errorMsg = tr("Error occurred while writing ZIP file: %1").arg(zipFilePath);
        return false;
    }

    mz_zip_writer_end(&zip);
    return true;
}

bool SessionManager::unpackZip(const QString &zipFilePath, const QString &destDir, QString *errorMsg)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    QByteArray nativePath = zipFilePath.toUtf8();
    if (!mz_zip_reader_init_file(&zip, nativePath.constData(), 0)) {
        if (errorMsg) *errorMsg = tr("Failed to open ZIP archive: %1").arg(zipFilePath);
        return false;
    }

    const mz_uint numFiles = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < numFiles; ++i) {
        mz_zip_archive_file_stat fileStat;
        if (!mz_zip_reader_file_stat(&zip, i, &fileStat)) {
            continue;
        }

        QString normalizedPath = QString::fromUtf8(fileStat.m_filename);
        normalizedPath.replace(QLatin1Char('\\'), QLatin1Char('/'));

        // If the archive was created with the "bento/" or legacy "ssp/" prefix, strip it
        if (normalizedPath.startsWith(QStringLiteral("bento/"))) {
            normalizedPath = normalizedPath.mid(6);
        } else if (normalizedPath == QStringLiteral("bento")) {
            continue;
        } else if (normalizedPath.startsWith(QStringLiteral("ssp/"))) {
            normalizedPath = normalizedPath.mid(4);
        } else if (normalizedPath == QStringLiteral("ssp")) {
            continue;
        }

        if (normalizedPath.isEmpty()) {
            continue;
        }

        QString targetPath = QDir(destDir).filePath(normalizedPath);
        if (mz_zip_reader_is_file_a_directory(&zip, i) || normalizedPath.endsWith(QLatin1Char('/'))) {
            QDir().mkpath(targetPath);
        } else {
            QFileInfo targetInfo(targetPath);
            QDir().mkpath(targetInfo.absolutePath());

            size_t uncompSize = 0;
            void *pData = mz_zip_reader_extract_to_heap(&zip, i, &uncompSize, 0);
            if (pData) {
                QFile file(targetPath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(reinterpret_cast<const char*>(pData), static_cast<qint64>(uncompSize));
                    file.close();
                }
                mz_free(pData);
            }
        }
    }

    mz_zip_reader_end(&zip);
    return true;
}

bool SessionManager::saveSessionToBento(const QString &targetBentoPath, QString *errorMsg)
{
    if (m_sessionDir.isEmpty()) {
        if (errorMsg) *errorMsg = tr("No active session workspace to save.");
        return false;
    }

    if (targetBentoPath.isEmpty()) {
        if (errorMsg) *errorMsg = tr("Target path cannot be empty.");
        return false;
    }

    QFileInfo targetInfo(targetBentoPath);
    QDir().mkpath(targetInfo.absolutePath());

    QString tempBentoPath = targetBentoPath + QStringLiteral(".tmp");

    // Remove old temp file if it exists
    if (QFile::exists(tempBentoPath)) {
        QFile::remove(tempBentoPath);
    }

    m_originalFilePath = targetInfo.absoluteFilePath();
    writeSessionLock(QStringLiteral("active"));

#ifdef HAVE_LIBGIT2
    gitCommit(QStringLiteral("Project saved to archive: ") + targetInfo.fileName());
#endif

    // Pack scratch session directory to temp file
    if (!packZip(m_sessionDir, tempBentoPath, errorMsg)) {
        QFile::remove(tempBentoPath);
        return false;
    }

    // Atomic replacement: remove target if exists and rename temp -> target
    if (QFile::exists(targetBentoPath)) {
        if (!QFile::remove(targetBentoPath)) {
            if (errorMsg) *errorMsg = tr("Failed to overwrite existing file: %1").arg(targetBentoPath);
            QFile::remove(tempBentoPath);
            return false;
        }
    }

    if (!QFile::rename(tempBentoPath, targetBentoPath)) {
        if (errorMsg) *errorMsg = tr("Failed to atomically rename %1 to %2").arg(tempBentoPath, targetBentoPath);
        return false;
    }

    emit sessionSaved(targetBentoPath);
    return true;
}

bool SessionManager::isGitAvailable()
{
#ifdef HAVE_LIBGIT2
    return true;
#else
    return false;
#endif
}

bool SessionManager::gitInit()
{
#ifdef HAVE_LIBGIT2
    if (m_sessionDir.isEmpty()) return false;
    git_libgit2_init();

    // 1. Ensure .gitignore exists in workspace
    QString gitignorePath = QDir(m_sessionDir).filePath(QStringLiteral(".gitignore"));
    if (!QFile::exists(gitignorePath)) {
        QFile gi(gitignorePath);
        if (gi.open(QIODevice::WriteOnly | QIODevice::Text)) {
            gi.write(".session_lock\n*.tmp\n");
            gi.close();
        }
    }

    git_repository *repo = nullptr;
    if (git_repository_open(&repo, m_sessionDir.toUtf8().constData()) != 0) {
        int error = git_repository_init(&repo, m_sessionDir.toUtf8().constData(), 0);
        if (error != 0 || !repo) {
            return false;
        }
    }
    git_repository_free(repo);
    return true;
#else
    return false;
#endif
}

bool SessionManager::gitCommit(const QString &message)
{
#ifdef HAVE_LIBGIT2
    if (m_sessionDir.isEmpty()) return false;

    git_libgit2_init();
    git_repository *repo = nullptr;
    if (git_repository_open(&repo, m_sessionDir.toUtf8().constData()) != 0) {
        return false;
    }

    // Ensure .gitignore exists
    QString gitignorePath = QDir(m_sessionDir).filePath(QStringLiteral(".gitignore"));
    if (!QFile::exists(gitignorePath)) {
        QFile gi(gitignorePath);
        if (gi.open(QIODevice::WriteOnly | QIODevice::Text)) {
            gi.write(".session_lock\n*.tmp\n");
            gi.close();
        }
    }

    git_index *index = nullptr;
    if (git_repository_index(&index, repo) != 0) {
        git_repository_free(repo);
        return false;
    }

    // Add all untracked & modified files while honoring .gitignore
    git_index_add_all(index, nullptr, GIT_INDEX_ADD_DISABLE_PATHSPEC_MATCH, nullptr, nullptr);
    git_index_update_all(index, nullptr, nullptr, nullptr);
    git_index_remove_bypath(index, ".session_lock");
    git_index_write(index);

    git_oid tree_id;
    if (git_index_write_tree(&tree_id, index) != 0) {
        git_index_free(index);
        git_repository_free(repo);
        return false;
    }

    git_tree *tree = nullptr;
    if (git_tree_lookup(&tree, repo, &tree_id) != 0) {
        git_index_free(index);
        git_repository_free(repo);
        return false;
    }

    git_oid parent_id;
    git_commit *parent_commit = nullptr;
    const git_commit **parents = nullptr;
    size_t parent_count = 0;

    if (git_reference_name_to_id(&parent_id, repo, "HEAD") == 0) {
        if (git_commit_lookup(&parent_commit, repo, &parent_id) == 0) {
            const git_oid *parentTreeId = git_commit_tree_id(parent_commit);
            bool isExplicitSave = message.startsWith(QStringLiteral("Project saved"));
            if (!isExplicitSave && parentTreeId && git_oid_cmp(parentTreeId, &tree_id) == 0) {
                // Working tree has not changed since last commit
                git_commit_free(parent_commit);
                git_tree_free(tree);
                git_index_free(index);
                git_repository_free(repo);
                return true;
            }
            parents = const_cast<const git_commit**>(&parent_commit);
            parent_count = 1;
        }
    }

    git_signature *sig = nullptr;
    QString actAuthor = authorName();
    QString actEmail = authorEmail();
    if (git_signature_now(&sig, actAuthor.toUtf8().constData(), actEmail.toUtf8().constData()) != 0 || !sig) {
        git_signature_now(&sig, "BentoPack", "bentopack@local");
    }

    git_oid commit_id;
    int commitResult = git_commit_create(
        &commit_id,
        repo,
        "HEAD",
        sig,
        sig,
        "UTF-8",
        message.toUtf8().constData(),
        tree,
        parent_count,
        parents
    );

    char oidStr[GIT_OID_HEXSZ + 1];
    git_oid_tostr(oidStr, sizeof(oidStr), &commit_id);
    QString commitHashStr = QString::fromLatin1(oidStr);

    if (commitResult == 0) {
        // Create persistent branch reference for this commit to ensure branched tips remain reachable
        QString refName = QStringLiteral("refs/heads/rev_%1").arg(commitHashStr.left(12));
        git_reference *branchRef = nullptr;
        git_reference_create(&branchRef, repo, refName.toUtf8().constData(), &commit_id, 1, "Session commit tracking");
        if (branchRef) {
            git_reference_free(branchRef);
        }
    }

    if (parent_commit) git_commit_free(parent_commit);
    git_signature_free(sig);
    git_tree_free(tree);
    git_index_free(index);
    git_repository_free(repo);

    if (commitResult == 0) {
        emit gitCommitted(commitHashStr, message);
        return true;
    }
    return false;
#else
    Q_UNUSED(message);
    return false;
#endif
}

QList<GitCommitInfo> SessionManager::gitLog() const
{
    QList<GitCommitInfo> result;
#ifdef HAVE_LIBGIT2
    if (m_sessionDir.isEmpty()) return result;

    git_libgit2_init();
    git_repository *repo = nullptr;
    if (git_repository_open(&repo, m_sessionDir.toUtf8().constData()) != 0) {
        return result;
    }

    git_revwalk *walker = nullptr;
    if (git_revwalk_new(&walker, repo) != 0) {
        git_repository_free(repo);
        return result;
    }

    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
    bool pushed = false;
    if (git_revwalk_push_head(walker) == 0) pushed = true;
    if (git_revwalk_push_glob(walker, "refs/*") == 0) pushed = true;

    if (!pushed) {
        git_revwalk_free(walker);
        git_repository_free(repo);
        return result;
    }

    git_oid oid;
    while (git_revwalk_next(&oid, walker) == 0) {
        git_commit *commit = nullptr;
        if (git_commit_lookup(&commit, repo, &oid) == 0) {
            GitCommitInfo info;
            char buf[GIT_OID_HEXSZ + 1];
            git_oid_tostr(buf, sizeof(buf), &oid);
            info.hash = QString::fromLatin1(buf);
            info.shortHash = info.hash.left(7);

            const git_signature *author = git_commit_author(commit);
            if (author) {
                info.author = QString::fromUtf8(author->name);
                info.email = QString::fromUtf8(author->email);
                info.timestamp = QDateTime::fromSecsSinceEpoch(author->when.time);
            }

            const char *msg = git_commit_message(commit);
            if (msg) {
                info.message = QString::fromUtf8(msg).trimmed();
            }

            unsigned int parentCount = git_commit_parentcount(commit);
            for (unsigned int p = 0; p < parentCount; ++p) {
                const git_oid *parentId = git_commit_parent_id(commit, p);
                if (parentId) {
                    char pBuf[GIT_OID_HEXSZ + 1];
                    git_oid_tostr(pBuf, sizeof(pBuf), parentId);
                    info.parentHashes.append(QString::fromLatin1(pBuf));
                }
            }

            result.append(info);
            git_commit_free(commit);
        }
    }

    git_revwalk_free(walker);
    git_repository_free(repo);
#endif
    return result;
}

QString SessionManager::gitHeadCommitHash() const
{
#ifdef HAVE_LIBGIT2
    if (m_sessionDir.isEmpty()) return QString();

    git_libgit2_init();
    git_repository *repo = nullptr;
    if (git_repository_open(&repo, m_sessionDir.toUtf8().constData()) != 0) {
        return QString();
    }

    git_oid headOid;
    QString result;
    if (git_reference_name_to_id(&headOid, repo, "HEAD") == 0) {
        char buf[GIT_OID_HEXSZ + 1];
        git_oid_tostr(buf, sizeof(buf), &headOid);
        result = QString::fromLatin1(buf);
    }

    git_repository_free(repo);
    return result;
#else
    return QString();
#endif
}

bool SessionManager::gitCheckout(const QString &commitHash, QString *errorMsg)
{
#ifdef HAVE_LIBGIT2
    if (m_sessionDir.isEmpty()) {
        if (errorMsg) *errorMsg = tr("No active session workspace.");
        return false;
    }

    git_libgit2_init();
    git_repository *repo = nullptr;
    if (git_repository_open(&repo, m_sessionDir.toUtf8().constData()) != 0) {
        if (errorMsg) *errorMsg = tr("Failed to open repository.");
        return false;
    }

    git_oid oid;
    if (git_oid_fromstr(&oid, commitHash.toUtf8().constData()) != 0) {
        if (errorMsg) *errorMsg = tr("Invalid commit hash: %1").arg(commitHash);
        git_repository_free(repo);
        return false;
    }

    git_commit *commit = nullptr;
    if (git_commit_lookup(&commit, repo, &oid) != 0) {
        if (errorMsg) *errorMsg = tr("Commit not found: %1").arg(commitHash);
        git_repository_free(repo);
        return false;
    }

    if (git_repository_set_head_detached(repo, &oid) != 0) {
        if (errorMsg) *errorMsg = tr("Failed to set detached HEAD to %1").arg(commitHash);
        git_commit_free(commit);
        git_repository_free(repo);
        return false;
    }

    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_FORCE;
    int checkoutErr = git_checkout_tree(repo, reinterpret_cast<const git_object*>(commit), &opts);

    git_commit_free(commit);
    git_repository_free(repo);

    if (checkoutErr != 0) {
        if (errorMsg) *errorMsg = tr("Checkout tree failed with error code: %1").arg(checkoutErr);
        return false;
    }

    return true;
#else
    Q_UNUSED(commitHash);
    if (errorMsg) *errorMsg = tr("Git integration is not compiled in.");
    return false;
#endif
}

QList<GitCommitInfo> SessionManager::gitChildrenOf(const QString &parentHash) const
{
    QList<GitCommitInfo> children;
    if (parentHash.isEmpty()) return children;

    const QList<GitCommitInfo> log = gitLog();
    for (const GitCommitInfo &info : log) {
        if (info.parentHashes.contains(parentHash)) {
            children.append(info);
        }
    }
    return children;
}

QString SessionManager::gitParentCommitHash(const QString &commitHash) const
{
    if (commitHash.isEmpty()) return QString();

    const QList<GitCommitInfo> log = gitLog();
    for (const GitCommitInfo &info : log) {
        if (info.hash == commitHash && !info.parentHashes.isEmpty()) {
            return info.parentHashes.first();
        }
    }
    return QString();
}

void SessionManager::setAuthorIdentity(const QString &name, const QString &email)
{
    m_authorName = name;
    m_authorEmail = email;
}

QString SessionManager::authorName() const
{
    if (!m_authorName.isEmpty()) return m_authorName;
    QString cfgName = AppConfig::instance().git().authorName.trimmed();
    if (!cfgName.isEmpty()) return cfgName;
    QString detectedName;
    GitConfig::detectSystemIdentity(&detectedName, nullptr);
    return detectedName.isEmpty() ? QStringLiteral("BentoPack") : detectedName;
}

QString SessionManager::authorEmail() const
{
    if (!m_authorEmail.isEmpty()) return m_authorEmail;
    QString cfgEmail = AppConfig::instance().git().authorEmail.trimmed();
    if (!cfgEmail.isEmpty()) return cfgEmail;
    QString detectedEmail;
    GitConfig::detectSystemIdentity(nullptr, &detectedEmail);
    return detectedEmail.isEmpty() ? QStringLiteral("bentopack@local") : detectedEmail;
}

