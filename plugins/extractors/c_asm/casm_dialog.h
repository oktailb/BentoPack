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

#ifndef CASM_DIALOG_H
#define CASM_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include "casm_options.h"
#include "model/spritedocument.h"

namespace Retro {

class CAsmDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CAsmDialog(const SpriteDocument &doc, const QString &baseName, QWidget *parent = nullptr);
    ~CAsmDialog() override = default;

    CAsmConfig config() const { return m_config; }

private slots:
    void onPresetChanged(int index);
    void onControlChanged();
    void updatePreview();
    void onCopyToClipboard();

private:
    void setupUi();
    void populateControlsFromConfig();

    const SpriteDocument &m_doc;
    QString               m_baseName;
    CAsmConfig            m_config;
    bool                  m_isUpdatingControls = false;

    // UI Widgets
    QComboBox       *m_comboPreset = nullptr;
    QComboBox       *m_comboLanguage = nullptr;
    QComboBox       *m_comboLayout = nullptr;
    QComboBox       *m_comboNumberFormat = nullptr;
    QSpinBox        *m_spinBytesPerLine = nullptr;
    QLineEdit       *m_txtPrefix = nullptr;
    QLineEdit       *m_txtCustomType = nullptr;
    QLineEdit       *m_txtStorageQualifier = nullptr;

    QCheckBox       *m_chkExportPalette = nullptr;
    QCheckBox       *m_chkExportAnimTable = nullptr;
    QCheckBox       *m_chkExportMetadata = nullptr;
    QCheckBox       *m_chkHeaderGuards = nullptr;
    QCheckBox       *m_chkLinterComments = nullptr;

    QLabel          *m_lblFootprint = nullptr;
    QLabel          *m_lblWarning = nullptr;
    QPlainTextEdit  *m_txtPreview = nullptr;
    QPushButton     *m_btnCopy = nullptr;
};

} // namespace Retro

#endif // CASM_DIALOG_H
