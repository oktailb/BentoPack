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

#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <memory>
#include "extractor/export.h"
#include "model/spritedocument.h"

namespace Ui {
class ExportDialog;
}

class ProjectController;
class QTimer;

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(const SpriteDocument *document, const QString &defaultPath = QString(), QWidget *parent = nullptr, ProjectController *controller = nullptr);
    ~ExportDialog() override;

    QString exportFilePath() const;
    ExportOptions exportOptions() const;

public slots:
    void accept() override;
    void reject() override;

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onBrowseClicked();
    void updateStats();
    void onFormatChanged(int index);
    void onTextureFormatChanged(int index);
    void validateFilePath();

private:
    void setControlsEnabled(bool enabled);

    std::unique_ptr<Ui::ExportDialog> ui;
    const SpriteDocument *m_document = nullptr;
    ProjectController *m_controller = nullptr;
    QTimer *m_debounceTimer = nullptr;
    bool m_isExporting = false;
};

#endif // EXPORTDIALOG_H
