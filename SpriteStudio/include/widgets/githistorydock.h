#ifndef GITHISTORYDOCK_H
#define GITHISTORYDOCK_H

#include <QDockWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QPointer>
#include "project/sessionmanager.h"

class ProjectController;

/**
 * @brief Interactive graphical item representing a single Git commit node.
 */
class GitCommitNodeItem : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT

public:
    GitCommitNodeItem(const GitCommitInfo &info, bool isHead, QGraphicsItem *parent = nullptr);

    const GitCommitInfo& commitInfo() const { return m_info; }
    bool isHead() const { return m_isHead; }
    bool isSelectedCommit() const { return m_isSelected; }
    void setSelectedCommit(bool sel);

signals:
    void commitClicked(const GitCommitInfo &info);
    void commitDoubleClicked(const GitCommitInfo &info);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    void updateVisualState();

    GitCommitInfo m_info;
    bool m_isHead = false;
    bool m_isSelected = false;
};

/**
 * @brief Custom QGraphicsView tailored for rendering the Git commit tree.
 */
class GitHistoryGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit GitHistoryGraphicsView(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
};

/**
 * @brief Dockable widget displaying the Git commit tree, revision metadata, and time-travel controls.
 */
class GitHistoryDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit GitHistoryDock(QWidget *parent = nullptr);
    ~GitHistoryDock() override = default;

    void setProjectController(ProjectController *controller);

public slots:
    void refreshHistory();
    void checkoutRevision(const QString &hash);
    void checkoutHead();

signals:
    void revisionCheckoutRequested(const QString &hash);

private slots:
    void onCommitSelected(const GitCommitInfo &info);
    void onCommitDoubleClicked(const GitCommitInfo &info);
    void onCheckoutSelectedClicked();

private:
    void setupUi();
    void buildCommitTree(const QList<GitCommitInfo> &log, const QString &headHash);
    void updateDetailsPane(const GitCommitInfo *info);

    QPointer<ProjectController> m_controller;

    // UI elements
    QToolButton *m_btnRefresh = nullptr;
    QToolButton *m_btnCheckoutHead = nullptr;
    QLabel      *m_lblStatus = nullptr;
    QLabel      *m_lblDisabledBanner = nullptr;

    GitHistoryGraphicsView *m_graphicsView = nullptr;
    QGraphicsScene         *m_scene = nullptr;

    QWidget     *m_detailsWidget = nullptr;
    QLabel      *m_lblHash = nullptr;
    QLabel      *m_lblDate = nullptr;
    QLabel      *m_lblAuthor = nullptr;
    QLabel      *m_lblMessage = nullptr;
    QPushButton *m_btnCheckoutSelected = nullptr;

    QList<GitCommitNodeItem*> m_nodeItems;
    GitCommitInfo m_selectedCommit;
    bool m_hasSelection = false;
};

#endif // GITHISTORYDOCK_H
