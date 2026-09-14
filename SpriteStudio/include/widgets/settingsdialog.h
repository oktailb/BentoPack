#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

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

private:
    void setupUI();
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

    // General Page Controls
    QSpinBox *m_spinUndoLimit = nullptr;
    QSpinBox *m_spinMaxRecentFiles = nullptr;
    QSpinBox *m_spinAlphaThreshold = nullptr;
    QSpinBox *m_spinBgRemovalTol = nullptr;

    // Git Page Controls
    QLineEdit   *m_editGitAuthorName = nullptr;
    QLineEdit   *m_editGitAuthorEmail = nullptr;
    QPushButton *m_btnDetectGit = nullptr;
    QLabel      *m_lblGitStatus = nullptr;

    // Atlas Page Controls
    QDoubleSpinBox *m_spinZoomStep = nullptr;
    QDoubleSpinBox *m_spinZoomMin = nullptr;
    QDoubleSpinBox *m_spinZoomMax = nullptr;
    QSpinBox       *m_spinMinSliceSize = nullptr;
    QSpinBox       *m_spinFitPadding = nullptr;
};

#endif // SETTINGSDIALOG_H
