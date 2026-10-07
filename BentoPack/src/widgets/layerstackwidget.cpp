/**
 * Copyright (c) 2026 Vincent LECOQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "widgets/layerstackwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLineEdit>
#include <QInputDialog>
#include <QIcon>
#include <QStyle>
#include <algorithm>

namespace {

struct BlendModeEntry {
    QPainter::CompositionMode mode;
    const char *id;
};

static const BlendModeEntry kBlendModes[] = {
    { QPainter::CompositionMode_SourceOver, "Normal" },
    { QPainter::CompositionMode_Multiply,   "Multiply" },
    { QPainter::CompositionMode_Screen,     "Screen" },
    { QPainter::CompositionMode_Overlay,    "Overlay" },
    { QPainter::CompositionMode_Darken,     "Darken" },
    { QPainter::CompositionMode_Lighten,    "Lighten" },
    { QPainter::CompositionMode_ColorDodge, "Color Dodge" },
    { QPainter::CompositionMode_ColorBurn,  "Color Burn" },
    { QPainter::CompositionMode_HardLight,  "Hard Light" },
    { QPainter::CompositionMode_SoftLight,  "Soft Light" },
    { QPainter::CompositionMode_Difference, "Difference" },
    { QPainter::CompositionMode_Exclusion,  "Exclusion" }
};

static const int kBlendModeCount = sizeof(kBlendModes) / sizeof(kBlendModes[0]);

} // namespace

LayerStackWidget::LayerStackWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void LayerStackWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // 1. Header controls (Active layer blend mode + opacity)
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);

    m_blendCombo = new QComboBox(this);
    m_blendCombo->setToolTip(tr("Blend mode of the active layer"));
    for (int i = 0; i < kBlendModeCount; ++i) {
        m_blendCombo->addItem(blendModeName(kBlendModes[i].mode), static_cast<int>(kBlendModes[i].mode));
    }
    connect(m_blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LayerStackWidget::onBlendModeComboChanged);
    headerLayout->addWidget(m_blendCombo, 1);

    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    m_opacitySlider->setToolTip(tr("Opacity of the active layer (0-100%)"));
    connect(m_opacitySlider, &QSlider::valueChanged,
            this, &LayerStackWidget::onOpacitySliderChanged);
    headerLayout->addWidget(m_opacitySlider, 1);

    m_opacityLabel = new QLabel("100%", this);
    m_opacityLabel->setFixedWidth(36);
    m_opacityLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerLayout->addWidget(m_opacityLabel);

    mainLayout->addLayout(headerLayout);

    // 2. Layer list widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_listWidget->setUniformItemSizes(true);
    m_listWidget->setMinimumHeight(120);
    connect(m_listWidget, &QListWidget::itemSelectionChanged,
            this, &LayerStackWidget::onItemSelectionChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &LayerStackWidget::onItemDoubleClicked);
    mainLayout->addWidget(m_listWidget, 1);

    // 3. Action toolbar (Add, Duplicate, Remove, Up, Down, Merge Down, Flatten)
    auto *toolsLayout = new QHBoxLayout();
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(3);

    m_btnAdd = new QPushButton("+", this);
    m_btnAdd->setToolTip(tr("Add a new transparent layer"));
    m_btnAdd->setFixedWidth(28);
    connect(m_btnAdd, &QPushButton::clicked, this, &LayerStackWidget::addLayerRequested);
    toolsLayout->addWidget(m_btnAdd);

    m_btnDuplicate = new QPushButton(QString::fromUtf8("⧉"), this);
    m_btnDuplicate->setToolTip(tr("Duplicate active layer"));
    m_btnDuplicate->setFixedWidth(28);
    connect(m_btnDuplicate, &QPushButton::clicked, this, &LayerStackWidget::duplicateLayerRequested);
    toolsLayout->addWidget(m_btnDuplicate);

    m_btnRemove = new QPushButton(QString::fromUtf8("−"), this);
    m_btnRemove->setToolTip(tr("Delete active layer"));
    m_btnRemove->setFixedWidth(28);
    connect(m_btnRemove, &QPushButton::clicked, this, &LayerStackWidget::removeLayerRequested);
    toolsLayout->addWidget(m_btnRemove);

    m_btnMoveUp = new QPushButton(QString::fromUtf8("▲"), this);
    m_btnMoveUp->setToolTip(tr("Move active layer up"));
    m_btnMoveUp->setFixedWidth(28);
    connect(m_btnMoveUp, &QPushButton::clicked, this, &LayerStackWidget::moveLayerUpRequested);
    toolsLayout->addWidget(m_btnMoveUp);

    m_btnMoveDown = new QPushButton(QString::fromUtf8("▼"), this);
    m_btnMoveDown->setToolTip(tr("Move active layer down"));
    m_btnMoveDown->setFixedWidth(28);
    connect(m_btnMoveDown, &QPushButton::clicked, this, &LayerStackWidget::moveLayerDownRequested);
    toolsLayout->addWidget(m_btnMoveDown);

    m_btnMergeDown = new QPushButton(QString::fromUtf8("⤓"), this);
    m_btnMergeDown->setToolTip(tr("Merge active layer down"));
    m_btnMergeDown->setFixedWidth(28);
    connect(m_btnMergeDown, &QPushButton::clicked, this, &LayerStackWidget::mergeLayerDownRequested);
    toolsLayout->addWidget(m_btnMergeDown);

    m_btnFlatten = new QPushButton(tr("Flatten"), this);
    m_btnFlatten->setToolTip(tr("Flatten all visible layers into a single layer"));
    connect(m_btnFlatten, &QPushButton::clicked, this, &LayerStackWidget::flattenLayersRequested);
    toolsLayout->addWidget(m_btnFlatten);

    mainLayout->addLayout(toolsLayout);

    // 4. Options Checkboxes
    m_checkSampleAll = new QCheckBox(tr("Sample all visible layers"), this);
    m_checkSampleAll->setToolTip(tr("Eyedropper, Bucket Fill and Wand detect color contours across all visible layers"));
    connect(m_checkSampleAll, &QCheckBox::toggled, this, &LayerStackWidget::sampleAllLayersChanged);
    mainLayout->addWidget(m_checkSampleAll);

    m_checkOnionCurrentOnly = new QCheckBox(tr("Onion skin: active layer only"), this);
    m_checkOnionCurrentOnly->setToolTip(tr("Isolates onion skinning to the active layer rather than the full composite"));
    connect(m_checkOnionCurrentOnly, &QCheckBox::toggled, this, &LayerStackWidget::onionSkinCurrentLayerOnlyChanged);
    mainLayout->addWidget(m_checkOnionCurrentOnly);
}

void LayerStackWidget::setLayers(const QList<CanvasLayer> &layers, int activeIndex)
{
    m_layers = layers;
    int maxIdx = static_cast<int>(m_layers.size() - 1);
    m_activeIndex = std::clamp(activeIndex, 0, std::max(0, maxIdx));
    refreshList();
    updateHeaderControls();
}

void LayerStackWidget::setActiveIndex(int index)
{
    if (m_layers.isEmpty()) {
        m_activeIndex = 0;
        return;
    }
    int maxIdx = static_cast<int>(m_layers.size() - 1);
    int clamped = std::clamp(index, 0, maxIdx);
    if (m_activeIndex != clamped) {
        m_activeIndex = clamped;
        refreshList();
        updateHeaderControls();
        emit activeLayerChanged(m_activeIndex);
    }
}

bool LayerStackWidget::isSampleAllLayers() const
{
    return m_checkSampleAll && m_checkSampleAll->isChecked();
}

bool LayerStackWidget::isOnionSkinCurrentLayerOnly() const
{
    return m_checkOnionCurrentOnly && m_checkOnionCurrentOnly->isChecked();
}

void LayerStackWidget::refreshList()
{
    m_updating = true;
    m_listWidget->clear();

    const int count = m_layers.size();
    // Top layer at top of list (row 0 = layer index count - 1)
    for (int i = count - 1; i >= 0; --i) {
        const CanvasLayer &layer = m_layers.at(i);

        auto *item = new QListWidgetItem(m_listWidget);
        item->setSizeHint(QSize(180, 32));
        item->setData(Qt::UserRole, i); // Store actual layer index

        auto *rowWidget = new QWidget();
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(4, 2, 4, 2);
        rowLayout->setSpacing(6);

        // Visibility Toggle Button
        auto *visBtn = new QToolButton(rowWidget);
        visBtn->setText(layer.visible ? QString::fromUtf8("👁") : QString::fromUtf8("⊘"));
        visBtn->setToolTip(layer.visible ? tr("Hide layer") : tr("Show layer"));
        visBtn->setAutoRaise(true);
        visBtn->setFixedSize(22, 22);
        int layerIdx = i;
        connect(visBtn, &QToolButton::clicked, this, [this, layerIdx]() {
            if (layerIdx >= 0 && layerIdx < m_layers.size()) {
                bool nextVis = !m_layers[layerIdx].visible;
                m_layers[layerIdx].visible = nextVis;
                refreshList();
                emit layerVisibilityChanged(layerIdx, nextVis);
            }
        });
        rowLayout->addWidget(visBtn);

        // Lock Toggle Button
        auto *lockBtn = new QToolButton(rowWidget);
        lockBtn->setText(layer.locked ? QString::fromUtf8("🔒") : QString::fromUtf8("🔓"));
        lockBtn->setToolTip(layer.locked ? tr("Unlock layer") : tr("Lock layer"));
        lockBtn->setAutoRaise(true);
        lockBtn->setFixedSize(22, 22);
        connect(lockBtn, &QToolButton::clicked, this, [this, layerIdx]() {
            if (layerIdx >= 0 && layerIdx < m_layers.size()) {
                bool nextLock = !m_layers[layerIdx].locked;
                m_layers[layerIdx].locked = nextLock;
                refreshList();
                emit layerLockChanged(layerIdx, nextLock);
            }
        });
        rowLayout->addWidget(lockBtn);

        // Layer Name Label
        auto *nameLabel = new QLabel(layer.name.isEmpty() ? tr("Layer %1").arg(layerIdx + 1) : layer.name, rowWidget);
        if (!layer.visible) {
            QFont f = nameLabel->font();
            f.setItalic(true);
            nameLabel->setFont(f);
            nameLabel->setEnabled(false);
        }
        rowLayout->addWidget(nameLabel, 1);

        m_listWidget->setItemWidget(item, rowWidget);

        if (i == m_activeIndex) {
            m_listWidget->setCurrentItem(item);
        }
    }

    m_updating = false;
}

void LayerStackWidget::updateHeaderControls()
{
    m_updating = true;
    bool hasActive = (m_activeIndex >= 0 && m_activeIndex < m_layers.size());

    m_blendCombo->setEnabled(hasActive);
    m_opacitySlider->setEnabled(hasActive);
    m_btnRemove->setEnabled(m_layers.size() > 1);
    m_btnMergeDown->setEnabled(hasActive && m_activeIndex > 0);
    m_btnMoveUp->setEnabled(hasActive && m_activeIndex < m_layers.size() - 1);
    m_btnMoveDown->setEnabled(hasActive && m_activeIndex > 0);
    m_btnFlatten->setEnabled(m_layers.size() > 1);

    if (hasActive) {
        const CanvasLayer &lyr = m_layers.at(m_activeIndex);
        int comboIdx = indexFromBlendMode(lyr.blendMode);
        m_blendCombo->setCurrentIndex(comboIdx >= 0 ? comboIdx : 0);

        int opPct = static_cast<int>(std::round((lyr.opacity / 255.0) * 100.0));
        m_opacitySlider->setValue(opPct);
        m_opacityLabel->setText(QString("%1%").arg(opPct));
    } else {
        m_opacitySlider->setValue(100);
        m_opacityLabel->setText("100%");
    }
    m_updating = false;
}

void LayerStackWidget::onItemSelectionChanged()
{
    if (m_updating) return;

    auto *cur = m_listWidget->currentItem();
    if (!cur) return;

    int layerIdx = cur->data(Qt::UserRole).toInt();
    if (layerIdx >= 0 && layerIdx < m_layers.size() && layerIdx != m_activeIndex) {
        m_activeIndex = layerIdx;
        updateHeaderControls();
        emit activeLayerChanged(m_activeIndex);
    }
}

void LayerStackWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;
    int layerIdx = item->data(Qt::UserRole).toInt();
    if (layerIdx < 0 || layerIdx >= m_layers.size()) return;

    QString currentName = m_layers.at(layerIdx).name;
    if (currentName.isEmpty()) currentName = tr("Layer %1").arg(layerIdx + 1);

    bool ok = false;
    QString newName = QInputDialog::getText(this, tr("Rename Layer"), tr("Layer name:"),
                                           QLineEdit::Normal, currentName, &ok);
    if (ok && !newName.trimmed().isEmpty()) {
        m_layers[layerIdx].name = newName.trimmed();
        refreshList();
        emit layerNameChanged(layerIdx, m_layers[layerIdx].name);
    }
}

void LayerStackWidget::onOpacitySliderChanged(int value)
{
    if (m_updating) return;
    if (m_activeIndex < 0 || m_activeIndex >= m_layers.size()) return;

    m_opacityLabel->setText(QString("%1%").arg(value));
    quint8 op255 = static_cast<quint8>(std::clamp(static_cast<int>(std::round((value / 100.0) * 255.0)), 0, 255));
    m_layers[m_activeIndex].opacity = op255;
    emit layerOpacityChanged(m_activeIndex, op255);
}

void LayerStackWidget::onBlendModeComboChanged(int comboIndex)
{
    if (m_updating) return;
    if (m_activeIndex < 0 || m_activeIndex >= m_layers.size()) return;

    QPainter::CompositionMode mode = blendModeFromIndex(comboIndex);
    m_layers[m_activeIndex].blendMode = mode;
    emit layerBlendModeChanged(m_activeIndex, mode);
}

void LayerStackWidget::retranslateUi()
{
    m_btnAdd->setToolTip(tr("Add a new transparent layer"));
    m_btnDuplicate->setToolTip(tr("Duplicate active layer"));
    m_btnRemove->setToolTip(tr("Delete active layer"));
    m_btnMoveUp->setToolTip(tr("Move active layer up"));
    m_btnMoveDown->setToolTip(tr("Move active layer down"));
    m_btnMergeDown->setToolTip(tr("Merge active layer down"));
    m_btnFlatten->setText(tr("Flatten"));
    m_btnFlatten->setToolTip(tr("Flatten all visible layers into a single layer"));
    m_checkSampleAll->setText(tr("Sample all visible layers"));
    m_checkSampleAll->setToolTip(tr("Eyedropper, Bucket Fill and Wand detect color contours across all visible layers"));
    m_checkOnionCurrentOnly->setText(tr("Onion skin: active layer only"));
    m_checkOnionCurrentOnly->setToolTip(tr("Isolates onion skinning to the active layer rather than the full composite"));

    m_updating = true;
    m_blendCombo->clear();
    for (int i = 0; i < kBlendModeCount; ++i) {
        m_blendCombo->addItem(blendModeName(kBlendModes[i].mode), static_cast<int>(kBlendModes[i].mode));
    }
    m_updating = false;

    refreshList();
    updateHeaderControls();
}

QString LayerStackWidget::blendModeName(QPainter::CompositionMode mode)
{
    switch (mode) {
    case QPainter::CompositionMode_SourceOver: return tr("Normal");
    case QPainter::CompositionMode_Multiply:   return tr("Multiply");
    case QPainter::CompositionMode_Screen:     return tr("Screen");
    case QPainter::CompositionMode_Overlay:    return tr("Overlay");
    case QPainter::CompositionMode_Darken:     return tr("Darken");
    case QPainter::CompositionMode_Lighten:    return tr("Lighten");
    case QPainter::CompositionMode_ColorDodge: return tr("Color Dodge");
    case QPainter::CompositionMode_ColorBurn:  return tr("Color Burn");
    case QPainter::CompositionMode_HardLight:  return tr("Hard Light");
    case QPainter::CompositionMode_SoftLight:  return tr("Soft Light");
    case QPainter::CompositionMode_Difference: return tr("Difference");
    case QPainter::CompositionMode_Exclusion:  return tr("Exclusion");
    default: return tr("Normal");
    }
}

QPainter::CompositionMode LayerStackWidget::blendModeFromIndex(int index)
{
    if (index >= 0 && index < kBlendModeCount) {
        return kBlendModes[index].mode;
    }
    return QPainter::CompositionMode_SourceOver;
}

int LayerStackWidget::indexFromBlendMode(QPainter::CompositionMode mode)
{
    for (int i = 0; i < kBlendModeCount; ++i) {
        if (kBlendModes[i].mode == mode) return i;
    }
    return 0;
}
