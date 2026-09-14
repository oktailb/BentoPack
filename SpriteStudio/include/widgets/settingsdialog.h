#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>

/**
 * @brief Modal configuration dialog for SpriteStudio.
 *
 * Provides a structured settings interface with a search filter field and category
 * tree on the left, and corresponding setting pages in a stacked widget on the right.
 * Supports configuring Git commit author identity, general project limits, and atlas visuals.
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    enum PageIndex {
        PageGeneral = 0,
        PageGit = 1,
        PageAtlas = 2
    };

    explicit SettingsDialog(QWidget *parent = nullptr, PageIndex initialPage = PageGeneral);
    ~SettingsDialog() override = default;

    void setCurrentPage(PageIndex page);

public slots:
    void applySettings();
    void restoreDefaults();

private slots:
    void onSearchTextChanged(const QString &text);
    void onCategoryItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onDetectSystemGitIdentity();

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUI();
    void retranslateUi();
    QWidget* createGeneralPage();
    QWidget* createGitPage();
    QWidget* createAtlasPage();

    void loadSettings();
    void saveSettings();

    // UI Elements - Navigation & Search
    QLineEdit      *m_searchEdit = nullptr;
    QTreeWidget    *m_categoryTree = nullptr;
    QStackedWidget *m_pagesStack = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;

    // Tree items
    QTreeWidgetItem *m_itemGeneral = nullptr;
    QTreeWidgetItem *m_itemGit = nullptr;
    QTreeWidgetItem *m_itemAtlas = nullptr;

    // General Page Controls & Labels
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

    // Git Page Controls & Labels
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

    // Atlas Page Controls & Labels
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
};

#endif // SETTINGSDIALOG_H
