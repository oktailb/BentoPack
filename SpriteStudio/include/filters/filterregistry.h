#ifndef FILTERREGISTRY_H
#define FILTERREGISTRY_H

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>
#include "filters/filterplugin.h"

class QMenu;
class SpriteDocument;
class QUndoStack;
class QWidget;

/**
 * @brief Central registry managing built-in and dynamic filter plugins.
 */
class FilterRegistry : public QObject
{
    Q_OBJECT

public:
    static FilterRegistry& instance();
    ~FilterRegistry() override;

    void registerFilter(std::unique_ptr<FilterPlugin> filter);
    void registerFilter(FilterPlugin *filter, bool takeOwnership = true);

    const QList<FilterPlugin*>& filters() const { return m_filters; }
    FilterPlugin* findFilter(const QString &id) const;

    QStringList categories() const;
    QList<FilterPlugin*> filtersByCategory(const QString &category) const;

    void initDefaultFilters();

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
    bool                                        m_initialized = false;
};

#endif // FILTERREGISTRY_H
