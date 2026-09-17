#include "widgets/atlaspackingdialog.h"
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

AtlasPackingDialog::AtlasPackingDialog(SpriteDocument *doc, QUndoStack *undoStack, QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Atlas Bin-Packing (MaxRects)"));
    setupUI();

    // Do not show the generic Auto-detect Sprite Boxes checkbox since packing computes exact atlas placements
    if (m_autoDetectBoxesCheck) {
        m_autoDetectBoxesCheck->setVisible(false);
    }

    // Load persistent settings or defaults
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    int algo = settings.value(QStringLiteral("atlasPacking/algorithm"), 0).toInt();
    int pad = settings.value(QStringLiteral("atlasPacking/padding"), 2).toInt();
    int border = settings.value(QStringLiteral("atlasPacking/borderPadding"), 0).toInt();
    int extrude = settings.value(QStringLiteral("atlasPacking/extrude"), 0).toInt();
    bool pot = settings.value(QStringLiteral("atlasPacking/powerOfTwo"), false).toBool();
    bool square = settings.value(QStringLiteral("atlasPacking/forceSquare"), false).toBool();
    bool dedup = settings.value(QStringLiteral("atlasPacking/deduplicate"), false).toBool();
    bool trim = settings.value(QStringLiteral("atlasPacking/trim"), false).toBool();

    m_comboAlgorithm->setCurrentIndex(qBound(0, algo, m_comboAlgorithm->count() - 1));
    m_spinPadding->setValue(qBound(0, pad, 64));
    m_spinBorderPadding->setValue(qBound(0, border, 64));
    m_spinExtrude->setValue(qBound(0, extrude, 2));
    m_checkPowerOfTwo->setChecked(pot);
    m_checkForceSquare->setChecked(square);
    m_checkDeduplicate->setChecked(dedup);
    m_checkTrim->setChecked(trim);

    // Initial estimation
    schedulePreview();
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

    constLayout->addWidget(m_checkPowerOfTwo);
    constLayout->addWidget(m_checkForceSquare);
    constLayout->addWidget(m_checkDeduplicate);
    constLayout->addWidget(m_checkTrim);
    layout->addWidget(grpConstraints);

    // 4. Group: Live Packing Statistics
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

    // Connections to trigger debounce live preview
    connect(m_comboAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AtlasPackingDialog::onParametersChanged);
    connect(m_spinPadding, QOverload<int>::of(&QSpinBox::valueChanged), this, &AtlasPackingDialog::onParametersChanged);
    connect(m_spinBorderPadding, QOverload<int>::of(&QSpinBox::valueChanged), this, &AtlasPackingDialog::onParametersChanged);
    connect(m_spinExtrude, QOverload<int>::of(&QSpinBox::valueChanged), this, &AtlasPackingDialog::onParametersChanged);
    connect(m_checkPowerOfTwo, &QCheckBox::toggled, this, &AtlasPackingDialog::onParametersChanged);
    connect(m_checkForceSquare, &QCheckBox::toggled, this, &AtlasPackingDialog::onParametersChanged);
    connect(m_checkDeduplicate, &QCheckBox::toggled, this, &AtlasPackingDialog::onParametersChanged);
    connect(m_checkTrim, &QCheckBox::toggled, this, &AtlasPackingDialog::onParametersChanged);
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
        opts.algorithm = AtlasPacker::PowerOfTwoPacker;
        break;
    case 6:
        opts.algorithm = AtlasPacker::RowPacker;
        break;
    case 7:
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

    QList<QImage> workingFrames = m_initialFrames;
    QList<SpriteBox> workingBoxes = m_initialBoxes;

    // Optional Trim step
    if (isTrimEnabled()) {
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
                }
            }
        }
    }

    AtlasPacker::PackOptions opts = packOptions();
    AtlasPackResult res = AtlasPacker::pack(workingFrames, opts);

    updateStatsUI(res);
    if (!res.success) return;

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
                if (i < res.frameRects.size()) {
                    b.rect = res.frameRects.at(i);
                }
                if (i < workingBoxes.size()) {
                    b.pivot = workingBoxes.at(i).pivot;
                    b.hasCustomPivot = workingBoxes.at(i).hasCustomPivot;
                    b.selected = workingBoxes.at(i).selected;
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

        m_document->setAtlas(m_previewAtlas);
        m_document->setFrames(m_previewFrames, m_previewBoxes);
        m_document->setAnimations(m_previewAnimations);
    } else {
        m_previewFrames = workingFrames;
        m_previewBoxes = workingBoxes;
        for (int i = 0; i < m_previewBoxes.size(); ++i) {
            if (i < res.frameRects.size()) {
                m_previewBoxes[i].rect = res.frameRects.at(i);
                m_previewBoxes[i].index = i;
            }
        }
        m_previewAnimations = m_initialAnimations;

        m_document->setAtlas(m_previewAtlas);
        m_document->setFrames(m_previewFrames, m_previewBoxes);
        m_document->setAnimations(m_previewAnimations);
    }
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
}
