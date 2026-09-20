#include "widgets/exportdialog.h"
#include "ui_exportdialog.h"
#include "extractor/extractorregistry.h"
#include "packer/vramtexturecompressor.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>
#include <QPushButton>
#include <QMessageBox>

ExportDialog::ExportDialog(const SpriteDocument *document, const QString &defaultPath, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::ExportDialog>())
    , m_document(document)
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

    // Initialize default path
    QString defaultExt = QStringLiteral(".tres");
    if (ui->comboFormat->count() > 0) {
        QString firstId = ui->comboFormat->itemData(0).toString();
        Extractor *firstExt = ExtractorRegistry::instance().findExtractorById(firstId);
        if (firstExt && !firstExt->supportedExtensions().isEmpty()) {
            defaultExt = QStringLiteral(".") + firstExt->supportedExtensions().first();
        }
    }

    if (!defaultPath.isEmpty()) {
        ui->txtFilePath->setText(defaultPath);
    } else if (m_document && !m_document->filePath().isEmpty()) {
        QFileInfo fi(m_document->filePath());
        QString baseName = fi.completeBaseName();
        if (baseName.isEmpty()) baseName = QStringLiteral("atlas");
        QString defExport = fi.dir().filePath(baseName + defaultExt);
        ui->txtFilePath->setText(defExport);
    } else {
        QString defExport = QDir::current().filePath(QStringLiteral("atlas") + defaultExt);
        ui->txtFilePath->setText(defExport);
    }

    connect(ui->txtFilePath, &QLineEdit::textChanged, this, &ExportDialog::validateFilePath);

    // Configure debounce timer for live stats calculation
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(60);
    connect(m_debounceTimer, &QTimer::timeout, this, &ExportDialog::updateStats);

    // Connect signals to debounce timer
    connect(ui->btnBrowse, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);
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

    // PackOptions
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

    return opts;
}

void ExportDialog::onBrowseClicked()
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

    QString initialPath = ui->txtFilePath->text();
    if (initialPath.isEmpty() && m_document) {
        initialPath = m_document->filePath();
    }

    QString chosen = QFileDialog::getSaveFileName(this, tr("Select Export Destination"), initialPath, filter);
    if (!chosen.isEmpty()) {
        QFileInfo fi(chosen);
        if (fi.completeBaseName().trimmed().isEmpty()) {
            QString defaultExt = (ext && !ext->supportedExtensions().isEmpty()) ? ext->supportedExtensions().first() : QStringLiteral("tres");
            chosen = fi.dir().filePath(QStringLiteral("atlas.") + defaultExt);
        }
        ui->txtFilePath->setText(chosen);
    }
}

void ExportDialog::onFormatChanged(int index)
{
    QString current = ui->txtFilePath->text().trimmed();
    if (!current.isEmpty()) {
        QFileInfo fi(current);
        QString base = fi.completeBaseName();
        if (base.isEmpty()) base = QStringLiteral("atlas");
        if (base.endsWith(QStringLiteral(".unity"), Qt::CaseInsensitive)) base.chop(6);
        if (base.endsWith(QStringLiteral(".paper2d"), Qt::CaseInsensitive)) base.chop(8);

        QString extId = ui->comboFormat->itemData(index).toString();
        Extractor *ext = ExtractorRegistry::instance().findExtractorById(extId);
        QString extension = (ext && !ext->supportedExtensions().isEmpty()) ? ext->supportedExtensions().first() : QStringLiteral("tres");
        if (!extension.startsWith('.')) extension.prepend('.');

        ui->txtFilePath->setText(fi.dir().filePath(base + extension));
    }
    m_debounceTimer->start();
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
            double savingsPct = (1.0 - static_cast<double>(vramBytes) / static_cast<double>(rgbaBytes)) * 100.0;
            QString fmtName = (opts.vramOptions.format == VramFormat::KTX2_UASTC) ? QStringLiteral("UASTC 4x4") : QStringLiteral("ETC1S");

            ui->lblVramSavings->setText(tr("GPU VRAM: %1 MB (%2) — Savings: -%3% vs RGBA")
                .arg(QString::number(vramMb, 'f', 2))
                .arg(fmtName)
                .arg(QString::number(savingsPct, 'f', 1)));
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

void ExportDialog::accept()
{
    QString path = exportFilePath();
    QFileInfo fi(path);
    if (path.isEmpty() || fi.completeBaseName().trimmed().isEmpty() || fi.isDir()) {
        QMessageBox::warning(this, tr("Export"), tr("Please specify a valid file name before exporting."));
        return;
    }
    QDialog::accept();
}

