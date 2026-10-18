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

#include "emissivegeneratorfilter.h"
#include "emissivegeneratordialog.h"
#include <QCoreApplication>

QString EmissiveGeneratorFilter::name() const
{
    return QCoreApplication::translate("EmissiveGeneratorFilter", "Emissive Map Generator ...");
}

QString EmissiveGeneratorFilter::description() const
{
    return QCoreApplication::translate("EmissiveGeneratorFilter",
        "Extracts emissive maps (_e) for neon, magic runes, fire, and glows using luminance thresholding or selective color key isolation.");
}

QString EmissiveGeneratorFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "2D Lighting & Materials ");
}

FilterDialogBase* EmissiveGeneratorFilter::createDialog(SpriteDocument *doc,
                                                        QUndoStack *undoStack,
                                                        QWidget *parent)
{
    return new EmissiveGeneratorDialog(doc, undoStack, parent);
}

QImage EmissiveGeneratorFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    int modeVal = params.value(QStringLiteral("mode"), 0).toInt();
    auto mode = static_cast<EmissiveGeneratorDialog::ExtractionMode>(modeVal);
    int threshold = params.value(QStringLiteral("threshold"), 180).toInt();
    int softKnee = params.value(QStringLiteral("softKnee"), 20).toInt();
    QRgb targetColor = params.value(QStringLiteral("targetColor"), qRgb(0, 255, 255)).toUInt();
    int hueTolerance = params.value(QStringLiteral("hueTolerance"), 30).toInt();
    double multiplier = params.value(QStringLiteral("multiplier"), 1.5).toDouble();
    bool blackBg = params.value(QStringLiteral("blackBackground"), false).toBool();

    return EmissiveGeneratorDialog::generateEmissiveMap(
        image, mode, threshold, softKnee, targetColor, hueTolerance, multiplier, blackBg);
}
