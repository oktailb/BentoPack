#ifndef PROJECTCONTROLLER_H
#define PROJECTCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QImage>
#include <QFutureWatcher>
#include "extractor/export.h"
#include "model/spritedocument.h"
#include "project/sessionmanager.h"

class SpriteDocument;
class QUndoStack;
class QWidget;

struct AsyncExtractionResult {
    enum JobType { JobOpen, JobRemoveBackground };
    JobType type = JobOpen;
    QImage atlas;
    QList<QImage> frameImages;
    QList<SpriteBox> boxes;
    QString filePath;
    bool success = false;
    QString errorMessage;
};

/**
 * @brief Controller managing project lifecycle, I/O operations, recent files, and image processing.
 */
class ProjectController : public QObject
{
    Q_OBJECT

public:
    explicit ProjectController(SpriteDocument *document, QUndoStack *undoStack = nullptr, QObject *parent = nullptr);
    ~ProjectController() override;

    QString currentFilePath() const;
    void setCurrentFilePath(const QString &filePath);

    // Native .ssp Project Management
    bool newProject();
    bool openProject(const QString &sspPath, QString *errorMsg = nullptr);
    bool saveProject(const QString &sspPath = QString(), QString *errorMsg = nullptr);
    bool saveProjectAs(const QString &sspPath, QString *errorMsg = nullptr);
    bool restoreSession(const QString &sessionDir, QString *errorMsg = nullptr);
    bool checkoutRevision(const QString &commitHash, QString *errorMsg = nullptr);

    bool canUndoGit() const;
    bool canRedoGit() const;
    bool hasMultipleRedoBranches() const;
    QList<GitCommitInfo> redoBranches() const;
    bool undoGit();
    bool redoGit(const QString &targetCommitHash = QString());
    bool promptAndRedoGit(QWidget *parent = nullptr);

    bool isProjectModified() const { return m_isModified; }
    void setProjectModified(bool modified);

    QString currentProjectPath() const;
    QString currentProjectName() const;
    class SessionManager* sessionManager() const { return m_sessionManager.get(); }

    QStringList recentProjects() const;
    void addRecentProject(const QString &filePath);
    void clearRecentProjects();

    bool openFile(const QString &filePath, QString *errorMsg = nullptr);
    void openFileAsync(const QString &filePath);

    bool save(const QString &filePath, QString *errorMsg = nullptr);
    bool exportData(const QString &filePath, const ExportOptions &options = ExportOptions{}, QString *errorMsg = nullptr);

    QStringList recentFiles() const;
    void addRecentFile(const QString &filePath);
    void clearRecentFiles();

    /**
     * @brief Detects the dominant background color in the given image.
     */
    static QRgb detectDominantBackgroundColor(const QImage &image, int minAlpha = -1);

    /**
     * @brief Detects the dominant background color and turns matching pixels transparent.
     * @param image Input image.
     * @param tolerance Color difference tolerance (0-255).
     * @return Processed image with transparent background.
     */
    static QImage removeBackgroundFromImage(const QImage &image, int tolerance = -1);

    /**
     * @brief Removes the background from the document's atlas and re-extracts sprites.
     */
    bool removeAtlasBackgroundAndRefresh(int alphaThreshold = 10,
                                         int verticalTolerance = 5,
                                         bool smartCrop = false,
                                         double overlapThreshold = 0.5);

    void removeAtlasBackgroundAndRefreshAsync(int alphaThreshold = 10,
                                             int verticalTolerance = 5,
                                             bool smartCrop = false,
                                             double overlapThreshold = 0.5);

    QMap<int, QString> undoCommitHistory() const { return m_undoCommitHistory; }

signals:
    void fileLoaded(const QString &filePath);
    void fileLoadError(const QString &filePath, const QString &errorMessage);
    void fileSaved(const QString &filePath);
    void projectLoaded(const QString &sspPath);
    void projectSaved(const QString &sspPath);
    void projectModifiedChanged(bool modified);
    void projectHistoryChanged();
    void recentFilesChanged(const QStringList &files);
    void recentProjectsChanged(const QStringList &projects);
    void statusMessage(const QString &message);
    void progressChanged(int percent);
    void backgroundRemoved();
    void processingStarted();
    void processingFinished();

private slots:
    void onAsyncJobFinished();
    void onUndoStackIndexChanged(int idx);

private:
    SpriteDocument *m_document;
    QUndoStack *m_undoStack;
    QString m_currentFilePath;
    QString m_currentProjectPath;
    std::unique_ptr<class SessionManager> m_sessionManager;
    bool m_isModified = false;
    bool m_isProjectLoading = false;
    int  m_lastUndoIndex = 0;
    QMap<int, QString> m_undoCommitHistory;
    QMap<int, const class QUndoCommand*> m_undoCommands;
    QFutureWatcher<AsyncExtractionResult> m_watcher;
    bool m_isProcessing = false;
};

#endif // PROJECTCONTROLLER_H
