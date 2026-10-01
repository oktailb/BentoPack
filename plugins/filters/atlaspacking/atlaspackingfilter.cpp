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

#include "atlaspackingfilter.h"
#include "atlaspackingdialog.h"
#include <QCoreApplication>

QString AtlasPackingFilter::name() const
{
    return QCoreApplication::translate("AtlasPackingFilter", "Atlas Bin-Packing (MaxRects)...");
}

QString AtlasPackingFilter::description() const
{
    return QCoreApplication::translate("AtlasPackingFilter",
        "Repacks sprites into a compact atlas using MaxRects, Power of Two, and animation-safe deduplication.");
}

QString AtlasPackingFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Geometry & Transform");
}

FilterDialogBase* AtlasPackingFilter::createDialog(SpriteDocument *doc,
                                                   QUndoStack *undoStack,
                                                   QWidget *parent)
{
    auto *dlg = new AtlasPackingDialog(doc, undoStack, parent);
    dlg->setAlgorithm(0); // 0 = MaxRects
    return dlg;
}
