#include "widgets/exportdialog.h"
#include "ui_exportdialog.h"
#include "extractor/extractorregistry.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>
#include <QPushButton>

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

    // Initialize default path
    if (!defaultPath.isEmpty()) {
        ui->txtFilePath->setText(defaultPath);
    } else if (m_document && !m_document->filePath().isEmpty()) {
        QFileInfo fi(m_document->filePath());
        QString baseName = fi.completeBaseName();
        QString defExport = fi.dir().filePath(baseName + ".tres");
        ui->txtFilePath->setText(defExport);
    }

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
    int fmtIdx = ui->comboFormat->currentIndex();
    if (fmtIdx == 0) {
        opts.format = FORMAT_GODOT;
    } else if (fmtIdx == 1) {
        opts.format = FORMAT_TEXTUREPACKER_JSON;
    } else {
        opts.format = FORMAT_ASEPRITE_JSON;
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
        pOpts.algorithm = AtlasPacker::PowerOfTwoPacker;
        break;
    case 6:
        pOpts.algorithm = AtlasPacker::RowPacker;
        break;
    case 7:
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

    return opts;
}

void ExportDialog::onBrowseClicked()
{
    QString filter;
    int fmtIdx = ui->comboFormat->currentIndex();
    if (fmtIdx == 0) {
        filter = tr("Godot 4 Resource (*.tres);;All Files (*.*)");
    } else {
        filter = tr("JSON SpriteSheet (*.json);;All Files (*.*)");
    }

    QString initialPath = ui->txtFilePath->text();
    if (initialPath.isEmpty() && m_document) {
        initialPath = m_document->filePath();
    }

    QString chosen = QFileDialog::getSaveFileName(this, tr("Select Export Destination"), initialPath, filter);
    if (!chosen.isEmpty()) {
        ui->txtFilePath->setText(chosen);
    }
}

void ExportDialog::onFormatChanged(int index)
{
    QString current = ui->txtFilePath->text();
    if (!current.isEmpty()) {
        QFileInfo fi(current);
        QString ext = (index == 0) ? ".tres" : ".json";
        ui->txtFilePath->setText(fi.dir().filePath(fi.completeBaseName() + ext));
    }
    m_debounceTimer->start();
}

void ExportDialog::updateStats()
{
    if (!m_document || m_document->frameCount() == 0) {
        ui->lblDimensions->setText(tr("Dimensions: --"));
        ui->lblEfficiency->setText(tr("Packing Efficiency: --"));
        ui->lblFrames->setText(tr("Frames: 0"));
        return;
    }

    ExportOptions opts = exportOptions();

    if (opts.packOptions.algorithm == AtlasPacker::KeepLayout) {
        if (!m_document->atlas().isNull()) {
            ui->lblDimensions->setText(tr("Dimensions: %1 x %2 px (Current Atlas)")
                .arg(m_document->atlas().width())
                .arg(m_document->atlas().height()));
            ui->lblEfficiency->setText(tr("Packing Efficiency: Preserved as-is (WYSIWYG)"));
            ui->lblFrames->setText(tr("Frames: %1 total").arg(m_document->frameCount()));
        } else {
            ui->lblDimensions->setText(tr("Dimensions: No current atlas"));
            ui->lblEfficiency->setText(tr("Packing Efficiency: --"));
            ui->lblFrames->setText(tr("Frames: %1 total").arg(m_document->frameCount()));
        }
        return;
    }

    AtlasPackResult res = AtlasPacker::pack(m_document->frames(), opts.packOptions);

    if (res.success) {
        ui->lblDimensions->setText(tr("Dimensions: %1 x %2 px")
            .arg(res.dimensions.width())
            .arg(res.dimensions.height()));

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
    } else {
        ui->lblDimensions->setText(tr("Dimensions: Does not fit in maximum bounds!"));
        ui->lblEfficiency->setText(tr("Packing Efficiency: 0%"));
        ui->lblFrames->setText(tr("Frames: %1").arg(m_document->frameCount()));
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

