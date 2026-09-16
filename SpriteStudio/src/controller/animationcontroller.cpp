#include "include/controller/animationcontroller.h"
#include "include/animation/animationplayer.h"
#include "include/model/spritedocument.h"
#include "include/commands/commands.h"
#include "include/config/appconfig.h"
#include "include/widgets/timelinefilmstripwidget.h"
#include <QUndoStack>
#include <QInputDialog>
#include <QMessageBox>
#include <QGraphicsLineItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsItemGroup>

AnimationController::AnimationController(SpriteDocument *document,
                                         QUndoStack *undoStack,
                                         AnimationPlayer *player,
                                         QTreeWidget *treeWidget,
                                         QGraphicsView *previewView,
                                         QObject *parent)
    : QObject(parent)
    , m_document(document)
    , m_undoStack(undoStack)
    , m_player(player)
    , m_ownsPlayer(false)
    , m_previewScene(new QGraphicsScene(this))
{
    if (!m_player) {
        m_player = new AnimationPlayer(this);
        m_ownsPlayer = true;
    }

    connect(m_player, &AnimationPlayer::frameChanged,
            this, &AnimationController::onPlayerFrameChanged);
    connect(m_player, &AnimationPlayer::playbackStateChanged,
            this, &AnimationController::onPlayerPlaybackStateChanged);

    if (m_document) {
        connect(m_document, &SpriteDocument::animationsChanged,
                this, &AnimationController::syncAnimationList);
        connect(m_document, &SpriteDocument::frameUpdated, this, [this](int globalIdx) {
            if (m_player && m_player->currentGlobalFrameIndex() == globalIdx) {
                renderCurrentFrame(globalIdx);
            }
        });
        connect(m_document, &SpriteDocument::boxPivotChanged, this, [this](int, const QPoint &) {
            updatePreview();
        });
    }

    if (treeWidget) {
        attachTreeWidget(treeWidget);
    }

    if (previewView) {
        attachPreviewView(previewView);
    }
}

AnimationController::~AnimationController()
{
    if (m_ownsPlayer && m_player) {
        m_player->stop();
    }
}

void AnimationController::attachTreeWidget(QTreeWidget *treeWidget)
{
    m_treeWidget = treeWidget;
    if (!m_treeWidget) return;

    m_treeWidget->setHeaderLabels({tr("KEY_ANIM_COL_NAME"), tr("KEY_ANIM_COL_FPS"), tr("KEY_ANIM_COL_MODE"), tr("KEY_ANIM_COL_FRAMES"), tr("KEY_ANIM_COL_DURATION")});

    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged,
            this, &AnimationController::onTreeItemSelectionChanged);
    connect(m_treeWidget, &QTreeWidget::itemClicked,
            this, &AnimationController::onTreeItemClicked);
    connect(m_treeWidget, &QTreeWidget::itemChanged,
            this, &AnimationController::onTreeItemChanged);

    syncAnimationList();
}

void AnimationController::attachPreviewView(QGraphicsView *previewView)
{
    m_previewView = previewView;
    if (m_previewView) {
        m_previewView->setScene(m_previewScene);
    }
}

void AnimationController::attachTimelineWidget(TimelineFilmstripWidget *timeline)
{
    m_timelineWidget = timeline;
    if (!m_timelineWidget) return;

    m_timelineWidget->setDocument(m_document);
    m_timelineWidget->setAnimation(m_currentAnimationName);
    if (m_player) {
        m_timelineWidget->setActiveSequenceIndex(m_player->currentSequenceIndex());
    }

    connect(m_timelineWidget, &TimelineFilmstripWidget::frameSeekRequested,
            this, &AnimationController::seek);
    connect(m_timelineWidget, &TimelineFilmstripWidget::sequenceReordered,
            this, &AnimationController::reorderAnimationFrames);
    connect(m_timelineWidget, &TimelineFilmstripWidget::addFrameRequested,
            this, &AnimationController::addFrameToAnimation);
    connect(m_timelineWidget, &TimelineFilmstripWidget::removeFrameRequested,
            this, &AnimationController::removeFrameFromAnimation);
    connect(m_timelineWidget, &TimelineFilmstripWidget::duplicateFrameRequested,
            this, &AnimationController::duplicateFrameInAnimation);
}

void AnimationController::attachScrubberSlider(QSlider *slider, QLabel *frameIndicator)
{
    m_scrubberSlider = slider;
    m_frameIndicator = frameIndicator;
    if (!m_scrubberSlider) return;

    connect(m_scrubberSlider, &QSlider::valueChanged,
            this, &AnimationController::onScrubberValueChanged);

    updateScrubberState();
}

void AnimationController::attachLoopModeComboBox(QComboBox *combo)
{
    m_loopModeCombo = combo;
    if (!m_loopModeCombo) return;

    m_loopModeCombo->clear();
    m_loopModeCombo->addItem(tr("KEY_LOOP_MODE_LOOP"), static_cast<int>(SpriteAnimation::Loop));
    m_loopModeCombo->addItem(tr("KEY_LOOP_MODE_ONCE"), static_cast<int>(SpriteAnimation::Once));
    m_loopModeCombo->addItem(tr("KEY_LOOP_MODE_PINGPONG"), static_cast<int>(SpriteAnimation::PingPong));

    connect(m_loopModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_isSyncingUi || !m_loopModeCombo) return;
        auto mode = static_cast<SpriteAnimation::LoopMode>(m_loopModeCombo->itemData(idx).toInt());
        setLoopMode(mode);
    });
}

void AnimationController::play()
{
    if (m_player) {
        m_player->play();
    }
}

void AnimationController::pause()
{
    if (m_player) {
        m_player->pause();
    }
}

void AnimationController::stop()
{
    if (m_player) {
        m_player->stop();
        m_previewScene->clear();
        m_previewPixmapItem = nullptr;
    }
}

void AnimationController::togglePlayPause()
{
    if (m_player) {
        m_player->togglePlayPause();
    }
}

void AnimationController::stepForward()
{
    if (m_player) {
        m_player->stepForward();
    }
}

void AnimationController::stepBackward()
{
    if (m_player) {
        m_player->stepBackward();
    }
}

void AnimationController::firstFrame()
{
    if (m_player) {
        m_player->firstFrame();
    }
}

void AnimationController::lastFrame()
{
    if (m_player) {
        m_player->lastFrame();
    }
}

void AnimationController::seek(int sequenceIndex)
{
    if (m_player) {
        m_player->seek(sequenceIndex);
    }
}

bool AnimationController::isPlaying() const
{
    return m_player ? m_player->isPlaying() : false;
}

int AnimationController::fps() const
{
    return m_player ? m_player->fps() : AppConfig::instance().animation().defaultFps;
}

void AnimationController::setFps(int fps)
{
    const AnimationConfig &cfg = AppConfig::instance().animation();
    fps = std::clamp(fps, cfg.minFps, cfg.maxFps);
    if (m_player) {
        m_player->setFps(fps);
    }

    if (m_document && !m_currentAnimationName.isEmpty() && m_document->hasAnimation(m_currentAnimationName)) {
        SpriteAnimation anim = m_document->animation(m_currentAnimationName);
        if (anim.fps != fps) {
            m_document->setAnimation(m_currentAnimationName, anim.frameIndices, fps, anim.loop, anim.loopMode);
        }
    }

    updateScrubberState();
    emit fpsChanged(fps);
}

SpriteAnimation::LoopMode AnimationController::loopMode() const
{
    if (m_document && !m_currentAnimationName.isEmpty() && m_document->hasAnimation(m_currentAnimationName)) {
        return m_document->animation(m_currentAnimationName).loopMode;
    }
    return m_player ? static_cast<SpriteAnimation::LoopMode>(m_player->loopMode()) : SpriteAnimation::Loop;
}

void AnimationController::setLoopMode(SpriteAnimation::LoopMode mode)
{
    if (m_player) {
        m_player->setLoopMode(static_cast<AnimationPlayer::LoopMode>(mode));
    }

    if (m_document && !m_currentAnimationName.isEmpty() && m_document->hasAnimation(m_currentAnimationName)) {
        if (m_undoStack && m_currentAnimationName != QLatin1String("current")) {
            const SpriteAnimation &anim = m_document->animation(m_currentAnimationName);
            if (anim.loopMode != mode) {
                m_undoStack->push(new ChangeAnimationPropertiesCommand(m_document, m_currentAnimationName, anim.fps, mode));
            }
        } else {
            m_document->setAnimationLoopMode(m_currentAnimationName, mode);
        }
    }

    emit loopModeChanged(mode);
}

int AnimationController::currentSequenceIndex() const
{
    return m_player ? m_player->currentSequenceIndex() : 0;
}

int AnimationController::currentGlobalFrameIndex() const
{
    return m_player ? m_player->currentGlobalFrameIndex() : -1;
}

void AnimationController::selectAnimation(const QString &name)
{
    if (!m_document || !m_document->hasAnimation(name)) return;

    m_currentAnimationName = name;
    SpriteAnimation anim = m_document->animation(name);

    if (m_player) {
        m_player->setSequence(anim.frameIndices, anim.fps, static_cast<AnimationPlayer::LoopMode>(anim.loopMode));
    }

    if (m_timelineWidget) {
        m_timelineWidget->setAnimation(name);
        if (m_player) {
            m_timelineWidget->setActiveSequenceIndex(m_player->currentSequenceIndex());
        }
    }

    updateScrubberState();

    if (m_loopModeCombo) {
        m_isSyncingUi = true;
        int idx = m_loopModeCombo->findData(static_cast<int>(anim.loopMode));
        if (idx >= 0) m_loopModeCombo->setCurrentIndex(idx);
        m_isSyncingUi = false;
    }

    emit currentAnimationChanged(name);
    if (name != QLatin1String("current")) {
        emit framesSelectedInAnimation(anim.frameIndices);
    }

    // Synchronize tree widget selection if attached
    if (m_treeWidget) {
        m_treeWidget->blockSignals(true);
        QList<QTreeWidgetItem*> items = m_treeWidget->findItems(name, Qt::MatchExactly, 0);
        if (!items.isEmpty()) {
            m_treeWidget->setCurrentItem(items.first());
        }
        m_treeWidget->blockSignals(false);
    }

    updatePreview();
}

void AnimationController::createAnimation(const QString &name, const QList<int> &frameIndices, int fps, SpriteAnimation::LoopMode loopMode)
{
    if (!m_document || name.isEmpty() || frameIndices.isEmpty()) return;
    if (fps <= 0) {
        fps = AppConfig::instance().animation().defaultFps;
    }

    if (name == QLatin1String("current")) {
        m_document->setAnimation(name, frameIndices, fps, true, loopMode);
        selectAnimation(name);
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new CreateAnimationCommand(m_document, name, frameIndices, fps));
        if (loopMode != SpriteAnimation::Loop) {
            m_undoStack->push(new ChangeAnimationPropertiesCommand(m_document, name, fps, loopMode));
        }
    } else {
        m_document->setAnimation(name, frameIndices, fps, true, loopMode);
    }

    selectAnimation(name);
    play();
}

void AnimationController::createAnimationFromSelection(const QList<int> &selectedIndices)
{
    if (selectedIndices.isEmpty()) return;

    bool ok = false;
    QString name = QInputDialog::getText(nullptr, tr("KEY_DIALOG_NEW_ANIM_TITLE"),
                                         tr("KEY_DIALOG_NEW_ANIM_PROMPT"), QLineEdit::Normal,
                                         QStringLiteral("anim_%1").arg(m_document ? m_document->animations().size() + 1 : 1),
                                         &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    createAnimation(name.trimmed(), selectedIndices, fps(), loopMode());
}

void AnimationController::promptCreateNewAnimation()
{
    if (!m_document) return;
    bool ok = false;
    QString name = QInputDialog::getText(nullptr, tr("KEY_DIALOG_NEW_ANIM_TITLE"),
                                         tr("KEY_DIALOG_NEW_ANIM_PROMPT"), QLineEdit::Normal,
                                         QStringLiteral("anim_%1").arg(m_document->animations().size() + 1),
                                         &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    name = name.trimmed();

    QList<int> selFrames = m_document->selectedFrameIndices();
    if (selFrames.isEmpty() && m_document->frameCount() > 0) {
        selFrames.append(0);
    }
    createAnimation(name, selFrames, fps(), loopMode());
}

void AnimationController::removeAnimation(const QString &name)
{
    if (!m_document || !m_document->hasAnimation(name)) return;

    if (m_undoStack) {
        m_undoStack->push(new DeleteAnimationCommand(m_document, name));
    } else {
        m_document->removeAnimation(name);
    }

    if (m_currentAnimationName == name) {
        stop();
        m_currentAnimationName.clear();
        if (m_timelineWidget) {
            m_timelineWidget->setAnimation(QString());
        }
    }
}

void AnimationController::removeAnimations(const QStringList &names)
{
    for (const QString &name : names) {
        removeAnimation(name);
    }
}

void AnimationController::removeSelectedAnimation()
{
    QStringList namesToDelete;
    if (m_treeWidget) {
        for (QTreeWidgetItem *item : m_treeWidget->selectedItems()) {
            namesToDelete.append(item->text(0));
        }
    } else if (!m_currentAnimationName.isEmpty()) {
        namesToDelete.append(m_currentAnimationName);
    }

    removeAnimations(namesToDelete);
}

void AnimationController::renameAnimation(const QString &oldName, const QString &newName)
{
    if (!m_document || !m_document->hasAnimation(oldName) || newName.isEmpty() || oldName == newName) return;

    if (m_undoStack) {
        m_undoStack->push(new RenameAnimationCommand(m_document, oldName, newName));
    } else {
        m_document->renameAnimation(oldName, newName);
    }

    if (m_currentAnimationName == oldName) {
        m_currentAnimationName = newName;
        if (m_timelineWidget) {
            m_timelineWidget->setAnimation(newName);
        }
        emit currentAnimationChanged(newName);
    }
}

void AnimationController::duplicateAnimation(const QString &sourceName)
{
    if (!m_document || !m_document->hasAnimation(sourceName)) return;

    QString newName = sourceName + QStringLiteral("_copy");
    int counter = 1;
    while (m_document->hasAnimation(newName)) {
        counter++;
        newName = QStringLiteral("%1_copy%2").arg(sourceName).arg(counter);
    }

    if (m_undoStack) {
        m_undoStack->push(new DuplicateAnimationCommand(m_document, sourceName, newName));
    } else {
        m_document->duplicateAnimation(sourceName, newName);
    }

    selectAnimation(newName);
}

void AnimationController::duplicateSelectedAnimation()
{
    QString targetName = m_currentAnimationName;
    if (m_treeWidget && !m_treeWidget->selectedItems().isEmpty()) {
        targetName = m_treeWidget->selectedItems().first()->text(0);
    }
    if (!targetName.isEmpty()) {
        duplicateAnimation(targetName);
    }
}

void AnimationController::reverseAnimationOrder()
{
    QString targetName = m_currentAnimationName;
    if (m_treeWidget && !m_treeWidget->selectedItems().isEmpty()) {
        targetName = m_treeWidget->selectedItems().first()->text(0);
    }

    if (targetName.isEmpty() || !m_document || !m_document->hasAnimation(targetName)) {
        return;
    }

    if (m_undoStack) {
        m_undoStack->push(new ReverseAnimationCommand(m_document, targetName));
    } else {
        SpriteAnimation anim = m_document->animation(targetName);
        std::reverse(anim.frameIndices.begin(), anim.frameIndices.end());
        m_document->setAnimation(targetName, anim.frameIndices, anim.fps, anim.loop, anim.loopMode);
    }

    selectAnimation(targetName);
}

void AnimationController::reorderAnimationFrames(const QString &name, const QList<int> &newSequence)
{
    if (!m_document || !m_document->hasAnimation(name) || newSequence.isEmpty()) return;

    if (m_undoStack && name != QLatin1String("current")) {
        m_undoStack->push(new ReorderAnimationFramesCommand(m_document, name, newSequence));
    } else {
        m_document->setAnimationFrameSequence(name, newSequence);
    }

    if (m_currentAnimationName == name) {
        selectAnimation(name);
    }
}

void AnimationController::addFrameToAnimation(const QString &name, int globalIndex)
{
    if (!m_document || !m_document->hasAnimation(name)) return;
    QList<int> seq = m_document->animation(name).frameIndices;
    seq.append(globalIndex);
    reorderAnimationFrames(name, seq);
}

void AnimationController::removeFrameFromAnimation(const QString &name, int seqIndex)
{
    if (!m_document || !m_document->hasAnimation(name)) return;
    QList<int> seq = m_document->animation(name).frameIndices;
    if (seqIndex >= 0 && seqIndex < seq.size()) {
        seq.removeAt(seqIndex);
        reorderAnimationFrames(name, seq);
    }
}

void AnimationController::duplicateFrameInAnimation(const QString &name, int seqIndex)
{
    if (!m_document || !m_document->hasAnimation(name)) return;
    QList<int> seq = m_document->animation(name).frameIndices;
    if (seqIndex >= 0 && seqIndex < seq.size()) {
        seq.insert(seqIndex + 1, seq.at(seqIndex));
        reorderAnimationFrames(name, seq);
    }
}

void AnimationController::updateCurrentAnimation(const QList<int> &selectedIndices)
{
    if (!m_document) return;

    if (selectedIndices.isEmpty()) {
        removeCurrentAnimation();
    } else {
        bool wasPlaying = isPlaying();
        m_document->setAnimation(QStringLiteral("current"), selectedIndices, fps(), true, loopMode());
        selectAnimation(QStringLiteral("current"));

        bool autoPlay = AppConfig::instance().animation().autoPlayOnSelection;
        if (selectedIndices.size() >= 2) {
            if (autoPlay || wasPlaying) {
                play();
            }
        } else {
            if (autoPlay) {
                pause();
            }
        }
    }
}

void AnimationController::removeCurrentAnimation()
{
    if (hasCurrentAnimation()) {
        m_document->removeAnimation(QStringLiteral("current"));
        if (m_currentAnimationName == QLatin1String("current")) {
            stop();
            m_currentAnimationName.clear();
        }
    }
}

bool AnimationController::hasCurrentAnimation() const
{
    return m_document && m_document->hasAnimation(QStringLiteral("current"));
}

void AnimationController::syncAnimationList()
{
    if (!m_treeWidget || !m_document) return;

    m_isSyncingUi = true;
    m_treeWidget->blockSignals(true);

    QString currentSelected = m_currentAnimationName;
    if (currentSelected.isEmpty() && !m_treeWidget->selectedItems().isEmpty()) {
        currentSelected = m_treeWidget->selectedItems().first()->text(0);
    }

    const auto &animMap = m_document->animations();

    m_treeWidget->clear();
    for (auto it = animMap.begin(); it != animMap.end(); ++it) {
        const QString &name = it.key();
        const SpriteAnimation &anim = it.value();

        auto *item = new QTreeWidgetItem(m_treeWidget);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, name);
        item->setText(1, QString::number(anim.fps));

        QString modeStr = tr("KEY_LOOP_MODE_LOOP");
        if (anim.loopMode == SpriteAnimation::Once) {
            modeStr = tr("KEY_LOOP_MODE_ONCE");
        } else if (anim.loopMode == SpriteAnimation::PingPong) {
            modeStr = tr("KEY_LOOP_MODE_PINGPONG");
        }
        item->setText(2, modeStr);
        item->setText(3, QString::number(anim.frameIndices.size()));
        item->setText(4, tr("KEY_DURATION_MS_FORMAT").arg(anim.durationMs()));

        item->setFlags(item->flags() | Qt::ItemIsEditable);

        if (name == currentSelected) {
            item->setSelected(true);
            m_treeWidget->setCurrentItem(item);
        }
    }

    m_treeWidget->blockSignals(false);
    m_isSyncingUi = false;

    if ((currentSelected.isEmpty() || !animMap.contains(currentSelected)) && m_treeWidget->topLevelItemCount() > 0) {
        selectAnimation(m_treeWidget->topLevelItem(0)->text(0));
    }

    if (m_timelineWidget) {
        m_timelineWidget->refresh();
    }

    emit animationListChanged();
}

void AnimationController::updatePreview()
{
    if (m_player) {
        int globalIdx = m_player->currentGlobalFrameIndex();
        if (globalIdx >= 0) {
            renderCurrentFrame(globalIdx);
        }
    }
}

void AnimationController::setShowPivotReticle(bool show)
{
    if (m_showPivotReticle != show) {
        m_showPivotReticle = show;
        updatePreview();
        emit showPivotReticleChanged(show);
    }
}

void AnimationController::renderCurrentFrame(int globalFrameIndex)
{
    if (!m_document || globalFrameIndex < 0 || globalFrameIndex >= m_document->frameCount()) {
        return;
    }

    QImage frameImg = m_document->frame(globalFrameIndex);
    if (frameImg.isNull()) return;
    QPixmap currentFrame = QPixmap::fromImage(frameImg);

    QRect envelope = m_document->computeAnimationEnvelope(m_currentAnimationName);
    int canvasW = qMax(1, envelope.width());
    int canvasH = qMax(1, envelope.height());
    m_previewScene->setSceneRect(0, 0, canvasW, canvasH);

    if (!m_previewPixmapItem || m_previewPixmapItem->scene() != m_previewScene) {
        m_previewScene->clear();
        m_reticleGroup = nullptr;
        m_previewPixmapItem = m_previewScene->addPixmap(currentFrame);
        m_previewPixmapItem->setZValue(1.0);
    } else {
        m_previewPixmapItem->setPixmap(currentFrame);
    }

    QPoint pivot = m_document->boxPivot(globalFrameIndex);
    qreal xOffset = envelope.x() - pivot.x();
    qreal yOffset = envelope.y() - pivot.y();
    m_previewPixmapItem->setPos(xOffset, yOffset);

    // Update or create reticle & ground line
    if (m_showPivotReticle) {
        if (!m_reticleGroup || m_reticleGroup->scene() != m_previewScene) {
            m_reticleGroup = new QGraphicsItemGroup();
            m_reticleGroup->setZValue(10.0);
            m_previewScene->addItem(m_reticleGroup);
        } else {
            qDeleteAll(m_reticleGroup->childItems());
        }

        qreal px = envelope.x();
        qreal py = envelope.y();

        // 1. Ground line (horizontal dashed line at pivot Y across entire canvas)
        QGraphicsLineItem *groundLine = new QGraphicsLineItem(0, py, canvasW, py);
        QPen groundPen(QColor(255, 60, 60, 180), 1.0, Qt::DashLine);
        groundPen.setCosmetic(true);
        groundLine->setPen(groundPen);
        m_reticleGroup->addToGroup(groundLine);

        // 2. Vertical line across canvas
        QGraphicsLineItem *vertLine = new QGraphicsLineItem(px, 0, px, canvasH);
        QPen vertPen(QColor(60, 160, 255, 140), 1.0, Qt::DashLine);
        vertPen.setCosmetic(true);
        vertLine->setPen(vertPen);
        m_reticleGroup->addToGroup(vertLine);

        // 3. Center crosshair circle
        QGraphicsEllipseItem *circle = new QGraphicsEllipseItem(px - 5, py - 5, 10, 10);
        QPen circlePen(QColor(0, 230, 255), 1.5);
        circlePen.setCosmetic(true);
        circle->setPen(circlePen);
        circle->setBrush(Qt::NoBrush);
        m_reticleGroup->addToGroup(circle);

        // Center dot
        QGraphicsEllipseItem *dot = new QGraphicsEllipseItem(px - 1.5, py - 1.5, 3, 3);
        dot->setPen(Qt::NoPen);
        dot->setBrush(QColor(0, 230, 255));
        m_reticleGroup->addToGroup(dot);

        m_reticleGroup->setVisible(true);
    } else {
        if (m_reticleGroup) {
            m_reticleGroup->setVisible(false);
        }
    }

    if (m_previewView) {
        m_previewView->viewport()->update();
    }
}

void AnimationController::updateScrubberState()
{
    int total = m_player ? m_player->frameCount() : 0;
    int curr = m_player ? m_player->currentSequenceIndex() : 0;

    if (m_scrubberSlider) {
        m_scrubberSlider->blockSignals(true);
        m_scrubberSlider->setEnabled(total > 1);
        m_scrubberSlider->setRange(0, qMax(0, total - 1));
        m_scrubberSlider->setValue(curr);
        m_scrubberSlider->blockSignals(false);
    }

    if (m_frameIndicator) {
        if (total > 0) {
            m_frameIndicator->setText(tr("KEY_FRAME_INDICATOR_FORMAT")
                                          .arg(curr + 1)
                                          .arg(total));
        } else {
            m_frameIndicator->setText(tr("KEY_NO_FRAMES"));
        }
    }
}

void AnimationController::onPlayerFrameChanged(int seqIndex, int globalIndex)
{
    renderCurrentFrame(globalIndex);
    if (m_timelineWidget) {
        m_timelineWidget->setActiveSequenceIndex(seqIndex);
    }
    updateScrubberState();
    emit frameChanged(seqIndex, globalIndex);
}

void AnimationController::onPlayerPlaybackStateChanged(bool playing)
{
    emit playbackStateChanged(playing);
}

void AnimationController::onTreeItemSelectionChanged()
{
    if (!m_treeWidget || m_treeWidget->selectedItems().isEmpty()) return;
    QString animName = m_treeWidget->selectedItems().first()->text(0);
    selectAnimation(animName);
}

void AnimationController::onTreeItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    selectAnimation(item->text(0));
}

void AnimationController::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_isSyncingUi || !item || !m_document) return;

    QString originalName = item->data(0, Qt::UserRole).toString();
    if (originalName.isEmpty() || !m_document->hasAnimation(originalName)) return;

    if (column == 0) { // Rename
        QString newName = item->text(0).trimmed();
        if (!newName.isEmpty() && newName != originalName) {
            renameAnimation(originalName, newName);
        } else {
            m_isSyncingUi = true;
            item->setText(0, originalName);
            m_isSyncingUi = false;
        }
    } else if (column == 1) { // FPS
        bool ok = false;
        int newFps = item->text(1).toInt(&ok);
        if (ok && newFps > 0) {
            const SpriteAnimation &anim = m_document->animation(originalName);
            if (anim.fps != newFps) {
                if (m_undoStack && originalName != QLatin1String("current")) {
                    m_undoStack->push(new ChangeAnimationPropertiesCommand(m_document, originalName, newFps, anim.loopMode));
                } else {
                    m_document->setAnimation(originalName, anim.frameIndices, newFps, anim.loop, anim.loopMode);
                }
                if (m_currentAnimationName == originalName) {
                    setFps(newFps);
                }
            }
        }
    }
}

void AnimationController::onScrubberValueChanged(int value)
{
    if (m_isSyncingUi) return;
    seek(value);
}
