#include "atlaspackingdialog.h"
#include "commands/filtercommands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QSettings>
#include <QtConcurrent>
#include <QProgressBar>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QCoreApplication>

AtlasPackingDialog::AtlasPackingDialog(SpriteDocument *doc, QUndoStack *undoStack, QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Atlas Bin-Packing (MaxRects)"));
    setupUI();

    connect(&m_futureWatcher, &QFutureWatcher<AsyncPackJobResult>::finished,
            this, &AtlasPackingDialog::onPackingFinished);

    // Do not show the generic Auto-detect Sprite Boxes checkbox since packing computes exact atlas placements
    if (m_autoDetectBoxesCheck) {
        m_autoDetectBoxesCheck->setVisible(false);
    }

    // Check if document has any polygon mesh configured
    bool hasAnyMesh = false;
    if (m_document) {
        for (const SpriteBox &b : m_document->boxes()) {
            if (b.hasPolygonMesh && !b.polygon.isEmpty()) {
                hasAnyMesh = true;
                break;
            }
        }
    }

    // Load persistent settings or defaults
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    int defaultAlgo = hasAnyMesh ? 5 : 0; // Index 5 is TightPolygon
    int algo = settings.value(QStringLiteral("atlasPacking/algorithm"), defaultAlgo).toInt();
    if (hasAnyMesh && algo < 5) {
        algo = 5; // Prefer TightPolygon if polygons exist and algo was standard rectangle
    }
    int pad = settings.value(QStringLiteral("atlasPacking/padding"), 2).toInt();
    int border = settings.value(QStringLiteral("atlasPacking/borderPadding"), 0).toInt();
    int extrude = settings.value(QStringLiteral("atlasPacking/extrude"), 0).toInt();
    bool pot = settings.value(QStringLiteral("atlasPacking/powerOfTwo"), false).toBool();
    bool square = settings.value(QStringLiteral("atlasPacking/forceSquare"), false).toBool();
    bool dedup = settings.value(QStringLiteral("atlasPacking/deduplicate"), false).toBool();
    bool trim = settings.value(QStringLiteral("atlasPacking/trim"), false).toBool();
    const int maxCores = std::max(1, QThread::idealThreadCount());
    int threads = settings.value(QStringLiteral("atlasPacking/threads"), maxCores).toInt();

    m_comboAlgorithm->setCurrentIndex(qBound(0, algo, m_comboAlgorithm->count() - 1));
    m_spinPadding->setValue(qBound(0, pad, 64));
    m_spinBorderPadding->setValue(qBound(0, border, 64));
    m_spinExtrude->setValue(qBound(0, extrude, 2));
    m_checkPowerOfTwo->setChecked(pot);
    m_checkForceSquare->setChecked(square);
    m_checkDeduplicate->setChecked(dedup);
    m_checkTrim->setChecked(trim);
    if (m_spinThreads) {
        m_spinThreads->setValue(qBound(1, threads, maxCores));
    }

    // Connect signals now that initial settings are loaded without firing premature previews
    connect(m_comboAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AtlasPackingDialog::onParametersChanged);
    connect(m_spinPadding, QOverload<int>::of(&QSpinBox::valueChanged), this, &AtlasPackingDialog::onParametersChanged);
    connect(m_spinBorderPadding, QOverload<int>::of(&QSpinBox::valueChanged), this, &AtlasPackingDialog::onParametersChanged);
    connect(m_spinExtrude, QOverload<int>::of(&QSpinBox::valueChanged), this, &AtlasPackingDialog::onParametersChanged);
    connect(m_checkPowerOfTwo, &QCheckBox::toggled, this, &AtlasPackingDialog::onParametersChanged);
    connect(m_checkForceSquare, &QCheckBox::toggled, this, &AtlasPackingDialog::onParametersChanged);
    connect(m_checkDeduplicate, &QCheckBox::toggled, this, &AtlasPackingDialog::onParametersChanged);
    connect(m_checkTrim, &QCheckBox::toggled, this, &AtlasPackingDialog::onParametersChanged);
    if (m_spinThreads) {
        connect(m_spinThreads, QOverload<int>::of(&QSpinBox::valueChanged), this, &AtlasPackingDialog::onParametersChanged);
    }

    // Do NOT launch packing automatically upon opening the dialog
    if (m_livePreviewCheck) {
        m_livePreviewCheck->setChecked(false);
    }
    setStatusText(tr("Ready — Adjust parameters and click 'Compute Packing'"));
}

AtlasPackingDialog::~AtlasPackingDialog()
{
    m_currentJobId++;
    if (m_futureWatcher.isRunning()) {
        m_futureWatcher.cancel();
        m_futureWatcher.waitForFinished();
    }
}

void AtlasPackingDialog::setupUI()
{
    QVBoxLayout *layout = contentLayout();

    // 1. Group: Packing Algorithm
    QGroupBox *grpAlgo = new QGroupBox(tr("Packing Algorithm"), this);
    QVBoxLayout *algoLayout = new QVBoxLayout(grpAlgo);

    m_comboAlgorithm = new QComboBox(grpAlgo);
    m_comboAlgorithm->addItem(tr("MaxRects — Best Short Side Fit (Default, Recommended)"));
    m_comboAlgorithm->addItem(tr("MaxRects — Best Area Fit (Maximum Compaction)"));
    m_comboAlgorithm->addItem(tr("MaxRects — Best Long Side Fit"));
    m_comboAlgorithm->addItem(tr("MaxRects — Bottom Left Rule"));
    m_comboAlgorithm->addItem(tr("MaxRects — Contact Point Rule"));
    m_comboAlgorithm->addItem(tr("Tight Polygon Packing (Nesting — Overlapping Rects)"));
    m_comboAlgorithm->addItem(tr("Power of Two Shelf Packer (2^n Dimensions)"));
    m_comboAlgorithm->addItem(tr("Basic Row / Shelf Packer"));
    m_comboAlgorithm->addItem(tr("Uniform Grid Packer"));
    algoLayout->addWidget(m_comboAlgorithm);
    layout->addWidget(grpAlgo);

    // 2. Group: Spacing & Bleeding Protection
    QGroupBox *grpSpacing = new QGroupBox(tr("Spacing & Texture Bleeding Protection"), this);
    QFormLayout *formSpacing = new QFormLayout(grpSpacing);

    m_spinPadding = new QSpinBox(grpSpacing);
    m_spinPadding->setRange(0, 64);
    m_spinPadding->setValue(2);
    m_spinPadding->setSuffix(QStringLiteral(" px"));
    m_spinPadding->setToolTip(tr("Inner margin between adjacent sprites"));
    formSpacing->addRow(tr("Inner Padding:"), m_spinPadding);

    m_spinBorderPadding = new QSpinBox(grpSpacing);
    m_spinBorderPadding->setRange(0, 64);
    m_spinBorderPadding->setValue(0);
    m_spinBorderPadding->setSuffix(QStringLiteral(" px"));
    m_spinBorderPadding->setToolTip(tr("Outer margin around the edges of the atlas"));
    formSpacing->addRow(tr("Border Padding:"), m_spinBorderPadding);

    m_spinExtrude = new QSpinBox(grpSpacing);
    m_spinExtrude->setRange(0, 2);
    m_spinExtrude->setValue(0);
    m_spinExtrude->setSuffix(QStringLiteral(" px"));
    m_spinExtrude->setToolTip(tr("Repeats border pixels outward (1-2px) to prevent bilinear interpolation artifacts in game engines"));
    formSpacing->addRow(tr("Extrude (Anti-Bleeding):"), m_spinExtrude);

    layout->addWidget(grpSpacing);

    // 3. Group: Constraints & Optimizations
    QGroupBox *grpConstraints = new QGroupBox(tr("GPU Constraints & Optimizations"), this);
    QVBoxLayout *constLayout = new QVBoxLayout(grpConstraints);

    m_checkPowerOfTwo = new QCheckBox(tr("Force Power of Two Dimensions (2^n: 512, 1024, 2048...)"), grpConstraints);
    m_checkForceSquare = new QCheckBox(tr("Force Square Atlas (Width == Height)"), grpConstraints);
    m_checkDeduplicate = new QCheckBox(tr("Auto-Aliasing (Merge identical frames without breaking animations)"), grpConstraints);
    m_checkTrim = new QCheckBox(tr("Trim Transparent Borders before packing"), grpConstraints);
    m_checkTrim->setToolTip(tr("Crops transparent margins around frames while preserving animation pivots. Highly recommended for animation spritesheets to eliminate empty spaces and maximize packing density."));

    constLayout->addWidget(m_checkPowerOfTwo);
    constLayout->addWidget(m_checkForceSquare);
    constLayout->addWidget(m_checkDeduplicate);
    constLayout->addWidget(m_checkTrim);
    layout->addWidget(grpConstraints);

    // 4. Group: Performance & Multithreading
    QGroupBox *grpPerf = new QGroupBox(tr("Performance & Multithreading"), this);
    QFormLayout *perfLayout = new QFormLayout(grpPerf);

    const int maxCores = std::max(1, QThread::idealThreadCount());
    m_spinThreads = new QSpinBox(grpPerf);
    m_spinThreads->setRange(1, maxCores);
    m_spinThreads->setValue(maxCores);
    m_spinThreads->setSuffix(QStringLiteral(" / %1").arg(maxCores));
    m_spinThreads->setToolTip(tr("Number of CPU threads to use for parallel processing (1 to %1 cores)").arg(maxCores));
    perfLayout->addRow(tr("Worker Threads:"), m_spinThreads);
    layout->addWidget(grpPerf);

    // 5. Action Button to manually run packing
    m_btnPackNow = new QPushButton(tr("Compute Packing"), this);
    m_btnPackNow->setStyleSheet(QStringLiteral(
        "QPushButton { font-weight: bold; padding: 7px 16px; background-color: #2980b9; color: white; border-radius: 4px; font-size: 13px; }"
        "QPushButton:hover { background-color: #3498db; }"
        "QPushButton:pressed { background-color: #1f618d; }"
        "QPushButton:disabled { background-color: #566573; color: #bdc3c7; }"
    ));
    connect(m_btnPackNow, &QPushButton::clicked, this, [this]() {
        applyPreview();
    });
    layout->addWidget(m_btnPackNow);

    // 6. Group: Live Packing Statistics
    QGroupBox *grpStats = new QGroupBox(tr("Live Packing Metrics"), this);
    QFormLayout *statsForm = new QFormLayout(grpStats);

    m_lblDimensions = new QLabel(QStringLiteral("-- x -- px"), grpStats);
    m_lblDimensions->setStyleSheet(QStringLiteral("font-weight: bold; color: #2980b9;"));
    statsForm->addRow(tr("Atlas Dimensions:"), m_lblDimensions);

    m_lblEfficiency = new QLabel(QStringLiteral("-- %"), grpStats);
    m_lblEfficiency->setStyleSheet(QStringLiteral("font-weight: bold; color: #27ae60;"));
    statsForm->addRow(tr("Packing Efficiency:"), m_lblEfficiency);

    m_lblSavedFrames = new QLabel(QStringLiteral("--"), grpStats);
    statsForm->addRow(tr("Frames Count:"), m_lblSavedFrames);

    layout->addWidget(grpStats);

    // 7. Progress bar for background packing computation
    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(QStringLiteral(
        "QProgressBar { border: none; background: #2c3e50; border-radius: 2px; } "
        "QProgressBar::chunk { background-color: #00bcd4; border-radius: 2px; }"
    ));
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);
}

void AtlasPackingDialog::setProcessingState(bool processing)
{
    m_isProcessing = processing;
    if (m_buttonBox && m_buttonBox->button(QDialogButtonBox::Ok)) {
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!processing);
    }
    if (m_resetDefaultsBtn) {
        m_resetDefaultsBtn->setEnabled(!processing);
    }
    if (m_btnPackNow) {
        m_btnPackNow->setEnabled(!processing);
        m_btnPackNow->setText(processing ? tr("Computing...") : tr("Compute Packing"));
    }
    if (m_progressBar) {
        m_progressBar->setVisible(processing);
        if (processing) {
            m_progressBar->setRange(0, 0); // Animated indeterminate busy mode
        } else {
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(100);
        }
    }
    if (processing) {
        setStatusText(tr("Computing packing..."));
    }
}

void AtlasPackingDialog::onParametersChanged()
{
    schedulePreview();
}

AtlasPacker::PackOptions AtlasPackingDialog::packOptions() const
{
    AtlasPacker::PackOptions opts;

    int idx = m_comboAlgorithm->currentIndex();
    switch (idx) {
    case 0:
        opts.algorithm = AtlasPacker::MaxRects;
        opts.heuristic = MaxRectsHeuristic::BestShortSideFit;
        break;
    case 1:
        opts.algorithm = AtlasPacker::MaxRects;
        opts.heuristic = MaxRectsHeuristic::BestAreaFit;
        break;
    case 2:
        opts.algorithm = AtlasPacker::MaxRects;
        opts.heuristic = MaxRectsHeuristic::BestLongSideFit;
        break;
    case 3:
        opts.algorithm = AtlasPacker::MaxRects;
        opts.heuristic = MaxRectsHeuristic::BottomLeft;
        break;
    case 4:
        opts.algorithm = AtlasPacker::MaxRects;
        opts.heuristic = MaxRectsHeuristic::ContactPoint;
        break;
    case 5:
        opts.algorithm = AtlasPacker::TightPolygon;
        break;
    case 6:
        opts.algorithm = AtlasPacker::PowerOfTwoPacker;
        break;
    case 7:
        opts.algorithm = AtlasPacker::RowPacker;
        break;
    case 8:
        opts.algorithm = AtlasPacker::GridPacker;
        break;
    default:
        opts.algorithm = AtlasPacker::MaxRects;
        opts.heuristic = MaxRectsHeuristic::BestShortSideFit;
        break;
    }

    opts.padding = m_spinPadding->value();
    opts.borderPadding = m_spinBorderPadding->value();
    opts.extrude = m_spinExtrude->value();
    opts.powerOfTwo = m_checkPowerOfTwo->isChecked();
    opts.forceSquare = m_checkForceSquare->isChecked();
    opts.deduplicate = m_checkDeduplicate->isChecked();
    if (m_spinThreads) {
        opts.threadCount = m_spinThreads->value();
    }

    return opts;
}

bool AtlasPackingDialog::isTrimEnabled() const
{
    return m_checkTrim && m_checkTrim->isChecked();
}

void AtlasPackingDialog::setDeduplicate(bool dedup)
{
    if (m_checkDeduplicate) {
        m_checkDeduplicate->setChecked(dedup);
    }
}

bool AtlasPackingDialog::isDeduplicateEnabled() const
{
    return m_checkDeduplicate && m_checkDeduplicate->isChecked();
}

void AtlasPackingDialog::setAlgorithm(int algo)
{
    if (m_comboAlgorithm && algo >= 0 && algo < m_comboAlgorithm->count()) {
        m_comboAlgorithm->setCurrentIndex(algo);
    }
}

void AtlasPackingDialog::updateStatsUI(const AtlasPackResult &res)
{
    if (!res.success) {
        m_lblDimensions->setText(tr("Packing Failed (Exceeded max dimensions)"));
        m_lblEfficiency->setText(QStringLiteral("0%"));
        m_lblSavedFrames->setText(QStringLiteral("--"));
        setStatusText(tr("Error: Cannot fit sprites in atlas"));
        return;
    }

    m_lblDimensions->setText(tr("%1 x %2 px").arg(res.dimensions.width()).arg(res.dimensions.height()));
    m_lblEfficiency->setText(QStringLiteral("%1%").arg(QString::number(res.efficiency, 'f', 1)));

    int total = m_initialFrames.size();
    int unique = res.uniqueFramesCount;
    int saved = total - unique;

    if (m_checkDeduplicate->isChecked() && saved > 0) {
        m_lblSavedFrames->setText(tr("%1 unique / %2 total (%3 frame(s) saved)").arg(unique).arg(total).arg(saved));
    } else {
        m_lblSavedFrames->setText(tr("%1 frame(s)").arg(total));
    }

    setStatusText(tr("Packed in %1x%2 (%3%)").arg(res.dimensions.width()).arg(res.dimensions.height()).arg(QString::number(res.efficiency, 'f', 1)));
}

void AtlasPackingDialog::applyPreview()
{
    if (!m_document || m_initialFrames.isEmpty()) return;

    m_currentJobId++;
    uint64_t jobId = m_currentJobId;

    QList<QImage> workingFrames = m_initialFrames;
    QList<SpriteBox> workingBoxes = m_initialBoxes;
    bool trim = isTrimEnabled();
    AtlasPacker::PackOptions opts = packOptions();

    setProcessingState(true);

    m_futureWatcher.setFuture(QtConcurrent::run([workingFrames, workingBoxes, trim, opts, jobId]() mutable {
        AsyncPackJobResult res;
        res.jobId = jobId;
        res.opts = opts;

        // Optional Trim step (computed in background thread)
        if (trim) {
            for (int i = 0; i < workingFrames.size(); ++i) {
                const QImage &img = workingFrames.at(i);
                int minX = img.width(), maxX = -1, minY = img.height(), maxY = -1;
                for (int y = 0; y < img.height(); ++y) {
                    const QRgb *line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
                    for (int x = 0; x < img.width(); ++x) {
                        if (qAlpha(line[x]) > 1) {
                            if (x < minX) minX = x;
                            if (x > maxX) maxX = x;
                            if (y < minY) minY = y;
                            if (y > maxY) maxY = y;
                        }
                    }
                }
                if (minX <= maxX && minY <= maxY) {
                    QRect cropRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
                    workingFrames[i] = img.copy(cropRect);
                    // Adjust pivot relative to new cropped frame
                    if (i < workingBoxes.size()) {
                        QPoint oldP = workingBoxes[i].effectivePivot();
                        workingBoxes[i].pivot = QPoint(oldP.x() - minX, oldP.y() - minY);
                        workingBoxes[i].hasCustomPivot = true;

                        if (workingBoxes[i].hasPolygonMesh) {
                            workingBoxes[i].polygon.translate(-minX, -minY);
                            for (QPointF &v : workingBoxes[i].vertices) {
                                v -= QPointF(minX, minY);
                            }
                        }
                    }
                }
            }
        }

        QList<QPolygonF> workingPolygons;
        workingPolygons.reserve(workingBoxes.size());
        for (const SpriteBox &b : workingBoxes) {
            workingPolygons.append(b.hasPolygonMesh ? b.polygon : QPolygonF());
        }

        res.packResult = AtlasPacker::pack(workingFrames, opts, workingPolygons);
        res.workingFrames = workingFrames;
        res.workingBoxes = workingBoxes;
        return res;
    }));
}

void AtlasPackingDialog::onPackingFinished()
{
    AsyncPackJobResult jobResult = m_futureWatcher.result();
    if (jobResult.jobId != m_currentJobId) {
        // Outdated result: a newer job was already launched
        return;
    }

    setProcessingState(false);

    const AtlasPackResult &res = jobResult.packResult;
    updateStatsUI(res);
    if (!res.success) return;

    const QList<QImage> &workingFrames = jobResult.workingFrames;
    const QList<SpriteBox> &workingBoxes = jobResult.workingBoxes;
    const AtlasPacker::PackOptions &opts = jobResult.opts;

    m_previewAtlas = res.atlas;

    if (opts.deduplicate) {
        int uCount = res.uniqueFramesCount;
        m_previewFrames.clear();
        m_previewBoxes.clear();
        m_previewFrames.resize(uCount);
        m_previewBoxes.resize(uCount);

        QSet<int> populated;
        for (int i = 0; i < workingFrames.size(); ++i) {
            int u = (i < res.duplicateMapping.size()) ? res.duplicateMapping.at(i) : i;
            if (u >= 0 && u < uCount && !populated.contains(u)) {
                populated.insert(u);
                m_previewFrames[u] = workingFrames.at(i);

                SpriteBox b;
                if (i < workingBoxes.size()) {
                    b = workingBoxes.at(i);
                }
                if (i < res.frameRects.size()) {
                    b.rect = res.frameRects.at(i);
                }
                b.index = u;
                m_previewBoxes[u] = b;
            }
        }

        // Remap animations to point to canonical unique frames
        m_previewAnimations = m_initialAnimations;
        for (auto it = m_previewAnimations.begin(); it != m_previewAnimations.end(); ++it) {
            QList<int> remapped;
            remapped.reserve(it.value().frameIndices.size());
            for (int oldIdx : it.value().frameIndices) {
                if (oldIdx >= 0 && oldIdx < res.duplicateMapping.size()) {
                    remapped.append(res.duplicateMapping.at(oldIdx));
                } else {
                    remapped.append(oldIdx);
                }
            }
            it.value().frameIndices = remapped;
        }

        if (m_document) {
            m_document->setAtlas(m_previewAtlas);
            m_document->setFrames(m_previewFrames, m_previewBoxes);
            m_document->setAnimations(m_previewAnimations);
        }
    } else {
        m_previewFrames = workingFrames;
        m_previewBoxes = workingBoxes;
        for (int i = 0; i < m_previewBoxes.size(); ++i) {
            if (i < res.frameRects.size()) {
                m_previewBoxes[i].rect = res.frameRects.at(i);
            }
            m_previewBoxes[i].index = i;
        }
        m_previewAnimations = m_initialAnimations;

        if (m_document) {
            m_document->setAtlas(m_previewAtlas);
            m_document->setFrames(m_previewFrames, m_previewBoxes);
            m_document->setAnimations(m_previewAnimations);
        }
    }
}

void AtlasPackingDialog::waitForPendingPreview()
{
    if (m_futureWatcher.isRunning()) {
        m_futureWatcher.waitForFinished();
        QCoreApplication::processEvents();
    }
}

void AtlasPackingDialog::accept()
{
    if (m_previewAtlas.isNull()) {
        applyPreview();
    }
    waitForPendingPreview();
    FilterDialogBase::accept();
}

void AtlasPackingDialog::reject()
{
    m_currentJobId++;
    if (m_futureWatcher.isRunning()) {
        m_futureWatcher.cancel();
    }
    FilterDialogBase::reject();
}

QUndoCommand* AtlasPackingDialog::createUndoCommand()
{
    if (m_previewAtlas.isNull() || m_previewBoxes.isEmpty()) {
        return nullptr;
    }

    return new ApplyFilterCommand(m_document,
                                  tr("Atlas Bin-Packing (MaxRects)"),
                                  m_initialAtlas,
                                  m_initialFrames,
                                  m_initialBoxes,
                                  m_initialAnimations,
                                  m_previewAtlas,
                                  m_previewFrames,
                                  m_previewBoxes,
                                  m_previewAnimations);
}

void AtlasPackingDialog::resetDefaults()
{
    m_comboAlgorithm->setCurrentIndex(0);
    m_spinPadding->setValue(2);
    m_spinBorderPadding->setValue(0);
    m_spinExtrude->setValue(0);
    m_checkPowerOfTwo->setChecked(false);
    m_checkForceSquare->setChecked(false);
    m_checkDeduplicate->setChecked(false);
    m_checkTrim->setChecked(false);
    if (m_spinThreads) {
        m_spinThreads->setValue(std::max(1, QThread::idealThreadCount()));
    }
}

void AtlasPackingDialog::saveSettings()
{
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    settings.setValue(QStringLiteral("atlasPacking/algorithm"), m_comboAlgorithm->currentIndex());
    settings.setValue(QStringLiteral("atlasPacking/padding"), m_spinPadding->value());
    settings.setValue(QStringLiteral("atlasPacking/borderPadding"), m_spinBorderPadding->value());
    settings.setValue(QStringLiteral("atlasPacking/extrude"), m_spinExtrude->value());
    settings.setValue(QStringLiteral("atlasPacking/powerOfTwo"), m_checkPowerOfTwo->isChecked());
    settings.setValue(QStringLiteral("atlasPacking/forceSquare"), m_checkForceSquare->isChecked());
    settings.setValue(QStringLiteral("atlasPacking/deduplicate"), m_checkDeduplicate->isChecked());
    settings.setValue(QStringLiteral("atlasPacking/trim"), m_checkTrim->isChecked());
    if (m_spinThreads) {
        settings.setValue(QStringLiteral("atlasPacking/threads"), m_spinThreads->value());
    }
}
