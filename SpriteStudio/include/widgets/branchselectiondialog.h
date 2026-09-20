#ifndef BRANCHSELECTIONDIALOG_H
#define BRANCHSELECTIONDIALOG_H

#include <QDialog>
#include <QList>
#include "project/sessionmanager.h"

class QListWidget;
class QPushButton;

class BranchSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BranchSelectionDialog(const QList<GitCommitInfo> &branches, QWidget *parent = nullptr);
    ~BranchSelectionDialog() override = default;

    QString selectedCommitHash() const { return m_selectedHash; }

    static QString selectBranch(const QList<GitCommitInfo> &branches, QWidget *parent = nullptr);

private slots:
    void onSelectionChanged();
    void onItemDoubleClicked();
    void onAccept();

private:
    void setupUi();

    QList<GitCommitInfo> m_branches;
    QString              m_selectedHash;
    QListWidget         *m_listWidget = nullptr;
    QPushButton         *m_btnRestore = nullptr;
    QPushButton         *m_btnCancel = nullptr;
};

#endif // BRANCHSELECTIONDIALOG_H
