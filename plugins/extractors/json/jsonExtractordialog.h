// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#ifndef JSONEXTRACTORDIALOG_H
#define JSONEXTRACTORDIALOG_H

#include <QDialog>
#include <memory>
#include "extractor/export.h"
#include "model/spritedocument.h"

namespace Ui {
class jsonExtractorDialog;
}

class jsonExtractorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit jsonExtractorDialog(const SpriteDocument &doc, const QString &baseName, QWidget *parent = nullptr);
    ~jsonExtractorDialog();

    ExportOptions getOpts() const;
    bool replaceAtlas();
    Format selectedFormat();
    QImage::Format imageFormat();
    QString imageFormatAsString();
    QList<QString> selectedAnimations() const;
    AtlasStrategy selectedStrategy() const;

private slots:
    void on_animations_itemSelectionChanged();
    void on_replaceExistingAtlas_checkStateChanged(const Qt::CheckState &state);
    void on_atlasSaveStrategy_currentIndexChanged(int index);

private:
    std::unique_ptr<Ui::jsonExtractorDialog> ui;
    QString                   m_baseName;
    ExportOptions             m_opts;
    QList<QString>            m_selectedAnimations;
    AtlasStrategy             m_selectedStrategy;
};

#endif // JSONEXTRACTORDIALOG_H
