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
