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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QTextBrowser>
#include "bentopackwidgets_export.h"

class QNetworkAccessManager;
class QNetworkReply;
class QVBoxLayout;

/**
 * @brief Modal configuration dialog for BentoPack.
 *
 * Provides a structured settings interface with a search filter field and category
 * tree on the left, and corresponding setting pages in a stacked widget on the right.
 * Supports configuring General options, Atlas visuals, Export defaults, Plugin management,
 * Git commit author identity, and GitHub update detection.
 */
class BENTOPACK_WIDGETS_EXPORT SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    enum PageIndex {
        PageGeneral = 0,
        PageAtlas = 1,
        PageExport = 2,
        PageRetro = 3,
        PagePlugins = 4,
        PageGit = 5,
        PageUpdates = 6
    };

    explicit SettingsDialog(QWidget *parent = nullptr, PageIndex initialPage = PageGeneral);
    ~SettingsDialog() override;

    void setCurrentPage(PageIndex page);

public slots:
    void applySettings();
    void restoreDefaults();

private slots:
    void onSearchTextChanged(const QString &text);
    void onCategoryItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onDetectSystemGitIdentity();
    void onOpenPluginsFolder();
    void onReloadPlugins();
    void onCheckForUpdates();
    void onUpdateReplyFinished(QNetworkReply *reply);
    void onFilterTreeSelectionChanged();
    void onExtractorTreeSelectionChanged();

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUI();
    void retranslateUi();
    QWidget* createGeneralPage();
    QWidget* createAtlasPage();
    QWidget* createExportPage();
    QWidget* createRetroPage();
    QWidget* createPluginsPage();
    QWidget* createGitPage();
    QWidget* createUpdatesPage();

    void loadSettings();
    void saveSettings();
    void refreshPluginTrees();

    // UI Elements - Navigation & Search
    QLineEdit      *m_searchEdit = nullptr;
    QTreeWidget    *m_categoryTree = nullptr;
    QStackedWidget *m_pagesStack = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;

    // Tree navigation items
    QTreeWidgetItem *m_itemGeneral = nullptr;
    QTreeWidgetItem *m_itemAtlas = nullptr;
    QTreeWidgetItem *m_itemExport = nullptr;
    QTreeWidgetItem *m_itemRetro = nullptr;
    QTreeWidgetItem *m_itemPlugins = nullptr;
    QTreeWidgetItem *m_itemGit = nullptr;
    QTreeWidgetItem *m_itemUpdates = nullptr;

    // General Page
    QLabel    *m_hdrGeneral = nullptr;
    QGroupBox *m_grpHistory = nullptr;
    QLabel    *m_lblUndoLimit = nullptr;
    QLabel    *m_lblMaxRecentFiles = nullptr;
    QSpinBox  *m_spinUndoLimit = nullptr;
    QSpinBox  *m_spinMaxRecentFiles = nullptr;

    QGroupBox *m_grpExtraction = nullptr;
    QLabel    *m_lblAlphaThreshold = nullptr;
    QLabel    *m_lblBgRemovalTol = nullptr;
    QLabel    *m_lblBgMinAlpha = nullptr;
    QLabel    *m_lblVertTolerance = nullptr;
    QSpinBox  *m_spinAlphaThreshold = nullptr;
    QSpinBox  *m_spinBgRemovalTol = nullptr;
    QSpinBox  *m_spinBgMinAlpha = nullptr;
    QSpinBox  *m_spinVertTolerance = nullptr;

    QGroupBox *m_grpAnimation = nullptr;
    QLabel    *m_lblDefaultFps = nullptr;
    QLabel    *m_lblMaxFps = nullptr;
    QSpinBox  *m_spinDefaultFps = nullptr;
    QSpinBox  *m_spinMaxFps = nullptr;
    QCheckBox *m_chkAutoPlayOnSelection = nullptr;

    QGroupBox *m_grpLang = nullptr;
    QLabel    *m_lblLangApp = nullptr;
    QComboBox *m_comboLanguage = nullptr;
    QLabel    *m_lblLangHint = nullptr;
    QGroupBox *m_grpStartup = nullptr;
    QCheckBox *m_chkCheckUpdatesOnStartup = nullptr;
    QCheckBox *m_chkReopenLastProject = nullptr;
    QCheckBox *m_chkAutoSaveSession = nullptr;

    // Atlas Page
    QLabel         *m_hdrAtlas = nullptr;
    QGroupBox      *m_grpZoom = nullptr;
    QLabel         *m_lblZoomStep = nullptr;
    QLabel         *m_lblZoomMin = nullptr;
    QLabel         *m_lblZoomMax = nullptr;
    QLabel         *m_lblFitPadding = nullptr;
    QDoubleSpinBox *m_spinZoomStep = nullptr;
    QDoubleSpinBox *m_spinZoomMin = nullptr;
    QDoubleSpinBox *m_spinZoomMax = nullptr;
    QSpinBox       *m_spinFitPadding = nullptr;

    QGroupBox      *m_grpSlicing = nullptr;
    QLabel         *m_lblMinSliceSize = nullptr;
    QSpinBox       *m_spinMinSliceSize = nullptr;

    QGroupBox      *m_grpNudge = nullptr;
    QLabel         *m_lblNudgeSmall = nullptr;
    QLabel         *m_lblNudgeLarge = nullptr;
    QSpinBox       *m_spinNudgeSmall = nullptr;
    QSpinBox       *m_spinNudgeLarge = nullptr;

    QGroupBox      *m_grpBoxStyle = nullptr;
    QLabel         *m_lblHandleSize = nullptr;
    QLabel         *m_lblHandleMargin = nullptr;
    QDoubleSpinBox *m_spinHandleSize = nullptr;
    QDoubleSpinBox *m_spinHandleMargin = nullptr;
    QLabel         *m_lblSelectedBoxColor = nullptr;
    QLabel         *m_lblSelectedBoxFillColor = nullptr;
    QLabel         *m_lblUnselectedBoxColor = nullptr;
    QLabel         *m_lblMarqueeColor = nullptr;
    QLabel         *m_lblPreviewColor = nullptr;
    QLabel         *m_lblMeshWireframeColor = nullptr;
    QPushButton    *m_btnSelectedBoxColor = nullptr;
    QPushButton    *m_btnSelectedBoxFillColor = nullptr;
    QPushButton    *m_btnUnselectedBoxColor = nullptr;
    QPushButton    *m_btnMarqueeColor = nullptr;
    QPushButton    *m_btnPreviewColor = nullptr;
    QPushButton    *m_btnMeshWireframeColor = nullptr;
    QCheckBox      *m_chkShowPolygonMesh = nullptr;

    QColor m_colSelectedBox;
    QColor m_colSelectedBoxFill;
    QColor m_colUnselectedBox;
    QColor m_colMarquee;
    QColor m_colPreview;
    QColor m_colMeshWireframe;

    void updateColorButton(QPushButton *btn, const QColor &color);
    void pickColor(QWidget *parent, QColor &targetColor, QPushButton *button, bool alpha = false);

    // Export Page
    QLabel    *m_hdrExport = nullptr;
    QGroupBox *m_grpExportDefaults = nullptr;
    QLabel    *m_lblDefaultFormat = nullptr;
    QLabel    *m_lblDefaultTextureFormat = nullptr;
    QLabel    *m_lblDefaultAlgorithm = nullptr;
    QLabel    *m_lblZstdLevel = nullptr;
    QComboBox *m_comboDefaultFormat = nullptr;
    QComboBox *m_comboDefaultTextureFormat = nullptr;
    QComboBox *m_comboDefaultAlgorithm = nullptr;
    QCheckBox *m_chkDefaultZstd = nullptr;
    QSpinBox  *m_spinDefaultZstdLevel = nullptr;

    QGroupBox *m_grpPackDefaults = nullptr;
    QLabel    *m_lblDefaultPadding = nullptr;
    QLabel    *m_lblDefaultBorderPadding = nullptr;
    QLabel    *m_lblDefaultExtrude = nullptr;
    QLabel    *m_lblMaxDimensions = nullptr;
    QSpinBox  *m_spinDefaultPadding = nullptr;
    QSpinBox  *m_spinDefaultBorderPadding = nullptr;
    QSpinBox  *m_spinDefaultExtrude = nullptr;
    QComboBox *m_comboMaxDimensions = nullptr;
    QCheckBox *m_chkDefaultPowerOfTwo = nullptr;
    QCheckBox *m_chkDefaultForceSquare = nullptr;
    QCheckBox *m_chkDefaultDeduplicate = nullptr;
    QCheckBox *m_chkDefaultTrimSprites = nullptr;
    QCheckBox *m_chkNormalMapYFlip = nullptr;

    // Retro Page
    QLabel    *m_hdrRetro = nullptr;
    QCheckBox *m_chkRetroEnabled = nullptr;
    QGroupBox *m_grpRetroProfile = nullptr;
    QLabel    *m_lblRetroProfile = nullptr;
    QComboBox *m_comboRetroProfile = nullptr;
    QLabel    *m_lblRetroProfileDesc = nullptr;
    QGroupBox *m_grpRetroLimits = nullptr;
    QLabel    *m_lblScanlineLimit = nullptr;
    QSpinBox  *m_spinScanlineLimit = nullptr;
    QLabel    *m_lblMaxVramTiles = nullptr;
    QSpinBox  *m_spinMaxVramTiles = nullptr;
    QLabel    *m_lblTileSize = nullptr;
    QComboBox *m_comboTileSize = nullptr;
    QLabel    *m_lblGuideLineThickness = nullptr;
    QSpinBox  *m_spinGuideLineThickness = nullptr;
    QLabel    *m_lblMaxSpritesPerScanline = nullptr;
    QSpinBox  *m_spinMaxSpritesPerScanline = nullptr;
    QLabel    *m_lblMaxColorsPerSprite = nullptr;
    QSpinBox  *m_spinMaxColorsPerSprite = nullptr;
    QGroupBox *m_grpRetroVisuals = nullptr;
    QCheckBox *m_chkShowTileGrid = nullptr;
    QCheckBox *m_chkShowScanlineGuide = nullptr;
    QCheckBox *m_chkShowTimelineBadges = nullptr;
    QCheckBox *m_chkShowVramGauge = nullptr;
    QCheckBox *m_chkShowAtlasGuides = nullptr;

    // Plugins Page
    QLabel      *m_hdrPlugins = nullptr;
    QTabWidget  *m_tabPlugins = nullptr;
    QTreeWidget *m_treeFilters = nullptr;
    QTreeWidget *m_treeExtractors = nullptr;
    QLabel      *m_lblPluginInfo = nullptr;
    QGroupBox   *m_grpPluginConfig = nullptr;
    QVBoxLayout *m_pluginConfigLayout = nullptr;
    QWidget     *m_currentPluginConfigWidget = nullptr;
    QPushButton *m_btnOpenPluginsFolder = nullptr;
    QPushButton *m_btnReloadPlugins = nullptr;
    void updatePluginConfigWidget(QWidget *customWidget);

    // Git Page
    QLabel      *m_hdrGit = nullptr;
    QGroupBox   *m_grpAuthor = nullptr;
    QLabel      *m_lblAuthorInfo = nullptr;
    QLabel      *m_lblAuthorName = nullptr;
    QLabel      *m_lblAuthorEmail = nullptr;
    QLineEdit   *m_editGitAuthorName = nullptr;
    QLineEdit   *m_editGitAuthorEmail = nullptr;
    QPushButton *m_btnDetectGit = nullptr;
    QLabel      *m_lblGitStatus = nullptr;
    QGroupBox   *m_grpGitEngine = nullptr;
    QLabel      *m_lblGitDesc = nullptr;

    // Updates Page
    QLabel                *m_hdrUpdates = nullptr;
    QLabel                *m_lblCurrentVersion = nullptr;
    QPushButton           *m_btnCheckUpdates = nullptr;
    QLabel                *m_lblUpdateStatus = nullptr;
    QGroupBox             *m_grpUpdateDetails = nullptr;
    QLabel                *m_lblLatestVersion = nullptr;
    QLabel                *m_lblReleaseDate = nullptr;
    QTextBrowser          *m_textReleaseNotes = nullptr;
    QPushButton           *m_btnDownloadUpdate = nullptr;
    QString                m_latestReleaseUrl;
    QNetworkAccessManager *m_networkManager = nullptr;
};

#endif // SETTINGSDIALOG_H
