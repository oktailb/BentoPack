#include "include/widgets/settingsdialog.h"
#include "config/appconfig.h"
#include "include/project/sessionmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget *parent, PageIndex initialPage)
    : QDialog(parent)
{
    setupUI();
    loadSettings();
    setCurrentPage(initialPage);

    setWindowTitle(tr("Préférences") + QStringLiteral(" - Sprite Studio"));
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
    m_searchEdit->setPlaceholderText(tr("Rechercher dans les réglages..."));
    m_searchEdit->setClearButtonEnabled(true);
    leftLayout->addWidget(m_searchEdit);

    m_categoryTree = new QTreeWidget(leftWidget);
    m_categoryTree->setHeaderHidden(true);
    m_categoryTree->setRootIsDecorated(false);
    m_categoryTree->setIndentation(12);

    m_itemGeneral = new QTreeWidgetItem(m_categoryTree);
    m_itemGeneral->setText(0, tr("Général"));
    m_itemGeneral->setData(0, Qt::UserRole, PageGeneral);

    m_itemGit = new QTreeWidgetItem(m_categoryTree);
    m_itemGit->setText(0, tr("Contrôle de Version (Git)"));
    m_itemGit->setData(0, Qt::UserRole, PageGit);

    m_itemAtlas = new QTreeWidgetItem(m_categoryTree);
    m_itemAtlas->setText(0, tr("Affichage & Atlas"));
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
}

QWidget* SettingsDialog::createGeneralPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 0, 0, 0);

    QLabel *header = new QLabel(tr("Réglages Généraux"));
    header->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; margin-bottom: 8px;"));
    layout->addWidget(header);

    // Project & History Group
    QGroupBox *grpHistory = new QGroupBox(tr("Historique & Projet"), page);
    QFormLayout *formHistory = new QFormLayout(grpHistory);

    m_spinUndoLimit = new QSpinBox(grpHistory);
    m_spinUndoLimit->setRange(5, 500);
    m_spinUndoLimit->setSuffix(tr(" actions"));
    formHistory->addRow(tr("Limite d'annulation (Undo/Redo) :"), m_spinUndoLimit);

    m_spinMaxRecentFiles = new QSpinBox(grpHistory);
    m_spinMaxRecentFiles->setRange(1, 30);
    formHistory->addRow(tr("Nombre maximal de fichiers récents :"), m_spinMaxRecentFiles);
    layout->addWidget(grpHistory);

    // Segmentation & Background Group
    QGroupBox *grpExtraction = new QGroupBox(tr("Extraction & Arrière-plan"), page);
    QFormLayout *formExtraction = new QFormLayout(grpExtraction);

    m_spinAlphaThreshold = new QSpinBox(grpExtraction);
    m_spinAlphaThreshold->setRange(0, 255);
    formExtraction->addRow(tr("Seuil d'opacité alpha par défaut :"), m_spinAlphaThreshold);

    m_spinBgRemovalTol = new QSpinBox(grpExtraction);
    m_spinBgRemovalTol->setRange(0, 100);
    formExtraction->addRow(tr("Tolérance de suppression du fond :"), m_spinBgRemovalTol);
    layout->addWidget(grpExtraction);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createGitPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 0, 0, 0);

    QLabel *header = new QLabel(tr("Contrôle de Version (Git)"));
    header->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; margin-bottom: 8px;"));
    layout->addWidget(header);

    // Author Identity Group
    QGroupBox *grpAuthor = new QGroupBox(tr("Identité de l'auteur (Commits Git)"), page);
    QVBoxLayout *authorLayout = new QVBoxLayout(grpAuthor);

    QLabel *infoLabel = new QLabel(
        tr("Cette identité est inscrite comme signature d'auteur sur chaque commit de l'historique du projet .ssp.")
    );
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(QStringLiteral("color: #7f8c8d; margin-bottom: 6px;"));
    authorLayout->addWidget(infoLabel);

    QFormLayout *formAuthor = new QFormLayout();
    m_editGitAuthorName = new QLineEdit(grpAuthor);
    m_editGitAuthorName->setPlaceholderText(tr("ex: John Doe"));
    formAuthor->addRow(tr("Nom de l'auteur :"), m_editGitAuthorName);

    m_editGitAuthorEmail = new QLineEdit(grpAuthor);
    m_editGitAuthorEmail->setPlaceholderText(tr("ex: john.doe@example.com"));
    formAuthor->addRow(tr("Email de l'auteur :"), m_editGitAuthorEmail);
    authorLayout->addLayout(formAuthor);

    QHBoxLayout *detectLayout = new QHBoxLayout();
    m_btnDetectGit = new QPushButton(tr("Détecter depuis la configuration Git système"), grpAuthor);
    connect(m_btnDetectGit, &QPushButton::clicked, this, &SettingsDialog::onDetectSystemGitIdentity);
    detectLayout->addWidget(m_btnDetectGit);
    detectLayout->addStretch();
    authorLayout->addLayout(detectLayout);

    layout->addWidget(grpAuthor);

    // Git Engine Status Group
    QGroupBox *grpEngine = new QGroupBox(tr("Moteur Git Intégré"), page);
    QVBoxLayout *engineLayout = new QVBoxLayout(grpEngine);

    bool gitAvailable = SessionManager::isGitAvailable();
    QString statusText = gitAvailable
        ? tr("Statut : <b style='color:#27ae60;'>LibGit2 actif</b> (gestion d'historique et branches opérationnelle)")
        : tr("Statut : <b style='color:#e74c3c;'>LibGit2 non compilé</b> (historique git désactivé)");

    m_lblGitStatus = new QLabel(statusText, grpEngine);
    m_lblGitStatus->setTextFormat(Qt::RichText);
    engineLayout->addWidget(m_lblGitStatus);

    QLabel *engineDesc = new QLabel(
        tr("Chaque action d'édition (découpe, renommage, fusion de sprite, création d'animation) "
           "génère un commit incrémental et atomique dans le dépôt Git transparent du projet .ssp.")
    );
    engineDesc->setWordWrap(true);
    engineDesc->setStyleSheet(QStringLiteral("color: #7f8c8d;"));
    engineLayout->addWidget(engineDesc);

    layout->addWidget(grpEngine);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createAtlasPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 0, 0, 0);

    QLabel *header = new QLabel(tr("Affichage & Atlas"));
    header->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; margin-bottom: 8px;"));
    layout->addWidget(header);

    // Zoom and Navigation
    QGroupBox *grpZoom = new QGroupBox(tr("Navigation & Zoom"), page);
    QFormLayout *formZoom = new QFormLayout(grpZoom);

    m_spinZoomStep = new QDoubleSpinBox(grpZoom);
    m_spinZoomStep->setRange(1.05, 2.50);
    m_spinZoomStep->setSingleStep(0.05);
    formZoom->addRow(tr("Facteur de zoom (molette) :"), m_spinZoomStep);

    m_spinZoomMin = new QDoubleSpinBox(grpZoom);
    m_spinZoomMin->setRange(0.01, 1.0);
    m_spinZoomMin->setSingleStep(0.05);
    formZoom->addRow(tr("Niveau de dézoom minimal :"), m_spinZoomMin);

    m_spinZoomMax = new QDoubleSpinBox(grpZoom);
    m_spinZoomMax->setRange(1.0, 50.0);
    m_spinZoomMax->setSingleStep(1.0);
    formZoom->addRow(tr("Niveau de zoom maximal :"), m_spinZoomMax);

    m_spinFitPadding = new QSpinBox(grpZoom);
    m_spinFitPadding->setRange(0, 200);
    m_spinFitPadding->setSuffix(QStringLiteral(" px"));
    formZoom->addRow(tr("Marge de cadrage automatique :"), m_spinFitPadding);

    layout->addWidget(grpZoom);

    // Slicing parameters
    QGroupBox *grpSlicing = new QGroupBox(tr("Découpage interactif"), page);
    QFormLayout *formSlicing = new QFormLayout(grpSlicing);

    m_spinMinSliceSize = new QSpinBox(grpSlicing);
    m_spinMinSliceSize->setRange(1, 100);
    m_spinMinSliceSize->setSuffix(QStringLiteral(" px"));
    formSlicing->addRow(tr("Taille minimale de boîte de découpe :"), m_spinMinSliceSize);

    layout->addWidget(grpSlicing);
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

    bool matchGeneral = QStringLiteral("général general projet project undo redo recent récents alpha seuil tolerance fond background").contains(query);
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
    saveSettings();
}

void SettingsDialog::restoreDefaults()
{
    if (QMessageBox::question(this, tr("Rétablir les valeurs par défaut"),
                              tr("Voulez-vous vraiment réinitialiser tous les paramètres à leurs valeurs par défaut ?"),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        AppConfig::instance().resetToDefaults();
        loadSettings();
    }
}
