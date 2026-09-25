/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#include "widgets/filtermenubuilder.h"
#include "filters/filterregistry.h"
#include "filters/filterplugin.h"
#include "widgets/filterdialogbase.h"
#include "model/spritedocument.h"
#include <QAction>
#include <QMessageBox>

void FilterMenuBuilder::populateMenu(QMenu *menu,
                                     SpriteDocument *doc,
                                     QUndoStack *undoStack,
                                     QWidget *parentWindow)
{
    if (!menu) return;
    menu->clear();

    FilterRegistry &reg = FilterRegistry::instance();

    QStringList cats = reg.categories();
    for (int i = 0; i < cats.size(); ++i) {
        const QString &cat = cats[i];
        if (i > 0) {
            menu->addSeparator();
        }

        // Section header for category
        QString catDisplay = cat;
        if (cat == QLatin1String("Cleanup"))       catDisplay = QObject::tr("Cleanup");
        else if (cat == QLatin1String("Colors"))   catDisplay = QObject::tr("Colors");
        else if (cat == QLatin1String("Effects"))  catDisplay = QObject::tr("Effects");
        else if (cat == QLatin1String("Geometry")) catDisplay = QObject::tr("Geometry");

        menu->addSection(catDisplay);

        QList<FilterPlugin*> catFilters = reg.filtersByCategory(cat);
        for (FilterPlugin *filter : catFilters) {
            if (!filter) continue;

            QAction *action = menu->addAction(filter->name());
            action->setToolTip(filter->description());
            if (!filter->shortcut().isEmpty()) {
                action->setShortcut(filter->shortcut());
            }
            if (!filter->icon().isNull()) {
                action->setIcon(filter->icon());
            }

            QObject *context = parentWindow ? static_cast<QObject*>(parentWindow) : static_cast<QObject*>(menu);
            QObject::connect(action, &QAction::triggered, context, [filter, doc, undoStack, parentWindow]() {
                if (!doc || doc->atlas().isNull()) {
                    QMessageBox::information(parentWindow,
                                             QObject::tr("No Atlas Loaded"),
                                             QObject::tr("Please open or import a sprite sheet first before applying a filter."));
                    return;
                }

                FilterDialogBase *dlg = filter->createDialog(doc, undoStack, parentWindow);
                if (dlg) {
                    dlg->exec();
                    delete dlg;
                }
            });
        }
    }
}
