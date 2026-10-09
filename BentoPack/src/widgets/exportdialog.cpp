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

#include "widgets/exportdialog.h"
#include "ui_exportdialog.h"
#include "extractor/extractorregistry.h"
#include "packer/vramtexturecompressor.h"
#include "controller/projectcontroller.h"
#include "config/appconfig.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>
#include <QPushButton>
#include <QMessageBox>
#include <QCloseEvent>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

ExportDialog::ExportDialog(const SpriteDocument *document, const QString &defaultPath, QWidget *parent, ProjectController *controller)
    : QDialog(parent)
    , ui(std::make_unique<Ui::ExportDialog>())
    , m_document(document)
    , m_controller(controller)
    , m_debounceTimer(new QTimer(this))
{
    ui->setupUi(this);

    // Set button texts
    if (QPushButton *okBtn = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        okBtn->setText(tr("Export"));
    }
    if (QPushButton *cancelBtn = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelBtn->setText(tr("Cancel"));
    }

    // Populate formats dynamically from registered Extractor plugins
    ui->comboFormat->clear();
    const auto &extractors = ExtractorRegistry::instance().extractors();
    for (Extractor *ext : extractors) {
        if (ext && ext->capabilities().testFlag(Extractor::CanExport)) {
            ui->comboFormat->addItem(ext->displayName(), ext->id());
        }
    }

    // Determine initial directory, base name, and format
    QString initialDir;
    QString initialBaseName;

    if (!defaultPath.isEmpty()) {
        QFileInfo fi(defaultPath);
        initialDir = fi.absolutePath();
        initialBaseName = fi.completeBaseName();
        if (initialBaseName.endsWith(QStringLiteral(".unity"), Qt::CaseInsensitive)) initialBaseName.chop(6);
        if (initialBaseName.endsWith(QStringLiteral(".paper2d"), Qt::CaseInsensitive)) initialBaseName.chop(8);

        // Match format by file extension
        QString sfx = fi.suffix().toLower();
        for (int i = 0; i < ui->comboFormat->count(); ++i) {
            QString extId = ui->comboFormat->itemData(i).toString();
            Extractor *ext = ExtractorRegistry::instance().findExtractorById(extId);
            if (ext) {
                for (const QString &extSfx : ext->supportedExtensions()) {
                    if (extSfx.compare(sfx, Qt::CaseInsensitive) == 0) {
                        ui->comboFormat->setCurrentIndex(i);
                        break;
                    }
                }
            }
        }
    } else if (m_document && !m_document->filePath().isEmpty()) {
        QFileInfo fi(m_document->filePath());
        initialDir = fi.absolutePath();
        initialBaseName = fi.completeBaseName();
    } else {
        initialDir = QDir::currentPath();
    }

    if (initialBaseName.isEmpty()) {
        if (m_document && !m_document->projectName().isEmpty() && m_document->projectName() != QStringLiteral("untitled")) {
            initialBaseName = m_document->projectName();
        } else {
            initialBaseName = QStringLiteral("atlas");
        }
    }

    // Initialize GIF FPS from document if available
    if (m_document && !m_document->animations().isEmpty()) {
        int firstFps = m_document->animations().first().fps;
        if (firstFps > 0) {
            ui->spinGifFps->setValue(firstFps);
        }
    }

    ui->txtOutputDir->setText(QDir::toNativeSeparators(initialDir));
    ui->txtBaseName->setText(initialBaseName);

    // Connect signals for destination fields
    connect(ui->btnBrowseDir, &QPushButton::clicked, this, &ExportDialog::onBrowseDirClicked);
    connect(ui->btnBrowse, &QPushButton::clicked, this, &ExportDialog::onBrowseFileClicked);
    connect(ui->txtOutputDir, &QLineEdit::textChanged, this, &ExportDialog::updateComputedPath);
    connect(ui->txtBaseName, &QLineEdit::textChanged, this, &ExportDialog::updateComputedPath);
    connect(ui->txtFilePath, &QLineEdit::textChanged, this, [this](const QString &text) {
        validateFilePath();
        if (m_updatingPathInternally) return;
        QString trimmed = text.trimmed();
        if (!trimmed.isEmpty()) {
            QFileInfo fi(trimmed);
            if (!fi.completeBaseName().isEmpty()) {
                m_updatingPathInternally = true;
                if (!fi.path().isEmpty() && fi.path() != QStringLiteral(".")) {
                    ui->txtOutputDir->setText(QDir::toNativeSeparators(fi.path()));
                }
                ui->txtBaseName->setText(fi.completeBaseName());
                m_updatingPathInternally = false;
            }
        }
    });

    // Configure debounce timer for live stats calculation
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(60);
    connect(m_debounceTimer, &QTimer::timeout, this, &ExportDialog::updateStats);

    // Format & packing signals
    connect(ui->comboFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExportDialog::onFormatChanged);
    connect(ui->comboAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() { m_debounceTimer->start(); });
    connect(ui->spinPadding, QOverload<int>::of(&QSpinBox::valueChanged), [this]() { m_debounceTimer->start(); });
    connect(ui->spinBorderPadding, QOverload<int>::of(&QSpinBox::valueChanged), [this]() { m_debounceTimer->start(); });
    connect(ui->spinExtrude, QOverload<int>::of(&QSpinBox::valueChanged), [this]() { m_debounceTimer->start(); });
    connect(ui->chkPowerOfTwo, &QCheckBox::toggled, [this]() { m_debounceTimer->start(); });
    connect(ui->chkForceSquare, &QCheckBox::toggled, [this]() { m_debounceTimer->start(); });
    connect(ui->chkDeduplicate, &QCheckBox::toggled, [this]() { m_debounceTimer->start(); });
    connect(ui->chkTrim, &QCheckBox::toggled, [this]() { m_debounceTimer->start(); });

    // VRAM compression signals
    connect(ui->comboTextureFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ExportDialog::onTextureFormatChanged);
    connect(ui->comboVramQuality, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() { m_debounceTimer->start(); });
    connect(ui->chkZstd, &QCheckBox::toggled, [this](bool checked) {
        ui->spinZstdLevel->setEnabled(checked && ui->comboTextureFormat->currentIndex() > 0);
        m_debounceTimer->start();
    });
    connect(ui->spinZstdLevel, QOverload<int>::of(&QSpinBox::valueChanged), [this]() { m_debounceTimer->start(); });

    // Load defaults from AppConfig
    const auto &expCfg = AppConfig::instance().exportSettings();
    if (!expCfg.defaultFormatId.isEmpty()) {
        int idx = ui->comboFormat->findData(expCfg.defaultFormatId);
        if (idx >= 0) {
            ui->comboFormat->setCurrentIndex(idx);
        }
    }
    if (expCfg.defaultTextureFormatIndex >= 0 && expCfg.defaultTextureFormatIndex < ui->comboTextureFormat->count()) {
        ui->comboTextureFormat->setCurrentIndex(expCfg.defaultTextureFormatIndex);
    }
    if (expCfg.defaultAlgorithmIndex >= 0 && expCfg.defaultAlgorithmIndex < ui->comboAlgorithm->count()) {
        ui->comboAlgorithm->setCurrentIndex(expCfg.defaultAlgorithmIndex);
    }
    ui->chkZstd->setChecked(expCfg.defaultZstd);
    ui->spinZstdLevel->setValue(expCfg.defaultZstdLevel);

    updateComputedPath();
    updateContextualVisibility();
    onTextureFormatChanged(ui->comboTextureFormat->currentIndex());
    validateFilePath();
    updateStats();
}

ExportDialog::~ExportDialog() = default;

QString ExportDialog::exportFilePath() const
{
    return ui->txtFilePath->text().trimmed();
}

ExportOptions ExportDialog::exportOptions() const
{
    ExportOptions opts;

    // Format
    QString extId = ui->comboFormat->currentData().toString();
    opts.formatId = extId;
    if (extId == QStringLiteral("godot_extractor")) {
        opts.format = FORMAT_GODOT;
    } else if (extId == QStringLiteral("json_extractor")) {
        opts.format = FORMAT_TEXTUREPACKER_JSON;
    } else if (extId == QStringLiteral("unity")) {
        opts.format = FORMAT_UNITY;
    } else if (extId == QStringLiteral("unreal")) {
        opts.format = FORMAT_UNREAL;
    }

    // Format-specific parameters
    if (extId == QStringLiteral("gif_extractor")) {
        opts.extraParams[QStringLiteral("gifLoopMode")] = ui->comboGifLoop->currentIndex();
        opts.extraParams[QStringLiteral("gifFps")] = ui->spinGifFps->value();
        opts.extraParams[QStringLiteral("gifAlphaThreshold")] = ui->spinGifAlphaThreshold->value();
        opts.extraParams[QStringLiteral("gifAllAnimations")] = ui->chkGifAllAnimations->isChecked();
    } else if (extId == QStringLiteral("aseprite_extractor")) {
        opts.extraParams[QStringLiteral("asepriteCompress")] = ui->chkAsepriteCompress->isChecked();
        opts.extraParams[QStringLiteral("asepriteExportTags")] = ui->chkAsepriteTags->isChecked();
    }

    // Atlas PackOptions
    AtlasPacker::PackOptions &pOpts = opts.packOptions;

    int algoIdx = ui->comboAlgorithm->currentIndex();
    switch (algoIdx) {
    case 0:
        pOpts.algorithm = AtlasPacker::KeepLayout;
        break;
    case 1:
        pOpts.algorithm = AtlasPacker::MaxRects;
        pOpts.heuristic = MaxRectsHeuristic::BestShortSideFit;
        break;
    case 2:
        pOpts.algorithm = AtlasPacker::MaxRects;
        pOpts.heuristic = MaxRectsHeuristic::BestAreaFit;
        break;
    case 3:
        pOpts.algorithm = AtlasPacker::MaxRects;
        pOpts.heuristic = MaxRectsHeuristic::BestLongSideFit;
        break;
    case 4:
        pOpts.algorithm = AtlasPacker::MaxRects;
        pOpts.heuristic = MaxRectsHeuristic::BottomLeft;
        break;
    case 5:
        pOpts.algorithm = AtlasPacker::TightPolygon;
        break;
    case 6:
        pOpts.algorithm = AtlasPacker::PowerOfTwoPacker;
        break;
    case 7:
        pOpts.algorithm = AtlasPacker::RowPacker;
        break;
    case 8:
        pOpts.algorithm = AtlasPacker::GridPacker;
        break;
    default:
        pOpts.algorithm = AtlasPacker::KeepLayout;
        break;
    }

    pOpts.padding = ui->spinPadding->value();
    pOpts.borderPadding = ui->spinBorderPadding->value();
    pOpts.extrude = ui->spinExtrude->value();
    pOpts.powerOfTwo = ui->chkPowerOfTwo->isChecked();
    pOpts.forceSquare = ui->chkForceSquare->isChecked();
    pOpts.deduplicate = ui->chkDeduplicate->isChecked();

    opts.padding = pOpts.padding;
    opts.trimSprites = ui->chkTrim->isChecked();

    // VRAM texture options
    int texFmtIdx = ui->comboTextureFormat->currentIndex();
    if (texFmtIdx == 0) {
        opts.textureFormat = TEXTURE_FORMAT_PNG;
    } else if (texFmtIdx == 1) {
        opts.textureFormat = TEXTURE_FORMAT_KTX2_UASTC;
        opts.vramOptions.format = VramFormat::KTX2_UASTC;
    } else if (texFmtIdx == 2) {
        opts.textureFormat = TEXTURE_FORMAT_KTX2_ETC1S;
        opts.vramOptions.format = VramFormat::KTX2_ETC1S;
    }

    int qIdx = ui->comboVramQuality->currentIndex();
    opts.vramOptions.qualityLevel = (qIdx == 0 ? 1 : (qIdx == 1 ? 2 : 3));
    opts.vramOptions.zstdSupercompression = ui->chkZstd->isChecked();
    opts.vramOptions.zstdLevel = ui->spinZstdLevel->value();

    // 2D Lighting / Material maps (M21)
    opts.exportMaterialMaps = ui->chkExportMaterialMaps->isChecked();
    opts.normalMapYFlip = ui->chkNormalMapYFlip->isChecked();

    return opts;
}

void ExportDialog::onBrowseDirClicked()
{
    QString currentDir = ui->txtOutputDir->text().trimmed();
    if (currentDir.isEmpty() || !QDir(currentDir).exists()) {
        currentDir = (m_document && !m_document->filePath().isEmpty())
            ? QFileInfo(m_document->filePath()).absolutePath()
            : QDir::currentPath();
    }

    QString chosenDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Export Directory"),
        currentDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!chosenDir.isEmpty()) {
        ui->txtOutputDir->setText(QDir::toNativeSeparators(chosenDir));
        updateComputedPath();
    }
}

void ExportDialog::onBrowseFileClicked()
{
    QString filter;
    QString extId = ui->comboFormat->currentData().toString();
    Extractor *ext = ExtractorRegistry::instance().findExtractorById(extId);
    if (ext) {
        QStringList patterns;
        for (const QString &s : ext->supportedExtensions()) {
            patterns << QStringLiteral("*.%1").arg(s);
        }
        filter = QStringLiteral("%1 (%2);;%3 (*.*)")
                    .arg(ext->displayName(), patterns.join(QStringLiteral(" ")), tr("All Files"));
    } else {
        filter = tr("All Files (*.*)");
    }

    QString initialPath = exportFilePath();
    QString chosen = QFileDialog::getSaveFileName(this, tr("Select Export Destination"), initialPath, filter);
    if (!chosen.isEmpty()) {
        QFileInfo fi(chosen);
        ui->txtOutputDir->setText(QDir::toNativeSeparators(fi.absolutePath()));
        ui->txtBaseName->setText(fi.completeBaseName());
        updateComputedPath();
    }
}

void ExportDialog::updateComputedPath()
{
    if (m_updatingPathInternally) return;
    m_updatingPathInternally = true;

    QString dirPath = ui->txtOutputDir->text().trimmed();
    if (dirPath.isEmpty()) {
        dirPath = QDir::currentPath();
    }
    QString baseName = ui->txtBaseName->text().trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("atlas");
    }

    QString extId = ui->comboFormat->currentData().toString();
    Extractor *ext = ExtractorRegistry::instance().findExtractorById(extId);
    QString extension = (ext && !ext->supportedExtensions().isEmpty()) ? ext->supportedExtensions().first() : QStringLiteral("tres");
    if (!extension.startsWith('.')) {
        extension.prepend('.');
    }

    QDir dir(dirPath);
    QString fullPath = dir.filePath(baseName + extension);
    ui->txtFilePath->setText(QDir::toNativeSeparators(fullPath));

    m_updatingPathInternally = false;
    validateFilePath();
}

void ExportDialog::updateContextualVisibility()
{
    QString extId = ui->comboFormat->currentData().toString();
    bool isGif = (extId == QStringLiteral("gif_extractor"));
    bool isAseprite = (extId == QStringLiteral("aseprite_extractor"));
    bool isAtlas = (!isGif && !isAseprite);

    // Show/hide contextual groups
    ui->grpGeometry->setVisible(isAtlas);
    ui->grpVram->setVisible(isAtlas);
    bool hasAux = m_document && m_document->hasAnyAuxiliaryMaps();
    ui->grpLighting->setVisible(isAtlas && hasAux);
    ui->grpStats->setVisible(isAtlas);
    ui->grpGifOptions->setVisible(isGif);
    ui->grpAsepriteOptions->setVisible(isAseprite);

    // Contextual description
    if (isGif) {
        ui->lblFormatDesc->setText(tr("Generates animated multi-frame GIF files with frame timings, transparency, and looping options."));
    } else if (isAseprite) {
        ui->lblFormatDesc->setText(tr("Generates a native Aseprite binary project (.ase/.aseprite) preserving layers, cels, and animation tags."));
    } else if (extId == QStringLiteral("godot_extractor")) {
        ui->lblFormatDesc->setText(tr("Exports Godot 4 SpriteFrames resource (.tres) with atlas texture and animation metadata."));
    } else if (extId == QStringLiteral("unity")) {
        ui->lblFormatDesc->setText(tr("Exports Unity 2D Sprite Mesh metadata (.unity.json) with atlas texture."));
    } else if (extId == QStringLiteral("unreal")) {
        ui->lblFormatDesc->setText(tr("Exports Unreal Engine Paper2D sprite definitions (.paper2d.json) with atlas texture."));
    } else if (extId == QStringLiteral("libgdx_spine_extractor")) {
        ui->lblFormatDesc->setText(tr("Exports LibGDX / Spine atlas format (.atlas) with companion atlas image."));
    } else if (extId == QStringLiteral("json_extractor")) {
        ui->lblFormatDesc->setText(tr("Exports TexturePacker JSON / Aseprite JSON atlas format with sprite frames."));
    } else {
        ui->lblFormatDesc->setText(tr("Exports packed texture atlas and companion sprite definitions."));
    }

    if (isAtlas) {
        m_debounceTimer->start();
    }

    adjustSize();
}

void ExportDialog::onFormatChanged(int index)
{
    Q_UNUSED(index);
    updateComputedPath();
    updateContextualVisibility();
}

void ExportDialog::onTextureFormatChanged(int index)
{
    bool isVram = (index > 0);
    ui->lblVramQuality->setEnabled(isVram);
    ui->comboVramQuality->setEnabled(isVram);
    ui->chkZstd->setEnabled(isVram);
    ui->lblZstdLevel->setEnabled(isVram && ui->chkZstd->isChecked());
    ui->spinZstdLevel->setEnabled(isVram && ui->chkZstd->isChecked());

    m_debounceTimer->start();
}

void ExportDialog::updateStats()
{
    QString extId = ui->comboFormat->currentData().toString();
    bool isAtlas = (extId != QStringLiteral("gif_extractor") && extId != QStringLiteral("aseprite_extractor"));
    if (!isAtlas) {
        return;
    }

    if (!m_document || m_document->frameCount() == 0) {
        ui->lblDimensions->setText(tr("Dimensions: --"));
        ui->lblEfficiency->setText(tr("Packing Efficiency: --"));
        ui->lblFrames->setText(tr("Frames: 0"));
        ui->lblVramSavings->setText(tr("GPU VRAM: --"));
        return;
    }

    ExportOptions opts = exportOptions();

    auto updateVramLabel = [this, &opts](int w, int h) {
        if (w <= 0 || h <= 0) {
            ui->lblVramSavings->setText(tr("GPU VRAM: --"));
            return;
        }

        quint64 rgbaBytes = static_cast<quint64>(w) * h * 4;
        double rgbaMb = static_cast<double>(rgbaBytes) / (1024.0 * 1024.0);

        if (opts.textureFormat == TEXTURE_FORMAT_PNG) {
            ui->lblVramSavings->setText(tr("GPU VRAM: %1 MB (Standard RGBA8888, uncompressed on GPU)")
                .arg(QString::number(rgbaMb, 'f', 2)));
        } else {
            quint64 vramBytes = VramTextureCompressor::estimateVramBytes(w, h, opts.vramOptions.format);
            double vramMb = static_cast<double>(vramBytes) / (1024.0 * 1024.0);
            double vramSavingsPct = (1.0 - static_cast<double>(vramBytes) / static_cast<double>(rgbaBytes)) * 100.0;
            QString fmtName = (opts.vramOptions.format == VramFormat::KTX2_UASTC) ? QStringLiteral("UASTC 4x4") : QStringLiteral("ETC1S");

            // Disk storage estimate based on format and Zstd supercompression level
            double estDiskMb = vramMb;
            if (opts.vramOptions.zstdSupercompression) {
                // Zstandard compresses UASTC payloads by ~35-55% and ETC1S by ~60-80% depending on level
                double zstdFactor = (opts.vramOptions.format == VramFormat::KTX2_UASTC)
                    ? (0.65 - (opts.vramOptions.zstdLevel - 1) * 0.01)
                    : (0.40 - (opts.vramOptions.zstdLevel - 1) * 0.008);
                estDiskMb = vramMb * qBound(0.12, zstdFactor, 0.85);
            }

            QString diskStr;
            if (estDiskMb < 1.0) {
                diskStr = tr("~%1 KB on disk").arg(QString::number(estDiskMb * 1024.0, 'f', 0));
            } else {
                diskStr = tr("~%1 MB on disk").arg(QString::number(estDiskMb, 'f', 2));
            }

            QString zstdDesc = opts.vramOptions.zstdSupercompression
                ? tr("+Zstd L%1").arg(opts.vramOptions.zstdLevel)
                : tr("Raw");

            ui->lblVramSavings->setText(tr("GPU VRAM: %1 MB (%2, -%3% hardware) | File: %4 (%5)")
                .arg(QString::number(vramMb, 'f', 2))
                .arg(fmtName)
                .arg(QString::number(vramSavingsPct, 'f', 0))
                .arg(zstdDesc)
                .arg(diskStr));
        }
    };

    if (opts.packOptions.algorithm == AtlasPacker::KeepLayout) {
        if (!m_document->atlas().isNull()) {
            int w = m_document->atlas().width();
            int h = m_document->atlas().height();
            ui->lblDimensions->setText(tr("Dimensions: %1 x %2 px (Current Atlas)")
                .arg(w)
                .arg(h));
            ui->lblEfficiency->setText(tr("Packing Efficiency: Preserved as-is (WYSIWYG)"));
            ui->lblFrames->setText(tr("Frames: %1 total").arg(m_document->frameCount()));
            updateVramLabel(w, h);
        } else {
            ui->lblDimensions->setText(tr("Dimensions: No current atlas"));
            ui->lblEfficiency->setText(tr("Packing Efficiency: --"));
            ui->lblFrames->setText(tr("Frames: %1 total").arg(m_document->frameCount()));
            ui->lblVramSavings->setText(tr("GPU VRAM: --"));
        }
        return;
    }

    QList<QPolygonF> docPolygons;
    docPolygons.reserve(m_document->frameCount());
    for (int i = 0; i < m_document->frameCount(); ++i) {
        docPolygons.append(m_document->box(i).hasPolygonMesh ? m_document->box(i).polygon : QPolygonF());
    }

    AtlasPackResult res = AtlasPacker::pack(m_document->frames(), opts.packOptions, docPolygons);

    if (res.success) {
        int w = res.dimensions.width();
        int h = res.dimensions.height();
        ui->lblDimensions->setText(tr("Dimensions: %1 x %2 px")
            .arg(w)
            .arg(h));

        ui->lblEfficiency->setText(tr("Packing Efficiency: %1%")
            .arg(QString::number(res.efficiency, 'f', 1)));

        int duplicates = m_document->frameCount() - res.uniqueFramesCount;
        if (duplicates > 0) {
            ui->lblFrames->setText(tr("Frames: %1 total (%2 unique, %3 duplicates saved)")
                .arg(m_document->frameCount())
                .arg(res.uniqueFramesCount)
                .arg(duplicates));
        } else {
            ui->lblFrames->setText(tr("Frames: %1 total (all unique)")
                .arg(m_document->frameCount()));
        }
        updateVramLabel(w, h);
    } else {
        ui->lblDimensions->setText(tr("Dimensions: Does not fit in maximum bounds!"));
        ui->lblEfficiency->setText(tr("Packing Efficiency: 0%"));
        ui->lblFrames->setText(tr("Frames: %1").arg(m_document->frameCount()));
        ui->lblVramSavings->setText(tr("GPU VRAM: --"));
    }
}

void ExportDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        if (QPushButton *okBtn = ui->buttonBox->button(QDialogButtonBox::Ok)) {
            okBtn->setText(tr("Export"));
        }
        if (QPushButton *cancelBtn = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
            cancelBtn->setText(tr("Cancel"));
        }
        updateContextualVisibility();
        updateStats();
    }
    QDialog::changeEvent(event);
}

void ExportDialog::validateFilePath()
{
    QString path = exportFilePath();
    bool valid = false;
    if (!path.isEmpty()) {
        QFileInfo fi(path);
        if (!fi.completeBaseName().trimmed().isEmpty() && !fi.isDir()) {
            valid = true;
        }
    }
    if (QPushButton *okBtn = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        okBtn->setEnabled(valid);
    }
}

void ExportDialog::setControlsEnabled(bool enabled)
{
    ui->grpDestination->setEnabled(enabled);
    ui->grpFormat->setEnabled(enabled);
    ui->grpGeometry->setEnabled(enabled);
    ui->grpVram->setEnabled(enabled);
    ui->grpGifOptions->setEnabled(enabled);
    ui->grpAsepriteOptions->setEnabled(enabled);
    if (QPushButton *okBtn = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        okBtn->setEnabled(enabled);
    }
    if (QPushButton *cancelBtn = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelBtn->setEnabled(enabled);
    }
}

void ExportDialog::closeEvent(QCloseEvent *event)
{
    if (m_isExporting) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void ExportDialog::reject()
{
    if (m_isExporting) {
        return;
    }
    QDialog::reject();
}

void ExportDialog::accept()
{
    if (m_isExporting) return;

    QString path = exportFilePath();
    QFileInfo fi(path);
    if (path.isEmpty() || fi.completeBaseName().trimmed().isEmpty() || fi.isDir()) {
        QMessageBox::warning(this, tr("Export"), tr("Please specify a valid file name before exporting."));
        return;
    }

    setControlsEnabled(false);
    ui->progressBarExport->setVisible(true);
    ui->progressBarExport->setRange(0, 0); // Animated indeterminate progress
    ui->lblExportStatus->setVisible(true);

    QString extId = ui->comboFormat->currentData().toString();
    if (extId == QStringLiteral("gif_extractor")) {
        ui->lblExportStatus->setText(tr("Rendering and encoding animated GIF frames..."));
    } else if (extId == QStringLiteral("aseprite_extractor")) {
        ui->lblExportStatus->setText(tr("Encoding native Aseprite binary project..."));
    } else {
        ui->lblExportStatus->setText(tr("Exporting and compressing textures (GPU VRAM / KTX2)..."));
    }
    m_isExporting = true;

    ExportOptions options = exportOptions();
    ProjectController *controller = m_controller;
    const SpriteDocument *doc = m_document;

    auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, [this, watcher, path]() {
        QPair<bool, QString> res = watcher->result();
        watcher->deleteLater();
        m_isExporting = false;

        if (res.first) {
            ui->progressBarExport->setRange(0, 100);
            ui->progressBarExport->setValue(100);
            ui->lblExportStatus->setText(tr("Export completed successfully!"));
            if (m_controller) {
                m_controller->addRecentFile(path);
                emit m_controller->statusMessage(tr("Saved %1 successfully.").arg(QFileInfo(path).fileName()));
                emit m_controller->fileSaved(path);
            }
            QDialog::accept();
        } else {
            ui->progressBarExport->setVisible(false);
            ui->lblExportStatus->setVisible(false);
            setControlsEnabled(true);
            validateFilePath();
            QMessageBox::critical(this, tr("Export Error"), res.second.isEmpty() ? tr("An error occurred during export.") : res.second);
        }
    });

    QFuture<QPair<bool, QString>> future = QtConcurrent::run([doc, path, options, controller]() -> QPair<bool, QString> {
        QString errStr;
        bool ok = false;
        if (controller) {
            ok = controller->exportData(path, options, &errStr);
        } else {
            Extractor *extractor = ExtractorRegistry::instance().findEncoder(path);
            if (!extractor) {
                return qMakePair(false, QObject::tr("No suitable exporter found for format: %1").arg(path));
            }
            ExtractorError err;
            ok = extractor->write(path, *doc, options, &err);
            if (!ok) {
                errStr = err.toString();
            }
        }
        return qMakePair(ok, errStr);
    });

    watcher->setFuture(future);
}
