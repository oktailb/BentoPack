#include "widgets/outlinefilterdialog.h"
#include "commands/filtercommands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QColorDialog>
#include <cmath>

OutlineFilterDialog::OutlineFilterDialog(SpriteDocument *doc,
                                         QUndoStack *undoStack,
                                         QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Outline & Silhouette Generator"));
    setMinimumWidth(400);

    setupFilterUI();
    setAutoDetectBoxesEnabled(true);
    schedulePreview();
}

void OutlineFilterDialog::setupFilterUI()
{
    // Color selector row
    QHBoxLayout *colorRow = new QHBoxLayout();
    QLabel *colorLabel = new QLabel(tr("Outline color:"), this);
    colorRow->addWidget(colorLabel);

    m_swatchLabel = new QLabel(this);
    m_swatchLabel->setFixedSize(36, 22);
    m_swatchLabel->setFrameShape(QFrame::Box);
    updateColorSwatch(m_strokeColor);
    colorRow->addWidget(m_swatchLabel);

    m_pickColorBtn = new QPushButton(tr("Pick..."), this);
    colorRow->addWidget(m_pickColorBtn);

    // Color presets
    QPushButton *blackBtn = new QPushButton(tr("Black"), this);
    QPushButton *whiteBtn = new QPushButton(tr("White"), this);
    QPushButton *goldBtn  = new QPushButton(tr("Gold"), this);
    colorRow->addWidget(blackBtn);
    colorRow->addWidget(whiteBtn);
    colorRow->addWidget(goldBtn);
    colorRow->addStretch();
    contentLayout()->addLayout(colorRow);

    // Thickness row
    QLabel *thickLabel = new QLabel(tr("Stroke thickness (1 to 4 px):"), this);
    contentLayout()->addWidget(thickLabel);

    QHBoxLayout *thickRow = new QHBoxLayout();
    m_thicknessSlider = new QSlider(Qt::Horizontal, this);
    m_thicknessSlider->setRange(1, 4);
    m_thicknessSlider->setValue(1);
    thickRow->addWidget(m_thicknessSlider, 1);

    m_thicknessSpin = new QSpinBox(this);
    m_thicknessSpin->setRange(1, 4);
    m_thicknessSpin->setValue(1);
    thickRow->addWidget(m_thicknessSpin);
    contentLayout()->addLayout(thickRow);

    // Connectivity row
    QHBoxLayout *connectRow = new QHBoxLayout();
    QLabel *connectLabel = new QLabel(tr("Connectivity:"), this);
    connectRow->addWidget(connectLabel);

    m_connectCombo = new QComboBox(this);
    m_connectCombo->addItem(tr("4-connected (Orthogonal crisp - Retro pixel art)"), FourConnected);
    m_connectCombo->addItem(tr("8-connected (Diagonal included - Smooth outline)"), EightConnected);
    connectRow->addWidget(m_connectCombo, 1);
    contentLayout()->addLayout(connectRow);

    // Options row
    m_silhouetteCheck = new QCheckBox(tr("Solid silhouette / Hit-flash (Fill sprite interior)"), this);
    contentLayout()->addWidget(m_silhouetteCheck);

    // Scope selection
    m_selectedOnlyCheck = new QCheckBox(tr("Apply to selected frames only"), this);
    bool hasSelection = m_document && !m_document->selectedFrameIndices().isEmpty();
    m_selectedOnlyCheck->setChecked(hasSelection);
    m_selectedOnlyCheck->setEnabled(hasSelection);
    if (!hasSelection) {
        m_selectedOnlyCheck->setToolTip(tr("No frames selected: applies to entire atlas"));
    }
    contentLayout()->addWidget(m_selectedOnlyCheck);

    // Connect signals
    connect(m_pickColorBtn, &QPushButton::clicked, this, &OutlineFilterDialog::pickStrokeColor);
    connect(blackBtn, &QPushButton::clicked, this, [this]() { setPresetColor(Qt::black); });
    connect(whiteBtn, &QPushButton::clicked, this, [this]() { setPresetColor(Qt::white); });
    connect(goldBtn,  &QPushButton::clicked, this, [this]() { setPresetColor(QColor(241, 196, 15)); });

    connect(m_thicknessSlider, &QSlider::valueChanged, m_thicknessSpin, &QSpinBox::setValue);
    connect(m_thicknessSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_thicknessSlider, &QSlider::setValue);
    connect(m_thicknessSlider, &QSlider::valueChanged, this, &OutlineFilterDialog::onParametersChanged);
    connect(m_connectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OutlineFilterDialog::onParametersChanged);
    connect(m_silhouetteCheck, &QCheckBox::toggled, this, &OutlineFilterDialog::onParametersChanged);
    connect(m_selectedOnlyCheck, &QCheckBox::toggled, this, &OutlineFilterDialog::onParametersChanged);
}

void OutlineFilterDialog::updateColorSwatch(QRgb color)
{
    if (!m_swatchLabel) return;
    QColor c(color);
    m_swatchLabel->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;")
                                 .arg(c.name()));
}

void OutlineFilterDialog::pickStrokeColor()
{
    QColor initialColor(m_strokeColor);
    QColor chosen = QColorDialog::getColor(initialColor, this, tr("Select Outline Color"));
    if (chosen.isValid()) {
        m_strokeColor = chosen.rgb();
        updateColorSwatch(m_strokeColor);
        onParametersChanged();
    }
}

void OutlineFilterDialog::setPresetColor(const QColor &c)
{
    m_strokeColor = c.rgb();
    updateColorSwatch(m_strokeColor);
    onParametersChanged();
}

int OutlineFilterDialog::thickness() const
{
    return m_thicknessSpin ? m_thicknessSpin->value() : 1;
}

OutlineFilterDialog::Connectivity OutlineFilterDialog::connectivity() const
{
    return m_connectCombo ? static_cast<Connectivity>(m_connectCombo->currentData().toInt()) : FourConnected;
}

bool OutlineFilterDialog::isSilhouetteMode() const
{
    return m_silhouetteCheck && m_silhouetteCheck->isChecked();
}

bool OutlineFilterDialog::isSelectedFramesOnly() const
{
    return m_selectedOnlyCheck && m_selectedOnlyCheck->isChecked();
}

void OutlineFilterDialog::onParametersChanged()
{
    schedulePreview();
}

void OutlineFilterDialog::resetDefaults()
{
    m_strokeColor = qRgb(0, 0, 0);
    updateColorSwatch(m_strokeColor);
    if (m_thicknessSlider) m_thicknessSlider->setValue(1);
    if (m_connectCombo) m_connectCombo->setCurrentIndex(0);
    if (m_silhouetteCheck) m_silhouetteCheck->setChecked(false);
}

void OutlineFilterDialog::saveSettings()
{
}

QImage OutlineFilterDialog::applyOutline(const QImage &source,
                                        int thickness,
                                        QRgb strokeColor,
                                        Connectivity connectivity,
                                        bool silhouette,
                                        const QList<QRect> &targetAreas)
{
    if (source.isNull()) return source;

    QImage dest = source.convertToFormat(QImage::Format_ARGB32);
    int w = dest.width();
    int h = dest.height();

    int strokeR = qRed(strokeColor);
    int strokeG = qGreen(strokeColor);
    int strokeB = qBlue(strokeColor);

    auto isInRange = [&](int dx, int dy) -> bool {
        if (connectivity == FourConnected) {
            return (std::abs(dx) + std::abs(dy)) <= thickness;
        } else {
            return std::max(std::abs(dx), std::abs(dy)) <= thickness;
        }
    };

    auto processRegion = [&](const QRect &boundRect) {
        QRect r = boundRect.intersected(QRect(0, 0, w, h));
        for (int y = r.top(); y <= r.bottom(); ++y) {
            for (int x = r.left(); x <= r.right(); ++x) {
                QRgb p = source.pixel(x, y);
                int a = qAlpha(p);

                if (a > 0) {
                    if (silhouette) {
                        dest.setPixel(x, y, qRgba(strokeR, strokeG, strokeB, a));
                    }
                } else {
                    // Transparent pixel: check if neighbor within radius is opaque
                    bool foundOpaque = false;
                    for (int dy = -thickness; dy <= thickness && !foundOpaque; ++dy) {
                        for (int dx = -thickness; dx <= thickness && !foundOpaque; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            if (!isInRange(dx, dy)) continue;

                            int nx = x + dx;
                            int ny = y + dy;
                            if (nx >= 0 && ny >= 0 && nx < w && ny < h) {
                                if (qAlpha(source.pixel(nx, ny)) > 0) {
                                    foundOpaque = true;
                                }
                            }
                        }
                    }

                    if (foundOpaque) {
                        dest.setPixel(x, y, qRgba(strokeR, strokeG, strokeB, 255));
                    }
                }
            }
        }
    };

    if (targetAreas.isEmpty()) {
        processRegion(QRect(0, 0, w, h));
    } else {
        for (const QRect &rect : targetAreas) {
            // Expand rect by thickness to avoid clipping the generated outline
            QRect expanded = rect.adjusted(-thickness, -thickness, thickness, thickness);
            processRegion(expanded);
        }
    }

    return dest;
}

void OutlineFilterDialog::applyPreview()
{
    if (!m_document || m_initialAtlas.isNull()) return;

    QList<QRect> targetAreas;
    if (isSelectedFramesOnly()) {
        QList<int> sel = m_document->selectedFrameIndices();
        for (int idx : sel) {
            if (idx >= 0 && idx < m_initialBoxes.size()) {
                targetAreas.append(m_initialBoxes[idx].rect);
            }
        }
    }

    m_previewAtlas = applyOutline(m_initialAtlas, thickness(), m_strokeColor,
                                  connectivity(), isSilhouetteMode(), targetAreas);

    updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes);

    m_document->setAtlas(m_previewAtlas);
    m_document->setFrames(m_previewFrames, m_previewBoxes);

    if (isAutoDetectBoxesEnabled()) {
        setStatusText(tr("Outline %1 px applied").arg(thickness()) + QStringLiteral(" — ") + tr("%1 frame(s) detected").arg(m_previewBoxes.size()));
    } else {
        setStatusText(tr("Outline %1 px applied").arg(thickness()));
    }
}

QUndoCommand* OutlineFilterDialog::createUndoCommand()
{
    if (!m_document || m_previewAtlas.isNull()) return nullptr;

    return new ApplyFilterCommand(
        m_document,
        tr("Filter: Outline & Silhouette"),
        m_initialAtlas, m_initialFrames, m_initialBoxes, m_initialAnimations,
        m_previewAtlas, m_previewFrames, m_previewBoxes, m_initialAnimations
    );
}
