#include "AboutDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QIcon>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("About TypeGo.JsonView"));
    setFixedSize(400, 300);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Icon
    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon(":/icon.png").pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(iconLabel);

    // Application name
    auto *nameLabel = new QLabel(tr("<h2>TypeGo.JsonView</h2>"), this);
    nameLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(nameLabel);

    // Version
    auto *versionLabel = new QLabel(tr("Version 1.0.0"), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(versionLabel);

    // Copyright
    auto *copyrightLabel = new QLabel(tr("Copyright 2025. All rights reserved."), this);
    copyrightLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(copyrightLabel);

    mainLayout->addSpacing(10);

    // Open source components
    auto *openSourceLabel = new QLabel(
        tr("This application uses the following open source components:<br>"
           "<b>yyjson</b> - A high performance JSON library<br>"
           "Licensed under the MIT License"),
        this);
    openSourceLabel->setAlignment(Qt::AlignLeft);
    openSourceLabel->setWordWrap(true);
    mainLayout->addWidget(openSourceLabel);

    mainLayout->addStretch();

    // Button
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    mainLayout->addWidget(buttonBox, 0, Qt::AlignCenter);
}
