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

#include "widgets/filtermenubuilder.h"
#include "filters/filterregistry.h"
#include "filters/filterplugin.h"
#include "widgets/filterdialogbase.h"
#include "widgets/polygonmeshdialog.h"
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
        if (cat == QLatin1String("Cleanup") || cat == QLatin1String("Cleanup & Extraction"))
            catDisplay = QObject::tr("Cleanup & Extraction");
        else if (cat == QLatin1String("Colors") || cat == QLatin1String("Colors & Palettes"))
            catDisplay = QObject::tr("Colors & Palettes");
        else if (cat == QLatin1String("Effects") || cat == QLatin1String("Effects & Outlines"))
            catDisplay = QObject::tr("Effects & Outlines");
        else if (cat == QLatin1String("Geometry") || cat == QLatin1String("Geometry & Transform"))
            catDisplay = QObject::tr("Geometry & Transform");

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
                    int res = dlg->exec();
                    delete dlg;

                    if (res == QDialog::Accepted && filter->isGeometryModifier() && doc && doc->frameCount() > 0) {
                        bool hasAnyMesh = false;
                        for (const auto &box : doc->boxes()) {
                            if (box.hasPolygonMesh) {
                                hasAnyMesh = true;
                                break;
                            }
                        }

                        QString title = QObject::tr("Geometry Modified");
                        QString prompt = hasAnyMesh
                            ? QObject::tr("The applied filter modified sprite geometry and silhouettes.\nExisting polygon meshes might no longer match the new silhouettes.\n\nWould you like to open the Polygon Mesh tool to recalculate meshes?")
                            : QObject::tr("The applied filter modified sprite geometry and silhouettes.\n\nWould you like to open the Polygon Mesh tool to generate tight polygon meshes and reduce GPU overdraw?");

                        int reply = QMessageBox::question(parentWindow, title, prompt, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                        if (reply == QMessageBox::Yes) {
                            PolygonMeshDialog meshDlg(doc, undoStack, 0, parentWindow);
                            meshDlg.exec();
                        }
                    }
                }
            });
        }
    }
}
