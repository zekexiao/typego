#include "SearchDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

SearchDialog::SearchDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Find"));
    setModal(false);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *mainLayout = new QVBoxLayout(this);

    auto *findLayout = new QGridLayout();
    auto *findLabel = new QLabel(tr("Find what:"), this);
    m_patternEdit = new QLineEdit(this);
    findLayout->addWidget(findLabel, 0, 0);
    findLayout->addWidget(m_patternEdit, 0, 1);
    mainLayout->addLayout(findLayout);

    auto *directionGroup = new QGroupBox(tr("Direction"), this);
    auto *directionLayout = new QHBoxLayout(directionGroup);
    m_downRadio = new QRadioButton(tr("Down"), directionGroup);
    m_upRadio = new QRadioButton(tr("Up"), directionGroup);
    m_downRadio->setChecked(true);
    directionLayout->addWidget(m_downRadio);
    directionLayout->addWidget(m_upRadio);
    directionLayout->addStretch();
    mainLayout->addWidget(directionGroup);

    m_ignoreCaseCheck = new QCheckBox(tr("Ignore case"), this);
    m_wrapAroundCheck = new QCheckBox(tr("Wrap around"), this);
    m_wrapAroundCheck->setChecked(true);
    m_regexCheck = new QCheckBox(tr("Use regular expression"), this);
    m_searchInValueCheck = new QCheckBox(tr("Search in value"), this);
    mainLayout->addWidget(m_ignoreCaseCheck);
    mainLayout->addWidget(m_wrapAroundCheck);
    mainLayout->addWidget(m_regexCheck);
    mainLayout->addWidget(m_searchInValueCheck);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *findNextButton = buttons->addButton(tr("Find Next"), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    mainLayout->addWidget(buttons);

    connect(findNextButton, &QPushButton::clicked, this, &SearchDialog::onFindNextClicked);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    connect(m_patternEdit, &QLineEdit::returnPressed, this, &SearchDialog::onFindNextClicked);

    resize(380, sizeHint().height());
}

SearchOptions SearchDialog::options() const {
    SearchOptions result;
    result.text = m_patternEdit->text().trimmed();
    result.backward = m_upRadio->isChecked();
    result.ignoreCase = m_ignoreCaseCheck->isChecked();
    result.wrapAround = m_wrapAroundCheck->isChecked();
    result.useRegex = m_regexCheck->isChecked();
    result.searchInValue = m_searchInValueCheck->isChecked();
    return result;
}

void SearchDialog::setOptions(const SearchOptions &options) {
    m_patternEdit->setText(options.text);
    setDirection(options.backward);
    m_ignoreCaseCheck->setChecked(options.ignoreCase);
    m_wrapAroundCheck->setChecked(options.wrapAround);
    m_regexCheck->setChecked(options.useRegex);
    m_searchInValueCheck->setChecked(options.searchInValue);
}

void SearchDialog::setDirection(bool backward) {
    if (backward) {
        m_upRadio->setChecked(true);
    } else {
        m_downRadio->setChecked(true);
    }
}

void SearchDialog::focusPattern() {
    m_patternEdit->setFocus();
    m_patternEdit->selectAll();
}

void SearchDialog::onFindNextClicked() {
    emit findRequested(options());
}
