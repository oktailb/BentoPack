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

#include "aboutdialog.h"
#include "generated/version.h"
#include "widgets/settingsdialog.h"
#include "license/licensemanager.h"
#include "license/integrityguard.h"
#include "extractor/extractorregistry.h"
#include "filters/filterregistry.h"
#include "localizationmanager.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QScreen>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFrame>
#include <QEvent>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
  setupUI();
  loadApplicationInfo();
  loadPricing();
  loadPluginsInfo();
  loadCredits();
  loadLicense();

  setWindowTitle(tr("KEY_DIALOG_ABOUT_TITLE") + QStringLiteral(" - BentoPack"));
  setMinimumSize(640, 520);
  resize(740, 580);

  QScreen *screen = QGuiApplication::primaryScreen();
  if (screen) {
      QRect screenGeometry = screen->geometry();
      move(screenGeometry.center() - rect().center());
  }
}

void AboutDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        setWindowTitle(tr("KEY_DIALOG_ABOUT_TITLE") + QStringLiteral(" - BentoPack"));
        auto sanitizeTabTitle = [](const QString &title) {
            QString s = title;
            s.replace(QStringLiteral("&&"), QStringLiteral("__DOUBLE_AMP__"));
            s.replace(QStringLiteral("&"), QStringLiteral("&&"));
            s.replace(QStringLiteral("__DOUBLE_AMP__"), QStringLiteral("&&"));
            return s;
        };
        tabWidget->setTabText(0, sanitizeTabTitle(tr("KEY_DIALOG_ABOUT_TITLE")));
        tabWidget->setTabText(1, sanitizeTabTitle(tr("KEY_DIALOG_ABOUT_PRICING")));
        tabWidget->setTabText(2, sanitizeTabTitle(tr("KEY_DIALOG_ABOUT_PLUGINS")));
        tabWidget->setTabText(3, sanitizeTabTitle(tr("KEY_DIALOG_ABOUT_CREDITS")));
        tabWidget->setTabText(4, sanitizeTabTitle(tr("KEY_DIALOG_LICENCE_TITLE")));
        settingsButton->setText(tr("KEY_ACTION_SETTINGS"));
        closeButton->setText(tr("KEY_DIALOG_ABOUT_CLOSE"));

        if (BentoPack::IntegrityGuard::isTampered()) {
            editionNoticeLabel->setText(tr("KEY_EDITION_NOTICE_TAMPERED"));
        } else if (BentoPack::LicenseManager::isCommercial()) {
            editionNoticeLabel->setText(tr("KEY_EDITION_NOTICE_COMMERCIAL"));
        } else {
            editionNoticeLabel->setText(tr("KEY_EDITION_NOTICE_COMMUNITY"));
        }

        loadApplicationInfo();
        loadPricing();
        loadPluginsInfo();
        loadCredits();
        loadLicense();
    }
    QDialog::changeEvent(event);
}

AboutDialog::ThemeColors AboutDialog::getThemeColors() const
{
    ThemeColors c;
    const QPalette &pal = qApp ? qApp->palette() : palette();
    c.isDark = (pal.color(QPalette::Window).lightness() < 140) ||
               (pal.color(QPalette::WindowText).lightness() > 140) ||
               (pal.color(QPalette::Text).lightness() > 140);
    if (c.isDark) {
        c.bgDialog     = QStringLiteral("#181920");
        c.paneBg       = QStringLiteral("#1f222d");
        c.cardBg       = QStringLiteral("#262936");
        c.cardBorder   = QStringLiteral("#374151");
        c.textColor    = QStringLiteral("#e2e8f0");
        c.textMuted    = QStringLiteral("#94a3b8");
        c.headingColor = QStringLiteral("#60a5fa");
        c.tabBg        = QStringLiteral("#272a38");
        c.tabText      = QStringLiteral("#cbd5e1");
    } else {
        c.bgDialog     = QStringLiteral("#f8fafc");
        c.paneBg       = QStringLiteral("#ffffff");
        c.cardBg       = QStringLiteral("#f1f5f9");
        c.cardBorder   = QStringLiteral("#e2e8f0");
        c.textColor    = QStringLiteral("#1e293b");
        c.textMuted    = QStringLiteral("#64748b");
        c.headingColor = QStringLiteral("#2563eb");
        c.tabBg        = QStringLiteral("#e2e8f0");
        c.tabText      = QStringLiteral("#334155");
    }
    return c;
}

void AboutDialog::setupUI()
{
  ThemeColors colors = getThemeColors();
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, 20, 20, 20);
  mainLayout->setSpacing(14);

  // En-tête avec icône, titre, version et badge d'édition
  QHBoxLayout *headerLayout = new QHBoxLayout();
  headerLayout->setSpacing(16);

  iconLabel = new QLabel(this);
  QPixmap appIcon(":/drawer/icons/bentopack.png");
  if (!appIcon.isNull()) {
      iconLabel->setPixmap(appIcon.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  iconLabel->setAlignment(Qt::AlignCenter);

  QVBoxLayout *titleLayout = new QVBoxLayout();
  titleLayout->setSpacing(4);

  titleLabel = new QLabel(QStringLiteral("BentoPack"), this);
  titleLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: bold; color: %1;").arg(colors.textColor));

  QHBoxLayout *versionRow = new QHBoxLayout();
  versionRow->setSpacing(8);
  versionLabel = new QLabel(QStringLiteral("Version %1").arg(PROJECT_VERSION), this);
  versionLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 500; color: %1;").arg(colors.textMuted));

  editionBadgeLabel = new QLabel(this);
  editionNoticeLabel = new QLabel(this);

  if (BentoPack::IntegrityGuard::isTampered()) {
      editionBadgeLabel->setText(QStringLiteral("⚠️ TAMPERED / CIRCUMVENTED BUILD"));
      editionBadgeLabel->setStyleSheet(
          QStringLiteral("background: #ef4444; color: #ffffff; font-weight: bold; "
                         "border-radius: 4px; padding: 2px 8px; font-size: 11px;"));
      editionNoticeLabel->setText(tr("KEY_EDITION_NOTICE_TAMPERED"));
      editionNoticeLabel->setStyleSheet(QStringLiteral("color: #ef4444; font-size: 11px; font-weight: bold;"));
  } else if (BentoPack::LicenseManager::isCommercial()) {
      editionBadgeLabel->setText(QStringLiteral("★ COMMERCIAL EDITION"));
      editionBadgeLabel->setStyleSheet(
          QStringLiteral("background: #10b981; color: #ffffff; font-weight: bold; "
                         "border-radius: 4px; padding: 2px 8px; font-size: 11px;"));
      editionNoticeLabel->setText(tr("KEY_EDITION_NOTICE_COMMERCIAL"));
      editionNoticeLabel->setStyleSheet(QStringLiteral("color: #10b981; font-size: 11px; font-weight: 500;"));
  } else {
      editionBadgeLabel->setText(QStringLiteral("COMMUNITY EDITION"));
      editionBadgeLabel->setStyleSheet(
          QStringLiteral("background: #3b82f6; color: #ffffff; font-weight: bold; "
                         "border-radius: 4px; padding: 2px 8px; font-size: 11px;"));
      editionNoticeLabel->setText(tr("KEY_EDITION_NOTICE_COMMUNITY"));
      editionNoticeLabel->setStyleSheet(QStringLiteral("color: #3b82f6; font-size: 11px; font-weight: 500;"));
  }

  versionRow->addWidget(versionLabel);
  versionRow->addWidget(editionBadgeLabel);
  versionRow->addStretch();

  titleLayout->addWidget(titleLabel);
  titleLayout->addLayout(versionRow);
  titleLayout->addWidget(editionNoticeLabel);

  headerLayout->addWidget(iconLabel);
  headerLayout->addLayout(titleLayout);
  headerLayout->addStretch();

  mainLayout->addLayout(headerLayout);

  // Ligne de séparation
  QFrame *line = new QFrame(this);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Plain);
  line->setStyleSheet(QStringLiteral("color: %1; background-color: %1; height: 1px;").arg(colors.cardBorder));
  mainLayout->addWidget(line);

  // Système d'onglets
  tabWidget = new QTabWidget(this);

  auto setupEditor = [this, &colors](QTextEdit* &edit, const QString &tabTitle) {
      edit = new QTextEdit(this);
      edit->setReadOnly(true);
      edit->setStyleSheet(QStringLiteral(
          "QTextEdit { background: transparent; border: none; color: %1; font-size: 13px; }"
          "QScrollBar:vertical { background: %2; width: 8px; border-radius: 4px; }"
          "QScrollBar::handle:vertical { background: %3; border-radius: 4px; min-height: 20px; }"
      ).arg(colors.textColor, colors.cardBg, colors.cardBorder));
      tabWidget->addTab(edit, tabTitle);
  };

  auto sanitizeTabTitle = [](const QString &title) {
      QString s = title;
      s.replace(QStringLiteral("&&"), QStringLiteral("__DOUBLE_AMP__"));
      s.replace(QStringLiteral("&"), QStringLiteral("&&"));
      s.replace(QStringLiteral("__DOUBLE_AMP__"), QStringLiteral("&&"));
      return s;
  };

  setupEditor(aboutText, sanitizeTabTitle(tr("KEY_DIALOG_ABOUT_TITLE")));
  setupEditor(pricingText, sanitizeTabTitle(tr("KEY_DIALOG_ABOUT_PRICING")));
  setupEditor(pluginsText, sanitizeTabTitle(tr("KEY_DIALOG_ABOUT_PLUGINS")));
  setupEditor(creditsText, sanitizeTabTitle(tr("KEY_DIALOG_ABOUT_CREDITS")));
  setupEditor(licenseText, sanitizeTabTitle(tr("KEY_DIALOG_LICENCE_TITLE")));

  mainLayout->addWidget(tabWidget);

  // Boutons d'action inférieurs
  QHBoxLayout *buttonLayout = new QHBoxLayout();

  settingsButton = new QPushButton(tr("KEY_ACTION_SETTINGS"), this);
  settingsButton->setStyleSheet(QStringLiteral(
      "QPushButton {"
      "    background: %1;"
      "    color: %2;"
      "    border: 1px solid %3;"
      "    padding: 7px 16px;"
      "    border-radius: 5px;"
      "    font-weight: 600;"
      "}"
      "QPushButton:hover { background: %4; border-color: %5; }"
  ).arg(colors.cardBg, colors.textColor, colors.cardBorder, colors.tabBg, colors.headingColor));

  connect(settingsButton, &QPushButton::clicked, this, [this]() {
      SettingsDialog dlg(this);
      dlg.exec();
  });

  closeButton = new QPushButton(tr("KEY_DIALOG_ABOUT_CLOSE"), this);
  closeButton->setStyleSheet(QStringLiteral(
      "QPushButton {"
      "    background: #3b82f6;"
      "    color: #ffffff;"
      "    border: none;"
      "    padding: 7px 20px;"
      "    border-radius: 5px;"
      "    font-weight: bold;"
      "}"
      "QPushButton:hover { background: #2563eb; }"
      "QPushButton:pressed { background: #1d4ed8; }"
  ));
  connect(closeButton, &QPushButton::clicked, this, &AboutDialog::accept);

  buttonLayout->addWidget(settingsButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(closeButton);

  mainLayout->addLayout(buttonLayout);

  // Feuille de style globale adaptée au thème sombre / clair
  setStyleSheet(QStringLiteral(
      "AboutDialog {"
      "    background-color: %1;"
      "}"
      "QTabWidget::pane {"
      "    border: 1px solid %2;"
      "    background-color: %3;"
      "    border-radius: 6px;"
      "}"
      "QTabWidget::tab-bar {"
      "    alignment: left;"
      "}"
      "QTabBar::tab {"
      "    background-color: %4;"
      "    color: %5;"
      "    border: 1px solid %2;"
      "    padding: 7px 14px;"
      "    margin-right: 4px;"
      "    border-top-left-radius: 5px;"
      "    border-top-right-radius: 5px;"
      "    font-weight: 500;"
      "    font-size: 12px;"
      "}"
      "QTabBar::tab:selected {"
      "    background-color: #3b82f6;"
      "    color: #ffffff;"
      "    border-color: #3b82f6;"
      "    font-weight: bold;"
      "}"
      "QTabBar::tab:hover:!selected {"
      "    background-color: %6;"
      "    color: %7;"
      "}"
  ).arg(colors.bgDialog, colors.cardBorder, colors.paneBg,
        colors.tabBg, colors.tabText, colors.cardBg, colors.textColor));
}

void AboutDialog::loadApplicationInfo()
{
  ThemeColors colors = getThemeColors();

  QString aboutHtml = QString(
      "<div style='padding: 10px; font-family: sans-serif; color: %1;'>"
      "  <div style='text-align: center; margin-bottom: 20px;'>"
      "    <h3 style='color: %2; margin-bottom: 6px;'>BentoPack</h3>"
      "    <p style='color: %3; font-size: 13px; margin: 0;'>%4</p>"
      "  </div>"
      "  <div style='background: %5; border: 1px solid %6; border-radius: 8px; padding: 12px 16px; margin-bottom: 14px;'>"
      "    <div style='margin-bottom: 6px;'><b>Tipeee :</b> <a style='color: #38bdf8;' href='https://en.tipeee.com/lecoq-vincent'>https://en.tipeee.com/lecoq-vincent</a></div>"
      "    <div><b>GitHub :</b> <a style='color: #38bdf8;' href='https://github.com/oktailb/BentoPack'>https://github.com/oktailb/BentoPack</a></div>"
      "  </div>"
      "  <div style='background: %5; border: 1px solid %6; border-radius: 8px; padding: 14px 16px; margin-bottom: 14px;'>"
      "    <h4 style='color: %2; margin-top: 0; margin-bottom: 8px;'>Environnement d'Exécution</h4>"
      "    <table style='width: 100%; border-collapse: collapse; font-size: 12px; color: %1;'>"
      "      <tr><td style='padding: 3px 0; color: %3; width: 140px;'><b>Version :</b></td><td>%7</td></tr>"
      "      <tr><td style='padding: 3px 0; color: %3;'><b>Date de build :</b></td><td>%8</td></tr>"
      "      <tr><td style='padding: 3px 0; color: %3;'><b>Qt Runtime :</b></td><td>Qt %9</td></tr>"
      "      <tr><td style='padding: 3px 0; color: %3;'><b>Compilateur :</b></td><td>%10 %11 (%12)</td></tr>"
      "      <tr><td style='padding: 3px 0; color: %3;'><b>Système :</b></td><td>%13</td></tr>"
      "    </table>"
      "  </div>"
      "  <div style='background: %5; border: 1px solid %6; border-radius: 8px; padding: 14px 16px;'>"
      "    <h4 style='color: %2; margin-top: 0; margin-bottom: 8px;'>%14</h4>"
      "    <table style='width: 100%; border-collapse: collapse; font-size: 12px; color: %1;'>"
      "      <tr><td style='padding: 3px 0; color: %3; width: 140px;'><b>Branche :</b></td><td>%15</td></tr>"
      "      <tr><td style='padding: 3px 0; color: %3;'><b>Commit :</b></td><td><code>%16</code></td></tr>"
      "      <tr><td style='padding: 3px 0; color: %3;'><b>Date :</b></td><td>%17</td></tr>"
      "      <tr><td style='padding: 3px 0; color: %3;'><b>Auteur :</b></td><td>%18</td></tr>"
      "    </table>"
      "  </div>"
      "</div>"
  )
  .arg(colors.textColor)
  .arg(colors.headingColor)
  .arg(colors.textMuted)
  .arg(tr("KEY_ABOUT_PURPOSE"))
  .arg(colors.cardBg)
  .arg(colors.cardBorder)
  .arg(PROJECT_VERSION)
  .arg(PROJECT_BUILD_DATE)
  .arg(QT_VERSION_STR)
  .arg(CMAKE_CXX_COMPILER_ID)
  .arg(CMAKE_CXX_COMPILER)
  .arg(CMAKE_CXX_COMPILER_VERSION)
  .arg(QSysInfo::prettyProductName())
  .arg(tr("KEY_ABOUT_GIT_INFO"))
  .arg(GIT_BRANCH)
  .arg(GIT_COMMIT_HASH)
  .arg(GIT_COMMIT_DATE)
  .arg(GIT_LAST_AUTHOR);

  aboutText->setHtml(aboutHtml);
}

void AboutDialog::loadPricing()
{
  ThemeColors colors = getThemeColors();

  // Détermination de la langue active
  QString lang = QStringLiteral("en");
  QString currentLocale = LocalizationManager::instance().currentLanguage();
  if (currentLocale.isEmpty() || currentLocale == QStringLiteral("system")) {
      currentLocale = QLocale::system().name();
  }
  if (currentLocale.startsWith(QStringLiteral("fr"), Qt::CaseInsensitive)) {
      lang = QStringLiteral("fr");
  } else if (currentLocale.startsWith(QStringLiteral("ja"), Qt::CaseInsensitive)) {
      lang = QStringLiteral("ja");
  }

  // Lambdas de résolution des champs multilingues
  auto resolveI18nString = [&lang](const QJsonValue &val, const QString &defaultStr = QString()) -> QString {
      if (val.isObject()) {
          QJsonObject obj = val.toObject();
          if (obj.contains(lang)) {
              return obj.value(lang).toString();
          }
          if (obj.contains(QStringLiteral("en"))) {
              return obj.value(QStringLiteral("en")).toString();
          }
          if (obj.contains(QStringLiteral("fr"))) {
              return obj.value(QStringLiteral("fr")).toString();
          }
          if (!obj.isEmpty()) {
              return obj.begin().value().toString();
          }
          return defaultStr;
      }
      return val.isString() ? val.toString() : defaultStr;
  };

  auto resolveI18nList = [&lang](const QJsonValue &val) -> QStringList {
      QStringList result;
      if (val.isObject()) {
          QJsonObject obj = val.toObject();
          QJsonValue targetVal;
          if (obj.contains(lang)) {
              targetVal = obj.value(lang);
          } else if (obj.contains(QStringLiteral("en"))) {
              targetVal = obj.value(QStringLiteral("en"));
          } else if (obj.contains(QStringLiteral("fr"))) {
              targetVal = obj.value(QStringLiteral("fr"));
          } else if (!obj.isEmpty()) {
              targetVal = obj.begin().value();
          }
          if (targetVal.isArray()) {
              for (const QJsonValue &item : targetVal.toArray()) {
                  result << item.toString();
              }
          }
      } else if (val.isArray()) {
          for (const QJsonValue &item : val.toArray()) {
              result << item.toString();
          }
      }
      return result;
  };

  // Lecture du fichier ressource de tarification
  QString pricingJsonStr = readTextFile(QStringLiteral(":/text/pricing.json"));
  if (pricingJsonStr.isEmpty()) {
      pricingJsonStr = readTextFile(QStringLiteral(":/text/res/pricing.json"));
  }

  QJsonDocument doc = QJsonDocument::fromJson(pricingJsonStr.toUtf8());
  QJsonObject rootObj = doc.isObject() ? doc.object() : QJsonObject();
  QJsonArray tiersArray = rootObj.value(QStringLiteral("tiers")).toArray();

  QString currentEdition = BentoPack::IntegrityGuard::isTampered()
      ? QStringLiteral("tampered")
      : (BentoPack::LicenseManager::isCommercial() ? QStringLiteral("gui_seat") : QStringLiteral("community"));

  QString titleText = resolveI18nString(rootObj.value(QStringLiteral("title")), QStringLiteral("Grille Tarifaire"));
  QString disclaimerText = resolveI18nString(rootObj.value(QStringLiteral("disclaimer")));

  QString html = QStringLiteral(
      "<div style='padding: 10px; font-family: sans-serif; color: %1;'>"
      "  <div style='text-align: center; margin-bottom: 16px;'>"
      "    <h3 style='color: %2; margin-bottom: 4px;'>%3</h3>"
      "    <p style='color: %4; font-size: 12px; margin: 0;'>%5</p>"
      "  </div>"
  ).arg(colors.textColor, colors.headingColor, titleText, colors.textMuted, disclaimerText);

  for (const QJsonValue &val : tiersArray) {
      QJsonObject tier = val.toObject();
      QString id = tier.value(QStringLiteral("id")).toString();
      QString name = resolveI18nString(tier.value(QStringLiteral("name")));
      QString price = resolveI18nString(tier.value(QStringLiteral("price")));
      QString period = resolveI18nString(tier.value(QStringLiteral("period")));
      QString badge = resolveI18nString(tier.value(QStringLiteral("badge")));
      QString badgeColor = tier.value(QStringLiteral("badge_color")).toString(QStringLiteral("#3b82f6"));
      QString target = resolveI18nString(tier.value(QStringLiteral("target")));
      QStringList features = resolveI18nList(tier.value(QStringLiteral("features")));

      bool isActive = (id == currentEdition);
      QString borderStyle = isActive ? QStringLiteral("2px solid #3b82f6") : QStringLiteral("1px solid %1").arg(colors.cardBorder);

      QString activeTag = isActive ? QStringLiteral(" <span style='background: #10b981; color: #ffffff; border-radius: 4px; padding: 2px 8px; font-size: 10px; font-weight: bold;'>%1</span>").arg(tr("KEY_PRICING_ACTIVE_EDITION")) : QString();

      html += QString(
          "  <div style='background: %1; border: %2; border-radius: 8px; padding: 14px; margin-bottom: 14px;'>"
          "    <div style='display: flex; align-items: center; margin-bottom: 8px;'>"
          "      <span style='font-size: 16px; font-weight: bold; color: %3;'>%4</span>"
          "      <span style='background: %5; color: #ffffff; border-radius: 4px; padding: 2px 8px; font-size: 10px; font-weight: bold; margin-left: 8px;'>%6</span>"
          "      %7"
          "    </div>"
          "    <div style='margin-bottom: 8px;'>"
          "      <span style='font-size: 22px; font-weight: bold; color: %3;'>%8</span>"
          "      <span style='color: %9; font-size: 12px;'> / %10</span>"
          "    </div>"
          "    <p style='color: %9; font-size: 12px; margin: 0 0 10px 0;'><b>%11</b> %12</p>"
          "    <ul style='margin: 0; padding-left: 18px; font-size: 12px; color: %13;'>"
      ).arg(colors.cardBg, borderStyle, colors.textColor, name, badgeColor, badge, activeTag, price, colors.textMuted, period, tr("KEY_PRICING_TARGET_LABEL"), target, colors.textColor);

      for (const QString &feat : features) {
          html += QStringLiteral("<li style='margin-bottom: 3px;'>%1</li>").arg(feat);
      }

      html += QStringLiteral("    </ul>\n  <hr>\n  </div>\n");
  }

  html += QStringLiteral("</div>");
  pricingText->setHtml(html);
}

void AboutDialog::loadPluginsInfo()
{
  ThemeColors colors = getThemeColors();

  QString html = QStringLiteral(
      "<div style='padding: 10px; font-family: sans-serif; color: %1;'>"
      "  <div style='margin-bottom: 16px;'>"
      "    <h3 style='color: %2; margin-bottom: 6px;'>%3</h3>"
      "    <p style='color: %4; font-size: 12px; margin: 0;'>%5</p>"
      "  </div>"
      "  <div style='background: %6; border: 1px solid %7; border-radius: 8px; padding: 12px 16px; margin-bottom: 10px;'>"
      "    <b style='color: %2;'>%8</b> BentoPackCore (<code>libBentoPackCore.so / .dll</code>)<br>"
      "    <span style='color: %4; font-size: 12px;'>%9</span>"
      "  </div>"
      "  <div style='background: %6; border: 1px solid %7; border-radius: 8px; padding: 12px 16px; margin-bottom: 14px;'>"
      "    <b style='color: %2;'>%10</b> Codecs d'exportation & filtres (<code>bin/plugins/...</code>)<br>"
      "    <span style='color: %4; font-size: 12px;'>%11</span>"
      "  </div>"
  ).arg(colors.textColor, colors.headingColor, tr("KEY_PLUGINS_SECTION_TITLE"), colors.textMuted,
        tr("KEY_PLUGINS_SECTION_DESC"), colors.cardBg, colors.cardBorder,
        tr("KEY_PLUGINS_CORE_TITLE"), tr("KEY_PLUGINS_CORE_DESC"),
        tr("KEY_PLUGINS_EXT_TITLE"), tr("KEY_PLUGINS_EXT_DESC"));

  // Détermination du statut de licence des plugins
  QString pluginLicenseBadge;
  if (BentoPack::IntegrityGuard::isTampered()) {
      pluginLicenseBadge = tr("KEY_PLUGINS_LIC_TAMPERED");
  } else if (BentoPack::LicenseManager::isCommercial()) {
      pluginLicenseBadge = tr("KEY_PLUGINS_LIC_COMMERCIAL");
  } else {
      pluginLicenseBadge = tr("KEY_PLUGINS_LIC_COMMUNITY");
  }

  // Section Codecs d'Exportation
  html += QStringLiteral(
      "  <div style='background: %1; border: 1px solid %2; border-radius: 8px; padding: 14px; margin-bottom: 14px;'>"
      "    <h4 style='color: %3; margin-top: 0; margin-bottom: 8px;'>%4</h4>"
      "    <table style='width: 100%; border-collapse: collapse; font-size: 12px; color: %5;'>"
      "      <tr style='border-bottom: 1px solid %2; text-align: left;'>"
      "        <th style='padding: 4px 6px; color: %6;'>%7</th>"
      "        <th style='padding: 4px 6px; color: %6;'>%8</th>"
      "        <th style='padding: 4px 6px; color: %6;'>%9</th>"
      "      </tr>"
  ).arg(colors.cardBg, colors.cardBorder, colors.headingColor, tr("KEY_PLUGINS_CODECS_TITLE"),
        colors.textColor, colors.textMuted, tr("KEY_PLUGINS_COL_FORMAT"), tr("KEY_PLUGINS_COL_ID"), tr("KEY_PLUGINS_COL_LICENSE"));

  const auto &extractors = ExtractorRegistry::instance().extractors();
  for (Extractor *ext : extractors) {
      if (!ext) continue;
      html += QStringLiteral(
          "      <tr style='border-bottom: 1px solid %1;'>"
          "        <td style='padding: 5px 6px;'><b>%2</b></td>"
          "        <td style='padding: 5px 6px;'><code>%3</code></td>"
          "        <td style='padding: 5px 6px;'>%4</td>"
          "      </tr>"
      ).arg(colors.cardBorder, ext->displayName(), ext->id(), pluginLicenseBadge);
  }
  html += QStringLiteral("    </table>\n  </div>\n");

  // Section Filtres Graphiques
  html += QStringLiteral(
      "  <div style='background: %1; border: 1px solid %2; border-radius: 8px; padding: 14px;'>"
      "    <h4 style='color: %3; margin-top: 0; margin-bottom: 8px;'>%4</h4>"
      "    <table style='width: 100%; border-collapse: collapse; font-size: 12px; color: %5;'>"
      "      <tr style='border-bottom: 1px solid %2; text-align: left;'>"
      "        <th style='padding: 4px 6px; color: %6;'>%7</th>"
      "        <th style='padding: 4px 6px; color: %6;'>%8</th>"
      "        <th style='padding: 4px 6px; color: %6;'>%9</th>"
      "      </tr>"
  ).arg(colors.cardBg, colors.cardBorder, colors.headingColor, tr("KEY_PLUGINS_FILTERS_TITLE"),
        colors.textColor, colors.textMuted, tr("KEY_PLUGINS_COL_FILTER"), tr("KEY_PLUGINS_COL_CATEGORY"), tr("KEY_PLUGINS_COL_LICENSE"));

  const auto &filters = FilterRegistry::instance().filters();
  for (FilterPlugin *flt : filters) {
      if (!flt) continue;
      html += QStringLiteral(
          "      <tr style='border-bottom: 1px solid %1;'>"
          "        <td style='padding: 5px 6px;'><b>%2</b></td>"
          "        <td style='padding: 5px 6px;'>%3</td>"
          "        <td style='padding: 5px 6px;'>%4</td>"
          "      </tr>"
      ).arg(colors.cardBorder, flt->name(), flt->category(), pluginLicenseBadge);
  }
  html += QStringLiteral("    </table>\n  </div>\n</div>");

  pluginsText->setHtml(html);
}

void AboutDialog::loadCredits()
{
  ThemeColors colors = getThemeColors();

  QString creditsHtml = QString(
      "<div style='padding: 10px; font-family: sans-serif; color: %1;'>"
      "  <div style='text-align: center; margin-bottom: 20px;'>"
      "    <h3 style='color: %2; margin-bottom: 6px;'>%3</h3>"
      "  </div>"
      "  <div style='background: %4; border: 1px solid %5; border-radius: 8px; padding: 14px 16px; margin-bottom: 14px;'>"
      "    <h4 style='color: %2; margin-top: 0; margin-bottom: 6px;'>%6</h4>"
      "    <p style='margin: 0; font-size: 13px;'>%7</p>"
      "  </div>"
      "  <div style='background: %4; border: 1px solid %5; border-radius: 8px; padding: 14px 16px; margin-bottom: 14px;'>"
      "    <h4 style='color: %2; margin-top: 0; margin-bottom: 8px;'>%8</h4>"
      "    <ul style='margin: 0; padding-left: 18px; font-size: 12px; color: %1;'>"
      "      <li style='margin-bottom: 4px;'><b>Qt Framework :</b> Version %9 (LGPLv3 / Commercial) <a style='color: #38bdf8;' href='https://www.qt.io'>https://www.qt.io</a></li>"
      "      <li style='margin-bottom: 4px;'><b>LibGit2 :</b> Moteur Git natif pour l'historique et les snapshots de versions</li>"
      "      <li style='margin-bottom: 4px;'><b>Basis Universal :</b> Encodeur GPU / VRAM (Khronos KTX2 / UASTC / ETC1S)</li>"
      "      <li style='margin-bottom: 4px;'><b>Zstandard (zstd) :</b> Algorithme de super-compression de textures</li>"
      "      <li style='margin-bottom: 4px;'><b>Miniz :</b> Compression & décompression ZIP sans dépendance externe</li>"
      "      <li style='margin-bottom: 4px;'><b>Compilateur C++ :</b> %10 %11 (%12)</li>"
      "      <li style='margin-bottom: 4px;'><b>Système de build :</b> CMake %13</li>"
      "    </ul>"
      "  </div>"
      "  <div style='background: %4; border: 1px solid %5; border-radius: 8px; padding: 14px 16px;'>"
      "    <h4 style='color: %2; margin-top: 0; margin-bottom: 6px;'>%14</h4>"
      "    <p style='margin: 0; font-size: 13px; color: %15;'>%16</p>"
      "  </div>"
      "</div>"
  )
  .arg(colors.textColor)
  .arg(colors.headingColor)
  .arg(tr("KEY_ABOUT_CREDITS_AND_GREETINGS"))
  .arg(colors.cardBg)
  .arg(colors.cardBorder)
  .arg(tr("_contributors"))
  .arg(GIT_AUTHORS)
  .arg(tr("_used_techno"))
  .arg(QT_VERSION_STR)
  .arg(CMAKE_CXX_COMPILER_ID)
  .arg(CMAKE_CXX_COMPILER)
  .arg(CMAKE_CXX_COMPILER_VERSION)
  .arg(CMAKE_VERSION)
  .arg(tr("_greetings_title"))
  .arg(colors.textMuted)
  .arg(tr("_greetings"));

  creditsText->setHtml(creditsHtml);
}

QString AboutDialog::readTextFile(const QString &filePath)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return QString();
  }
  QTextStream in(&file);
  return in.readAll();
}

void AboutDialog::loadLicense()
{
  ThemeColors colors = getThemeColors();

  QString pluginLicenseContent = readTextFile(QStringLiteral(":/text/LICENSE-PLUGINS.md"));
  if (pluginLicenseContent.isEmpty()) {
      pluginLicenseContent = readTextFile(QStringLiteral(":/text/LICENSE-PLUGINS"));
  }
  if (pluginLicenseContent.isEmpty()) {
      pluginLicenseContent = readTextFile(QStringLiteral("../plugins/LICENSE-PLUGINS.md"));
  }

  QString coreLicenseContent = readTextFile(QStringLiteral(":/text/LICENSE"));
  if (coreLicenseContent.isEmpty()) {
      coreLicenseContent = readTextFile(QStringLiteral(":/text/license.txt"));
  }

  QString licenseHtml = QString(
      "<div style='padding: 10px; font-family: sans-serif; font-size: 12px; color: %1;'>"
      "  <div style='background: %2; border: 1px solid %3; border-radius: 8px; padding: 14px 16px; margin-bottom: 16px;'>"
      "    <h4 style='color: %4; margin-top: 0; margin-bottom: 6px;'>%5</h4>"
      "    <p style='color: %6; font-size: 11px; margin: 0 0 10px 0;'>%7</p>"
      "    <pre style='background: %8; border: 1px solid %3; border-radius: 6px; padding: 12px; color: %1; font-family: monospace; font-size: 11px; white-space: pre-wrap; word-wrap: break-word; margin: 0;'>%9</pre>"
      "  </div>"
      "  <hr>"
      "  <div style='background: %2; border: 1px solid %3; border-radius: 8px; padding: 14px 16px;'>"
      "    <h4 style='color: %4; margin-top: 0; margin-bottom: 6px;'>%10</h4>"
      "    <p style='color: %6; font-size: 11px; margin: 0 0 10px 0;'>%11</p>"
      "    <pre style='background: %8; border: 1px solid %3; border-radius: 6px; padding: 12px; color: %1; font-family: monospace; font-size: 11px; white-space: pre-wrap; word-wrap: break-word; margin: 0;'>%12</pre>"
      "  </div>"
      "</div>"
  )
  .arg(colors.textColor)
  .arg(colors.cardBg)
  .arg(colors.cardBorder)
  .arg(colors.headingColor)
  .arg(tr("KEY_LICENSE_PLUGINS_TITLE"))
  .arg(colors.textMuted)
  .arg(tr("KEY_LICENSE_PLUGINS_NOTICE"))
  .arg(colors.paneBg)
  .arg(pluginLicenseContent.toHtmlEscaped())
  .arg(tr("KEY_LICENSE_CORE_TITLE"))
  .arg(tr("KEY_LICENSE_CORE_NOTICE"))
  .arg(coreLicenseContent.toHtmlEscaped());

  licenseText->setHtml(licenseHtml);
}
