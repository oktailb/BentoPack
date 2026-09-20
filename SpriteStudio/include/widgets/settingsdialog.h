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

class QNetworkAccessManager;
class QNetworkReply;
class QVBoxLayout;

/**
 * @brief Modal configuration dialog for SpriteStudio.
 *
 * Provides a structured settings interface with a search filter field and category
 * tree on the left, and corresponding setting pages in a stacked widget on the right.
 * Supports configuring General options, Atlas visuals, Export defaults, Plugin management,
 * Git commit author identity, and GitHub update detection.
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    enum PageIndex {
        PageGeneral = 0,
        PageAtlas = 1,
        PageExport = 2,
        PagePlugins = 3,
        PageGit = 4,
        PageUpdates = 5
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
    QSpinBox  *m_spinAlphaThreshold = nullptr;
    QSpinBox  *m_spinBgRemovalTol = nullptr;

    QGroupBox *m_grpLang = nullptr;
    QLabel    *m_lblLangApp = nullptr;
    QComboBox *m_comboLanguage = nullptr;
    QLabel    *m_lblLangHint = nullptr;
    QGroupBox *m_grpStartup = nullptr;
    QCheckBox *m_chkCheckUpdatesOnStartup = nullptr;
    QCheckBox *m_chkReopenLastProject = nullptr;

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
