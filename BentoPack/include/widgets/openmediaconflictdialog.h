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

#ifndef OPENMEDIACONFLICTDIALOG_H
#define OPENMEDIACONFLICTDIALOG_H

#include <QDialog>
#include <QString>
#include "bentopackwidgets_export.h"

class QPushButton;

class BENTOPACK_WIDGETS_EXPORT OpenMediaConflictDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Choice {
        CloseAndOpenNew = 1,
        ReplaceExistingAtlas = 2,
        AddNewLayers = 3,
        AddAsCommonBackground = 4
    };

    explicit OpenMediaConflictDialog(const QString &filePath, QWidget *parent = nullptr);
    ~OpenMediaConflictDialog() override = default;

    Choice selectedChoice() const { return m_choice; }

private:
    void setupUi();
    QPushButton* createChoiceButton(const QString &badge,
                                   const QString &title,
                                   const QString &description,
                                   const QString &accentColor);

    QString m_filePath;
    Choice  m_choice = Choice::CloseAndOpenNew;
};

#endif // OPENMEDIACONFLICTDIALOG_H
