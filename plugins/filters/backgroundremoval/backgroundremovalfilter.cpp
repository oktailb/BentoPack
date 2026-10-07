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

#include "backgroundremovalfilter.h"
#include "backgroundremovaldialog.h"
#include "controller/projectcontroller.h"
#include <QCoreApplication>

QString BackgroundRemovalFilter::name() const
{
    return QCoreApplication::translate("BackgroundRemovalFilter", "Background Removal...");
}

QString BackgroundRemovalFilter::description() const
{
    return QCoreApplication::translate("BackgroundRemovalFilter",
        "Detects dominant background color and makes pixels transparent with automatic bounding box recalculation.");
}

QString BackgroundRemovalFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Cleanup & Extraction");
}

FilterDialogBase* BackgroundRemovalFilter::createDialog(SpriteDocument *doc,
                                                        QUndoStack *undoStack,
                                                        QWidget *parent)
{
    return new BackgroundRemovalDialog(doc, undoStack, parent);
}

QImage BackgroundRemovalFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    int tol = params.value(QStringLiteral("colorTolerance"), 20).toInt();
    return ProjectController::removeBackgroundFromImage(image, tol);
}
