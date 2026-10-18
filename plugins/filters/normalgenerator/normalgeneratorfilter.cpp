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

#include "normalgeneratorfilter.h"
#include "normalgeneratordialog.h"
#include <QCoreApplication>

QString NormalGeneratorFilter::name() const
{
    return QCoreApplication::translate("NormalGeneratorFilter", "Normal Map Generator ...");
}

QString NormalGeneratorFilter::description() const
{
    return QCoreApplication::translate("NormalGeneratorFilter",
        "Generates tangent-space normal maps (_n) with Sobel height convolution, volumetric alpha silhouette beveling, and interactive 2D light preview.");
}

QString NormalGeneratorFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "2D Lighting & Materials ");
}

FilterDialogBase* NormalGeneratorFilter::createDialog(SpriteDocument *doc,
                                                      QUndoStack *undoStack,
                                                      QWidget *parent)
{
    return new NormalGeneratorDialog(doc, undoStack, parent);
}

QImage NormalGeneratorFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    double bump = params.value(QStringLiteral("bumpStrength"), 2.0).toDouble();
    int bevel = params.value(QStringLiteral("bevelDepth"), 2).toInt();
    bool invertY = params.value(QStringLiteral("invertY"), false).toBool();
    bool invertX = params.value(QStringLiteral("invertX"), false).toBool();
    return NormalGeneratorDialog::generateNormalMap(image, bump, bevel, invertY, invertX);
}
