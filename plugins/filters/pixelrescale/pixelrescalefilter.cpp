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

#include "pixelrescalefilter.h"
#include "pixelrescalefilterdialog.h"
#include <QCoreApplication>

QString PixelRescaleFilter::name() const
{
    return QCoreApplication::translate("PixelRescaleFilter", "Pixel Art Rescale...");
}

QString PixelRescaleFilter::description() const
{
    return QCoreApplication::translate("PixelRescaleFilter",
        "Rescales the atlas cleanly using Nearest-Neighbor (pixel-perfect) or Scale2x (smooth contours).");
}

QString PixelRescaleFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Geometry & Transform");
}

FilterDialogBase* PixelRescaleFilter::createDialog(SpriteDocument *doc,
                                                   QUndoStack *undoStack,
                                                   QWidget *parent)
{
    return new PixelRescaleFilterDialog(doc, undoStack, parent);
}

QImage PixelRescaleFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    double factor = params.value(QStringLiteral("factor"), 2.0).toDouble();
    QString algoStr = params.value(QStringLiteral("algorithm"), QStringLiteral("scale2x")).toString().toLower();
    PixelRescaleFilterDialog::Algorithm algo = PixelRescaleFilterDialog::Scale2x;
    if (algoStr == QStringLiteral("nearest") || algoStr == QStringLiteral("nearestneighbor")) {
        algo = PixelRescaleFilterDialog::NearestNeighbor;
    }
    return PixelRescaleFilterDialog::applyRescale(image, factor, algo);
}
