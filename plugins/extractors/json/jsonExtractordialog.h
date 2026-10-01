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
