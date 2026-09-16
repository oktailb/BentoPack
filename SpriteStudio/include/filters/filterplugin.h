#ifndef FILTERPLUGIN_H
#define FILTERPLUGIN_H

#include <QString>
#include <QKeySequence>
#include <QIcon>

class SpriteDocument;
class QUndoStack;
class QWidget;
class FilterDialogBase;

/**
 * @brief Abstract interface for all image and sprite filter plugins in SpriteStudio.
 */
class FilterPlugin
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
};

#endif // FILTERPLUGIN_H
