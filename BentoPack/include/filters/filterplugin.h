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
class BENTOPACK_CORE_EXPORT FilterPlugin
{
public:
    /**
     * @brief Declares the functional nature of mutations performed by the filter.
     */
    enum FilterModifierFlag {
        NoModifier       = 0x0,
        PixelModifier    = 0x1, ///< Modifies color channels/palette without altering alpha silhouettes or dimensions.
        GeometryModifier = 0x2, ///< Modifies alpha silhouettes, dimensions, or bounds (e.g. Outline, Rescale, BackgroundRemoval).
        AtlasModifier    = 0x4  ///< Reorganizes or packs rectangles/sprites across the atlas sheet.
    };
    Q_DECLARE_FLAGS(FilterModifierFlags, FilterModifierFlag)

    virtual ~FilterPlugin() = default;

    /**
     * @brief Returns bitwise flags describing what this filter modifies.
     */
    virtual FilterModifierFlags modifierFlags() const { return PixelModifier; }

    bool isPixelModifier() const { return modifierFlags().testFlag(PixelModifier); }
    bool isGeometryModifier() const { return modifierFlags().testFlag(GeometryModifier); }
    bool isAtlasModifier() const { return modifierFlags().testFlag(AtlasModifier); }

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
Q_DECLARE_OPERATORS_FOR_FLAGS(FilterPlugin::FilterModifierFlags)

#endif // FILTERPLUGIN_H
