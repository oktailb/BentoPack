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

#include "tightpolygonpackingfilter.h"
#include "atlaspackingdialog.h"
#include <QCoreApplication>

QString TightPolygonPackingFilter::name() const
{
    return QCoreApplication::translate("TightPolygonPackingFilter", "Tight Polygon Packing (Nesting)...");
}

QString TightPolygonPackingFilter::description() const
{
    return QCoreApplication::translate("TightPolygonPackingFilter",
        "Packs sprites tightly into the atlas using tight polygonal envelopes, overlapping bounding boxes, and multi-threaded collision detection.");
}

QString TightPolygonPackingFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Geometry & Transform");
}

FilterDialogBase* TightPolygonPackingFilter::createDialog(SpriteDocument *doc,
                                                          QUndoStack *undoStack,
                                                          QWidget *parent)
{
    auto *dlg = new AtlasPackingDialog(doc, undoStack, parent);
    dlg->setWindowTitle(QCoreApplication::translate("TightPolygonPackingFilter", "Tight Polygon Packing (Nesting)"));
    dlg->setAlgorithm(5); // 5 = Tight Polygon Packing (Nesting — Overlapping Rects)
    return dlg;
}
