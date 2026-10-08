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

#ifndef LAYERSTACKWIDGET_H
#define LAYERSTACKWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QPainter>
#include "widgets/pixelcanvas.h"
#include "bentopackwidgets_export.h"

/**
 * @brief Professional layer stack dock widget for surgical multi-layer editing (M18).
 *
 * Provides reordering, visibility/lock toggling, blend modes, opacity sliders,
 * merging, flattening, and multi-layer sampling configuration.
 */
class BENTOPACK_WIDGETS_EXPORT LayerStackWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LayerStackWidget(QWidget *parent = nullptr);
    ~LayerStackWidget() override = default;

    void setLayers(const QList<CanvasLayer> &layers, int activeIndex = 0);
    int activeIndex() const { return m_activeIndex; }
    void setActiveIndex(int index);
    void setActiveLayerIndex(int index) { setActiveIndex(index); }

    bool isSampleAllLayers() const;
    bool isOnionSkinCurrentLayerOnly() const;

    QListWidget* listWidget() const { return m_listWidget; }
    QPushButton* addLayerButton() const { return m_btnAdd; }
    QPushButton* duplicateLayerButton() const { return m_btnDuplicate; }
    QPushButton* removeLayerButton() const { return m_btnRemove; }
    QPushButton* moveLayerUpButton() const { return m_btnMoveUp; }
    QPushButton* moveLayerDownButton() const { return m_btnMoveDown; }
    QPushButton* mergeLayerDownButton() const { return m_btnMergeDown; }
    QPushButton* flattenLayersButton() const { return m_btnFlatten; }
    QCheckBox* sampleAllLayersCheckBox() const { return m_checkSampleAll; }
    QCheckBox* onionSkinCurrentLayerOnlyCheckBox() const { return m_checkOnionCurrentOnly; }
    QComboBox* blendModeComboBox() const { return m_blendCombo; }
    QSlider* opacitySlider() const { return m_opacitySlider; }

public slots:
    void retranslateUi();

signals:
    void activeLayerChanged(int index);
    void layerVisibilityChanged(int index, bool visible);
    void layerLockChanged(int index, bool locked);
    void layerOpacityChanged(int index, quint8 opacity);
    void layerBlendModeChanged(int index, QPainter::CompositionMode mode);
    void layerNameChanged(int index, const QString &name);

    void addLayerRequested();
    void duplicateLayerRequested();
    void removeLayerRequested();
    void moveLayerUpRequested();
    void moveLayerDownRequested();
    void mergeLayerDownRequested();
    void flattenLayersRequested();

    void sampleAllLayersChanged(bool enabled);
    void onionSkinCurrentLayerOnlyChanged(bool enabled);

private slots:
    void onItemSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onOpacitySliderChanged(int value);
    void onBlendModeComboChanged(int comboIndex);

private:
    void setupUi();
    void refreshList();
    void updateHeaderControls();

    static QString blendModeName(QPainter::CompositionMode mode);
    static QPainter::CompositionMode blendModeFromIndex(int index);
    static int indexFromBlendMode(QPainter::CompositionMode mode);

private:
    QList<CanvasLayer>  m_layers;
    int                 m_activeIndex = 0;
    bool                m_updating = false;

    // Header Controls (Active Layer Properties)
    QComboBox*          m_blendCombo = nullptr;
    QSlider*            m_opacitySlider = nullptr;
    QLabel*             m_opacityLabel = nullptr;

    // Layer List
    QListWidget*        m_listWidget = nullptr;

    // Action Toolbar
    QPushButton*        m_btnAdd = nullptr;
    QPushButton*        m_btnDuplicate = nullptr;
    QPushButton*        m_btnRemove = nullptr;
    QPushButton*        m_btnMoveUp = nullptr;
    QPushButton*        m_btnMoveDown = nullptr;
    QPushButton*        m_btnMergeDown = nullptr;
    QPushButton*        m_btnFlatten = nullptr;

    // Options Checkboxes
    QCheckBox*          m_checkSampleAll = nullptr;
    QCheckBox*          m_checkOnionCurrentOnly = nullptr;
};

#endif // LAYERSTACKWIDGET_H
