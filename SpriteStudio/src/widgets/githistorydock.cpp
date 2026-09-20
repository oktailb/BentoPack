#include "include/widgets/githistorydock.h"
#include "include/controller/projectcontroller.h"
#include <QEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QStyle>
#include <QApplication>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QCursor>
#include <QDateTime>
#include <QToolTip>

// ============================================================================
// GitCommitNodeItem Implementation
// ============================================================================

GitCommitNodeItem::GitCommitNodeItem(const GitCommitInfo &info, bool isHead, const QColor &branchColor, QGraphicsItem *parent)
    : QObject()
    , QGraphicsEllipseItem(parent)
    , m_info(info)
    , m_isHead(isHead)
    , m_branchColor(branchColor)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);

    // Set tooltip with rich information
    QString tip = QStringLiteral(
        "<b>%1</b> %2<br/>"
        "<b>%3:</b> %4<br/>"
        "<b>%5:</b> %6<br/>"
        "<b>%7:</b> %8<br/><br/>"
        "<i>%9</i>"
    ).arg(
        m_info.shortHash,
        m_isHead ? QStringLiteral("<b style='color:#2ecc71;'>(HEAD)</b>") : QString(),
        tr("KEY_GIT_TIP_MSG"), m_info.message.toHtmlEscaped(),
        tr("KEY_GIT_TIP_AUTHOR"), m_info.author.toHtmlEscaped(),
        tr("KEY_GIT_TIP_DATE"), m_info.timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
        tr("KEY_GIT_TIP_RESTORE_HINT")
    );
    setToolTip(tip);

    updateVisualState();
}

void GitCommitNodeItem::setSelectedCommit(bool sel)
{
    if (m_isSelected != sel) {
        m_isSelected = sel;
        updateVisualState();
    }
}

void GitCommitNodeItem::updateVisualState()
{
    const qreal r = m_isSelected ? 8.0 : 7.0;
    setRect(-r, -r, 2 * r, 2 * r);

    QColor fillColor;
    QColor strokeColor;

    if (m_isHead) {
        fillColor = QColor(QStringLiteral("#2ecc71"));    // Emerald green
        strokeColor = QColor(QStringLiteral("#27ae60"));
    } else if (m_branchColor.isValid()) {
        fillColor = m_branchColor;
        strokeColor = m_branchColor.darker(120);
    } else {
        fillColor = QColor(QStringLiteral("#3498db"));    // Slate blue
        strokeColor = QColor(QStringLiteral("#2980b9"));
    }

    if (m_isSelected) {
        setPen(QPen(QColor(QStringLiteral("#f39c12")), 3.0)); // Amber border
        setBrush(QBrush(fillColor));
    } else {
        setPen(QPen(strokeColor, 1.5));
        setBrush(QBrush(fillColor));
    }
}

void GitCommitNodeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit commitClicked(m_info);
        event->accept();
        return;
    }
    QGraphicsEllipseItem::mousePressEvent(event);
}

void GitCommitNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit commitDoubleClicked(m_info);
        event->accept();
        return;
    }
    QGraphicsEllipseItem::mouseDoubleClickEvent(event);
}

void GitCommitNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    const qreal r = 9.0;
    setRect(-r, -r, 2 * r, 2 * r);
    setPen(QPen(QColor(QStringLiteral("#f1c40f")), 2.5));
    QGraphicsEllipseItem::hoverEnterEvent(event);
}

void GitCommitNodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    updateVisualState();
    QGraphicsEllipseItem::hoverLeaveEvent(event);
}

// ============================================================================
// GitHistoryGraphicsView Implementation
// ============================================================================

GitHistoryGraphicsView::GitHistoryGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
{
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameShape(QFrame::NoFrame);
}

void GitHistoryGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
}

// ============================================================================
// GitHistoryDock Implementation
// ============================================================================

GitHistoryDock::GitHistoryDock(QWidget *parent)
    : QDockWidget(parent)
{
    setWindowTitle(tr("KEY_DOCK_GIT_HISTORY"));
    setObjectName(QStringLiteral("gitHistoryDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

    setupUi();
}

void GitHistoryDock::setupUi()
{
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // 1. Top Toolbar
    QHBoxLayout *toolLayout = new QHBoxLayout();
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->setSpacing(4);

    m_btnRefresh = new QToolButton(mainWidget);
    m_btnRefresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_btnRefresh->setToolTip(tr("KEY_GIT_REFRESH_TOOLTIP"));
    connect(m_btnRefresh, &QToolButton::clicked, this, &GitHistoryDock::refreshHistory);
    toolLayout->addWidget(m_btnRefresh);

    m_btnCheckoutHead = new QToolButton(mainWidget);
    m_btnCheckoutHead->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    m_btnCheckoutHead->setText(tr("KEY_GIT_RETURN_PRESENT"));
    m_btnCheckoutHead->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnCheckoutHead->setToolTip(tr("KEY_GIT_RETURN_PRESENT_TOOLTIP"));
    connect(m_btnCheckoutHead, &QToolButton::clicked, this, &GitHistoryDock::checkoutHead);
    toolLayout->addWidget(m_btnCheckoutHead);

    toolLayout->addStretch();

    m_lblStatus = new QLabel(mainWidget);
    m_lblStatus->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
    toolLayout->addWidget(m_lblStatus);

    mainLayout->addLayout(toolLayout);

    // 2. Disabled banner (if libgit2 not available)
    if (!SessionManager::isGitAvailable()) {
        m_lblDisabledBanner = new QLabel(
            tr("KEY_GIT_DISABLED_BANNER"),
            mainWidget
        );
        m_lblDisabledBanner->setAlignment(Qt::AlignCenter);
        m_lblDisabledBanner->setStyleSheet(QStringLiteral(
            "background-color: #2c3e50; color: #ecf0f1; border-radius: 4px; padding: 12px; margin: 8px;"
        ));
        mainLayout->addWidget(m_lblDisabledBanner);
        m_btnRefresh->setEnabled(false);
        m_btnCheckoutHead->setEnabled(false);
    }

    // 3. Central Graphics View
    m_scene = new QGraphicsScene(this);
    m_graphicsView = new GitHistoryGraphicsView(mainWidget);
    m_graphicsView->setScene(m_scene);
    mainLayout->addWidget(m_graphicsView, 1);

    // 4. Bottom Details Pane
    m_detailsWidget = new QWidget(mainWidget);
    m_detailsWidget->setStyleSheet(QStringLiteral(
        "QWidget { background-color: palette(alternate-base); border-radius: 4px; padding: 4px; }"
    ));
    QGridLayout *detLayout = new QGridLayout(m_detailsWidget);
    detLayout->setContentsMargins(6, 6, 6, 6);
    detLayout->setHorizontalSpacing(8);
    detLayout->setVerticalSpacing(4);

    m_hdrHash = new QLabel(tr("KEY_GIT_HDR_COMMIT"), m_detailsWidget);
    m_hdrHash->setStyleSheet(QStringLiteral("font-weight: bold; color: gray;"));
    m_lblHash = new QLabel(m_detailsWidget);
    m_lblHash->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_lblHash->setFont(QFont(QStringLiteral("Monospace"), 9));
    detLayout->addWidget(m_hdrHash, 0, 0);
    detLayout->addWidget(m_lblHash, 0, 1);

    m_hdrDate = new QLabel(tr("KEY_GIT_HDR_DATE"), m_detailsWidget);
    m_hdrDate->setStyleSheet(QStringLiteral("font-weight: bold; color: gray;"));
    m_lblDate = new QLabel(m_detailsWidget);
    detLayout->addWidget(m_hdrDate, 1, 0);
    detLayout->addWidget(m_lblDate, 1, 1);

    m_hdrAuthor = new QLabel(tr("KEY_GIT_HDR_AUTHOR"), m_detailsWidget);
    m_hdrAuthor->setStyleSheet(QStringLiteral("font-weight: bold; color: gray;"));
    m_lblAuthor = new QLabel(m_detailsWidget);
    detLayout->addWidget(m_hdrAuthor, 2, 0);
    detLayout->addWidget(m_lblAuthor, 2, 1);

    m_hdrMsg = new QLabel(tr("KEY_GIT_HDR_ACTION"), m_detailsWidget);
    m_hdrMsg->setStyleSheet(QStringLiteral("font-weight: bold; color: gray;"));
    m_lblMessage = new QLabel(m_detailsWidget);
    m_lblMessage->setWordWrap(true);
    detLayout->addWidget(m_hdrMsg, 3, 0);
    detLayout->addWidget(m_lblMessage, 3, 1);

    m_btnCheckoutSelected = new QPushButton(tr("KEY_GIT_BTN_RESTORE"), m_detailsWidget);
    m_btnCheckoutSelected->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    m_btnCheckoutSelected->setEnabled(false);
    connect(m_btnCheckoutSelected, &QPushButton::clicked, this, &GitHistoryDock::onCheckoutSelectedClicked);
    detLayout->addWidget(m_btnCheckoutSelected, 4, 0, 1, 2);

    mainLayout->addWidget(m_detailsWidget);

    setWidget(mainWidget);
    updateDetailsPane(nullptr);
}

void GitHistoryDock::setProjectController(ProjectController *controller)
{
    m_controller = controller;
    if (!m_controller) return;

    connect(m_controller, &ProjectController::projectHistoryChanged, this, &GitHistoryDock::refreshHistory, Qt::QueuedConnection);
    connect(m_controller, &ProjectController::projectLoaded, this, &GitHistoryDock::refreshHistory, Qt::QueuedConnection);
    connect(m_controller, &ProjectController::projectSaved, this, &GitHistoryDock::refreshHistory, Qt::QueuedConnection);
    connect(m_controller, &ProjectController::fileLoaded, this, &GitHistoryDock::refreshHistory, Qt::QueuedConnection);

    if (SessionManager *sm = m_controller->sessionManager()) {
        connect(sm, &SessionManager::sessionStarted, this, &GitHistoryDock::refreshHistory, Qt::QueuedConnection);
        connect(sm, &SessionManager::gitCommitted, this, &GitHistoryDock::refreshHistory, Qt::QueuedConnection);
    }

    refreshHistory();
}

void GitHistoryDock::refreshHistory()
{
    if (!SessionManager::isGitAvailable()) {
        return;
    }

    m_scene->clear();
    m_nodeItems.clear();

    if (!m_controller || !m_controller->sessionManager() || !m_controller->sessionManager()->hasActiveSession()) {
        m_lblStatus->setText(tr("KEY_GIT_NO_ACTIVE_PROJECT"));
        updateDetailsPane(nullptr);
        return;
    }

    SessionManager *sm = m_controller->sessionManager();
    QList<GitCommitInfo> log = sm->gitLog();
    QString headHash = sm->gitHeadCommitHash();

    m_lblStatus->setText(tr("KEY_GIT_COMMITS_COUNT").arg(log.size()));

    if (log.isEmpty()) {
        QGraphicsTextItem *emptyText = m_scene->addText(tr("KEY_GIT_EMPTY_HISTORY"));
        emptyText->setDefaultTextColor(Qt::gray);
        emptyText->setPos(10, 10);
        updateDetailsPane(nullptr);
        return;
    }

    buildCommitTree(log, headHash);
}

void GitHistoryDock::buildCommitTree(const QList<GitCommitInfo> &log, const QString &headHash)
{
    const qreal startX = 24.0;
    const qreal laneWidth = 20.0;
    const qreal startY = 24.0;
    const qreal rowSpacing = 44.0;

    static const QColor laneColors[] = {
        QColor(QStringLiteral("#3498db")), // Blue
        QColor(QStringLiteral("#e74c3c")), // Red
        QColor(QStringLiteral("#9b59b6")), // Purple
        QColor(QStringLiteral("#f39c12")), // Orange
        QColor(QStringLiteral("#1abc9c")), // Turquoise
        QColor(QStringLiteral("#e67e22")), // Dark Orange
        QColor(QStringLiteral("#34495e"))  // Navy
    };
    const int numColors = sizeof(laneColors) / sizeof(laneColors[0]);

    // 1. Assign topological lanes
    QMap<QString, int> commitLanes;
    QList<QString> activeLanes;
    int maxLane = 0;

    for (int i = 0; i < log.size(); ++i) {
        const GitCommitInfo &commit = log.at(i);
        int lane = -1;

        int existingIdx = activeLanes.indexOf(commit.hash);
        if (existingIdx != -1) {
            lane = existingIdx;
        } else {
            int emptySlot = activeLanes.indexOf(QString());
            if (emptySlot != -1) {
                lane = emptySlot;
                activeLanes[emptySlot] = commit.hash;
            } else {
                lane = activeLanes.size();
                activeLanes.append(commit.hash);
            }
        }

        commitLanes.insert(commit.hash, lane);
        if (lane > maxLane) maxLane = lane;

        if (commit.parentHashes.isEmpty()) {
            activeLanes[lane] = QString();
        } else {
            const QString firstParent = commit.parentHashes.first();
            if (!activeLanes.contains(firstParent)) {
                activeLanes[lane] = firstParent;
            } else {
                activeLanes[lane] = QString();
            }

            for (int p = 1; p < commit.parentHashes.size(); ++p) {
                const QString otherParent = commit.parentHashes.at(p);
                if (!activeLanes.contains(otherParent)) {
                    int slot = activeLanes.indexOf(QString());
                    if (slot != -1) {
                        activeLanes[slot] = otherParent;
                    } else {
                        activeLanes.append(otherParent);
                    }
                }
            }
        }
    }

    QMap<QString, QPointF> commitPositions;
    for (int i = 0; i < log.size(); ++i) {
        const QString &hash = log.at(i).hash;
        int lane = commitLanes.value(hash, 0);
        qreal x = startX + lane * laneWidth;
        qreal y = startY + i * rowSpacing;
        commitPositions.insert(hash, QPointF(x, y));
    }

    // 2. Draw connecting lines with smooth cubic bezier curves
    for (int i = 0; i < log.size(); ++i) {
        const GitCommitInfo &commit = log.at(i);
        QPointF fromPos = commitPositions.value(commit.hash);
        int fromLane = commitLanes.value(commit.hash, 0);
        QColor branchColor = laneColors[fromLane % numColors];
        QPen linePen(branchColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

        for (const QString &parentHash : commit.parentHashes) {
            if (commitPositions.contains(parentHash)) {
                QPointF toPos = commitPositions.value(parentHash);
                if (qFuzzyCompare(fromPos.x(), toPos.x())) {
                    QGraphicsLineItem *lineItem = m_scene->addLine(fromPos.x(), fromPos.y(), toPos.x(), toPos.y(), linePen);
                    lineItem->setZValue(-1);
                } else {
                    QPainterPath path;
                    path.moveTo(fromPos);
                    qreal midY = (fromPos.y() + toPos.y()) / 2.0;
                    path.cubicTo(fromPos.x(), midY, toPos.x(), midY, toPos.x(), toPos.y());
                    QGraphicsPathItem *pathItem = m_scene->addPath(path, linePen);
                    pathItem->setZValue(-1);
                }
            }
        }
    }

    // 3. Draw nodes and label rows
    const qreal textStartX = startX + (maxLane + 1) * laneWidth + 14.0;
    for (int i = 0; i < log.size(); ++i) {
        const GitCommitInfo &commit = log.at(i);
        QPointF pos = commitPositions.value(commit.hash);
        int lane = commitLanes.value(commit.hash, 0);
        QColor branchColor = laneColors[lane % numColors];
        bool isHead = (!headHash.isEmpty() && commit.hash == headHash);

        // Commit Node Item
        GitCommitNodeItem *nodeItem = new GitCommitNodeItem(commit, isHead, branchColor);
        nodeItem->setPos(pos);
        nodeItem->setZValue(1);
        m_scene->addItem(nodeItem);
        m_nodeItems.append(nodeItem);

        connect(nodeItem, &GitCommitNodeItem::commitClicked, this, &GitHistoryDock::onCommitSelected);
        connect(nodeItem, &GitCommitNodeItem::commitDoubleClicked, this, &GitHistoryDock::onCommitDoubleClicked);

        // Short hash badge
        QGraphicsTextItem *hashItem = m_scene->addText(QStringLiteral("[%1]").arg(commit.shortHash));
        QFont monoFont(QStringLiteral("Monospace"), 8);
        monoFont.setStyleHint(QFont::Monospace);
        hashItem->setFont(monoFont);
        hashItem->setDefaultTextColor(branchColor);
        hashItem->setPos(textStartX, pos.y() - 14);

        // Date & Time
        QString timeStr = commit.timestamp.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
        QGraphicsTextItem *timeItem = m_scene->addText(timeStr);
        QFont timeFont = timeItem->font();
        timeFont.setPointSize(8);
        timeItem->setFont(timeFont);
        timeItem->setDefaultTextColor(QColor(QStringLiteral("#7f8c8d")));
        timeItem->setPos(textStartX + 60, pos.y() - 14);

        // HEAD Badge & Commit message
        qreal msgStartX = textStartX + 120;
        if (isHead) {
            QGraphicsTextItem *headBadge = m_scene->addText(QStringLiteral("[HEAD]"));
            QFont headFont = headBadge->font();
            headFont.setBold(true);
            headFont.setPointSize(8);
            headBadge->setFont(headFont);
            headBadge->setDefaultTextColor(QColor(QStringLiteral("#2ecc71")));
            headBadge->setPos(msgStartX, pos.y() - 14);
            msgStartX += 48;
        }

        // Commit message
        QGraphicsTextItem *msgItem = m_scene->addText(commit.message);
        QFont msgFont = msgItem->font();
        if (isHead) {
            msgFont.setBold(true);
        }
        msgItem->setFont(msgFont);
        msgItem->setDefaultTextColor(isHead ? QColor(QStringLiteral("#2ecc71")) : palette().text().color());
        msgItem->setPos(msgStartX, pos.y() - 14);

        // If this commit was selected previously, restore selection
        if (m_hasSelection && m_selectedCommit.hash == commit.hash) {
            nodeItem->setSelectedCommit(true);
        }
    }

    // Set scene rect with generous margin
    qreal totalHeight = startY + log.size() * rowSpacing + 20;
    qreal totalWidth = textStartX + 400;
    if (m_graphicsView && m_graphicsView->width() > totalWidth) {
        totalWidth = m_graphicsView->width();
    }
    m_scene->setSceneRect(0, 0, totalWidth, totalHeight);
}

void GitHistoryDock::updateDetailsPane(const GitCommitInfo *info)
{
    if (!info) {
        m_hasSelection = false;
        m_lblHash->setText(QStringLiteral("-"));
        m_lblDate->setText(QStringLiteral("-"));
        m_lblAuthor->setText(QStringLiteral("-"));
        m_lblMessage->setText(tr("KEY_GIT_NO_SELECTION"));
        m_btnCheckoutSelected->setEnabled(false);
        return;
    }

    m_hasSelection = true;
    m_selectedCommit = *info;

    m_lblHash->setText(info->hash);
    m_lblDate->setText(info->timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    m_lblAuthor->setText(QStringLiteral("%1 <%2>").arg(info->author, info->email));
    m_lblMessage->setText(info->message);
    m_btnCheckoutSelected->setEnabled(true);
}

void GitHistoryDock::onCommitSelected(const GitCommitInfo &info)
{
    for (GitCommitNodeItem *item : m_nodeItems) {
        item->setSelectedCommit(item->commitInfo().hash == info.hash);
    }
    updateDetailsPane(&info);
}

void GitHistoryDock::onCommitDoubleClicked(const GitCommitInfo &info)
{
    onCommitSelected(info);
    const QString targetHash = info.hash;
    // Defer the checkout so that the mouse double click event handler
    // on GitCommitNodeItem completes cleanly before scene items are modified or destroyed.
    QMetaObject::invokeMethod(this, [this, targetHash]() {
        checkoutRevision(targetHash);
    }, Qt::QueuedConnection);
}

void GitHistoryDock::onCheckoutSelectedClicked()
{
    if (m_hasSelection && !m_selectedCommit.hash.isEmpty()) {
        checkoutRevision(m_selectedCommit.hash);
    }
}

void GitHistoryDock::checkoutRevision(const QString &hash)
{
    emit revisionCheckoutRequested(hash);

    if (m_controller) {
        m_controller->checkoutRevision(hash);
    }
}

void GitHistoryDock::checkoutHead()
{
    if (!m_controller || !m_controller->sessionManager()) return;

    QList<GitCommitInfo> log = m_controller->sessionManager()->gitLog();
    if (!log.isEmpty()) {
        checkoutRevision(log.first().hash);
    }
}

void GitHistoryDock::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDockWidget::changeEvent(event);
}

void GitHistoryDock::retranslateUi()
{
    setWindowTitle(tr("KEY_DOCK_GIT_HISTORY"));
    if (m_btnRefresh) {
        m_btnRefresh->setToolTip(tr("KEY_GIT_REFRESH_TOOLTIP"));
    }
    if (m_btnCheckoutHead) {
        m_btnCheckoutHead->setText(tr("KEY_GIT_RETURN_PRESENT"));
        m_btnCheckoutHead->setToolTip(tr("KEY_GIT_RETURN_PRESENT_TOOLTIP"));
    }
    if (m_lblDisabledBanner) {
        m_lblDisabledBanner->setText(tr("KEY_GIT_DISABLED_BANNER"));
    }
    if (m_hdrHash) m_hdrHash->setText(tr("KEY_GIT_HDR_COMMIT"));
    if (m_hdrDate) m_hdrDate->setText(tr("KEY_GIT_HDR_DATE"));
    if (m_hdrAuthor) m_hdrAuthor->setText(tr("KEY_GIT_HDR_AUTHOR"));
    if (m_hdrMsg) m_hdrMsg->setText(tr("KEY_GIT_HDR_ACTION"));
    if (m_btnCheckoutSelected) m_btnCheckoutSelected->setText(tr("KEY_GIT_BTN_RESTORE"));
    if (!m_hasSelection && m_lblMessage) {
        m_lblMessage->setText(tr("KEY_GIT_NO_SELECTION"));
    }
}

