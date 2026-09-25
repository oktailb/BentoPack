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

#ifndef FILTERREGISTRY_H
#define FILTERREGISTRY_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>
#include "filters/filterplugin.h"

#include "bentopackcore_export.h"

class QMenu;
class SpriteDocument;
class QUndoStack;
class QWidget;

/**
 * @brief Central registry managing built-in and dynamic filter plugins.
 */
class SPRITESTUDIO_CORE_EXPORT FilterRegistry : public QObject
{
    Q_OBJECT

public:
    static FilterRegistry& instance();
    ~FilterRegistry() override;

    void registerFilter(std::unique_ptr<FilterPlugin> filter);
    void registerFilter(FilterPlugin *filter, bool takeOwnership = true);
    void loadPlugins(const QString &dirPath);

    const QList<FilterPlugin*>& filters() const { return m_filters; }
    FilterPlugin* findFilter(const QString &id) const;

    QStringList categories() const;
    QList<FilterPlugin*> filtersByCategory(const QString &category) const;

    void initDefaultFilters();
    void rescanPlugins();

    /**
     * @brief Populates a QMenu with actions for all registered filters grouped by category.
     */
    void populateMenu(QMenu *menu,
                      SpriteDocument *doc,
                      QUndoStack *undoStack,
                      QWidget *parentWindow);

private:
    FilterRegistry() = default;
    Q_DISABLE_COPY(FilterRegistry)

    QList<FilterPlugin*>                        m_filters;
    std::vector<std::unique_ptr<FilterPlugin>>  m_ownedFilters;
    QSet<QString>                               m_loadedLibraries;
    QSet<QString>                               m_scannedDirs;
    bool                                        m_initialized = false;
};

#endif // FILTERREGISTRY_H
