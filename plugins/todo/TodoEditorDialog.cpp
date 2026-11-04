#include "TodoEditorDialog.h"
#include "TodoWindow.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

TodoEditorDialog::TodoEditorDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Edit Task"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    auto *form = new QFormLayout;
    form->setSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_title = new QLineEdit(this);
    m_title->setPlaceholderText(tr("Task title"));
    connect(m_title, &QLineEdit::textChanged, this, [this](const QString &text) {
        auto *ok = findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        if (ok) ok->setEnabled(!text.trimmed().isEmpty());
    });
    form->addRow(tr("Task:"), m_title);

    m_priority = new QComboBox(this);
    m_priority->addItem(tr("High"), 2);
    m_priority->addItem(tr("Medium"), 1);
    m_priority->addItem(tr("Low"), 0);
    m_priority->setCurrentIndex(1);
    form->addRow(tr("Priority:"), m_priority);

    m_hasDue = new QCheckBox(tr("Set due time"), this);
    form->addRow(QString(), m_hasDue);

    m_due = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600), this);
    m_due->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_due->setCalendarPopup(true);
    m_due->setEnabled(false);
    connect(m_hasDue, &QCheckBox::toggled, m_due, &QDateTimeEdit::setEnabled);
    form->addRow(tr("Due:"), m_due);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("primary"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void TodoEditorDialog::setItem(const TodoItem &item)
{
    setWindowTitle(item.title.isEmpty() ? tr("Add Task") : tr("Edit Task"));
    m_title->setText(item.title);
    const int index = m_priority->findData(item.priority);
    m_priority->setCurrentIndex(index < 0 ? 1 : index);
    m_hasDue->setChecked(item.due.isValid());
    if (item.due.isValid()) m_due->setDateTime(item.due);
}

TodoItem TodoEditorDialog::item() const
{
    TodoItem result;
    result.title = m_title->text().trimmed();
    result.priority = m_priority->currentData().toInt();
    result.due = m_hasDue->isChecked() ? m_due->dateTime() : QDateTime();
    return result;
}
