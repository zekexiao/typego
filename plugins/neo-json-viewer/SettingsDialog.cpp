#include "SettingsDialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>

SettingsDialog::SettingsDialog(int maxTextLength, bool prettyPrint, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Settings"));

    auto *layout = new QFormLayout(this);

    m_maxTextSpin = new QSpinBox(this);
    m_maxTextSpin->setRange(0, 10000000);
    m_maxTextSpin->setValue(maxTextLength);
    m_maxTextSpin->setSuffix(tr(" chars"));
    m_maxTextSpin->setSpecialValueText(tr("Unlimited"));
    layout->addRow(tr("Max text display length:"), m_maxTextSpin);

    m_prettyCheck = new QCheckBox(this);
    m_prettyCheck->setChecked(prettyPrint);
    layout->addRow(tr("Pretty print output:"), m_prettyCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

int SettingsDialog::maxTextLength() const {
    return m_maxTextSpin->value();
}

bool SettingsDialog::prettyPrint() const {
    return m_prettyCheck->isChecked();
}
