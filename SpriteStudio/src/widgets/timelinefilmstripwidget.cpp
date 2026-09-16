#include "widgets/timelinefilmstripwidget.h"
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QEvent>

TimelineFilmstripWidget::TimelineFilmstripWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void TimelineFilmstripWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->setSpacing(2);

    // Header toolbar
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(2, 0, 2, 0);
    headerLayout->setSpacing(8);

    m_lblTitle = new QLabel(tr("KEY_TIMELINE_TITLE"), this);
    headerLayout->addWidget(m_lblTitle);

    m_lblDuration = new QLabel(this);
    m_lblDuration->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
    headerLayout->addWidget(m_lblDuration);

    headerLayout->addStretch();

    m_btnAddSelection = new QToolButton(this);
    m_btnAddSelection->setText(tr("KEY_TIMELINE_ADD_SELECTION"));
    m_btnAddSelection->setToolTip(tr("KEY_TIMELINE_ADD_SELECTION_TOOLTIP"));
    connect(m_btnAddSelection, &QToolButton::clicked, this, [this]() {
        if (!m_document || m_animationName.isEmpty()) return;
        for (int globalIdx : m_document->selectedFrameIndices()) {
            emit addFrameRequested(m_animationName, globalIdx);
        }
    });
    headerLayout->addWidget(m_btnAddSelection);

    mainLayout->addLayout(headerLayout);

    // List widget in IconMode
    m_listWidget = new QListWidget(this);
    m_listWidget->setViewMode(QListView::IconMode);
    m_listWidget->setFlow(QListView::LeftToRight);
    m_listWidget->setWrapping(false);
    m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listWidget->setIconSize(QSize(64, 64));
    m_listWidget->setSpacing(4);
    m_listWidget->setDragEnabled(true);
    m_listWidget->setAcceptDrops(true);
    m_listWidget->setDropIndicatorShown(true);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listWidget->setMinimumHeight(100);

    m_listWidget->setStyleSheet(QStringLiteral(
        "QListWidget::item {"
        "  border: 1px solid #c0c0c0;"
        "  border-radius: 4px;"
        "  padding: 2px;"
        "  background: #fafafa;"
        "}"
        "QListWidget::item:selected {"
        "  border: 2px solid #2980b9;"
        "  background: #e8f4fc;"
        "  color: #2980b9;"
        "  font-weight: bold;"
        "}"
    ));

    connect(m_listWidget, &QListWidget::itemClicked, this, &TimelineFilmstripWidget::onItemClicked);
    connect(m_listWidget, &QListWidget::customContextMenuRequested, this, &TimelineFilmstripWidget::onCustomContextMenuRequested);

    if (m_listWidget->model()) {
        connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved,
                this, &TimelineFilmstripWidget::onRowsMoved);
    }

    mainLayout->addWidget(m_listWidget);
}

void TimelineFilmstripWidget::setDocument(SpriteDocument *document)
{
    m_document = document;
    refresh();
}

void TimelineFilmstripWidget::setAnimation(const QString &animationName)
{
    m_animationName = animationName;
    m_activeSeqIndex = -1;
    refresh();
}

void TimelineFilmstripWidget::setActiveSequenceIndex(int seqIndex)
{
    m_activeSeqIndex = seqIndex;
    if (!m_listWidget || m_isRebuilding) return;

    if (seqIndex >= 0 && seqIndex < m_listWidget->count()) {
        m_listWidget->blockSignals(true);
        m_listWidget->setCurrentRow(seqIndex);
        m_listWidget->scrollToItem(m_listWidget->item(seqIndex), QAbstractItemView::EnsureVisible);
        m_listWidget->blockSignals(false);
    }
}

void TimelineFilmstripWidget::refresh()
{
    rebuildItems();
}

void TimelineFilmstripWidget::rebuildItems()
{
    if (!m_listWidget) return;

    m_isRebuilding = true;
    m_listWidget->clear();

    if (!m_document || m_animationName.isEmpty() || !m_document->hasAnimation(m_animationName)) {
        m_lblTitle->setText(tr("KEY_TIMELINE_NO_ANIM"));
        m_lblDuration->clear();
        m_btnAddSelection->setEnabled(false);
        m_isRebuilding = false;
        return;
    }

    m_btnAddSelection->setEnabled(true);
    const SpriteAnimation &anim = m_document->animation(m_animationName);

    m_lblTitle->setText(tr("KEY_TIMELINE_ANIM_INFO").arg(anim.name).arg(anim.frameIndices.size()));
    int ms = anim.durationMs();
    m_lblDuration->setText(tr("KEY_TIMELINE_DURATION_FPS").arg(ms).arg(anim.fps));

    int frameDuration = anim.fps > 0 ? (1000 / anim.fps) : 100;

    for (int seqIdx = 0; seqIdx < anim.frameIndices.size(); ++seqIdx) {
        int globalIdx = anim.frameIndices.at(seqIdx);
        auto *item = new QListWidgetItem(m_listWidget);

        QImage img;
        if (globalIdx >= 0 && globalIdx < m_document->frameCount()) {
            img = m_document->frame(globalIdx);
        }

        if (!img.isNull()) {
            item->setIcon(QIcon(QPixmap::fromImage(img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
        }

        item->setText(tr("#%1 (F%2)").arg(seqIdx + 1).arg(globalIdx + 1));
        item->setToolTip(tr("KEY_TIMELINE_FRAME_TOOLTIP")
                             .arg(seqIdx + 1).arg(globalIdx + 1).arg(frameDuration));
        item->setData(Qt::UserRole, globalIdx);
        item->setData(Qt::UserRole + 1, seqIdx);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);

        m_listWidget->addItem(item);
    }

    if (m_activeSeqIndex >= 0 && m_activeSeqIndex < m_listWidget->count()) {
        m_listWidget->setCurrentRow(m_activeSeqIndex);
    }

    m_isRebuilding = false;
}

void TimelineFilmstripWidget::onItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    int seqIdx = m_listWidget->row(item);
    emit frameSeekRequested(seqIdx);
}

void TimelineFilmstripWidget::onRowsMoved(const QModelIndex &/*sourceParent*/, int /*sourceStart*/, int /*sourceEnd*/,
                                          const QModelIndex &/*destinationParent*/, int /*destinationRow*/)
{
    if (m_isRebuilding || m_animationName.isEmpty()) return;

    QList<int> newSeq;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item) {
            newSeq.append(item->data(Qt::UserRole).toInt());
            item->setText(tr("#%1 (F%2)").arg(i + 1).arg(item->data(Qt::UserRole).toInt() + 1));
        }
    }

    emit sequenceReordered(m_animationName, newSeq);
}

void TimelineFilmstripWidget::onCustomContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem *item = m_listWidget->itemAt(pos);
    if (!item) return;

    int seqIdx = m_listWidget->row(item);
    int globalIdx = item->data(Qt::UserRole).toInt();

    QMenu menu(this);
    QAction *actDuplicate = menu.addAction(tr("KEY_TIMELINE_DUPLICATE_FRAME"));
    QAction *actRemove = menu.addAction(tr("KEY_TIMELINE_REMOVE_FRAME"));
    menu.addSeparator();
    QAction *actSelectAtlas = menu.addAction(tr("KEY_TIMELINE_SELECT_IN_ATLAS"));

    QAction *chosen = menu.exec(m_listWidget->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actDuplicate) {
        emit duplicateFrameRequested(m_animationName, seqIdx);
    } else if (chosen == actRemove) {
        emit removeFrameRequested(m_animationName, seqIdx);
    } else if (chosen == actSelectAtlas && m_document) {
        m_document->clearBoxSelections();
        m_document->setFrameSelected(globalIdx, true);
    }
}

void TimelineFilmstripWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void TimelineFilmstripWidget::retranslateUi()
{
    if (m_btnAddSelection) {
        m_btnAddSelection->setText(tr("KEY_TIMELINE_ADD_SELECTION"));
        m_btnAddSelection->setToolTip(tr("KEY_TIMELINE_ADD_SELECTION_TOOLTIP"));
    }
    rebuildItems();
}

