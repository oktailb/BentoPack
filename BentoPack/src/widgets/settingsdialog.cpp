/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#include "include/widgets/settingsdialog.h"
#include "config/appconfig.h"
#include "include/project/sessionmanager.h"
#include "include/localizationmanager.h"
#include "filters/filterregistry.h"
#include "extractor/extractorregistry.h"
#include "generated/version.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <QEvent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QCoreApplication>

SettingsDialog::SettingsDialog(QWidget *parent, PageIndex initialPage)
    : QDialog(parent)
{
    setupUI();
    loadSettings();
    setCurrentPage(initialPage);

    setWindowTitle(tr("KEY_SETTINGS_TITLE") + QStringLiteral(" - BentoPack"));
    resize(780, 540);
    setMinimumSize(660, 460);
}

SettingsDialog::~SettingsDialog()
{
    if (m_networkManager) {
        m_networkManager->deleteLater();
        m_networkManager = nullptr;
    }
}

void SettingsDialog::setCurrentPage(PageIndex page)
{
    if (m_pagesStack) {
        m_pagesStack->setCurrentIndex(static_cast<int>(page));
    }
    if (m_categoryTree) {
        switch (page) {
        case PageGeneral:
            if (m_itemGeneral) m_categoryTree->setCurrentItem(m_itemGeneral);
            break;
        case PageAtlas:
            if (m_itemAtlas) m_categoryTree->setCurrentItem(m_itemAtlas);
            break;
        case PageExport:
            if (m_itemExport) m_categoryTree->setCurrentItem(m_itemExport);
            break;
        case PagePlugins:
            if (m_itemPlugins) m_categoryTree->setCurrentItem(m_itemPlugins);
            break;
        case PageGit:
            if (m_itemGit) m_categoryTree->setCurrentItem(m_itemGit);
            break;
        case PageUpdates:
            if (m_itemUpdates) m_categoryTree->setCurrentItem(m_itemUpdates);
            break;
        }
    }
}

void SettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // Left Navigation Panel
    QWidget *leftWidget = new QWidget(splitter);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 8, 0);
    leftLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(leftWidget);
    m_searchEdit->setPlaceholderText(tr("KEY_SETTINGS_SEARCH_PLACEHOLDER"));
    m_searchEdit->setClearButtonEnabled(true);
    leftLayout->addWidget(m_searchEdit);

    m_categoryTree = new QTreeWidget(leftWidget);
    m_categoryTree->setHeaderHidden(true);
    m_categoryTree->setRootIsDecorated(false);
    m_categoryTree->setIndentation(12);

    m_itemGeneral = new QTreeWidgetItem(m_categoryTree);
    m_itemGeneral->setText(0, tr("KEY_SETTINGS_CAT_GENERAL"));
    m_itemGeneral->setData(0, Qt::UserRole, PageGeneral);

    m_itemAtlas = new QTreeWidgetItem(m_categoryTree);
    m_itemAtlas->setText(0, tr("KEY_SETTINGS_CAT_ATLAS"));
    m_itemAtlas->setData(0, Qt::UserRole, PageAtlas);

    m_itemExport = new QTreeWidgetItem(m_categoryTree);
    m_itemExport->setText(0, tr("Export & VRAM"));
    m_itemExport->setData(0, Qt::UserRole, PageExport);

    m_itemPlugins = new QTreeWidgetItem(m_categoryTree);
    m_itemPlugins->setText(0, tr("Plugins & Extensions"));
    m_itemPlugins->setData(0, Qt::UserRole, PagePlugins);

    m_itemGit = new QTreeWidgetItem(m_categoryTree);
    m_itemGit->setText(0, tr("KEY_SETTINGS_CAT_GIT"));
    m_itemGit->setData(0, Qt::UserRole, PageGit);

    m_itemUpdates = new QTreeWidgetItem(m_categoryTree);
    m_itemUpdates->setText(0, tr("Updates"));
    m_itemUpdates->setData(0, Qt::UserRole, PageUpdates);

    leftLayout->addWidget(m_categoryTree);
    splitter->addWidget(leftWidget);

    // Right Pages Stack
    m_pagesStack = new QStackedWidget(splitter);
    m_pagesStack->addWidget(createGeneralPage());
    m_pagesStack->addWidget(createAtlasPage());
    m_pagesStack->addWidget(createExportPage());
    m_pagesStack->addWidget(createPluginsPage());
    m_pagesStack->addWidget(createGitPage());
    m_pagesStack->addWidget(createUpdatesPage());
    splitter->addWidget(m_pagesStack);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 220 << 520);
    mainLayout->addWidget(splitter, 1);

    // Bottom Button Box
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply | QDialogButtonBox::RestoreDefaults,
        this
    );
    mainLayout->addWidget(m_buttonBox);

    // Connections
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SettingsDialog::onSearchTextChanged);
    connect(m_categoryTree, &QTreeWidget::currentItemChanged, this, &SettingsDialog::onCategoryItemChanged);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        applySettings();
        accept();
    });
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    QPushButton *applyBtn = m_buttonBox->button(QDialogButtonBox::Apply);
    if (applyBtn) {
        connect(applyBtn, &QPushButton::clicked, this, &SettingsDialog::applySettings);
    }

    QPushButton *restoreBtn = m_buttonBox->button(QDialogButtonBox::RestoreDefaults);
    if (restoreBtn) {
        connect(restoreBtn, &QPushButton::clicked, this, &SettingsDialog::restoreDefaults);
    }

    retranslateUi();
}

QWidget* SettingsDialog::createGeneralPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 0, 0, 0);

    m_hdrGeneral = new QLabel(tr("KEY_SETTINGS_HDR_GENERAL"));
    m_hdrGeneral->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; margin-bottom: 8px;"));
    layout->addWidget(m_hdrGeneral);

    // Project & History Group
    m_grpHistory = new QGroupBox(tr("KEY_SETTINGS_GRP_HISTORY"), page);
    QFormLayout *formHistory = new QFormLayout(m_grpHistory);

    m_spinUndoLimit = new QSpinBox(m_grpHistory);
    m_spinUndoLimit->setRange(5, 500);
    m_spinUndoLimit->setSuffix(tr("KEY_SETTINGS_SUFFIX_ACTIONS"));
    m_lblUndoLimit = new QLabel(tr("KEY_SETTINGS_UNDO_LIMIT"), m_grpHistory);
    formHistory->addRow(m_lblUndoLimit, m_spinUndoLimit);

    m_spinMaxRecentFiles = new QSpinBox(m_grpHistory);
    m_spinMaxRecentFiles->setRange(1, 30);
    m_lblMaxRecentFiles = new QLabel(tr("KEY_SETTINGS_MAX_RECENT_FILES"), m_grpHistory);
    formHistory->addRow(m_lblMaxRecentFiles, m_spinMaxRecentFiles);
    layout->addWidget(m_grpHistory);

    // Segmentation & Background Group
    m_grpExtraction = new QGroupBox(tr("KEY_SETTINGS_GRP_EXTRACTION"), page);
    QFormLayout *formExtraction = new QFormLayout(m_grpExtraction);

    m_spinAlphaThreshold = new QSpinBox(m_grpExtraction);
    m_spinAlphaThreshold->setRange(0, 255);
    m_lblAlphaThreshold = new QLabel(tr("KEY_SETTINGS_ALPHA_THRESHOLD"), m_grpExtraction);
    formExtraction->addRow(m_lblAlphaThreshold, m_spinAlphaThreshold);

    m_spinBgRemovalTol = new QSpinBox(m_grpExtraction);
    m_spinBgRemovalTol->setRange(0, 100);
    m_lblBgRemovalTol = new QLabel(tr("KEY_SETTINGS_BG_REMOVAL_TOL"), m_grpExtraction);
    formExtraction->addRow(m_lblBgRemovalTol, m_spinBgRemovalTol);
    layout->addWidget(m_grpExtraction);

    // Language / Localization Group
    m_grpLang = new QGroupBox(tr("KEY_SETTINGS_GRP_LANGUAGE"), page);
    QFormLayout *formLang = new QFormLayout(m_grpLang);

    m_comboLanguage = new QComboBox(m_grpLang);
    m_comboLanguage->addItem(tr("KEY_SETTINGS_LANG_SYSTEM"), QStringLiteral("system"));
    m_comboLanguage->addItem(QStringLiteral("Français"), QStringLiteral("fr_FR"));
    m_comboLanguage->addItem(QStringLiteral("English"), QStringLiteral("en_US"));
    m_comboLanguage->addItem(QStringLiteral("日本語"), QStringLiteral("ja_JA"));
    m_lblLangApp = new QLabel(tr("KEY_SETTINGS_LANG_APP"), m_grpLang);
    formLang->addRow(m_lblLangApp, m_comboLanguage);

    m_lblLangHint = new QLabel(
        tr("KEY_SETTINGS_LANG_HINT"),
        m_grpLang
    );
    m_lblLangHint->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
    m_lblLangHint->setWordWrap(true);
    formLang->addRow(m_lblLangHint);
    layout->addWidget(m_grpLang);

    // Startup & Updates Group
    m_grpStartup = new QGroupBox(tr("Startup & Behavior"), page);
    QVBoxLayout *startupLayout = new QVBoxLayout(m_grpStartup);

    m_chkCheckUpdatesOnStartup = new QCheckBox(tr("Check automatically for updates on startup"), m_grpStartup);
    startupLayout->addWidget(m_chkCheckUpdatesOnStartup);

    m_chkReopenLastProject = new QCheckBox(tr("Reopen last project on startup"), m_grpStartup);
    startupLayout->addWidget(m_chkReopenLastProject);

    layout->addWidget(m_grpStartup);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createGitPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 0, 0, 0);

    m_hdrGit = new QLabel(tr("KEY_SETTINGS_HDR_GIT"));
    m_hdrGit->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; margin-bottom: 8px;"));
    layout->addWidget(m_hdrGit);

    // Author Identity Group
    m_grpAuthor = new QGroupBox(tr("KEY_SETTINGS_GRP_AUTHOR"), page);
    QVBoxLayout *authorLayout = new QVBoxLayout(m_grpAuthor);

    m_lblAuthorInfo = new QLabel(
        tr("KEY_SETTINGS_AUTHOR_INFO")
    );
    m_lblAuthorInfo->setWordWrap(true);
    m_lblAuthorInfo->setStyleSheet(QStringLiteral("color: #7f8c8d; margin-bottom: 6px;"));
    authorLayout->addWidget(m_lblAuthorInfo);

    QFormLayout *formAuthor = new QFormLayout();
    m_editGitAuthorName = new QLineEdit(m_grpAuthor);
    m_editGitAuthorName->setPlaceholderText(tr("KEY_SETTINGS_AUTHOR_NAME_PLACEHOLDER"));
    m_lblAuthorName = new QLabel(tr("KEY_SETTINGS_AUTHOR_NAME"), m_grpAuthor);
    formAuthor->addRow(m_lblAuthorName, m_editGitAuthorName);

    m_editGitAuthorEmail = new QLineEdit(m_grpAuthor);
    m_editGitAuthorEmail->setPlaceholderText(tr("KEY_SETTINGS_AUTHOR_EMAIL_PLACEHOLDER"));
    m_lblAuthorEmail = new QLabel(tr("KEY_SETTINGS_AUTHOR_EMAIL"), m_grpAuthor);
    formAuthor->addRow(m_lblAuthorEmail, m_editGitAuthorEmail);
    authorLayout->addLayout(formAuthor);

    QHBoxLayout *detectLayout = new QHBoxLayout();
    m_btnDetectGit = new QPushButton(tr("KEY_SETTINGS_DETECT_GIT"), m_grpAuthor);
    connect(m_btnDetectGit, &QPushButton::clicked, this, &SettingsDialog::onDetectSystemGitIdentity);
    detectLayout->addWidget(m_btnDetectGit);
    detectLayout->addStretch();
    authorLayout->addLayout(detectLayout);

    layout->addWidget(m_grpAuthor);

    // Git Engine Status Group
    m_grpGitEngine = new QGroupBox(tr("KEY_SETTINGS_GRP_GIT_ENGINE"), page);
    QVBoxLayout *engineLayout = new QVBoxLayout(m_grpGitEngine);

    bool gitAvailable = SessionManager::isGitAvailable();
    QString statusText = gitAvailable
        ? tr("KEY_SETTINGS_GIT_STATUS_ACTIVE")
        : tr("KEY_SETTINGS_GIT_STATUS_INACTIVE");

    m_lblGitStatus = new QLabel(statusText, m_grpGitEngine);
    m_lblGitStatus->setTextFormat(Qt::RichText);
    engineLayout->addWidget(m_lblGitStatus);

    m_lblGitDesc = new QLabel(
        tr("KEY_SETTINGS_GIT_ENGINE_DESC")
    );
    m_lblGitDesc->setWordWrap(true);
    m_lblGitDesc->setStyleSheet(QStringLiteral("color: #7f8c8d;"));
    engineLayout->addWidget(m_lblGitDesc);

    layout->addWidget(m_grpGitEngine);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createAtlasPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 0, 0, 0);

    m_hdrAtlas = new QLabel(tr("KEY_SETTINGS_HDR_ATLAS"));
    m_hdrAtlas->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; margin-bottom: 8px;"));
    layout->addWidget(m_hdrAtlas);

    // Zoom and Navigation
    m_grpZoom = new QGroupBox(tr("KEY_SETTINGS_GRP_ZOOM"), page);
    QFormLayout *formZoom = new QFormLayout(m_grpZoom);

    m_spinZoomStep = new QDoubleSpinBox(m_grpZoom);
    m_spinZoomStep->setRange(1.05, 2.50);
    m_spinZoomStep->setSingleStep(0.05);
    m_lblZoomStep = new QLabel(tr("KEY_SETTINGS_ZOOM_STEP"), m_grpZoom);
    formZoom->addRow(m_lblZoomStep, m_spinZoomStep);

    m_spinZoomMin = new QDoubleSpinBox(m_grpZoom);
    m_spinZoomMin->setRange(0.01, 1.0);
    m_spinZoomMin->setSingleStep(0.05);
    m_lblZoomMin = new QLabel(tr("KEY_SETTINGS_ZOOM_MIN"), m_grpZoom);
    formZoom->addRow(m_lblZoomMin, m_spinZoomMin);

    m_spinZoomMax = new QDoubleSpinBox(m_grpZoom);
    m_spinZoomMax->setRange(1.0, 50.0);
    m_spinZoomMax->setSingleStep(1.0);
    m_lblZoomMax = new QLabel(tr("KEY_SETTINGS_ZOOM_MAX"), m_grpZoom);
    formZoom->addRow(m_lblZoomMax, m_spinZoomMax);

    m_spinFitPadding = new QSpinBox(m_grpZoom);
    m_spinFitPadding->setRange(0, 200);
    m_spinFitPadding->setSuffix(QStringLiteral(" px"));
    m_lblFitPadding = new QLabel(tr("KEY_SETTINGS_FIT_PADDING"), m_grpZoom);
    formZoom->addRow(m_lblFitPadding, m_spinFitPadding);

    layout->addWidget(m_grpZoom);

    // Slicing parameters
    m_grpSlicing = new QGroupBox(tr("KEY_SETTINGS_GRP_SLICING"), page);
    QFormLayout *formSlicing = new QFormLayout(m_grpSlicing);

    m_spinMinSliceSize = new QSpinBox(m_grpSlicing);
    m_spinMinSliceSize->setRange(1, 100);
    m_spinMinSliceSize->setSuffix(QStringLiteral(" px"));
    m_lblMinSliceSize = new QLabel(tr("KEY_SETTINGS_MIN_SLICE_SIZE"), m_grpSlicing);
    formSlicing->addRow(m_lblMinSliceSize, m_spinMinSliceSize);

    layout->addWidget(m_grpSlicing);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createExportPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 0, 0, 0);

    m_hdrExport = new QLabel(tr("Export & VRAM Defaults"));
    m_hdrExport->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; margin-bottom: 8px;"));
    layout->addWidget(m_hdrExport);

    m_grpExportDefaults = new QGroupBox(tr("Default Export Configurations"), page);
    QFormLayout *form = new QFormLayout(m_grpExportDefaults);

    // Default Target Format (dynamically populated from registered Extractor plugins)
    m_comboDefaultFormat = new QComboBox(m_grpExportDefaults);
    const auto &extractors = ExtractorRegistry::instance().extractors();
    for (Extractor *ext : extractors) {
        if (ext && ext->capabilities().testFlag(Extractor::CanExport)) {
            m_comboDefaultFormat->addItem(ext->displayName(), ext->id());
        }
    }
    if (m_comboDefaultFormat->count() == 0) {
        m_comboDefaultFormat->addItem(tr("(No export plugins loaded)"), QString());
        m_comboDefaultFormat->setEnabled(false);
    }
    m_lblDefaultFormat = new QLabel(tr("Default Target Format:"), m_grpExportDefaults);
    form->addRow(m_lblDefaultFormat, m_comboDefaultFormat);

    // Default Texture Format
    m_comboDefaultTextureFormat = new QComboBox(m_grpExportDefaults);
    m_comboDefaultTextureFormat->addItem(tr("PNG (Standard, Lossless)"));
    m_comboDefaultTextureFormat->addItem(tr("WebP (Modern Web, High Compression)"));
    m_comboDefaultTextureFormat->addItem(tr("KTX2 / Basis Universal (GPU Compressed VRAM)"));
    m_lblDefaultTextureFormat = new QLabel(tr("Default Texture Format:"), m_grpExportDefaults);
    form->addRow(m_lblDefaultTextureFormat, m_comboDefaultTextureFormat);

    // Default Packing Algorithm
    m_comboDefaultAlgorithm = new QComboBox(m_grpExportDefaults);
    m_comboDefaultAlgorithm->addItem(tr("MaxRects (Best Fit)"));
    m_comboDefaultAlgorithm->addItem(tr("Shelf / Next-Fit (Fast)"));
    m_comboDefaultAlgorithm->addItem(tr("Skyline (Efficient)"));
    m_comboDefaultAlgorithm->addItem(tr("Polygonal Concave (Tightest Packing)"));
    m_lblDefaultAlgorithm = new QLabel(tr("Default Packing Algorithm:"), m_grpExportDefaults);
    form->addRow(m_lblDefaultAlgorithm, m_comboDefaultAlgorithm);

    // Zstandard Compression
    m_chkDefaultZstd = new QCheckBox(tr("Enable Zstandard (Zstd) compression by default"), m_grpExportDefaults);
    form->addRow(QString(), m_chkDefaultZstd);

    m_spinDefaultZstdLevel = new QSpinBox(m_grpExportDefaults);
    m_spinDefaultZstdLevel->setRange(1, 22);
    m_spinDefaultZstdLevel->setValue(3);
    m_lblZstdLevel = new QLabel(tr("Zstd Compression Level:"), m_grpExportDefaults);
    form->addRow(m_lblZstdLevel, m_spinDefaultZstdLevel);

    connect(m_chkDefaultZstd, &QCheckBox::toggled, m_spinDefaultZstdLevel, &QSpinBox::setEnabled);
    connect(m_chkDefaultZstd, &QCheckBox::toggled, m_lblZstdLevel, &QLabel::setEnabled);

    layout->addWidget(m_grpExportDefaults);

    QLabel *lblTip = new QLabel(
        tr("<b>Tip:</b> GPU compressed textures (KTX2 / Basis Universal) reduce GPU memory usage (VRAM) and bandwidth on runtime devices."),
        page
    );
    lblTip->setWordWrap(true);
    lblTip->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px; margin-top: 6px;"));
    layout->addWidget(lblTip);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createPluginsPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 0, 0, 0);

    m_hdrPlugins = new QLabel(tr("Plugins & Extensions"));
    m_hdrPlugins->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; margin-bottom: 8px;"));
    layout->addWidget(m_hdrPlugins);

    QHBoxLayout *topBar = new QHBoxLayout();
    m_btnOpenPluginsFolder = new QPushButton(tr("Open Plugins Folder..."), page);
    connect(m_btnOpenPluginsFolder, &QPushButton::clicked, this, &SettingsDialog::onOpenPluginsFolder);
    topBar->addWidget(m_btnOpenPluginsFolder);

    m_btnReloadPlugins = new QPushButton(tr("Reload Plugins"), page);
    connect(m_btnReloadPlugins, &QPushButton::clicked, this, &SettingsDialog::onReloadPlugins);
    topBar->addWidget(m_btnReloadPlugins);
    topBar->addStretch();
    layout->addLayout(topBar);

    m_tabPlugins = new QTabWidget(page);

    // Filters Tab
    m_treeFilters = new QTreeWidget(m_tabPlugins);
    m_treeFilters->setHeaderLabels(QStringList() << tr("Filter Name") << tr("Category") << tr("Identifier"));
    m_treeFilters->setRootIsDecorated(false);
    m_treeFilters->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeFilters->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treeFilters->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_treeFilters->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(m_treeFilters, &QTreeWidget::itemSelectionChanged, this, &SettingsDialog::onFilterTreeSelectionChanged);
    m_tabPlugins->addTab(m_treeFilters, tr("Filters"));

    // Extractors Tab
    m_treeExtractors = new QTreeWidget(m_tabPlugins);
    m_treeExtractors->setHeaderLabels(QStringList() << tr("Extractor / Codec") << tr("Version") << tr("Extensions") << tr("Identifier"));
    m_treeExtractors->setRootIsDecorated(false);
    m_treeExtractors->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeExtractors->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treeExtractors->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_treeExtractors->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_treeExtractors->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    connect(m_treeExtractors, &QTreeWidget::itemSelectionChanged, this, &SettingsDialog::onExtractorTreeSelectionChanged);
    m_tabPlugins->addTab(m_treeExtractors, tr("Extractors & Codecs"));

    layout->addWidget(m_tabPlugins, 1);

    m_lblPluginInfo = new QLabel(tr("Select a plugin above to view its details."), page);
    m_lblPluginInfo->setWordWrap(true);
    m_lblPluginInfo->setStyleSheet(QStringLiteral("background: rgba(128, 128, 128, 0.1); border: 1px solid rgba(128, 128, 128, 0.2); border-radius: 4px; padding: 8px; font-size: 11px;"));
    layout->addWidget(m_lblPluginInfo);

    m_grpPluginConfig = new QGroupBox(tr("Plugin Configuration"), page);
    m_pluginConfigLayout = new QVBoxLayout(m_grpPluginConfig);
    m_grpPluginConfig->setVisible(false);
    layout->addWidget(m_grpPluginConfig);

    refreshPluginTrees();

    return page;
}

QWidget* SettingsDialog::createUpdatesPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 0, 0, 0);

    m_hdrUpdates = new QLabel(tr("Software Updates"));
    m_hdrUpdates->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; margin-bottom: 8px;"));
    layout->addWidget(m_hdrUpdates);

    QHBoxLayout *curVerLayout = new QHBoxLayout();
    m_lblCurrentVersion = new QLabel(tr("Current installed version: <b>v%1</b>").arg(QStringLiteral(PROJECT_VERSION)), page);
    curVerLayout->addWidget(m_lblCurrentVersion);

    m_btnCheckUpdates = new QPushButton(tr("Check for Updates Now"), page);
    connect(m_btnCheckUpdates, &QPushButton::clicked, this, &SettingsDialog::onCheckForUpdates);
    curVerLayout->addWidget(m_btnCheckUpdates);
    curVerLayout->addStretch();
    layout->addLayout(curVerLayout);

    m_lblUpdateStatus = new QLabel(tr("Click 'Check for Updates Now' to query the latest release on GitHub."), page);
    m_lblUpdateStatus->setWordWrap(true);
    m_lblUpdateStatus->setStyleSheet(QStringLiteral("margin-top: 4px; margin-bottom: 8px; color: #7f8c8d;"));
    layout->addWidget(m_lblUpdateStatus);

    m_grpUpdateDetails = new QGroupBox(tr("Release Information"), page);
    QVBoxLayout *detLayout = new QVBoxLayout(m_grpUpdateDetails);

    m_lblLatestVersion = new QLabel(m_grpUpdateDetails);
    detLayout->addWidget(m_lblLatestVersion);

    m_lblReleaseDate = new QLabel(m_grpUpdateDetails);
    detLayout->addWidget(m_lblReleaseDate);

    m_textReleaseNotes = new QTextBrowser(m_grpUpdateDetails);
    m_textReleaseNotes->setOpenExternalLinks(true);
    m_textReleaseNotes->setMinimumHeight(140);
    detLayout->addWidget(m_textReleaseNotes, 1);

    m_btnDownloadUpdate = new QPushButton(tr("Open Release Page on GitHub"), m_grpUpdateDetails);
    connect(m_btnDownloadUpdate, &QPushButton::clicked, this, [this]() {
        if (!m_latestReleaseUrl.isEmpty()) {
            QDesktopServices::openUrl(QUrl(m_latestReleaseUrl));
        }
    });
    detLayout->addWidget(m_btnDownloadUpdate);

    m_grpUpdateDetails->setVisible(false);
    layout->addWidget(m_grpUpdateDetails, 1);

    return page;
}

void SettingsDialog::refreshPluginTrees()
{
    if (m_treeFilters) {
        m_treeFilters->clear();
        const auto &filters = FilterRegistry::instance().filters();
        for (FilterPlugin *f : filters) {
            if (!f) continue;
            QTreeWidgetItem *item = new QTreeWidgetItem(m_treeFilters);
            item->setText(0, f->name());
            item->setText(1, f->category());
            item->setText(2, f->id());
            item->setIcon(0, f->icon());
            item->setData(0, Qt::UserRole, f->id());
        }
    }

    if (m_treeExtractors) {
        m_treeExtractors->clear();
        const auto &extractors = ExtractorRegistry::instance().extractors();
        for (Extractor *e : extractors) {
            if (!e) continue;
            QTreeWidgetItem *item = new QTreeWidgetItem(m_treeExtractors);
            item->setText(0, e->displayName());
            item->setText(1, e->version().toString());
            item->setText(2, e->supportedExtensions().join(QStringLiteral(", ")));
            item->setText(3, e->id());
            item->setData(0, Qt::UserRole, e->id());
        }
    }
}

void SettingsDialog::onOpenPluginsFolder()
{
    QString pluginsDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));
    if (!QDir(pluginsDir).exists()) {
        QDir().mkpath(pluginsDir);
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(pluginsDir));
}

void SettingsDialog::updatePluginConfigWidget(QWidget *customWidget)
{
    if (m_currentPluginConfigWidget) {
        m_pluginConfigLayout->removeWidget(m_currentPluginConfigWidget);
        delete m_currentPluginConfigWidget;
        m_currentPluginConfigWidget = nullptr;
    }
    if (customWidget) {
        m_currentPluginConfigWidget = customWidget;
        m_pluginConfigLayout->addWidget(m_currentPluginConfigWidget);
        m_grpPluginConfig->setVisible(true);
    } else if (m_grpPluginConfig) {
        m_grpPluginConfig->setVisible(false);
    }
}

void SettingsDialog::onReloadPlugins()
{
    FilterRegistry::instance().rescanPlugins();
    ExtractorRegistry::instance().rescanPlugins();
    updatePluginConfigWidget(nullptr);
    refreshPluginTrees();

    // Dynamically update export formats combo
    if (m_comboDefaultFormat) {
        QString currentId = m_comboDefaultFormat->currentData().toString();
        m_comboDefaultFormat->clear();
        const auto &extractors = ExtractorRegistry::instance().extractors();
        for (Extractor *ext : extractors) {
            if (ext && ext->capabilities().testFlag(Extractor::CanExport)) {
                m_comboDefaultFormat->addItem(ext->displayName(), ext->id());
            }
        }
        if (m_comboDefaultFormat->count() == 0) {
            m_comboDefaultFormat->addItem(tr("(No export plugins loaded)"), QString());
            m_comboDefaultFormat->setEnabled(false);
        } else {
            m_comboDefaultFormat->setEnabled(true);
            int idx = m_comboDefaultFormat->findData(currentId);
            if (idx >= 0) m_comboDefaultFormat->setCurrentIndex(idx);
        }
    }

    if (m_lblPluginInfo) {
        m_lblPluginInfo->setText(tr("Plugins reloaded successfully."));
    }
}

void SettingsDialog::onFilterTreeSelectionChanged()
{
    if (!m_treeFilters || !m_lblPluginInfo) return;
    auto items = m_treeFilters->selectedItems();
    if (items.isEmpty()) {
        updatePluginConfigWidget(nullptr);
        return;
    }
    QString id = items.first()->data(0, Qt::UserRole).toString();
    FilterPlugin *f = FilterRegistry::instance().findFilter(id);
    if (!f) {
        updatePluginConfigWidget(nullptr);
        return;
    }

    QString info = QStringLiteral("<b>%1</b> (ID: <code>%2</code>)<br>").arg(f->name(), f->id());
    info += QStringLiteral("<b>Category:</b> %1<br>").arg(f->category());
    if (!f->shortcut().isEmpty()) {
        info += QStringLiteral("<b>Shortcut:</b> %1<br>").arg(f->shortcut().toString(QKeySequence::NativeText));
    }
    info += QStringLiteral("<b>Description:</b> %1").arg(f->description());
    m_lblPluginInfo->setText(info);

    updatePluginConfigWidget(f->createSettingsWidget(m_grpPluginConfig));
}

void SettingsDialog::onExtractorTreeSelectionChanged()
{
    if (!m_treeExtractors || !m_lblPluginInfo) return;
    auto items = m_treeExtractors->selectedItems();
    if (items.isEmpty()) {
        updatePluginConfigWidget(nullptr);
        return;
    }
    QString id = items.first()->data(0, Qt::UserRole).toString();
    Extractor *e = ExtractorRegistry::instance().findExtractorById(id);
    if (!e) {
        updatePluginConfigWidget(nullptr);
        return;
    }

    QStringList caps;
    if (e->capabilities() & Extractor::CanImport) caps << QStringLiteral("Import");
    if (e->capabilities() & Extractor::CanExport) caps << QStringLiteral("Export");
    if (e->capabilities() & Extractor::SupportsAnimations) caps << QStringLiteral("Animations");
    if (e->capabilities() & Extractor::SupportsAtlasMetadata) caps << QStringLiteral("Atlas Metadata");

    QString info = QStringLiteral("<b>%1</b> (ID: <code>%2</code>, v%3)<br>").arg(e->displayName(), e->id(), e->version().toString());
    info += QStringLiteral("<b>Extensions:</b> %1<br>").arg(e->supportedExtensions().join(QStringLiteral(", ")));
    info += QStringLiteral("<b>Capabilities:</b> %1<br>").arg(caps.join(QStringLiteral(", ")));
    info += QStringLiteral("<b>Description:</b> %1").arg(e->description());
    m_lblPluginInfo->setText(info);

    updatePluginConfigWidget(e->createSettingsWidget(m_grpPluginConfig));
}

void SettingsDialog::onCheckForUpdates()
{
    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
        connect(m_networkManager, &QNetworkAccessManager::finished, this, &SettingsDialog::onUpdateReplyFinished);
    }

    if (m_btnCheckUpdates) m_btnCheckUpdates->setEnabled(false);
    if (m_lblUpdateStatus) {
        m_lblUpdateStatus->setText(tr("Checking for updates from GitHub..."));
    }

    QUrl url(QStringLiteral("https://api.github.com/repos/oktailb/BentoPack/releases/latest"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("BentoPack/%1").arg(QStringLiteral(PROJECT_VERSION)));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    m_networkManager->get(request);
}

void SettingsDialog::onUpdateReplyFinished(QNetworkReply *reply)
{
    if (m_btnCheckUpdates) m_btnCheckUpdates->setEnabled(true);
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (m_lblUpdateStatus) {
            m_lblUpdateStatus->setText(tr("<span style='color: #e74c3c;'>Failed to check for updates: %1</span>").arg(reply->errorString()));
        }
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (m_lblUpdateStatus) {
            m_lblUpdateStatus->setText(tr("<span style='color: #e74c3c;'>Invalid response from GitHub.</span>"));
        }
        return;
    }

    QJsonObject obj = doc.object();
    QString tagName = obj.value(QStringLiteral("tag_name")).toString();
    QString releaseName = obj.value(QStringLiteral("name")).toString();
    QString body = obj.value(QStringLiteral("body")).toString();
    QString publishedAt = obj.value(QStringLiteral("published_at")).toString();
    QString htmlUrl = obj.value(QStringLiteral("html_url")).toString();

    QString cleanTag = tagName;
    if (cleanTag.startsWith('v') || cleanTag.startsWith('V')) {
        cleanTag = cleanTag.mid(1);
    }

    QVersionNumber latestVer = QVersionNumber::fromString(cleanTag);
    QVersionNumber currentVer = QVersionNumber::fromString(QStringLiteral(PROJECT_VERSION));

    m_latestReleaseUrl = htmlUrl.isEmpty() ? QStringLiteral("https://github.com/oktailb/BentoPack/releases") : htmlUrl;

    if (latestVer > currentVer) {
        if (m_lblUpdateStatus) {
            m_lblUpdateStatus->setText(tr("<span style='color: #27ae60; font-weight: bold;'>A new version is available: %1!</span>").arg(tagName));
        }
        if (m_grpUpdateDetails) {
            m_grpUpdateDetails->setVisible(true);
        }
        if (m_lblLatestVersion) {
            m_lblLatestVersion->setText(tr("Latest Version: <b>%1</b> (%2)").arg(tagName, releaseName));
        }
        if (m_lblReleaseDate) {
            m_lblReleaseDate->setText(tr("Released: %1").arg(publishedAt));
        }
        if (m_textReleaseNotes) {
            m_textReleaseNotes->setMarkdown(body);
        }
        if (m_btnDownloadUpdate) {
            m_btnDownloadUpdate->setVisible(true);
        }
    } else {
        if (m_lblUpdateStatus) {
            m_lblUpdateStatus->setText(tr("<span style='color: #27ae60;'>You are using the latest version (v%1).</span>").arg(QStringLiteral(PROJECT_VERSION)));
        }
        if (m_grpUpdateDetails) {
            m_grpUpdateDetails->setVisible(false);
        }
    }
}

void SettingsDialog::onSearchTextChanged(const QString &text)
{
    const QString query = text.trimmed().toLower();
    if (query.isEmpty()) {
        m_itemGeneral->setHidden(false);
        m_itemAtlas->setHidden(false);
        m_itemExport->setHidden(false);
        m_itemPlugins->setHidden(false);
        m_itemGit->setHidden(false);
        m_itemUpdates->setHidden(false);
        return;
    }

    bool matchGeneral = QStringLiteral("général general langue language locale fr en ja français english japonais projet project undo redo recent récents alpha seuil tolerance fond background startup demarrage démarrage").contains(query);
    bool matchAtlas = QStringLiteral("atlas affichage display zoom vue tranche slice padding cadrage").contains(query);
    bool matchExport = QStringLiteral("export exportation vram texture ktx2 basis zstd compression format algorithme algorithm paper2d godot unity sheet").contains(query);
    bool matchPlugins = QStringLiteral("plugin plugins extension extensions filtre filter extractor extracteur dll so module reload").contains(query);
    bool matchGit = QStringLiteral("git version control contrôle author auteur email mail nom signature libgit2 commit").contains(query);
    bool matchUpdates = QStringLiteral("update updates mise à jour github release version nouveautés nouvelle").contains(query);

    m_itemGeneral->setHidden(!matchGeneral);
    m_itemAtlas->setHidden(!matchAtlas);
    m_itemExport->setHidden(!matchExport);
    m_itemPlugins->setHidden(!matchPlugins);
    m_itemGit->setHidden(!matchGit);
    m_itemUpdates->setHidden(!matchUpdates);

    if (m_categoryTree->currentItem() && m_categoryTree->currentItem()->isHidden()) {
        if (!m_itemGeneral->isHidden()) {
            m_categoryTree->setCurrentItem(m_itemGeneral);
        } else if (!m_itemAtlas->isHidden()) {
            m_categoryTree->setCurrentItem(m_itemAtlas);
        } else if (!m_itemExport->isHidden()) {
            m_categoryTree->setCurrentItem(m_itemExport);
        } else if (!m_itemPlugins->isHidden()) {
            m_categoryTree->setCurrentItem(m_itemPlugins);
        } else if (!m_itemGit->isHidden()) {
            m_categoryTree->setCurrentItem(m_itemGit);
        } else if (!m_itemUpdates->isHidden()) {
            m_categoryTree->setCurrentItem(m_itemUpdates);
        }
    }
}

void SettingsDialog::onCategoryItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (!current) return;
    int pageIdx = current->data(0, Qt::UserRole).toInt();
    if (pageIdx >= 0 && pageIdx < m_pagesStack->count()) {
        m_pagesStack->setCurrentIndex(pageIdx);
    }
}

void SettingsDialog::onDetectSystemGitIdentity()
{
    QString name;
    QString email;
    GitConfig::detectSystemIdentity(&name, &email);

    if (!name.isEmpty()) {
        m_editGitAuthorName->setText(name);
    }
    if (!email.isEmpty()) {
        m_editGitAuthorEmail->setText(email);
    }
}

void SettingsDialog::loadSettings()
{
    const AppConfig &cfg = AppConfig::instance();

    // General
    int langIdx = m_comboLanguage ? m_comboLanguage->findData(cfg.general().language) : -1;
    if (m_comboLanguage) {
        m_comboLanguage->setCurrentIndex(langIdx >= 0 ? langIdx : 0);
    }

    if (m_chkCheckUpdatesOnStartup) {
        m_chkCheckUpdatesOnStartup->setChecked(cfg.general().checkUpdatesOnStartup);
    }
    if (m_chkReopenLastProject) {
        m_chkReopenLastProject->setChecked(cfg.general().reopenLastProject);
    }

    m_spinUndoLimit->setValue(cfg.project().undoLimit);
    m_spinMaxRecentFiles->setValue(cfg.project().maxRecentFiles);
    m_spinAlphaThreshold->setValue(cfg.atlas().defaultAlphaThreshold);
    m_spinBgRemovalTol->setValue(cfg.project().backgroundRemovalTolerance);

    // Git
    m_editGitAuthorName->setText(cfg.git().authorName);
    m_editGitAuthorEmail->setText(cfg.git().authorEmail);

    // Atlas
    m_spinZoomStep->setValue(cfg.atlas().zoomStep);
    m_spinZoomMin->setValue(cfg.atlas().zoomMin);
    m_spinZoomMax->setValue(cfg.atlas().zoomMax);
    m_spinFitPadding->setValue(cfg.atlas().fitViewPadding);
    m_spinMinSliceSize->setValue(cfg.atlas().minSliceSize);

    // Export Defaults
    if (m_comboDefaultFormat && !cfg.exportSettings().defaultFormatId.isEmpty()) {
        int idx = m_comboDefaultFormat->findData(cfg.exportSettings().defaultFormatId);
        if (idx >= 0) {
            m_comboDefaultFormat->setCurrentIndex(idx);
        }
    }
    if (m_comboDefaultTextureFormat) {
        m_comboDefaultTextureFormat->setCurrentIndex(cfg.exportSettings().defaultTextureFormatIndex);
    }
    if (m_comboDefaultAlgorithm) {
        m_comboDefaultAlgorithm->setCurrentIndex(cfg.exportSettings().defaultAlgorithmIndex);
    }
    if (m_chkDefaultZstd) {
        m_chkDefaultZstd->setChecked(cfg.exportSettings().defaultZstd);
    }
    if (m_spinDefaultZstdLevel) {
        m_spinDefaultZstdLevel->setValue(cfg.exportSettings().defaultZstdLevel);
    }
}

void SettingsDialog::saveSettings()
{
    AppConfig &cfg = AppConfig::instance();

    // General
    if (m_comboLanguage) {
        cfg.general().language = m_comboLanguage->currentData().toString();
    }
    if (m_chkCheckUpdatesOnStartup) {
        cfg.general().checkUpdatesOnStartup = m_chkCheckUpdatesOnStartup->isChecked();
    }
    if (m_chkReopenLastProject) {
        cfg.general().reopenLastProject = m_chkReopenLastProject->isChecked();
    }

    cfg.project().undoLimit = m_spinUndoLimit->value();
    cfg.project().maxRecentFiles = m_spinMaxRecentFiles->value();
    cfg.atlas().defaultAlphaThreshold = m_spinAlphaThreshold->value();
    cfg.project().backgroundRemovalTolerance = m_spinBgRemovalTol->value();

    // Git
    cfg.git().authorName = m_editGitAuthorName->text().trimmed();
    cfg.git().authorEmail = m_editGitAuthorEmail->text().trimmed();

    // Atlas
    cfg.atlas().zoomStep = m_spinZoomStep->value();
    cfg.atlas().zoomMin = m_spinZoomMin->value();
    cfg.atlas().zoomMax = m_spinZoomMax->value();
    cfg.atlas().fitViewPadding = m_spinFitPadding->value();
    cfg.atlas().minSliceSize = m_spinMinSliceSize->value();

    // Export Defaults
    if (m_comboDefaultFormat && m_comboDefaultFormat->currentIndex() >= 0) {
        cfg.exportSettings().defaultFormatId = m_comboDefaultFormat->currentData().toString();
    }
    if (m_comboDefaultTextureFormat) {
        cfg.exportSettings().defaultTextureFormatIndex = m_comboDefaultTextureFormat->currentIndex();
    }
    if (m_comboDefaultAlgorithm) {
        cfg.exportSettings().defaultAlgorithmIndex = m_comboDefaultAlgorithm->currentIndex();
    }
    if (m_chkDefaultZstd) {
        cfg.exportSettings().defaultZstd = m_chkDefaultZstd->isChecked();
    }
    if (m_spinDefaultZstdLevel) {
        cfg.exportSettings().defaultZstdLevel = m_spinDefaultZstdLevel->value();
    }

    cfg.save();
}

void SettingsDialog::applySettings()
{
    QString oldLang = AppConfig::instance().general().language;
    saveSettings();
    QString newLang = AppConfig::instance().general().language;
    if (newLang != oldLang) {
        LocalizationManager::instance().setLanguage(newLang);
    }
}

void SettingsDialog::restoreDefaults()
{
    if (QMessageBox::question(this, tr("KEY_SETTINGS_RESET_TITLE"),
                              tr("KEY_SETTINGS_RESET_CONFIRM"),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        QString oldLang = AppConfig::instance().general().language;
        AppConfig::instance().resetToDefaults();
        loadSettings();
        QString newLang = AppConfig::instance().general().language;
        if (newLang != oldLang) {
            LocalizationManager::instance().setLanguage(newLang);
        }
    }
}

void SettingsDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void SettingsDialog::retranslateUi()
{
    setWindowTitle(tr("KEY_SETTINGS_TITLE") + QStringLiteral(" - BentoPack"));
    if (m_searchEdit) m_searchEdit->setPlaceholderText(tr("KEY_SETTINGS_SEARCH_PLACEHOLDER"));
    if (m_itemGeneral) m_itemGeneral->setText(0, tr("KEY_SETTINGS_CAT_GENERAL"));
    if (m_itemAtlas) m_itemAtlas->setText(0, tr("KEY_SETTINGS_CAT_ATLAS"));
    if (m_itemExport) m_itemExport->setText(0, tr("Export & VRAM"));
    if (m_itemPlugins) m_itemPlugins->setText(0, tr("Plugins & Extensions"));
    if (m_itemGit) m_itemGit->setText(0, tr("KEY_SETTINGS_CAT_GIT"));
    if (m_itemUpdates) m_itemUpdates->setText(0, tr("Updates"));

    // General Page
    if (m_hdrGeneral) m_hdrGeneral->setText(tr("KEY_SETTINGS_HDR_GENERAL"));
    if (m_grpHistory) m_grpHistory->setTitle(tr("KEY_SETTINGS_GRP_HISTORY"));
    if (m_lblUndoLimit) m_lblUndoLimit->setText(tr("KEY_SETTINGS_UNDO_LIMIT"));
    if (m_lblMaxRecentFiles) m_lblMaxRecentFiles->setText(tr("KEY_SETTINGS_MAX_RECENT_FILES"));
    if (m_spinUndoLimit) m_spinUndoLimit->setSuffix(tr("KEY_SETTINGS_SUFFIX_ACTIONS"));

    if (m_grpExtraction) m_grpExtraction->setTitle(tr("KEY_SETTINGS_GRP_EXTRACTION"));
    if (m_lblAlphaThreshold) m_lblAlphaThreshold->setText(tr("KEY_SETTINGS_ALPHA_THRESHOLD"));
    if (m_lblBgRemovalTol) m_lblBgRemovalTol->setText(tr("KEY_SETTINGS_BG_REMOVAL_TOL"));

    if (m_grpLang) m_grpLang->setTitle(tr("KEY_SETTINGS_GRP_LANGUAGE"));
    if (m_lblLangApp) m_lblLangApp->setText(tr("KEY_SETTINGS_LANG_APP"));
    if (m_comboLanguage) m_comboLanguage->setItemText(0, tr("KEY_SETTINGS_LANG_SYSTEM"));
    if (m_lblLangHint) m_lblLangHint->setText(tr("KEY_SETTINGS_LANG_HINT"));

    if (m_grpStartup) m_grpStartup->setTitle(tr("Startup & Behavior"));
    if (m_chkCheckUpdatesOnStartup) m_chkCheckUpdatesOnStartup->setText(tr("Check automatically for updates on startup"));
    if (m_chkReopenLastProject) m_chkReopenLastProject->setText(tr("Reopen last project on startup"));

    // Git Page
    if (m_hdrGit) m_hdrGit->setText(tr("KEY_SETTINGS_HDR_GIT"));
    if (m_grpAuthor) m_grpAuthor->setTitle(tr("KEY_SETTINGS_GRP_AUTHOR"));
    if (m_lblAuthorInfo) m_lblAuthorInfo->setText(tr("KEY_SETTINGS_AUTHOR_INFO"));
    if (m_lblAuthorName) m_lblAuthorName->setText(tr("KEY_SETTINGS_AUTHOR_NAME"));
    if (m_lblAuthorEmail) m_lblAuthorEmail->setText(tr("KEY_SETTINGS_AUTHOR_EMAIL"));
    if (m_editGitAuthorName) m_editGitAuthorName->setPlaceholderText(tr("KEY_SETTINGS_AUTHOR_NAME_PLACEHOLDER"));
    if (m_editGitAuthorEmail) m_editGitAuthorEmail->setPlaceholderText(tr("KEY_SETTINGS_AUTHOR_EMAIL_PLACEHOLDER"));
    if (m_btnDetectGit) m_btnDetectGit->setText(tr("KEY_SETTINGS_DETECT_GIT"));
    if (m_grpGitEngine) m_grpGitEngine->setTitle(tr("KEY_SETTINGS_GRP_GIT_ENGINE"));
    if (m_lblGitStatus) {
        bool gitAvailable = SessionManager::isGitAvailable();
        m_lblGitStatus->setText(gitAvailable ? tr("KEY_SETTINGS_GIT_STATUS_ACTIVE") : tr("KEY_SETTINGS_GIT_STATUS_INACTIVE"));
    }
    if (m_lblGitDesc) m_lblGitDesc->setText(tr("KEY_SETTINGS_GIT_ENGINE_DESC"));

    // Atlas Page
    if (m_hdrAtlas) m_hdrAtlas->setText(tr("KEY_SETTINGS_HDR_ATLAS"));
    if (m_grpZoom) m_grpZoom->setTitle(tr("KEY_SETTINGS_GRP_ZOOM"));
    if (m_lblZoomStep) m_lblZoomStep->setText(tr("KEY_SETTINGS_ZOOM_STEP"));
    if (m_lblZoomMin) m_lblZoomMin->setText(tr("KEY_SETTINGS_ZOOM_MIN"));
    if (m_lblZoomMax) m_lblZoomMax->setText(tr("KEY_SETTINGS_ZOOM_MAX"));
    if (m_lblFitPadding) m_lblFitPadding->setText(tr("KEY_SETTINGS_FIT_PADDING"));
    if (m_grpSlicing) m_grpSlicing->setTitle(tr("KEY_SETTINGS_GRP_SLICING"));
    if (m_lblMinSliceSize) m_lblMinSliceSize->setText(tr("KEY_SETTINGS_MIN_SLICE_SIZE"));

    // Export Page
    if (m_hdrExport) m_hdrExport->setText(tr("Export & VRAM Defaults"));
    if (m_grpExportDefaults) m_grpExportDefaults->setTitle(tr("Default Export Configurations"));
    if (m_lblDefaultFormat) m_lblDefaultFormat->setText(tr("Default Target Format:"));
    if (m_lblDefaultTextureFormat) m_lblDefaultTextureFormat->setText(tr("Default Texture Format:"));
    if (m_lblDefaultAlgorithm) m_lblDefaultAlgorithm->setText(tr("Default Packing Algorithm:"));
    if (m_lblZstdLevel) m_lblZstdLevel->setText(tr("Zstd Compression Level:"));
    if (m_chkDefaultZstd) m_chkDefaultZstd->setText(tr("Enable Zstandard (Zstd) compression by default"));

    // Plugins Page
    if (m_hdrPlugins) m_hdrPlugins->setText(tr("Plugins & Extensions"));
    if (m_btnOpenPluginsFolder) m_btnOpenPluginsFolder->setText(tr("Open Plugins Folder..."));
    if (m_btnReloadPlugins) m_btnReloadPlugins->setText(tr("Reload Plugins"));
    if (m_tabPlugins) {
        m_tabPlugins->setTabText(0, tr("Filters"));
        m_tabPlugins->setTabText(1, tr("Extractors & Codecs"));
    }

    // Updates Page
    if (m_hdrUpdates) m_hdrUpdates->setText(tr("Software Updates"));
    if (m_btnCheckUpdates) m_btnCheckUpdates->setText(tr("Check for Updates Now"));
    if (m_grpUpdateDetails) m_grpUpdateDetails->setTitle(tr("Release Information"));
    if (m_btnDownloadUpdate) m_btnDownloadUpdate->setText(tr("Open Release Page on GitHub"));

    if (m_buttonBox) {
        if (QPushButton *ok = m_buttonBox->button(QDialogButtonBox::Ok)) {
            ok->setText(tr("OK"));
        }
        if (QPushButton *cancel = m_buttonBox->button(QDialogButtonBox::Cancel)) {
            cancel->setText(tr("Cancel"));
        }
        if (QPushButton *apply = m_buttonBox->button(QDialogButtonBox::Apply)) {
            apply->setText(tr("Apply"));
        }
        if (QPushButton *restore = m_buttonBox->button(QDialogButtonBox::RestoreDefaults)) {
            restore->setText(tr("Restore Defaults"));
        }
    }
}
