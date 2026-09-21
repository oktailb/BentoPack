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

#include "include/widgets/branchselectiondialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QFont>
#include <QDateTime>

BranchSelectionDialog::BranchSelectionDialog(const QList<GitCommitInfo> &branches, QWidget *parent)
    : QDialog(parent)
    , m_branches(branches)
{
    setWindowTitle(tr("Redo — Select Branch"));
    setModal(true);
    resize(500, 320);

    setupUi();
}

void BranchSelectionDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    QLabel *headerLabel = new QLabel(
        tr("Multiple branches diverge from the current revision.\nChoose which branch to restore:"),
        this
    );
    headerLabel->setWordWrap(true);
    QFont hFont = headerLabel->font();
    hFont.setBold(true);
    headerLabel->setFont(hFont);
    mainLayout->addWidget(headerLabel);

    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setSpacing(4);

    for (const GitCommitInfo &b : m_branches) {
        QListWidgetItem *item = new QListWidgetItem(m_listWidget);
        item->setData(Qt::UserRole, b.hash);

        QWidget *rowWidget = new QWidget(m_listWidget);
        QVBoxLayout *rowLayout = new QVBoxLayout(rowWidget);
        rowLayout->setContentsMargins(6, 4, 6, 4);
        rowLayout->setSpacing(2);

        // Top line: Hash badge + Message
        QHBoxLayout *topLine = new QHBoxLayout();
        topLine->setContentsMargins(0, 0, 0, 0);
        topLine->setSpacing(6);

        QLabel *hashLabel = new QLabel(QStringLiteral("[%1]").arg(b.shortHash), rowWidget);
        hashLabel->setStyleSheet(QStringLiteral("color: #3498db; font-family: monospace; font-weight: bold;"));
        topLine->addWidget(hashLabel);

        QLabel *msgLabel = new QLabel(b.message.isEmpty() ? tr("(No message)") : b.message, rowWidget);
        QFont msgFont = msgLabel->font();
        msgFont.setBold(true);
        msgLabel->setFont(msgFont);
        topLine->addWidget(msgLabel, 1);
        rowLayout->addLayout(topLine);

        // Bottom line: Timestamp and author
        QString metaText = QStringLiteral("%1 • %2").arg(
            b.timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
            b.author.isEmpty() ? tr("Unknown") : b.author
        );
        QLabel *metaLabel = new QLabel(metaText, rowWidget);
        metaLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
        rowLayout->addWidget(metaLabel);

        item->setSizeHint(rowWidget->sizeHint());
        m_listWidget->setItemWidget(item, rowWidget);
    }

    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
        m_selectedHash = m_listWidget->currentItem()->data(Qt::UserRole).toString();
    }

    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &BranchSelectionDialog::onSelectionChanged);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &BranchSelectionDialog::onItemDoubleClicked);

    mainLayout->addWidget(m_listWidget, 1);

    // Button Row
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnRestore = new QPushButton(tr("Restore Branch"), this);
    m_btnRestore->setDefault(true);
    m_btnRestore->setStyleSheet(QStringLiteral("font-weight: bold; padding: 6px 14px;"));
    connect(m_btnRestore, &QPushButton::clicked, this, &BranchSelectionDialog::onAccept);
    btnLayout->addWidget(m_btnRestore);

    mainLayout->addLayout(btnLayout);
}

void BranchSelectionDialog::onSelectionChanged()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (item) {
        m_selectedHash = item->data(Qt::UserRole).toString();
        m_btnRestore->setEnabled(true);
    } else {
        m_selectedHash.clear();
        m_btnRestore->setEnabled(false);
    }
}

void BranchSelectionDialog::onItemDoubleClicked()
{
    onAccept();
}

void BranchSelectionDialog::onAccept()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (item) {
        m_selectedHash = item->data(Qt::UserRole).toString();
        accept();
    }
}

QString BranchSelectionDialog::selectBranch(const QList<GitCommitInfo> &branches, QWidget *parent)
{
    if (branches.isEmpty()) return QString();
    if (branches.size() == 1) return branches.first().hash;

    BranchSelectionDialog dlg(branches, parent);
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.selectedCommitHash();
    }
    return QString();
}
