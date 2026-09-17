#include "include/widgets/settingsdialog.h"
#include "config/appconfig.h"
#include "include/project/sessionmanager.h"
#include "include/localizationmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <QEvent>

SettingsDialog::SettingsDialog(QWidget *parent, PageIndex initialPage)
    : QDialog(parent)
{
    setupUI();
    loadSettings();
    setCurrentPage(initialPage);

    setWindowTitle(tr("KEY_SETTINGS_TITLE") + QStringLiteral(" - Sprite Studio"));
    resize(760, 520);
    setMinimumSize(640, 440);
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
        case PageGit:
            if (m_itemGit) m_categoryTree->setCurrentItem(m_itemGit);
            break;
        case PageAtlas:
            if (m_itemAtlas) m_categoryTree->setCurrentItem(m_itemAtlas);
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

    m_itemGit = new QTreeWidgetItem(m_categoryTree);
    m_itemGit->setText(0, tr("KEY_SETTINGS_CAT_GIT"));
    m_itemGit->setData(0, Qt::UserRole, PageGit);

    m_itemAtlas = new QTreeWidgetItem(m_categoryTree);
    m_itemAtlas->setText(0, tr("KEY_SETTINGS_CAT_ATLAS"));
    m_itemAtlas->setData(0, Qt::UserRole, PageAtlas);

    leftLayout->addWidget(m_categoryTree);
    splitter->addWidget(leftWidget);

    // Right Pages Stack
    m_pagesStack = new QStackedWidget(splitter);
    m_pagesStack->addWidget(createGeneralPage());
    m_pagesStack->addWidget(createGitPage());
    m_pagesStack->addWidget(createAtlasPage());
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

void SettingsDialog::onSearchTextChanged(const QString &text)
{
    const QString query = text.trimmed().toLower();
    if (query.isEmpty()) {
        m_itemGeneral->setHidden(false);
        m_itemGit->setHidden(false);
        m_itemAtlas->setHidden(false);
        return;
    }

    bool matchGeneral = QStringLiteral("général general langue language locale fr en ja français english japonais projet project undo redo recent récents alpha seuil tolerance fond background").contains(query);
    bool matchGit = QStringLiteral("git version control contrôle author auteur email mail nom signature libgit2 commit").contains(query);
    bool matchAtlas = QStringLiteral("atlas affichage display zoom vue tranche slice padding cadrage").contains(query);

    m_itemGeneral->setHidden(!matchGeneral);
    m_itemGit->setHidden(!matchGit);
    m_itemAtlas->setHidden(!matchAtlas);

    if (m_categoryTree->currentItem() && m_categoryTree->currentItem()->isHidden()) {
        if (!m_itemGit->isHidden() && matchGit && !matchGeneral) {
            m_categoryTree->setCurrentItem(m_itemGit);
        } else if (!m_itemAtlas->isHidden() && matchAtlas && !matchGeneral && !matchGit) {
            m_categoryTree->setCurrentItem(m_itemAtlas);
        } else if (!m_itemGeneral->isHidden()) {
            m_categoryTree->setCurrentItem(m_itemGeneral);
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
}

void SettingsDialog::saveSettings()
{
    AppConfig &cfg = AppConfig::instance();

    // General
    if (m_comboLanguage) {
        cfg.general().language = m_comboLanguage->currentData().toString();
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
    setWindowTitle(tr("KEY_SETTINGS_TITLE") + QStringLiteral(" - Sprite Studio"));
    if (m_searchEdit) m_searchEdit->setPlaceholderText(tr("KEY_SETTINGS_SEARCH_PLACEHOLDER"));
    if (m_itemGeneral) m_itemGeneral->setText(0, tr("KEY_SETTINGS_CAT_GENERAL"));
    if (m_itemGit) m_itemGit->setText(0, tr("KEY_SETTINGS_CAT_GIT"));
    if (m_itemAtlas) m_itemAtlas->setText(0, tr("KEY_SETTINGS_CAT_ATLAS"));

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
