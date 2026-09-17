#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <memory>
#include "extractor/export.h"
#include "model/spritedocument.h"

namespace Ui {
class ExportDialog;
}

class QTimer;

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(const SpriteDocument *document, const QString &defaultPath = QString(), QWidget *parent = nullptr);
    ~ExportDialog() override;

    QString exportFilePath() const;
    ExportOptions exportOptions() const;

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void onBrowseClicked();
    void updateStats();
    void onFormatChanged(int index);

private:
    std::unique_ptr<Ui::ExportDialog> ui;
    const SpriteDocument *m_document = nullptr;
    QTimer *m_debounceTimer = nullptr;
};

#endif // EXPORTDIALOG_H
