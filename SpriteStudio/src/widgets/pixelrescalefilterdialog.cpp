#include "widgets/pixelrescalefilterdialog.h"
#include "commands/filtercommands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLabel>
#include <algorithm>

PixelRescaleFilterDialog::PixelRescaleFilterDialog(SpriteDocument *doc,
                                                   QUndoStack *undoStack,
                                                   QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Pixel Art Rescale"));
    setMinimumWidth(440);

    setupFilterUI();
    schedulePreview();
}

void PixelRescaleFilterDialog::setupFilterUI()
{
    QGroupBox *paramGroup = new QGroupBox(tr("Rescale Parameters"), this);
    QGridLayout *gridLayout = new QGridLayout(paramGroup);
    gridLayout->setContentsMargins(10, 10, 10, 10);
    gridLayout->setSpacing(10);

    // 1. Scale Factor
    QLabel *lblScale = new QLabel(tr("Scale Factor:"), this);
    m_scaleCombo = new QComboBox(this);
    m_scaleCombo->addItem(tr("0.5x (Downscale 50%)"), 0.5);
    m_scaleCombo->addItem(tr("2x (Double 200%)"), 2.0);
    m_scaleCombo->addItem(tr("3x (Triple 300%)"), 3.0);
    m_scaleCombo->addItem(tr("4x (Quadruple 400%)"), 4.0);
    m_scaleCombo->setCurrentIndex(1); // Default to 2x

    gridLayout->addWidget(lblScale, 0, 0);
    gridLayout->addWidget(m_scaleCombo, 0, 1);

    // 2. Algorithm
    QLabel *lblAlgo = new QLabel(tr("Resampling Engine:"), this);
    m_algoCombo = new QComboBox(this);
    m_algoCombo->addItem(tr("Nearest-Neighbor (Sharp / Pixel-Perfect)"), NearestNeighbor);
    m_algoCombo->addItem(tr("Scale2x / AdvMAME2x (Smooth Contours)"), Scale2x);
    m_algoCombo->setCurrentIndex(0);

    gridLayout->addWidget(lblAlgo, 1, 0);
    gridLayout->addWidget(m_algoCombo, 1, 1);

    // 3. Dimensions info preview
    m_dimLabel = new QLabel(this);
    m_dimLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #2980b9;"));
    gridLayout->addWidget(m_dimLabel, 2, 0, 1, 2);

    contentLayout()->addWidget(paramGroup);

    updateDimensionLabel();

    // Wire events
    connect(m_scaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PixelRescaleFilterDialog::onParametersChanged);
    connect(m_algoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PixelRescaleFilterDialog::onParametersChanged);
}

double PixelRescaleFilterDialog::scaleFactor() const
{
    return m_scaleCombo ? m_scaleCombo->currentData().toDouble() : 2.0;
}

PixelRescaleFilterDialog::Algorithm PixelRescaleFilterDialog::algorithm() const
{
    return m_algoCombo ? static_cast<Algorithm>(m_algoCombo->currentData().toInt()) : NearestNeighbor;
}

void PixelRescaleFilterDialog::updateDimensionLabel()
{
    if (!m_dimLabel) return;
    int srcW = m_initialAtlas.width();
    int srcH = m_initialAtlas.height();
    double s = scaleFactor();
    int dstW = std::max(1, qRound(srcW * s));
    int dstH = std::max(1, qRound(srcH * s));
    m_dimLabel->setText(tr("Atlas Dimensions: %1x%2 -> %3x%4 px")
                        .arg(srcW).arg(srcH).arg(dstW).arg(dstH));
}

void PixelRescaleFilterDialog::onParametersChanged()
{
    updateDimensionLabel();
    schedulePreview();
}

void PixelRescaleFilterDialog::resetDefaults()
{
    if (m_scaleCombo) m_scaleCombo->setCurrentIndex(1); // 2x
    if (m_algoCombo) m_algoCombo->setCurrentIndex(0);   // Nearest
    updateDimensionLabel();
}

void PixelRescaleFilterDialog::saveSettings()
{
}

QImage PixelRescaleFilterDialog::applyNearest(const QImage &source, double factor)
{
    if (source.isNull()) return source;
    int newW = std::max(1, qRound(source.width() * factor));
    int newH = std::max(1, qRound(source.height() * factor));
    return source.scaled(newW, newH, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

QImage PixelRescaleFilterDialog::applyScale2x(const QImage &source)
{
    if (source.isNull()) return source;

    int w = source.width();
    int h = source.height();
    QImage src = source.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w * 2, h * 2, QImage::Format_ARGB32);

    auto getPixelSafe = [&](int x, int y) -> QRgb {
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        return src.pixel(x, y);
    };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            QRgb p = src.pixel(x, y);

            if (qAlpha(p) == 0) {
                // If center pixel is transparent, keep transparent output
                dst.setPixel(x * 2,     y * 2,     qRgba(0, 0, 0, 0));
                dst.setPixel(x * 2 + 1, y * 2,     qRgba(0, 0, 0, 0));
                dst.setPixel(x * 2,     y * 2 + 1, qRgba(0, 0, 0, 0));
                dst.setPixel(x * 2 + 1, y * 2 + 1, qRgba(0, 0, 0, 0));
                continue;
            }

            QRgb a = getPixelSafe(x, y - 1); // Top
            QRgb b = getPixelSafe(x + 1, y); // Right
            QRgb c = getPixelSafe(x - 1, y); // Left
            QRgb d = getPixelSafe(x, y + 1); // Bottom

            QRgb e0 = (c == a && c != d && a != b) ? a : p;
            QRgb e1 = (a == b && a != c && b != d) ? b : p;
            QRgb e2 = (d == c && d != b && c != a) ? c : p;
            QRgb e3 = (b == d && b != a && d != c) ? d : p;

            dst.setPixel(x * 2,     y * 2,     e0);
            dst.setPixel(x * 2 + 1, y * 2,     e1);
            dst.setPixel(x * 2,     y * 2 + 1, e2);
            dst.setPixel(x * 2 + 1, y * 2 + 1, e3);
        }
    }

    return dst;
}

QImage PixelRescaleFilterDialog::applyScale3x(const QImage &source)
{
    if (source.isNull()) return source;

    int w = source.width();
    int h = source.height();
    QImage src = source.convertToFormat(QImage::Format_ARGB32);
    QImage dst(w * 3, h * 3, QImage::Format_ARGB32);

    auto getPixelSafe = [&](int x, int y) -> QRgb {
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        return src.pixel(x, y);
    };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            QRgb e = src.pixel(x, y);

            if (qAlpha(e) == 0) {
                for (int dy = 0; dy < 3; ++dy) {
                    for (int dx = 0; dx < 3; ++dx) {
                        dst.setPixel(x * 3 + dx, y * 3 + dy, qRgba(0, 0, 0, 0));
                    }
                }
                continue;
            }

            QRgb a = getPixelSafe(x - 1, y - 1);
            QRgb b = getPixelSafe(x,     y - 1);
            QRgb c = getPixelSafe(x + 1, y - 1);
            QRgb d = getPixelSafe(x - 1, y);
            QRgb f = getPixelSafe(x + 1, y);
            QRgb g = getPixelSafe(x - 1, y + 1);
            QRgb h_px = getPixelSafe(x, y + 1);
            QRgb i = getPixelSafe(x + 1, y + 1);

            QRgb e0 = (d == b && d != h_px && b != f) ? d : e;
            QRgb e1 = ((d == b && d != h_px && b != f && e != c) || (b == f && b != d && f != h_px && e != a)) ? b : e;
            QRgb e2 = (b == f && b != d && f != h_px) ? f : e;
            QRgb e3 = ((h_px == d && h_px != f && d != b && e != a) || (d == b && d != h_px && b != f && e != g)) ? d : e;
            QRgb e4 = e;
            QRgb e5 = ((b == f && b != d && f != h_px && e != i) || (f == h_px && f != b && h_px != d && e != c)) ? f : e;
            QRgb e6 = (h_px == d && h_px != f && d != b) ? d : e;
            QRgb e7 = ((f == h_px && f != b && h_px != d && e != g) || (h_px == d && h_px != f && d != b && e != i)) ? h_px : e;
            QRgb e8 = (f == h_px && f != b && h_px != d) ? f : e;

            dst.setPixel(x * 3,     y * 3,     e0);
            dst.setPixel(x * 3 + 1, y * 3,     e1);
            dst.setPixel(x * 3 + 2, y * 3,     e2);
            dst.setPixel(x * 3,     y * 3 + 1, e3);
            dst.setPixel(x * 3 + 1, y * 3 + 1, e4);
            dst.setPixel(x * 3 + 2, y * 3 + 1, e5);
            dst.setPixel(x * 3,     y * 3 + 2, e6);
            dst.setPixel(x * 3 + 1, y * 3 + 2, e7);
            dst.setPixel(x * 3 + 2, y * 3 + 2, e8);
        }
    }

    return dst;
}

QImage PixelRescaleFilterDialog::applyRescale(const QImage &source, double factor, Algorithm algo)
{
    if (source.isNull()) return source;

    if (algo == Scale2x) {
        if (qFuzzyCompare(factor, 2.0)) {
            return applyScale2x(source);
        } else if (qFuzzyCompare(factor, 3.0)) {
            return applyScale3x(source);
        } else if (qFuzzyCompare(factor, 4.0)) {
            return applyScale2x(applyScale2x(source));
        }
    }

    return applyNearest(source, factor);
}

void PixelRescaleFilterDialog::applyPreview()
{
    if (!m_document || m_initialAtlas.isNull()) return;

    double factor = scaleFactor();
    Algorithm algo = algorithm();

    m_previewAtlas = applyRescale(m_initialAtlas, factor, algo);

    if (isAutoDetectBoxesEnabled()) {
        updatePreviewFramesAndBoxes(m_previewAtlas, m_previewFrames, m_previewBoxes);
    } else {
        // Proportionally scale initial bounding boxes
        m_previewBoxes.clear();
        m_previewBoxes.reserve(m_initialBoxes.size());
        for (const SpriteBox &b : m_initialBoxes) {
            QRect r(qRound(b.rect.x() * factor),
                    qRound(b.rect.y() * factor),
                    std::max(1, qRound(b.rect.width() * factor)),
                    std::max(1, qRound(b.rect.height() * factor)));
            SpriteBox newBox;
            newBox.rect = r;
            newBox.index = b.index;
            newBox.selected = b.selected;
            newBox.groupId = b.groupId;
            m_previewBoxes.append(newBox);
        }

        m_previewFrames.clear();
        m_previewFrames.reserve(m_previewBoxes.size());
        for (const SpriteBox &box : m_previewBoxes) {
            QRect r = box.rect.intersected(m_previewAtlas.rect());
            m_previewFrames.append(QPixmap::fromImage(m_previewAtlas.copy(r)));
        }
    }

    m_document->setAtlas(m_previewAtlas);
    m_document->setFrames(m_previewFrames, m_previewBoxes);

    QString algoName = (algo == Scale2x && factor >= 2.0) ? QStringLiteral("Scale2x") : QStringLiteral("Nearest");
    setStatusText(tr("Rescaled %1x (%2)").arg(factor).arg(algoName)
                  + QStringLiteral(" — ") + tr("%1 frame(s) detected").arg(m_previewBoxes.size()));
}

QUndoCommand* PixelRescaleFilterDialog::createUndoCommand()
{
    if (!m_document || m_previewAtlas.isNull()) return nullptr;

    return new ApplyFilterCommand(
        m_document,
        tr("Filter: Pixel Art Rescale"),
        m_initialAtlas, m_initialFrames, m_initialBoxes, m_initialAnimations,
        m_previewAtlas, m_previewFrames, m_previewBoxes, m_initialAnimations
    );
}
