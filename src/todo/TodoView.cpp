#include "TodoView.h"

#include "TaskRepository.h"
#include "TaskService.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int TaskIdRole = Qt::UserRole;
constexpr int CompletedRole = Qt::UserRole + 1;
constexpr int PriorityRole = Qt::UserRole + 2;
constexpr int SeparatorRole = Qt::UserRole + 3;

}

TodoView::TodoView(TaskService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
    , m_input(new QLineEdit(this))
    , m_search(new QLineEdit(this))
    , m_filter(new QComboBox(this))
    , m_summary(new QLabel(this))
    , m_tree(new QTreeWidget(this))
    , m_addButton(new QPushButton(tr("Add"), this))
    , m_detailTitle(new QLabel(tr("Task details"), this))
    , m_titleEdit(new QLineEdit(this))
    , m_noteEdit(new QTextEdit(this))
    , m_priorityCombo(new QComboBox(this))
    , m_completedCheck(new QCheckBox(tr("Completed"), this))
    , m_saveButton(new QPushButton(tr("Save"), this))
    , m_subTaskButton(new QPushButton(tr("Add Subtask"), this))
    , m_deleteButton(new QPushButton(tr("Delete"), this))
    , m_toggleButton(new QPushButton(tr("Complete"), this))
    , m_detailPanel(new QWidget(this))
    , m_detailEditor(new QWidget(this))
    , m_detailActions(new QWidget(this))
    , m_emptyDetailLabel(new QLabel(tr("No task selected"), this))
{
    setObjectName(QStringLiteral("todoView"));

    auto *title = new QLabel(tr("Todo"), this);
    title->setObjectName(QStringLiteral("pageTitle"));

    auto *headerLayout = new QHBoxLayout;
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    m_input->setObjectName(QStringLiteral("todoInput"));
    m_input->setPlaceholderText(tr("Add a task and press Enter"));
    m_addButton->setObjectName(QStringLiteral("primary"));

    auto *inputLayout = new QHBoxLayout;
    inputLayout->setSpacing(8);
    inputLayout->addWidget(m_input, 1);
    inputLayout->addWidget(m_addButton);

    m_search->setObjectName(QStringLiteral("todoSearch"));
    m_search->setPlaceholderText(tr("Search tasks"));
    m_filter->addItem(tr("Active"), static_cast<int>(Filter::Active));
    m_filter->addItem(tr("All"), static_cast<int>(Filter::All));
    m_filter->addItem(tr("Completed"), static_cast<int>(Filter::Completed));
    m_filter->addItem(tr("High priority"), static_cast<int>(Filter::HighPriority));
    m_summary->setObjectName(QStringLiteral("muted"));

    auto *filterLayout = new QHBoxLayout;
    filterLayout->setSpacing(8);
    filterLayout->addWidget(m_search, 1);
    filterLayout->addWidget(m_filter);

    m_tree->setObjectName(QStringLiteral("todoTree"));
    m_tree->setColumnCount(1);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(22);
    m_tree->setAnimated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setRootIsDecorated(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->viewport()->installEventFilter(this);

    auto *leftPanel = new QWidget(this);
    leftPanel->setObjectName(QStringLiteral("todoListPanel"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(12);
    leftLayout->addLayout(inputLayout);
    leftLayout->addLayout(filterLayout);
    leftLayout->addWidget(m_tree, 1);
    leftLayout->addWidget(m_summary);

    m_detailTitle->setObjectName(QStringLiteral("pageTitle"));
    m_detailTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_titleEdit->setObjectName(QStringLiteral("todoTitleEdit"));
    m_noteEdit->setObjectName(QStringLiteral("todoNoteEdit"));
    m_noteEdit->setMinimumHeight(150);
    m_saveButton->setObjectName(QStringLiteral("primary"));
    m_deleteButton->setObjectName(QStringLiteral("danger"));
    m_priorityCombo->addItem(tr("Normal"), 0);
    m_priorityCombo->addItem(tr("High"), 1);
    m_priorityCombo->addItem(tr("Urgent"), 2);

    auto *detailForm = new QFormLayout;
    detailForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    detailForm->setContentsMargins(0, 0, 0, 0);
    detailForm->setFormAlignment(Qt::AlignTop);
    detailForm->setHorizontalSpacing(12);
    detailForm->setVerticalSpacing(12);
    detailForm->addRow(tr("Title"), m_titleEdit);
    detailForm->addRow(tr("Priority"), m_priorityCombo);
    detailForm->addRow(QString(), m_completedCheck);
    detailForm->addRow(tr("Note"), m_noteEdit);
    m_detailEditor->setObjectName(QStringLiteral("todoDetailEditor"));
    m_detailEditor->setLayout(detailForm);

    auto *detailActionsLayout = new QHBoxLayout(m_detailActions);
    detailActionsLayout->setContentsMargins(0, 0, 0, 0);
    detailActionsLayout->setSpacing(8);
    detailActionsLayout->addWidget(m_saveButton);
    detailActionsLayout->addWidget(m_toggleButton);
    detailActionsLayout->addWidget(m_subTaskButton);
    detailActionsLayout->addStretch();
    detailActionsLayout->addWidget(m_deleteButton);

    m_emptyDetailLabel->setObjectName(QStringLiteral("todoEmptyDetailLabel"));
    m_emptyDetailLabel->setAlignment(Qt::AlignCenter);

    auto *detailHeader = new QHBoxLayout;
    detailHeader->setContentsMargins(0, 0, 0, 0);
    detailHeader->addWidget(m_detailTitle, 1);

    m_detailPanel->setObjectName(QStringLiteral("todoDetailPanel"));
    auto *detailLayout = new QVBoxLayout(m_detailPanel);
    detailLayout->setContentsMargins(16, 16, 16, 16);
    detailLayout->setSpacing(12);
    detailLayout->addLayout(detailHeader);
    detailLayout->addWidget(m_emptyDetailLabel, 1);
    detailLayout->addWidget(m_detailEditor);
    detailLayout->addWidget(m_detailActions);
    detailLayout->addStretch();

    auto *contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(16);
    contentLayout->addWidget(leftPanel, 5);
    contentLayout->addWidget(m_detailPanel, 3);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addLayout(headerLayout);
    layout->addLayout(contentLayout, 1);

    connect(m_addButton, &QPushButton::clicked, this, &TodoView::addTask);
    connect(m_input, &QLineEdit::returnPressed, this, &TodoView::addTask);
    connect(m_search, &QLineEdit::textChanged, this, [this]() { rebuildTree(); });
    connect(m_filter, &QComboBox::currentIndexChanged, this, [this]() { rebuildTree(); });
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this]() { updateDetails(); });
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        if (m_rebuilding || !item) return;
        const QString id = item->data(0, TaskIdRole).toString();
        if (column == 0) {
            const bool completed = item->checkState(0) == Qt::Checked;
            if (completed != item->data(0, CompletedRole).toBool()) {
                m_service->setCompleted(id, completed);
            } else {
                m_service->updateTitle(id, item->text(0));
            }
            rebuildTree();
        }
    });
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTreeWidgetItem *item = m_tree->itemAt(pos);
        if (!item) return;
        m_tree->setCurrentItem(item);
        QMenu menu(this);
        QAction *completeAction = menu.addAction(item->checkState(0) == Qt::Checked ? tr("Mark Active") : tr("Mark Complete"));
        QAction *subTaskAction = menu.addAction(tr("Add Subtask"));
        QAction *deleteAction = menu.addAction(tr("Delete"));
        QAction *selected = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (selected == completeAction) toggleSelectedComplete();
        if (selected == subTaskAction) addSubTask();
        if (selected == deleteAction) removeSelectedTask();
    });
    connect(m_saveButton, &QPushButton::clicked, this, &TodoView::saveDetails);
    connect(m_subTaskButton, &QPushButton::clicked, this, &TodoView::addSubTask);
    connect(m_deleteButton, &QPushButton::clicked, this, &TodoView::removeSelectedTask);
    connect(m_toggleButton, &QPushButton::clicked, this, &TodoView::toggleSelectedComplete);
    connect(m_titleEdit, &QLineEdit::returnPressed, this, &TodoView::saveDetails);
    setDetailsEnabled(false);
    reload();
}

void TodoView::reload()
{
    rebuildTree();
    m_input->setFocus();
}

bool TodoView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tree->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && !m_tree->itemAt(mouseEvent->position().toPoint())) {
            m_tree->clearSelection();
            m_tree->setCurrentItem(nullptr);
            updateDetails();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TodoView::addTask()
{
    const QString title = m_input->text().trimmed();
    if (title.isEmpty()) return;
    const QString id = m_service->createTask(title);
    m_input->clear();
    rebuildTree();
    const QList<QTreeWidgetItem *> matches = m_tree->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive, 0);
    for (QTreeWidgetItem *item : matches) {
        if (item->data(0, TaskIdRole).toString() == id) {
            m_tree->setCurrentItem(item);
            break;
        }
    }
}

void TodoView::addSubTask()
{
    const QString parentId = selectedTaskId();
    if (parentId.isEmpty()) return;
    const QString id = m_service->createSubTask(parentId, tr("New Subtask"));
    if (id.isEmpty()) return;
    rebuildTree();
}

void TodoView::removeSelectedTask()
{
    const QString id = selectedTaskId();
    if (id.isEmpty()) return;
    m_service->deleteTask(id);
    rebuildTree();
}

void TodoView::toggleSelectedComplete()
{
    const Task task = selectedTask();
    if (task.id.isEmpty()) return;
    m_service->setCompleted(task.id, !task.completed);
    rebuildTree();
}

void TodoView::saveDetails()
{
    if (m_loadingDetails) return;
    const QString id = selectedTaskId();
    if (id.isEmpty()) return;

    const Task current = selectedTask();
    m_service->updateTitle(id, m_titleEdit->text());
    m_service->updateNote(id, m_noteEdit->toPlainText());
    m_service->updatePriority(id, m_priorityCombo->currentData().toInt());
    if (current.completed != m_completedCheck->isChecked()) {
        m_service->setCompleted(id, m_completedCheck->isChecked());
    }
    rebuildTree();
}

void TodoView::rebuildTree()
{
    const QString currentId = selectedTaskId();
    m_rebuilding = true;
    m_tree->setUpdatesEnabled(false);
    {
        QSignalBlocker blocker(m_tree);
        const QList<Task> tasks = m_service->repository()->tasks();
        ChildrenByParent childrenByParent;
        childrenByParent.reserve(tasks.size());
        for (const Task &task : tasks) {
            childrenByParent[task.parentId].append(task);
        }
        for (auto it = childrenByParent.begin(); it != childrenByParent.end(); ++it) {
            std::stable_sort(it.value().begin(), it.value().end(), [](const Task &a, const Task &b) {
                if (a.completed != b.completed) return !a.completed;
                if (a.priority != b.priority) return a.priority > b.priority;
                if (a.createdTime != b.createdTime) return a.createdTime < b.createdTime;
                return a.id < b.id;
            });
        }

        m_tree->clear();
        appendChildren(nullptr, QString(), childrenByParent);
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            applyFilter(m_tree->topLevelItem(i));
        }
        m_tree->expandAll();
    }
    m_tree->setUpdatesEnabled(true);
    m_rebuilding = false;

    if (!currentId.isEmpty()) {
        const QList<QTreeWidgetItem *> items = m_tree->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive, 0);
        for (QTreeWidgetItem *item : items) {
            if (item->data(0, TaskIdRole).toString() == currentId) {
                m_tree->setCurrentItem(item);
                break;
            }
        }
    }
    updateSummary();
    updateDetails();
}

void TodoView::appendChildren(QTreeWidgetItem *parentItem, const QString &parentId, const ChildrenByParent &childrenByParent)
{
    const auto children = childrenByParent.constFind(parentId);
    if (children == childrenByParent.cend()) return;

    bool completedSeparatorAdded = false;
    const bool canShowCompletedSeparator = parentId.isEmpty()
        && static_cast<Filter>(m_filter->currentData().toInt()) == Filter::All
        && m_search->text().trimmed().isEmpty();

    for (const Task &task : children.value()) {
        if (canShowCompletedSeparator && task.completed && !completedSeparatorAdded) {
            auto *separator = new QTreeWidgetItem;
            separator->setText(0, tr("Completed"));
            separator->setData(0, SeparatorRole, true);
            separator->setFlags(Qt::NoItemFlags);
            separator->setForeground(0, palette().placeholderText());
            m_tree->addTopLevelItem(separator);
            completedSeparatorAdded = true;
        }

        auto *item = new QTreeWidgetItem;
        const QStringList meta = {priorityText(task.priority)};
        QStringList visibleMeta;
        for (const QString &part : meta) {
            if (!part.isEmpty() && part != tr("Normal")) visibleMeta.append(part);
        }
        // 优先级后缀用中点分隔，与搜索结果保持一致。
        item->setText(0, visibleMeta.isEmpty()
                         ? task.title
                         : tr("%1  ·  %2").arg(task.title, visibleMeta.join(QStringLiteral(" · "))));
        item->setData(0, TaskIdRole, task.id);
        item->setData(0, CompletedRole, task.completed);
        item->setData(0, PriorityRole, task.priority);
        item->setCheckState(0, task.completed ? Qt::Checked : Qt::Unchecked);
        item->setFlags((item->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsUserCheckable);
        if (task.completed) item->setForeground(0, palette().placeholderText());
        else if (task.priority == 2) item->setForeground(0, QColor(QStringLiteral("#dc2626")));
        else if (task.priority == 1) item->setForeground(0, QColor(QStringLiteral("#d97706")));
        parentItem ? parentItem->addChild(item) : m_tree->addTopLevelItem(item);
        appendChildren(item, task.id, childrenByParent);
    }
}

bool TodoView::applyFilter(QTreeWidgetItem *item)
{
    if (item->data(0, SeparatorRole).toBool()) {
        item->setHidden(static_cast<Filter>(m_filter->currentData().toInt()) != Filter::All);
        return !item->isHidden();
    }

    bool childVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        childVisible = applyFilter(item->child(i)) || childVisible;
    }
    const Task task = m_service->repository()->find(item->data(0, TaskIdRole).toString());
    const bool selfVisible = taskMatchesFilter(task) && taskMatchesSearch(task);
    item->setHidden(!selfVisible && !childVisible);
    return selfVisible || childVisible;
}

bool TodoView::taskMatchesFilter(const Task &task) const
{
    const Filter filter = static_cast<Filter>(m_filter->currentData().toInt());
    if (filter == Filter::All) return true;
    if (filter == Filter::Active) return !task.completed;
    if (filter == Filter::Completed) return task.completed;
    if (filter == Filter::HighPriority) return task.priority > 0;
    return true;
}

bool TodoView::taskMatchesSearch(const Task &task) const
{
    const QString query = m_search->text().trimmed();
    if (query.isEmpty()) return true;
    return task.title.contains(query, Qt::CaseInsensitive)
        || task.note.contains(query, Qt::CaseInsensitive);
}

void TodoView::updateSummary()
{
    const QList<Task> tasks = m_service->repository()->tasks();
    int active = 0;
    for (const Task &task : tasks) {
        if (!task.completed) ++active;
    }
    m_summary->setText(tr("%1 active | %2 total").arg(active).arg(tasks.size()));
}

void TodoView::updateDetails()
{
    const Task task = selectedTask();
    m_loadingDetails = true;
    if (task.id.isEmpty()) {
        m_detailTitle->setText(tr("Task details"));
        m_emptyDetailLabel->show();
        m_detailEditor->hide();
        m_detailActions->hide();
        m_titleEdit->clear();
        m_noteEdit->clear();
        m_priorityCombo->setCurrentIndex(0);
        m_completedCheck->setChecked(false);
        m_toggleButton->setText(tr("Complete"));
        setDetailsEnabled(false);
        m_loadingDetails = false;
        return;
    }

    m_emptyDetailLabel->hide();
    m_detailEditor->show();
    m_detailActions->show();
    setDetailsEnabled(true);
    m_detailTitle->setText(task.title);
    m_titleEdit->setText(task.title);
    m_noteEdit->setPlainText(task.note);
    const int priorityIndex = m_priorityCombo->findData(task.priority);
    m_priorityCombo->setCurrentIndex(priorityIndex >= 0 ? priorityIndex : 0);
    m_completedCheck->setChecked(task.completed);
    m_toggleButton->setText(task.completed ? tr("Mark Active") : tr("Complete"));
    m_loadingDetails = false;
}

void TodoView::setDetailsEnabled(bool enabled)
{
    m_titleEdit->setEnabled(enabled);
    m_noteEdit->setEnabled(enabled);
    m_priorityCombo->setEnabled(enabled);
    m_completedCheck->setEnabled(enabled);
    m_saveButton->setEnabled(enabled);
    m_subTaskButton->setEnabled(enabled);
    m_deleteButton->setEnabled(enabled);
    m_toggleButton->setEnabled(enabled);
}

QString TodoView::selectedTaskId() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    return item ? item->data(0, TaskIdRole).toString() : QString();
}

Task TodoView::selectedTask() const
{
    return m_service->repository()->find(selectedTaskId());
}

QString TodoView::priorityText(int priority) const
{
    if (priority >= 2) return tr("Urgent");
    if (priority == 1) return tr("High");
    return tr("Normal");
}
