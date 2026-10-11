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

#include "include/widgets/openmediaconflictdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>

OpenMediaConflictDialog::OpenMediaConflictDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent)
    , m_filePath(filePath)
{
    setWindowTitle(tr("Open Media — Project Already Active"));
    setModal(true);
    setFixedWidth(560);

    setupUi();
    adjustSize();
}

void OpenMediaConflictDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 18);
    mainLayout->setSpacing(14);

    // Header Title
    QLabel *titleLabel = new QLabel(tr("A project is currently open"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // Subtitle with filename
    QString baseName = QFileInfo(m_filePath).fileName();
    QLabel *subLabel = new QLabel(
        tr("How would you like to handle the incoming media <b>%1</b>?").arg(baseName),
        this
    );
    subLabel->setWordWrap(true);
    subLabel->setStyleSheet(QStringLiteral("color: palette(window-text); margin-bottom: 2px;"));
    mainLayout->addWidget(subLabel);

    // 1. Close Existing & Open as New
    QPushButton *btn1 = createChoiceButton(
        QStringLiteral("1"),
        tr("Close Existing & Open as New"),
        tr("Closes current project (prompts to save if modified) and opens this file as a brand new project."),
        QStringLiteral("#3498db")
    );
    btn1->setObjectName(QStringLiteral("btnChoiceCloseAndOpen"));
    connect(btn1, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::CloseAndOpenNew;
        accept();
    });
    mainLayout->addWidget(btn1);

    // 2. Update / Replace Existing Atlas
    QPushButton *btn2 = createChoiceButton(
        QStringLiteral("2"),
        tr("Update / Replace Existing Atlas"),
        tr("Updates the atlas texture with this media. Preserves your existing sprite boxes, pivots, and animations (re-sliced from the new atlas)."),
        QStringLiteral("#2ecc71")
    );
    btn2->setObjectName(QStringLiteral("btnChoiceReplaceAtlas"));
    connect(btn2, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::ReplaceExistingAtlas;
        accept();
    });
    mainLayout->addWidget(btn2);

    // 3. Add as New Atlas Layer
    QPushButton *btn3 = createChoiceButton(
        QStringLiteral("3"),
        tr("Add as New Atlas Layer"),
        tr("Imports this media as an atlas layer (e.g. normal map, lighting, armor). Slices a cel for each animation frame matching its existing atlas box."),
        QStringLiteral("#9b59b6")
    );
    btn3->setObjectName(QStringLiteral("btnChoiceAddNewLayers"));
    connect(btn3, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::AddNewLayers;
        accept();
    });
    mainLayout->addWidget(btn3);

    // 4. Add as Common Background / Overlay
    QPushButton *btn4 = createChoiceButton(
        QStringLiteral("4"),
        tr("Add as Common Background / Overlay"),
        tr("Places this media as a shared background or overlay cel repeated identically across every animation frame."),
        QStringLiteral("#e67e22")
    );
    btn4->setObjectName(QStringLiteral("btnChoiceAddCommonBackground"));
    connect(btn4, &QPushButton::clicked, this, [this]() {
        m_choice = Choice::AddAsCommonBackground;
        accept();
    });
    mainLayout->addWidget(btn4);

    // Bottom action bar
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();

    QPushButton *btnCancel = new QPushButton(tr("Cancel"), this);
    btnCancel->setObjectName(QStringLiteral("btnCancel"));
    btnCancel->setStyleSheet(QStringLiteral("padding: 7px 20px; min-width: 80px;"));
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(btnCancel);

    mainLayout->addLayout(bottomLayout);
}

QPushButton* OpenMediaConflictDialog::createChoiceButton(const QString &badge,
                                                         const QString &title,
                                                         const QString &description,
                                                         const QString &accentColor)
{
    QPushButton *btn = new QPushButton(this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QHBoxLayout *btnLayout = new QHBoxLayout(btn);
    btnLayout->setContentsMargins(14, 12, 14, 12);
    btnLayout->setSpacing(14);

    QLabel *badgeLabel = new QLabel(badge, btn);
    badgeLabel->setFixedSize(26, 26);
    badgeLabel->setAlignment(Qt::AlignCenter);
    badgeLabel->setStyleSheet(QStringLiteral(
        "background-color: %1; color: white; font-weight: bold; font-size: 13px; border-radius: 13px;"
    ).arg(accentColor));
    badgeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    btnLayout->addWidget(badgeLabel);

    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(3);

    QLabel *lblTitle = new QLabel(title, btn);
    QFont titleFont = lblTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    lblTitle->setFont(titleFont);
    lblTitle->setStyleSheet(QStringLiteral("color: palette(window-text); font-weight: bold;"));
    lblTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    textLayout->addWidget(lblTitle);

    QLabel *lblDesc = new QLabel(description, btn);
    lblDesc->setWordWrap(true);
    lblDesc->setStyleSheet(QStringLiteral("color: palette(placeholder-text); font-size: 11px;"));
    lblDesc->setAttribute(Qt::WA_TransparentForMouseEvents);
    textLayout->addWidget(lblDesc);

    btnLayout->addLayout(textLayout, 1);

    QString qss = QStringLiteral(
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  background: palette(button);"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  border: 1.5px solid %1;"
        "  background: palette(light);"
        "}"
        "QPushButton:pressed {"
        "  background: palette(midlight);"
        "}"
    ).arg(accentColor);
    btn->setStyleSheet(qss);

    return btn;
}
