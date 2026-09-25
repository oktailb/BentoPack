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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPixmap>
#include <QIcon>
#include "bentopackwidgets_export.h"

class BENTOPACK_WIDGETS_EXPORT AboutDialog : public QDialog
{
  Q_OBJECT

public:
  explicit AboutDialog(QWidget *parent = nullptr);

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupUI();
  void loadApplicationInfo();
  void loadPricing();
  void loadPluginsInfo();
  void loadCredits();
  void loadLicense();
  QString readTextFile(const QString &filePath);

  struct ThemeColors {
      bool isDark;
      QString bgDialog;
      QString paneBg;
      QString cardBg;
      QString cardBorder;
      QString textColor;
      QString textMuted;
      QString headingColor;
      QString tabBg;
      QString tabText;
  };
  ThemeColors getThemeColors() const;

  QTabWidget *tabWidget;
  QTextEdit *aboutText;
  QTextEdit *pricingText;
  QTextEdit *pluginsText;
  QTextEdit *creditsText;
  QTextEdit *licenseText;
  QLabel *iconLabel;
  QLabel *titleLabel;
  QLabel *versionLabel;
  QLabel *editionBadgeLabel;
  QLabel *editionNoticeLabel;
  QPushButton *closeButton;
  QPushButton *settingsButton;
};

#endif // ABOUTDIALOG_H
