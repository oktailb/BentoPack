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

#ifndef WATCHCONFIGDIALOG_H
#define WATCHCONFIGDIALOG_H

#include <QDialog>
#include <memory>
#include "watcher/livewatchmanager.h"
#include "bentopackwidgets_export.h"

class QRadioButton;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QGroupBox;
class SpriteDocument;
class ProjectController;

/**
 * @brief Interactive configuration window for the GUI Live Watch (Hot-Reload Zéro-Clic) system.
 *
 * Allows users to specify:
 * - What to watch (single .ase/.aseprite/.png file or directory, with extensions and debounce filter)
 * - Where to export (target directory and base file name with live calculated path preview)
 * - All engine settings (Godot 4, Unity, Unreal Paper2D, TexturePacker JSON, GPU VRAM KTX2, 2D CDT Mesh,
 *   M22 Cel deduplication, M22 delta-patch slicing, POT, padding, trim)
 * - Fast 1-click presets from current open project and manual test trigger.
 */
class BENTOPACK_WIDGETS_EXPORT WatchConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WatchConfigDialog(LiveWatchManager *watchManager,
                               const SpriteDocument *document = nullptr,
                               ProjectController *controller = nullptr,
                               QWidget *parent = nullptr);
    ~WatchConfigDialog() override;

    LiveWatchConfig config() const;

private slots:
    void onBrowseSourceFile();
    void onBrowseSourceDir();
    void onBrowseOutputDir();
    void onUseCurrentProject();
    void onFormatChanged(int index);
    void updateComputedPathPreview();
    void onToggleWatchClicked();
    void onTestExportClicked();
    void onSaveDefaultsClicked();
    void updateStatusUI(WatchState state, const QString &message);

private:
    void setupUI();
    void loadConfigToUI(const LiveWatchConfig &cfg);
    LiveWatchConfig buildConfigFromUI() const;

    LiveWatchManager *m_watchManager = nullptr;
    const SpriteDocument *m_document = nullptr;
    ProjectController *m_controller = nullptr;

    // UI Widgets
    QLabel *m_lblStatusBadge = nullptr;
    QLabel *m_lblStatusDetail = nullptr;

    // Group 1: Source ("Quoi surveiller")
    QRadioButton *m_rbSingleFile = nullptr;
    QRadioButton *m_rbDirectory = nullptr;
    QLineEdit *m_txtSourcePath = nullptr;
    QPushButton *m_btnBrowseSourceFile = nullptr;
    QPushButton *m_btnBrowseSourceDir = nullptr;
    QPushButton *m_btnUseCurrentProject = nullptr;
    QLineEdit *m_txtFileFilters = nullptr;
    QSpinBox *m_spinDebounce = nullptr;

    // Group 2: Output ("Où exporter")
    QCheckBox *m_chkAutoExport = nullptr;
    QLineEdit *m_txtOutputDir = nullptr;
    QPushButton *m_btnBrowseOutputDir = nullptr;
    QLineEdit *m_txtBaseName = nullptr;
    QLabel *m_lblComputedPath = nullptr;

    // Group 3: Engine & Pipeline Options ("Options du Moteur")
    QComboBox *m_comboFormat = nullptr;
    QComboBox *m_comboTextureFormat = nullptr;
    QComboBox *m_comboMeshMode = nullptr;
    QCheckBox *m_chkModularCelDedup = nullptr;
    QCheckBox *m_chkDeltaPatch = nullptr;
    QCheckBox *m_chkDedupFrames = nullptr;
    QCheckBox *m_chkPowerOfTwo = nullptr;
    QCheckBox *m_chkTrim = nullptr;
    QSpinBox *m_spinPadding = nullptr;

    // Action Buttons
    QPushButton *m_btnToggleWatch = nullptr;
    QPushButton *m_btnTestExport = nullptr;
    QPushButton *m_btnSaveDefaults = nullptr;
    QPushButton *m_btnClose = nullptr;
};

#endif // WATCHCONFIGDIALOG_H
