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

#include "casm_extractor.h"
#include "casm_dialog.h"
#include "casm_generator.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QApplication>
#include <QThread>
#include <QVBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>

CAsmExtractor::CAsmExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool CAsmExtractor::canDecode(const QString &filePath) const
{
    Q_UNUSED(filePath);
    return false; // Pure exporter plugin
}

bool CAsmExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    Q_UNUSED(filePath);
    Q_UNUSED(outDoc);
    if (error) {
        error->code = ExtractorError::UnsupportedFormat;
        error->message = tr("The C/ASM plugin is an export-only plugin and does not import raw source code into sprite sheets.");
    }
    return false;
}

#include <QMessageBox>
#include <QTimer>

class CAsmExportCancelFilter : public QObject
{
public:
    explicit CAsmExportCancelFilter(QObject *parent) : QObject(parent) {}

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (ev->type() == QEvent::Show) {
            if (QMessageBox *msg = qobject_cast<QMessageBox*>(obj)) {
                msg->hide();
                QTimer::singleShot(0, msg, &QDialog::reject);
                this->deleteLater();
                return true;
            }
        }
        return QObject::eventFilter(obj, ev);
    }
};

bool CAsmExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    if (filePath.isEmpty()) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Export destination path is empty.");
        }
        return false;
    }

    QFileInfo fi(filePath);
    QString baseName = fi.completeBaseName();
    if (baseName.isEmpty()) {
        baseName = inDoc.projectName().isEmpty() ? QStringLiteral("sprites") : inDoc.projectName();
    }

    Retro::CAsmConfig config;
    config.loadFromSettings();

    // Check if dynamic interactive mode is enabled from options or CLI
    bool interactive = options.extraParams.value(QStringLiteral("interactive"), true).toBool();
    bool isGui = (qApp && qobject_cast<QApplication*>(qApp) != nullptr);

    if (isGui && interactive) {
        bool dialogAccepted = false;
        Retro::CAsmConfig userConfig = config;

        auto showDialog = [&]() {
            QWidget *parentWidget = qobject_cast<QApplication*>(qApp)->activeModalWidget();
            if (!parentWidget) parentWidget = qobject_cast<QApplication*>(qApp)->activeWindow();

            Retro::CAsmDialog dlg(inDoc, baseName, parentWidget);
            if (dlg.exec() == QDialog::Accepted) {
                userConfig = dlg.config();
                dialogAccepted = true;
            } else {
                // User cancelled or closed the dialog: absorb the redundant critical error box from ExportDialog
                qApp->installEventFilter(new CAsmExportCancelFilter(qApp));
            }
        };

        if (QThread::currentThread() == qApp->thread()) {
            showDialog();
        } else {
            QMetaObject::invokeMethod(qApp, showDialog, Qt::BlockingQueuedConnection);
        }

        if (!dialogAccepted) {
            if (error) {
                error->code = ExtractorError::WriteFailed;
                error->message = tr("Export cancelled by user.");
            }
            return false;
        }

        config = userConfig;
    }

    // Generate formatted source code
    setProgress(30);
    setStatusMessage(tr("Generating retro source code (%1)...").arg(baseName));

    QString generatedCode = Retro::CAsmGenerator::generate(inDoc, config, baseName);

    setProgress(70);
    setStatusMessage(tr("Writing file %1...").arg(fi.fileName()));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Could not open file for writing: %1 (%2)").arg(filePath, file.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QTextStream out(&file);
    out << generatedCode;
    file.close();

    setProgress(100);
    setStatusMessage(tr("C/ASM code exported successfully to %1.").arg(fi.fileName()));

    return true;
}

QWidget* CAsmExtractor::createSettingsWidget(QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *grp = new QGroupBox(tr("Default Retro Architecture Preset"), w);
    QVBoxLayout *grpLayout = new QVBoxLayout(grp);

    QComboBox *comboPreset = new QComboBox(grp);
    comboPreset->addItem(QStringLiteral("🎮 Sega Mega Drive / Genesis (SGDK - 68000)"), Retro::System_MegaDrive_SGDK);
    comboPreset->addItem(QStringLiteral("🕹️ Nintendo Game Boy / GBC (GBDK / RGBDS - SM83)"), Retro::System_GameBoy_GBDK);
    comboPreset->addItem(QStringLiteral("🍄 Nintendo NES / Famicom (cc65 / ca65 - 6502)"), Retro::System_NES_cc65);
    comboPreset->addItem(QStringLiteral("💾 Commodore Amiga 500/1200 (VASM / NDK - 68000)"), Retro::System_Amiga_OCS);
    comboPreset->addItem(QStringLiteral("🕹️ Atari ST (VASM / GCC - 68000)"), Retro::System_Atari_ST);
    comboPreset->addItem(QStringLiteral("🎮 Super Nintendo / SNES (pvsneslib - 65816)"), Retro::System_SNES_pvsneslib);
    comboPreset->addItem(QStringLiteral("⌨️ Commodore 64 (cc65 / KickAssembler - 6502)"), Retro::System_Commodore_64);
    comboPreset->addItem(QStringLiteral("🎮 Sega Master System / Game Gear (devkitSMS - Z80)"), Retro::System_MasterSystem_devkitSMS);
    comboPreset->addItem(QStringLiteral("📼 Amstrad CPC 464/6128 (cpctelera / RASM - Z80)"), Retro::System_Amstrad_CPC);
    comboPreset->addItem(QStringLiteral("🌈 Sinclair ZX Spectrum (z88dk / SJASMPLUS - Z80)"), Retro::System_ZX_Spectrum);
    comboPreset->addItem(QStringLiteral("💻 MS-DOS PC VGA Mode 13h (Open Watcom - x86)"), Retro::System_MSDOS_Mode13h);
    comboPreset->addItem(QStringLiteral("🐱 PICO-8 Fantasy Console (Lua Hex)"), Retro::System_Pico8);
    comboPreset->addItem(QStringLiteral("⚙️ Generic C/C++ Linear Array"), Retro::System_Generic_C);

    Retro::CAsmConfig cfg;
    cfg.loadFromSettings();
    int idx = comboPreset->findData(cfg.targetSystem);
    if (idx >= 0) comboPreset->setCurrentIndex(idx);

    QObject::connect(comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), [](int index) {
        Retro::CAsmConfig c;
        c.loadFromSettings();
        c.applyPreset(static_cast<Retro::TargetSystem>(index));
        c.saveToSettings();
    });

    grpLayout->addWidget(comboPreset);

    QCheckBox *chkLinter = new QCheckBox(tr("Enable Hardware Constraints Linter (PPU scanlines, VRAM warnings)"), grp);
    chkLinter->setChecked(cfg.includeLinterComments);
    QObject::connect(chkLinter, &QCheckBox::toggled, [](bool checked) {
        Retro::CAsmConfig c;
        c.loadFromSettings();
        c.includeLinterComments = checked;
        c.saveToSettings();
    });
    grpLayout->addWidget(chkLinter);

    layout->addWidget(grp);

    QLabel *info = new QLabel(tr("Exports native C/C++ arrays and pure Assembly data (.h, .c, .asm, .s, .inc) directly usable in homebrew SDKs without game engines."), w);
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
    layout->addWidget(info);

    return w;
}
