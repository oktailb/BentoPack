#include "sample_filter.h"

QImage SampleFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    Q_UNUSED(params);
    if (image.isNull()) {
        return image;
    }

    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        auto *scanline = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            QRgb pixel = scanline[x];
            int a = qAlpha(pixel);
            if (a > 0) {
                scanline[x] = qRgba(255 - qRed(pixel),
                                    255 - qGreen(pixel),
                                    255 - qBlue(pixel),
                                    a);
            }
        }
    }
    return result;
}
