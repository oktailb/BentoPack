#include "include/widgets/githistorydock.h"
#include "include/controller/projectcontroller.h"
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

GitCommitNodeItem::GitCommitNodeItem(const GitCommitInfo &info, bool isHead, QGraphicsItem *parent)
    : QObject()
    , QGraphicsEllipseItem(parent)
    , m_info(info)
    , m_isHead(isHead)
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
        tr("Message"), m_info.message.toHtmlEscaped(),
        tr("Auteur"), m_info.author.toHtmlEscaped(),
        tr("Date"), m_info.timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
        tr("Double-cliquez pour restaurer cette révision")
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
    setWindowTitle(tr("Historique Git"));
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
    m_btnRefresh->setToolTip(tr("Rafraîchir l'historique des révisions"));
    connect(m_btnRefresh, &QToolButton::clicked, this, &GitHistoryDock::refreshHistory);
    toolLayout->addWidget(m_btnRefresh);

    m_btnCheckoutHead = new QToolButton(mainWidget);
    m_btnCheckoutHead->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    m_btnCheckoutHead->setText(tr("Revenir au présent"));
    m_btnCheckoutHead->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnCheckoutHead->setToolTip(tr("Restaurer la dernière version (HEAD de la branche)"));
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
            tr("L'intégration Git n'est pas activée sur ce système.\n(libgit2 non détectée lors de la compilation)"),
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

    QLabel *hdrHash = new QLabel(tr("Commit :"), m_detailsWidget);
    hdrHash->setStyleSheet(QStringLiteral("font-weight: bold; color: gray;"));
    m_lblHash = new QLabel(m_detailsWidget);
    m_lblHash->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_lblHash->setFont(QFont(QStringLiteral("Monospace"), 9));
    detLayout->addWidget(hdrHash, 0, 0);
    detLayout->addWidget(m_lblHash, 0, 1);

    QLabel *hdrDate = new QLabel(tr("Date :"), m_detailsWidget);
    hdrDate->setStyleSheet(QStringLiteral("font-weight: bold; color: gray;"));
    m_lblDate = new QLabel(m_detailsWidget);
    detLayout->addWidget(hdrDate, 1, 0);
    detLayout->addWidget(m_lblDate, 1, 1);

    QLabel *hdrAuthor = new QLabel(tr("Auteur :"), m_detailsWidget);
    hdrAuthor->setStyleSheet(QStringLiteral("font-weight: bold; color: gray;"));
    m_lblAuthor = new QLabel(m_detailsWidget);
    detLayout->addWidget(hdrAuthor, 2, 0);
    detLayout->addWidget(m_lblAuthor, 2, 1);

    QLabel *hdrMsg = new QLabel(tr("Action :"), m_detailsWidget);
    hdrMsg->setStyleSheet(QStringLiteral("font-weight: bold; color: gray;"));
    m_lblMessage = new QLabel(m_detailsWidget);
    m_lblMessage->setWordWrap(true);
    detLayout->addWidget(hdrMsg, 3, 0);
    detLayout->addWidget(m_lblMessage, 3, 1);

    m_btnCheckoutSelected = new QPushButton(tr("Restaurer cette révision"), m_detailsWidget);
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
        m_lblStatus->setText(tr("Aucun projet actif"));
        updateDetailsPane(nullptr);
        return;
    }

    SessionManager *sm = m_controller->sessionManager();
    QList<GitCommitInfo> log = sm->gitLog();
    QString headHash = sm->gitHeadCommitHash();

    m_lblStatus->setText(tr("%n commit(s)", "", log.size()));

    if (log.isEmpty()) {
        QGraphicsTextItem *emptyText = m_scene->addText(tr("Historique Git vide pour ce projet."));
        emptyText->setDefaultTextColor(Qt::gray);
        emptyText->setPos(10, 10);
        updateDetailsPane(nullptr);
        return;
    }

    buildCommitTree(log, headHash);
}

void GitHistoryDock::buildCommitTree(const QList<GitCommitInfo> &log, const QString &headHash)
{
    const qreal nodeX = 24.0;
    const qreal rowSpacing = 44.0;
    const qreal startY = 24.0;

    QMap<QString, QPointF> commitPositions;
    for (int i = 0; i < log.size(); ++i) {
        qreal y = startY + i * rowSpacing;
        commitPositions.insert(log.at(i).hash, QPointF(nodeX, y));
    }

    // 1. Draw connecting lines between commits and parents
    QPen linePen(QColor(QStringLiteral("#7f8c8d")), 2.0);
    for (int i = 0; i < log.size(); ++i) {
        const GitCommitInfo &commit = log.at(i);
        QPointF fromPos = commitPositions.value(commit.hash);

        for (const QString &parentHash : commit.parentHashes) {
            if (commitPositions.contains(parentHash)) {
                QPointF toPos = commitPositions.value(parentHash);
                m_scene->addLine(fromPos.x(), fromPos.y(), toPos.x(), toPos.y(), linePen);
            }
        }
    }

    // 2. Draw nodes and label rows
    for (int i = 0; i < log.size(); ++i) {
        const GitCommitInfo &commit = log.at(i);
        QPointF pos = commitPositions.value(commit.hash);
        bool isHead = (!headHash.isEmpty() && commit.hash == headHash);

        // Commit Node Item
        GitCommitNodeItem *nodeItem = new GitCommitNodeItem(commit, isHead);
        nodeItem->setPos(pos);
        m_scene->addItem(nodeItem);
        m_nodeItems.append(nodeItem);

        connect(nodeItem, &GitCommitNodeItem::commitClicked, this, &GitHistoryDock::onCommitSelected);
        connect(nodeItem, &GitCommitNodeItem::commitDoubleClicked, this, &GitHistoryDock::onCommitDoubleClicked);

        // Short hash badge
        QGraphicsTextItem *hashItem = m_scene->addText(QStringLiteral("[%1]").arg(commit.shortHash));
        QFont monoFont(QStringLiteral("Monospace"), 8);
        monoFont.setStyleHint(QFont::Monospace);
        hashItem->setFont(monoFont);
        hashItem->setDefaultTextColor(QColor(QStringLiteral("#95a5a6")));
        hashItem->setPos(pos.x() + 16, pos.y() - 14);

        // Date & Time
        QString timeStr = commit.timestamp.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
        QGraphicsTextItem *timeItem = m_scene->addText(timeStr);
        QFont timeFont = timeItem->font();
        timeFont.setPointSize(8);
        timeItem->setFont(timeFont);
        timeItem->setDefaultTextColor(QColor(QStringLiteral("#7f8c8d")));
        timeItem->setPos(pos.x() + 74, pos.y() - 14);

        // HEAD Badge
        qreal textStartX = pos.x() + 130;
        if (isHead) {
            QGraphicsTextItem *headBadge = m_scene->addText(QStringLiteral("[HEAD]"));
            QFont headFont = headBadge->font();
            headFont.setBold(true);
            headFont.setPointSize(8);
            headBadge->setFont(headFont);
            headBadge->setDefaultTextColor(QColor(QStringLiteral("#2ecc71")));
            headBadge->setPos(textStartX, pos.y() - 14);
            textStartX += 48;
        }

        // Commit message
        QGraphicsTextItem *msgItem = m_scene->addText(commit.message);
        QFont msgFont = msgItem->font();
        if (isHead) {
            msgFont.setBold(true);
        }
        msgItem->setFont(msgFont);
        msgItem->setDefaultTextColor(isHead ? QColor(QStringLiteral("#2ecc71")) : palette().text().color());
        msgItem->setPos(textStartX, pos.y() - 14);

        // If this commit was selected previously, restore selection
        if (m_hasSelection && m_selectedCommit.hash == commit.hash) {
            nodeItem->setSelectedCommit(true);
        }
    }

    // Set scene rect with generous margin
    qreal totalHeight = startY + log.size() * rowSpacing + 20;
    m_scene->setSceneRect(0, 0, m_graphicsView->width() > 300 ? m_graphicsView->width() : 300, totalHeight);
}

void GitHistoryDock::updateDetailsPane(const GitCommitInfo *info)
{
    if (!info) {
        m_hasSelection = false;
        m_lblHash->setText(QStringLiteral("-"));
        m_lblDate->setText(QStringLiteral("-"));
        m_lblAuthor->setText(QStringLiteral("-"));
        m_lblMessage->setText(tr("Aucun commit sélectionné.\nCliquez sur un nœud pour afficher ses détails."));
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
