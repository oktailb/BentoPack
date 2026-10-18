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

#include "widgets/watchconfigdialog.h"
#include "extractor/extractorregistry.h"
#include "model/spritedocument.h"
#include "controller/projectcontroller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStyle>

WatchConfigDialog::WatchConfigDialog(LiveWatchManager *watchManager,
                                     const SpriteDocument *document,
                                     ProjectController *controller,
                                     QWidget *parent)
    : QDialog(parent)
    , m_watchManager(watchManager)
    , m_document(document)
    , m_controller(controller)
{
    setWindowTitle(tr("Live Watch Mode"));
    setMinimumWidth(620);
    resize(640, 680);

    setupUI();

    // Load active config if manager already holds one, or create sensible defaults
    LiveWatchConfig initialCfg;
    if (m_watchManager && m_watchManager->isWatching()) {
        initialCfg = m_watchManager->config();
    } else {
        QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
        initialCfg.load(settings);

        // If sourcePath is empty, prepopulate from active document
        if (initialCfg.sourcePath.isEmpty()) {
            QString curPath;
            if (m_document && !m_document->filePath().isEmpty()) {
                curPath = m_document->filePath();
            } else if (m_controller && !m_controller->currentFilePath().isEmpty()) {
                curPath = m_controller->currentFilePath();
            } else if (m_controller && !m_controller->currentProjectPath().isEmpty()) {
                curPath = m_controller->currentProjectPath();
            }
            if (!curPath.isEmpty()) {
                initialCfg = LiveWatchConfig::createDefault(curPath);
            }
        }
    }

    loadConfigToUI(initialCfg);

    if (m_watchManager) {
        connect(m_watchManager, &LiveWatchManager::stateChanged, this, &WatchConfigDialog::updateStatusUI);
        updateStatusUI(m_watchManager->state(), m_watchManager->lastStatusMessage());
    }
}

WatchConfigDialog::~WatchConfigDialog() = default;

void WatchConfigDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // ========================================================================
    // Header Banner
    // ========================================================================
    auto *headerLayout = new QHBoxLayout();
    auto *titleLayout = new QVBoxLayout();

    auto *lblTitle = new QLabel(tr("<b>Live Watch Mode</b> — Zero-Click Hot-Reload"), this);
    lblTitle->setStyleSheet(QStringLiteral("font-size: 15px;"));
    auto *lblSubtitle = new QLabel(tr("Monitors source files (Aseprite / PNG) and automatically re-exports to your game engine (Godot, Unity, Unreal) upon every save."), this);
    lblSubtitle->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px;"));
    lblSubtitle->setWordWrap(true);

    titleLayout->addWidget(lblTitle);
    titleLayout->addWidget(lblSubtitle);
    headerLayout->addLayout(titleLayout, 1);

    m_lblStatusBadge = new QLabel(this);
    m_lblStatusBadge->setAlignment(Qt::AlignCenter);
    m_lblStatusBadge->setMinimumWidth(110);
    m_lblStatusBadge->setFixedHeight(28);
    m_lblStatusBadge->setStyleSheet(QStringLiteral("border-radius: 14px; padding: 4px 10px; font-weight: bold; background-color: #333333; color: #aaaaaa;"));
    m_lblStatusBadge->setText(tr("Inactive"));
    headerLayout->addWidget(m_lblStatusBadge);

    mainLayout->addLayout(headerLayout);

    // ========================================================================
    // Group 1: What to watch (Source)
    // ========================================================================
    auto *grpSource = new QGroupBox(tr("1. What to watch (Source file or directory)"), this);
    auto *sourceLayout = new QVBoxLayout(grpSource);
    sourceLayout->setSpacing(8);

    auto *typeRow = new QHBoxLayout();
    m_rbSingleFile = new QRadioButton(tr("Single file (.ase, .aseprite, .png)"), grpSource);
    m_rbDirectory = new QRadioButton(tr("Entire directory (watch all sprites)"), grpSource);
    m_rbSingleFile->setChecked(true);
    typeRow->addWidget(m_rbSingleFile);
    typeRow->addWidget(m_rbDirectory);
    typeRow->addStretch();
    sourceLayout->addLayout(typeRow);

    auto *pathRow = new QHBoxLayout();
    m_txtSourcePath = new QLineEdit(grpSource);
    m_txtSourcePath->setPlaceholderText(tr("Path to file or directory to watch..."));
    m_btnBrowseSourceFile = new QPushButton(tr("File..."), grpSource);
    m_btnBrowseSourceDir = new QPushButton(tr("Directory..."), grpSource);
    m_btnUseCurrentProject = new QPushButton(tr("📌 Current Project"), grpSource);
    m_btnUseCurrentProject->setToolTip(tr("Auto-fill using the currently opened project source file"));

    pathRow->addWidget(m_txtSourcePath, 1);
    pathRow->addWidget(m_btnBrowseSourceFile);
    pathRow->addWidget(m_btnBrowseSourceDir);
    pathRow->addWidget(m_btnUseCurrentProject);
    sourceLayout->addLayout(pathRow);

    auto *optionsRow = new QHBoxLayout();
    auto *lblFilters = new QLabel(tr("Filters:"), grpSource);
    m_txtFileFilters = new QLineEdit(QStringLiteral("*.ase;*.aseprite;*.png"), grpSource);
    m_txtFileFilters->setToolTip(tr("Watched extensions separated by semicolons"));
    m_txtFileFilters->setMaximumWidth(180);

    auto *lblDebounce = new QLabel(tr("Debounce delay:"), grpSource);
    m_spinDebounce = new QSpinBox(grpSource);
    m_spinDebounce->setRange(100, 3000);
    m_spinDebounce->setValue(250);
    m_spinDebounce->setSuffix(QStringLiteral(" ms"));
    m_spinDebounce->setToolTip(tr("Wait time after file write detection to let Aseprite finish saving"));

    optionsRow->addWidget(lblFilters);
    optionsRow->addWidget(m_txtFileFilters);
    optionsRow->addSpacing(16);
    optionsRow->addWidget(lblDebounce);
    optionsRow->addWidget(m_spinDebounce);
    optionsRow->addStretch();
    sourceLayout->addLayout(optionsRow);

    mainLayout->addWidget(grpSource);

    // ========================================================================
    // Group 2: Where to export (Destination)
    // ========================================================================
    auto *grpOutput = new QGroupBox(tr("2. Where to export (Target and engine descriptor)"), this);
    auto *outputLayout = new QVBoxLayout(grpOutput);
    outputLayout->setSpacing(8);

    m_chkAutoExport = new QCheckBox(tr("Auto-export to game engine after each detected change"), grpOutput);
    m_chkAutoExport->setChecked(true);
    outputLayout->addWidget(m_chkAutoExport);

    auto *outDirRow = new QHBoxLayout();
    auto *lblOutDir = new QLabel(tr("Target directory:"), grpOutput);
    lblOutDir->setMinimumWidth(85);
    m_txtOutputDir = new QLineEdit(grpOutput);
    m_txtOutputDir->setPlaceholderText(tr("Asset destination directory..."));
    m_btnBrowseOutputDir = new QPushButton(tr("Browse..."), grpOutput);
    outDirRow->addWidget(lblOutDir);
    outDirRow->addWidget(m_txtOutputDir, 1);
    outDirRow->addWidget(m_btnBrowseOutputDir);
    outputLayout->addLayout(outDirRow);

    auto *baseNameRow = new QHBoxLayout();
    auto *lblBaseName = new QLabel(tr("Base name:"), grpOutput);
    lblBaseName->setMinimumWidth(85);
    m_txtBaseName = new QLineEdit(grpOutput);
    m_txtBaseName->setPlaceholderText(tr("Exported file base name (e.g., hero)"));
    baseNameRow->addWidget(lblBaseName);
    baseNameRow->addWidget(m_txtBaseName, 1);
    outputLayout->addLayout(baseNameRow);

    auto *previewRow = new QHBoxLayout();
    auto *lblPreviewTitle = new QLabel(tr("Generated file:"), grpOutput);
    lblPreviewTitle->setMinimumWidth(85);
    m_lblComputedPath = new QLabel(grpOutput);
    m_lblComputedPath->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 11px; color: #4caf50; background: rgba(0,0,0,0.15); padding: 4px; border-radius: 3px;"));
    m_lblComputedPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewRow->addWidget(lblPreviewTitle);
    previewRow->addWidget(m_lblComputedPath, 1);
    outputLayout->addLayout(previewRow);

    mainLayout->addWidget(grpOutput);

    // ========================================================================
    // Group 3: Engine Configuration & Compression Pipeline
    // ========================================================================
    auto *grpEngine = new QGroupBox(tr("3. Engine Configuration & Compression Pipeline"), this);
    auto *engineLayout = new QGridLayout(grpEngine);
    engineLayout->setHorizontalSpacing(12);
    engineLayout->setVerticalSpacing(6);

    // Format engine selector
    auto *lblFormat = new QLabel(tr("Target engine:"), grpEngine);
    m_comboFormat = new QComboBox(grpEngine);
    const auto &extractors = ExtractorRegistry::instance().extractors();
    for (Extractor *ext : extractors) {
        if (ext && ext->capabilities().testFlag(Extractor::CanExport)) {
            m_comboFormat->addItem(ext->displayName(), ext->id());
        }
    }
    // Default to Godot if present
    for (int i = 0; i < m_comboFormat->count(); ++i) {
        if (m_comboFormat->itemData(i).toString().contains(QStringLiteral("godot"), Qt::CaseInsensitive)) {
            m_comboFormat->setCurrentIndex(i);
            break;
        }
    }
    engineLayout->addWidget(lblFormat, 0, 0);
    engineLayout->addWidget(m_comboFormat, 0, 1);

    // Texture format
    auto *lblTexture = new QLabel(tr("Texture / VRAM:"), grpEngine);
    m_comboTextureFormat = new QComboBox(grpEngine);
    m_comboTextureFormat->addItem(tr("Standard PNG (RGBA8888 uncompressed GPU)"), TEXTURE_FORMAT_PNG);
    m_comboTextureFormat->addItem(tr("KTX2 / Basis Universal (GPU UASTC - High quality)"), TEXTURE_FORMAT_KTX2_UASTC);
    m_comboTextureFormat->addItem(tr("KTX2 / Basis Universal (GPU ETC1S - Maximum compression)"), TEXTURE_FORMAT_KTX2_ETC1S);
    engineLayout->addWidget(lblTexture, 0, 2);
    engineLayout->addWidget(m_comboTextureFormat, 0, 3);

    // 2D Mesh Mode
    auto *lblMesh = new QLabel(tr("2D Mesh:"), grpEngine);
    m_comboMeshMode = new QComboBox(grpEngine);
    m_comboMeshMode->addItem(tr("Classic Bounding Box (4-vertex Quad)"), false);
    m_comboMeshMode->addItem(tr("Tight Polygon CDT Mesh (Delaunay anti-overdraw)"), true);
    engineLayout->addWidget(lblMesh, 1, 0);
    engineLayout->addWidget(m_comboMeshMode, 1, 1);

    // Padding
    auto *lblPadding = new QLabel(tr("Padding:"), grpEngine);
    m_spinPadding = new QSpinBox(grpEngine);
    m_spinPadding->setRange(0, 32);
    m_spinPadding->setValue(2);
    m_spinPadding->setSuffix(QStringLiteral(" px"));
    engineLayout->addWidget(lblPadding, 1, 2);
    engineLayout->addWidget(m_spinPadding, 1, 3);

    // Checkboxes (M22, POT, Trim)
    m_chkModularCelDedup = new QCheckBox(tr("Modular cel deduplication (XXH3 64-bit)"), grpEngine);
    m_chkModularCelDedup->setChecked(true);
    m_chkModularCelDedup->setToolTip(tr("Deduplicates identical Aseprite cels across multiple layers"));
    engineLayout->addWidget(m_chkModularCelDedup, 2, 0, 1, 2);

    m_chkDeltaPatch = new QCheckBox(tr("Delta patch slicing (VRAM savings)"), grpEngine);
    m_chkDeltaPatch->setChecked(false);
    m_chkDeltaPatch->setToolTip(tr("Extracts modified delta patches between consecutive frames"));
    engineLayout->addWidget(m_chkDeltaPatch, 2, 2, 1, 2);

    m_chkDedupFrames = new QCheckBox(tr("Merge identical frames (Auto-Aliasing)"), grpEngine);
    m_chkDedupFrames->setChecked(true);
    engineLayout->addWidget(m_chkDedupFrames, 3, 0, 1, 2);

    m_chkPowerOfTwo = new QCheckBox(tr("Power of Two dimensions (POT)"), grpEngine);
    m_chkPowerOfTwo->setChecked(true);
    engineLayout->addWidget(m_chkPowerOfTwo, 3, 2, 1, 1);

    m_chkTrim = new QCheckBox(tr("Trim transparent pixels"), grpEngine);
    m_chkTrim->setChecked(false);
    engineLayout->addWidget(m_chkTrim, 3, 3, 1, 1);

    mainLayout->addWidget(grpEngine);

    // ========================================================================
    // Bottom Status & Actions
    // ========================================================================
    m_lblStatusDetail = new QLabel(tr("Ready."), this);
    m_lblStatusDetail->setStyleSheet(QStringLiteral("color: #aaaaaa; font-size: 11px;"));
    mainLayout->addWidget(m_lblStatusDetail);

    auto *btnLayout = new QHBoxLayout();
    m_btnToggleWatch = new QPushButton(tr("▶ Start Watching"), this);
    m_btnToggleWatch->setFixedHeight(32);
    m_btnToggleWatch->setStyleSheet(QStringLiteral("font-weight: bold; background-color: #2e7d32; color: white; border-radius: 4px; padding: 0 16px;"));

    m_btnTestExport = new QPushButton(tr("⚡ Export Now"), this);
    m_btnTestExport->setFixedHeight(32);
    m_btnTestExport->setToolTip(tr("Immediately triggers export to validate settings"));

    m_btnSaveDefaults = new QPushButton(tr("💾 Save"), this);
    m_btnSaveDefaults->setFixedHeight(32);

    m_btnClose = new QPushButton(tr("Close"), this);
    m_btnClose->setFixedHeight(32);

    btnLayout->addWidget(m_btnToggleWatch);
    btnLayout->addWidget(m_btnTestExport);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnSaveDefaults);
    btnLayout->addWidget(m_btnClose);
    mainLayout->addLayout(btnLayout);

    // ========================================================================
    // Signal Wiring
    // ========================================================================
    connect(m_btnBrowseSourceFile, &QPushButton::clicked, this, &WatchConfigDialog::onBrowseSourceFile);
    connect(m_btnBrowseSourceDir, &QPushButton::clicked, this, &WatchConfigDialog::onBrowseSourceDir);
    connect(m_btnBrowseOutputDir, &QPushButton::clicked, this, &WatchConfigDialog::onBrowseOutputDir);
    connect(m_btnUseCurrentProject, &QPushButton::clicked, this, &WatchConfigDialog::onUseCurrentProject);

    connect(m_txtSourcePath, &QLineEdit::textChanged, this, &WatchConfigDialog::updateComputedPathPreview);
    connect(m_txtOutputDir, &QLineEdit::textChanged, this, &WatchConfigDialog::updateComputedPathPreview);
    connect(m_txtBaseName, &QLineEdit::textChanged, this, &WatchConfigDialog::updateComputedPathPreview);
    connect(m_comboFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WatchConfigDialog::onFormatChanged);

    connect(m_btnToggleWatch, &QPushButton::clicked, this, &WatchConfigDialog::onToggleWatchClicked);
    connect(m_btnTestExport, &QPushButton::clicked, this, &WatchConfigDialog::onTestExportClicked);
    connect(m_btnSaveDefaults, &QPushButton::clicked, this, &WatchConfigDialog::onSaveDefaultsClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);

    connect(m_chkAutoExport, &QCheckBox::toggled, this, [this](bool checked) {
        m_txtOutputDir->setEnabled(checked);
        m_btnBrowseOutputDir->setEnabled(checked);
        m_txtBaseName->setEnabled(checked);
        m_comboFormat->setEnabled(checked);
        m_comboTextureFormat->setEnabled(checked);
        m_comboMeshMode->setEnabled(checked);
    });
}

void WatchConfigDialog::loadConfigToUI(const LiveWatchConfig &cfg)
{
    m_rbSingleFile->setChecked(cfg.sourceType == WatchSourceType::SingleFile);
    m_rbDirectory->setChecked(cfg.sourceType == WatchSourceType::Directory);
    m_txtSourcePath->setText(cfg.sourcePath);
    m_txtFileFilters->setText(cfg.fileFilters);
    m_spinDebounce->setValue(cfg.debounceMs);

    m_chkAutoExport->setChecked(cfg.autoExport);
    m_txtOutputDir->setText(cfg.outputDirectory);
    m_txtBaseName->setText(cfg.outputBaseName);

    // Select format in combo
    for (int i = 0; i < m_comboFormat->count(); ++i) {
        if (m_comboFormat->itemData(i).toString().compare(cfg.engineFormatId, Qt::CaseInsensitive) == 0) {
            m_comboFormat->setCurrentIndex(i);
            break;
        }
    }

    // Texture format
    for (int i = 0; i < m_comboTextureFormat->count(); ++i) {
        if (m_comboTextureFormat->itemData(i).toInt() == static_cast<int>(cfg.exportOptions.textureFormat)) {
            m_comboTextureFormat->setCurrentIndex(i);
            break;
        }
    }

    // Mesh
    m_comboMeshMode->setCurrentIndex(cfg.exportOptions.packOptions.algorithm == AtlasPacker::TightPolygon ? 1 : 0);

    m_chkModularCelDedup->setChecked(cfg.exportOptions.packOptions.enableCelDeduplication);
    m_chkDeltaPatch->setChecked(cfg.exportOptions.packOptions.enableDeltaPatching);
    m_chkDedupFrames->setChecked(cfg.exportOptions.packOptions.deduplicate);
    m_chkPowerOfTwo->setChecked(cfg.exportOptions.packOptions.powerOfTwo);
    m_chkTrim->setChecked(cfg.exportOptions.trimSprites);
    m_spinPadding->setValue(cfg.exportOptions.packOptions.padding);

    updateComputedPathPreview();
}

LiveWatchConfig WatchConfigDialog::buildConfigFromUI() const
{
    LiveWatchConfig cfg;
    cfg.sourceType = m_rbDirectory->isChecked() ? WatchSourceType::Directory : WatchSourceType::SingleFile;
    cfg.sourcePath = m_txtSourcePath->text().trimmed();
    cfg.fileFilters = m_txtFileFilters->text().trimmed();
    cfg.debounceMs = m_spinDebounce->value();

    cfg.autoExport = m_chkAutoExport->isChecked();
    cfg.outputDirectory = m_txtOutputDir->text().trimmed();
    cfg.outputBaseName = m_txtBaseName->text().trimmed();
    cfg.engineFormatId = m_comboFormat->currentData().toString();

    cfg.exportOptions.textureFormat = static_cast<TextureFormat>(m_comboTextureFormat->currentData().toInt());
    cfg.exportOptions.packOptions.algorithm = (m_comboMeshMode->currentIndex() == 1) ? AtlasPacker::TightPolygon : AtlasPacker::MaxRects;
    cfg.exportOptions.packOptions.enableCelDeduplication = m_chkModularCelDedup->isChecked();
    cfg.exportOptions.packOptions.enableDeltaPatching = m_chkDeltaPatch->isChecked();
    cfg.exportOptions.packOptions.deduplicate = m_chkDedupFrames->isChecked();
    cfg.exportOptions.packOptions.powerOfTwo = m_chkPowerOfTwo->isChecked();
    cfg.exportOptions.packOptions.padding = m_spinPadding->value();
    cfg.exportOptions.trimSprites = m_chkTrim->isChecked();

    // Map engine format
    QString fmtId = cfg.engineFormatId.toLower();
    if (fmtId.contains(QStringLiteral("unity"))) {
        cfg.exportOptions.format = FORMAT_UNITY;
    } else if (fmtId.contains(QStringLiteral("godot"))) {
        cfg.exportOptions.format = FORMAT_GODOT;
    } else if (fmtId.contains(QStringLiteral("unreal"))) {
        cfg.exportOptions.format = FORMAT_UNREAL;
    } else {
        cfg.exportOptions.format = FORMAT_TEXTUREPACKER_JSON;
    }

    return cfg;
}

LiveWatchConfig WatchConfigDialog::config() const
{
    return buildConfigFromUI();
}

void WatchConfigDialog::onBrowseSourceFile()
{
    QString initial = m_txtSourcePath->text().trimmed();
    if (initial.isEmpty() || !QFile::exists(initial)) {
        initial = QDir::currentPath();
    }
    QString file = QFileDialog::getOpenFileName(this, tr("Select source file to watch"),
                                                initial, tr("Sprites & Animations (*.ase *.aseprite *.png);;All Files (*.*)"));
    if (!file.isEmpty()) {
        m_txtSourcePath->setText(file);
        m_rbSingleFile->setChecked(true);
        if (m_txtOutputDir->text().trimmed().isEmpty()) {
            QFileInfo fi(file);
            m_txtOutputDir->setText(fi.absolutePath());
            m_txtBaseName->setText(fi.completeBaseName());
        }
        updateComputedPathPreview();
    }
}

void WatchConfigDialog::onBrowseSourceDir()
{
    QString initial = m_txtSourcePath->text().trimmed();
    if (initial.isEmpty() || !QDir(initial).exists()) {
        initial = QDir::currentPath();
    }
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select source directory to watch"), initial);
    if (!dir.isEmpty()) {
        m_txtSourcePath->setText(dir);
        m_rbDirectory->setChecked(true);
        if (m_txtOutputDir->text().trimmed().isEmpty()) {
            m_txtOutputDir->setText(dir);
            m_txtBaseName->setText(QFileInfo(dir).fileName());
        }
        updateComputedPathPreview();
    }
}

void WatchConfigDialog::onBrowseOutputDir()
{
    QString initial = m_txtOutputDir->text().trimmed();
    if (initial.isEmpty() || !QDir(initial).exists()) {
        initial = QDir::currentPath();
    }
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select destination directory"), initial);
    if (!dir.isEmpty()) {
        m_txtOutputDir->setText(dir);
        updateComputedPathPreview();
    }
}

void WatchConfigDialog::onUseCurrentProject()
{
    QString curPath;
    if (m_document && !m_document->filePath().isEmpty()) {
        curPath = m_document->filePath();
    } else if (m_controller && !m_controller->currentFilePath().isEmpty()) {
        curPath = m_controller->currentFilePath();
    } else if (m_controller && !m_controller->currentProjectPath().isEmpty()) {
        curPath = m_controller->currentProjectPath();
    }

    if (curPath.isEmpty()) {
        QMessageBox::information(this, tr("Live Watch Mode"), tr("No project or source file is currently open in BentoPack."));
        return;
    }

    LiveWatchConfig defaultCfg = LiveWatchConfig::createDefault(curPath);
    loadConfigToUI(defaultCfg);
}

void WatchConfigDialog::onFormatChanged(int /*index*/)
{
    updateComputedPathPreview();
}

void WatchConfigDialog::updateComputedPathPreview()
{
    LiveWatchConfig cfg = buildConfigFromUI();
    QString path = cfg.computedExportPath();
    m_lblComputedPath->setText(QDir::toNativeSeparators(path));
}

void WatchConfigDialog::onToggleWatchClicked()
{
    if (!m_watchManager) return;

    if (m_watchManager->isWatching()) {
        m_watchManager->stopWatching();
    } else {
        LiveWatchConfig cfg = buildConfigFromUI();
        QString err;
        if (!m_watchManager->startWatching(cfg, &err)) {
            QMessageBox::warning(this, tr("Live Watch Error"), err);
        } else {
            // Save settings automatically when started
            QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
            cfg.save(settings);
        }
    }
}

void WatchConfigDialog::onTestExportClicked()
{
    if (!m_watchManager) return;

    LiveWatchConfig cfg = buildConfigFromUI();
    m_watchManager->updateConfig(cfg);
    m_watchManager->triggerManualReload();
}

void WatchConfigDialog::onSaveDefaultsClicked()
{
    LiveWatchConfig cfg = buildConfigFromUI();
    QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
    cfg.save(settings);
    m_lblStatusDetail->setText(tr("Settings saved as default."));
}

void WatchConfigDialog::updateStatusUI(WatchState state, const QString &message)
{
    switch (state) {
    case WatchState::Idle:
        m_lblStatusBadge->setText(tr("Inactive"));
        m_lblStatusBadge->setStyleSheet(QStringLiteral("border-radius: 14px; padding: 4px 10px; font-weight: bold; background-color: #424242; color: #bdbdbd;"));
        m_btnToggleWatch->setText(tr("▶ Start Watching"));
        m_btnToggleWatch->setStyleSheet(QStringLiteral("font-weight: bold; background-color: #2e7d32; color: white; border-radius: 4px; padding: 0 16px;"));
        break;
    case WatchState::Watching:
        m_lblStatusBadge->setText(tr("🟢 Watching"));
        m_lblStatusBadge->setStyleSheet(QStringLiteral("border-radius: 14px; padding: 4px 10px; font-weight: bold; background-color: #1b5e20; color: #a5d6a7;"));
        m_btnToggleWatch->setText(tr("⏹ Stop Watching"));
        m_btnToggleWatch->setStyleSheet(QStringLiteral("font-weight: bold; background-color: #c62828; color: white; border-radius: 4px; padding: 0 16px;"));
        break;
    case WatchState::Compiling:
        m_lblStatusBadge->setText(tr("⚙ Compiling..."));
        m_lblStatusBadge->setStyleSheet(QStringLiteral("border-radius: 14px; padding: 4px 10px; font-weight: bold; background-color: #e65100; color: #ffe0b2;"));
        break;
    case WatchState::Success:
        m_lblStatusBadge->setText(tr("✅ Up to date"));
        m_lblStatusBadge->setStyleSheet(QStringLiteral("border-radius: 14px; padding: 4px 10px; font-weight: bold; background-color: #1b5e20; color: #a5d6a7;"));
        m_btnToggleWatch->setText(tr("⏹ Stop Watching"));
        m_btnToggleWatch->setStyleSheet(QStringLiteral("font-weight: bold; background-color: #c62828; color: white; border-radius: 4px; padding: 0 16px;"));
        break;
    case WatchState::Error:
        m_lblStatusBadge->setText(tr("⚠️ Error"));
        m_lblStatusBadge->setStyleSheet(QStringLiteral("border-radius: 14px; padding: 4px 10px; font-weight: bold; background-color: #b71c1c; color: #ffcdd2;"));
        break;
    }

    if (!message.isEmpty()) {
        m_lblStatusDetail->setText(message);
    }
}
