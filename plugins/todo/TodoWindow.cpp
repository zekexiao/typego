#include "TodoWindow.h"
#include "TodoEditorDialog.h"
#include <algorithm>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLocale>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int ColumnTitle = 0;
constexpr int ColumnPriority = 1;
constexpr int ColumnDue = 2;
constexpr int RoleId = Qt::UserRole;
}

TodoWindow::TodoWindow(const QString &storageDirectory, QWidget *parent)
    : QWidget(parent), m_store(storageDirectory + QStringLiteral("/todos.json"))
{
    // 目录不存在时保存会失败，先建好。
    QDir().mkpath(storageDirectory);
    QString error;
    if (!m_store.load(&error))
        qWarning("Todo: cannot load store: %s", qPrintable(error));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(8);
    m_filter = new QComboBox(this);
    m_filter->addItem(tr("All"));
    m_filter->addItem(tr("Active"));
    m_filter->addItem(tr("Completed"));
    connect(m_filter, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_filterIndex = index;
        reload();
    });
    topRow->addWidget(m_filter);
    topRow->addStretch(1);
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("todoSummary"));
    topRow->addWidget(m_summary);
    layout->addLayout(topRow);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("todoTree"));
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setHeaderLabels({tr("Task"), tr("Priority"), tr("Due")});
    m_tree->header()->setSectionResizeMode(ColumnTitle, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(ColumnPriority, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(ColumnDue, QHeaderView::ResizeToContents);
    // 勾选即完成；双击或回车编辑，Del 删除，落盘都走 id 查找不受排序影响。
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *entry, int column) {
        if (m_updating || column != ColumnTitle) return;
        const QString id = entry->data(ColumnTitle, RoleId).toString();
        for (auto &item : m_store.items()) {
            if (item.id != id) continue;
            if (item.done == (entry->checkState(ColumnTitle) == Qt::Checked)) return;
            item.done = entry->checkState(ColumnTitle) == Qt::Checked;
            persist();
            reload();
            return;
        }
    });
    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *, int) { editItem(); });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int) { editItem(); });
    layout->addWidget(m_tree, 1);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    auto *addButton = new QPushButton(tr("Add"), this);
    addButton->setObjectName(QStringLiteral("primary"));
    connect(addButton, &QPushButton::clicked, this, &TodoWindow::addItem);
    auto *editButton = new QPushButton(tr("Edit"), this);
    connect(editButton, &QPushButton::clicked, this, &TodoWindow::editItem);
    auto *removeButton = new QPushButton(tr("Delete"), this);
    connect(removeButton, &QPushButton::clicked, this, &TodoWindow::removeItem);
    auto *clearButton = new QPushButton(tr("Clear completed"), this);
    connect(clearButton, &QPushButton::clicked, this, &TodoWindow::clearDone);
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(editButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(clearButton);
    layout->addLayout(buttonRow);

    reload();
}

// 用存储数据整体重建列表；条目少，排序重建的开销可以忽略。
void TodoWindow::reload()
{
    m_updating = true;
    m_tree->clear();
    const auto items = TodoStore::sorted(m_store.items());
    for (const auto &item : items) {
        if (m_filterIndex == 1 && item.done) continue;
        if (m_filterIndex == 2 && !item.done) continue;
        auto *entry = new QTreeWidgetItem(m_tree);
        entry->setFlags(entry->flags() | Qt::ItemIsUserCheckable);
        entry->setCheckState(ColumnTitle, item.done ? Qt::Checked : Qt::Unchecked);
        QString title = item.title;
        if (item.due.isValid() && item.due < QDateTime::currentDateTime() && !item.done)
            title.prepend(tr("Overdue · "));
        entry->setText(ColumnTitle, title);
        entry->setText(ColumnPriority, priorityText(item.priority));
        if (item.due.isValid())
            entry->setText(ColumnDue, QLocale::system().toString(item.due,
                item.due.date() == QDate::currentDate()
                    ? QStringLiteral("HH:mm") : QStringLiteral("yyyy-MM-dd HH:mm")));
        entry->setData(ColumnTitle, RoleId, item.id);
        QFont font = entry->font(ColumnTitle);
        font.setWeight(item.done ? QFont::Normal : QFont::DemiBold);
        font.setStrikeOut(item.done);
        entry->setFont(ColumnTitle, font);
    }
    m_updating = false;
    updateSummary();
}

void TodoWindow::persist()
{
    QString error;
    if (!m_store.save(&error))
        qWarning("Todo: cannot save store: %s", qPrintable(error));
}

void TodoWindow::updateSummary()
{
    int active = 0;
    for (const auto &item : m_store.items())
        if (!item.done) ++active;
    m_summary->setText(tr("%n task(s) pending", "", active));
}

QString TodoWindow::priorityText(int priority) const
{
    switch (priority) {
    case 2: return tr("High");
    case 0: return tr("Low");
    default: return tr("Medium");
    }
}

void TodoWindow::addItem()
{
    TodoEditorDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;
    TodoItem item = dialog.item();
    item.id = TodoStore::newId();
    m_store.items().append(item);
    persist();
    reload();
}

void TodoWindow::editItem()
{
    auto *entry = m_tree->currentItem();
    if (!entry) return;
    const QString id = entry->data(ColumnTitle, RoleId).toString();
    int row = -1;
    for (int index = 0; index < m_store.items().size(); ++index)
        if (m_store.items().at(index).id == id) { row = index; break; }
    if (row < 0) return;

    TodoEditorDialog dialog(this);
    dialog.setItem(m_store.items().at(row));
    if (dialog.exec() != QDialog::Accepted) return;
    const TodoItem updated = dialog.item();
    m_store.items()[row].title = updated.title;
    m_store.items()[row].priority = updated.priority;
    m_store.items()[row].due = updated.due;
    persist();
    reload();
}

void TodoWindow::removeItem()
{
    auto *entry = m_tree->currentItem();
    if (!entry) return;
    const QString id = entry->data(ColumnTitle, RoleId).toString();
    auto &items = m_store.items();
    for (int index = items.size() - 1; index >= 0; --index)
        if (items.at(index).id == id) items.removeAt(index);
    persist();
    reload();
}

void TodoWindow::clearDone()
{
    auto &items = m_store.items();
    for (int index = items.size() - 1; index >= 0; --index)
        if (items.at(index).done) items.removeAt(index);
    persist();
    reload();
}
