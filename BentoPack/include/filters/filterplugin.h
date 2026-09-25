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

#ifndef FILTERPLUGIN_H
#define FILTERPLUGIN_H

#include <QString>
#include <QKeySequence>
#include <QIcon>
#include <QObject>
#include <QImage>
#include <QVariantMap>
#include "bentopackcore_export.h"

class SpriteDocument;
class QUndoStack;
class QWidget;
class FilterDialogBase;

/**
 * @brief Abstract interface for all image and sprite filter plugins in BentoPack.
 */
class SPRITESTUDIO_CORE_EXPORT FilterPlugin
{
public:
    virtual ~FilterPlugin() = default;

    /**
     * @brief Unique identifier for the filter (e.g., "background_removal", "despill").
     */
    virtual QString id() const = 0;

    /**
     * @brief Localized display name shown in menus and action lists.
     */
    virtual QString name() const = 0;

    /**
     * @brief Detailed tooltip or explanation of the filter.
     */
    virtual QString description() const = 0;

    /**
     * @brief Category name for grouping in menus (e.g., "Nettoyage", "Couleur", "Effets").
     */
    virtual QString category() const = 0;

    /**
     * @brief Optional default keyboard shortcut.
     */
    virtual QKeySequence shortcut() const { return QKeySequence(); }

    /**
     * @brief Optional icon.
     */
    virtual QIcon icon() const { return QIcon(); }

    /**
     * @brief Factory method creating the floating interactive dialog for this filter.
     * @param doc Target document.
     * @param undoStack Target undo stack.
     * @param parent Parent widget.
     * @return An instance of FilterDialogBase, or nullptr if unavailable.
     */
    virtual FilterDialogBase* createDialog(SpriteDocument *doc,
                                           QUndoStack *undoStack = nullptr,
                                           QWidget *parent = nullptr) = 0;

    /**
     * @brief Direct headless execution of the filter on an image (used by CLI and batch tools).
     */
    virtual QImage applyImage(const QImage &image, const QVariantMap &params = QVariantMap()) {
        Q_UNUSED(params);
        return image;
    }

    /**
     * @brief Creates an optional settings/configuration widget for this filter plugin.
     * Allows the plugin to expose its runtime settings dynamically in the host UI.
     * The caller takes ownership of the created widget.
     */
    virtual QWidget* createSettingsWidget(QWidget *parent = nullptr) {
        Q_UNUSED(parent);
        return nullptr;
    }
};

#define FilterPlugin_iid "com.bentopack.FilterPlugin/1.0"
Q_DECLARE_INTERFACE(FilterPlugin, FilterPlugin_iid)

#endif // FILTERPLUGIN_H
