#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QDir>
#include <QFileInfo>

/**
 * @brief Metadata stored in the .session_lock JSON file within each session workspace.
 */
struct SessionLockInfo {
    qint64    pid = 0;
    QString   sessionUuid;
    QString   originalFilePath;
    QString   status; // "active", "clean_closed"
    QDateTime created;
    QDateTime lastSaved;
    QDateTime lastModified;
};

/**
 * @brief Information about an interrupted / crashed session detected at startup.
 */
struct OrphanSessionInfo {
    QString         sessionDir;
    SessionLockInfo lockInfo;
    QString         projectName;
    QDateTime       lastActivity;
};

/**
 * @brief Represents a Git commit record in the session repository.
 */
struct GitCommitInfo {
    QString     hash;
    QString     shortHash;
    QString     author;
    QString     email;
    QDateTime   timestamp;
    QString     message;
    QStringList parentHashes;
};

/**
 * @brief Manages the on-disk scratch workspace (%TEMP%/SpriteStudio/sessions/<session_uuid>/),
 * session locking, crash recovery, atomic ZIP (.ssp) packing/unpacking, and Git tracking.
 */
class SessionManager : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager() override;

    // Session status
    bool hasActiveSession() const { return !m_sessionDir.isEmpty(); }
    QString currentSessionDir() const { return m_sessionDir; }
    QString currentSessionUuid() const { return m_sessionUuid; }
    QString currentOriginalFilePath() const { return m_originalFilePath; }
    void setOriginalFilePath(const QString &path);

    // Scratch workspace paths
    QString sessionAssetsDir() const;
    QString sessionProjectJsonPath() const;
    QString sessionAtlasImagePath() const;

    // Session lifecycle
    bool startNewSession(const QString &originalFilePath = QString());
    bool openSessionFromSsp(const QString &sspPath, QString *errorMsg = nullptr);
    bool restoreOrphanSession(const QString &sessionDir, QString *errorMsg = nullptr);
    void closeCurrentSession(bool discard = false);

    // Lock file maintenance
    bool writeSessionLock(const QString &status = QStringLiteral("active"));
    static SessionLockInfo readSessionLock(const QString &sessionDir);

    // Atomic Save / ZIP packing
    bool saveSessionToSsp(const QString &targetSspPath, QString *errorMsg = nullptr);

    // Crash detection & orphan cleanup
    static QString sessionsRootPath();
    static QList<OrphanSessionInfo> detectOrphanSessions();
    static bool discardOrphanSession(const QString &sessionDir);
    static bool isProcessAlive(qint64 pid);

    // ZIP helpers using Qt private QZipReader / QZipWriter
    static bool unpackZip(const QString &zipFilePath, const QString &destDir, QString *errorMsg = nullptr);
    static bool packZip(const QString &sourceDir, const QString &zipFilePath, QString *errorMsg = nullptr);

    // Embedded Git versioning (graceful fallback if libgit2 is not compiled in)
    bool gitInit();
    bool gitCommit(const QString &message);
    QList<GitCommitInfo> gitLog() const;
    QString gitHeadCommitHash() const;
    bool gitCheckout(const QString &commitHash, QString *errorMsg = nullptr);
    static bool isGitAvailable();

    // Git author identity
    void setAuthorIdentity(const QString &name, const QString &email);
    QString authorName() const;
    QString authorEmail() const;

signals:
    void sessionStarted(const QString &sessionDir);
    void sessionClosed(const QString &sessionDir);
    void sessionSaved(const QString &sspPath);
    void gitCommitted(const QString &commitHash, const QString &message);

private:
    void ensureDirectoriesExist();

    QString m_sessionUuid;
    QString m_sessionDir;
    QString m_originalFilePath;
    qint64  m_pid = 0;
    QDateTime m_sessionCreatedTime;
    QString m_authorName;
    QString m_authorEmail;
};

#endif // SESSIONMANAGER_H
