#include "filters/filterregistry.h"
#include "filters/backgroundremovalfilter.h"
#include "filters/despillfilter.h"
#include "filters/outlinefilter.h"
#include "filters/colorswapfilter.h"
#include "filters/coloradjustfilter.h"
#include "filters/pixelrescalefilter.h"
#include "filters/retropalettefilter.h"
#include "filters/atlaspackingfilter.h"
#include "widgets/filterdialogbase.h"
#include "model/spritedocument.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>

FilterRegistry& FilterRegistry::instance()
{
    static FilterRegistry s_instance;
    return s_instance;
}

FilterRegistry::~FilterRegistry()
{
}

void FilterRegistry::registerFilter(std::unique_ptr<FilterPlugin> filter)
{
    if (!filter) return;
    m_filters.append(filter.get());
    m_ownedFilters.push_back(std::move(filter));
}

void FilterRegistry::registerFilter(FilterPlugin *filter, bool takeOwnership)
{
    if (!filter) return;
    m_filters.append(filter);
    if (takeOwnership) {
        m_ownedFilters.emplace_back(filter);
    }
}

FilterPlugin* FilterRegistry::findFilter(const QString &id) const
{
    for (FilterPlugin *f : m_filters) {
        if (f && f->id() == id) {
            return f;
        }
    }
    return nullptr;
}

QStringList FilterRegistry::categories() const
{
    QStringList cats;
    for (FilterPlugin *f : m_filters) {
        if (f && !cats.contains(f->category())) {
            cats.append(f->category());
        }
    }
    return cats;
}

QList<FilterPlugin*> FilterRegistry::filtersByCategory(const QString &category) const
{
    QList<FilterPlugin*> list;
    for (FilterPlugin *f : m_filters) {
        if (f && f->category() == category) {
            list.append(f);
        }
    }
    return list;
}

void FilterRegistry::initDefaultFilters()
{
    if (m_initialized) return;

    registerFilter(std::make_unique<BackgroundRemovalFilter>());
    registerFilter(std::make_unique<DespillFilter>());
    registerFilter(std::make_unique<OutlineFilter>());
    registerFilter(std::make_unique<ColorSwapFilter>());
    registerFilter(std::make_unique<ColorAdjustFilter>());
    registerFilter(std::make_unique<PixelRescaleFilter>());
    registerFilter(std::make_unique<RetroPaletteFilter>());
    registerFilter(std::make_unique<AtlasPackingFilter>());

    m_initialized = true;
}

void FilterRegistry::populateMenu(QMenu *menu,
                                  SpriteDocument *doc,
                                  QUndoStack *undoStack,
                                  QWidget *parentWindow)
{
    if (!menu) return;
    menu->clear();

    if (!m_initialized) {
        initDefaultFilters();
    }

    QStringList cats = categories();
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

        QAction *headerAction = menu->addSection(catDisplay);
        Q_UNUSED(headerAction);

        QList<FilterPlugin*> catFilters = filtersByCategory(cat);
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
            connect(action, &QAction::triggered, context, [filter, doc, undoStack, parentWindow]() {
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
