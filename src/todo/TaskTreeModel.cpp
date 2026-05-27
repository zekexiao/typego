#include "TaskTreeModel.h"
#include "TaskRepository.h"
#include "TaskService.h"
#include "Task.h"
#include <QDebug>

TaskTreeModel::TaskTreeModel(TaskRepository *repo, TaskService *service, QObject *parent)
    : QAbstractItemModel(parent)
    , m_repo(repo)
    , m_service(service)
{
    connect(m_repo, &TaskRepository::taskAdded, this, [this](const QString &id) {
        QString parentId = m_repo->parentIdOf(id);
        int row = m_repo->indexOf(id);
        if (row < 0) {
            beginResetModel();
            endResetModel();
            return;
        }
        QModelIndex parentIndex = parentId.isEmpty() ? QModelIndex() : indexForTask(parentId);
        beginInsertRows(parentIndex, row, row);
        endInsertRows();
    });
    connect(m_repo, &TaskRepository::taskRemoved, this, [this](const QString &id) {
        // 级联删除涉及多行，简单起见仍用 reset
        Q_UNUSED(id)
        beginResetModel();
        endResetModel();
    });
    connect(m_repo, &TaskRepository::taskUpdated, this, [this](const QString &id) {
        QModelIndex idx = indexForTask(id);
        if (idx.isValid()) {
            emit dataChanged(idx, idx, {Qt::CheckStateRole, CompletedRole, TitleRole, NoteRole, PriorityRole});
        }
    });
}

QModelIndex TaskTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0) return QModelIndex();

    QString parentId = parent.isValid() ? static_cast<Node*>(parent.internalPointer())->taskId : QString();
    auto children = m_repo->childrenOf(parentId);
    if (row < 0 || row >= children.size()) return QModelIndex();

    const QString &id = children[row].id;
    Node &node = m_nodes[id];
    node.taskId = id;
    return createIndex(row, column, &node);
}

QModelIndex TaskTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return QModelIndex();

    Node *node = static_cast<Node*>(index.internalPointer());
    QString parentId = m_repo->parentIdOf(node->taskId);
    if (parentId.isEmpty()) return QModelIndex();

    QString grandParentId = m_repo->parentIdOf(parentId);
    auto siblings = m_repo->childrenOf(grandParentId);
    int row = -1;
    for (int i = 0; i < siblings.size(); ++i) {
        if (siblings[i].id == parentId) {
            row = i;
            break;
        }
    }
    if (row < 0) return QModelIndex();

    Node &parentNode = m_nodes[parentId];
    parentNode.taskId = parentId;
    return createIndex(row, 0, &parentNode);
}

int TaskTreeModel::rowCount(const QModelIndex &parent) const
{
    QString parentId = parent.isValid() ? static_cast<Node*>(parent.internalPointer())->taskId : QString();
    return m_repo->childrenOf(parentId).size();
}

int TaskTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant TaskTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    Node *node = static_cast<Node*>(index.internalPointer());
    Task t = m_repo->find(node->taskId);
    if (t.id.isEmpty()) return QVariant();

    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return t.title;
    case Qt::CheckStateRole:
    case CompletedRole:
        return t.completed ? Qt::Checked : Qt::Unchecked;
    case NoteRole:
        return t.note;
    case PriorityRole:
        return t.priority;
    case TaskIdRole:
        return t.id;
    case Qt::ToolTipRole:
        return t.id;
    default:
        return QVariant();
    }
}

bool TaskTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) return false;

    Node *node = static_cast<Node*>(index.internalPointer());
    QString id = node->taskId;

    if (role == Qt::CheckStateRole || role == CompletedRole) {
        bool completed = value.toInt() == Qt::Checked;
        m_service->setCompleted(id, completed);
        return true;
    }
    if (role == Qt::EditRole || role == TitleRole) {
        m_service->updateTitle(id, value.toString());
        return true;
    }
    return false;
}

Qt::ItemFlags TaskTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> TaskTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles[TitleRole] = "title";
    roles[CompletedRole] = "completed";
    roles[NoteRole] = "note";
    roles[PriorityRole] = "priority";
    roles[TaskIdRole] = "taskId";
    roles[Qt::CheckStateRole] = "checkState";
    return roles;
}

void TaskTreeModel::addTask(const QString &title, const QModelIndex &parent)
{
    QString parentId = parent.isValid() ? static_cast<Node*>(parent.internalPointer())->taskId : QString();
    m_service->createTask(title, parentId);
}

void TaskTreeModel::addSubTask(const QString &parentId, const QString &title)
{
    m_service->createSubTask(parentId, title);
}

void TaskTreeModel::removeTask(const QString &id)
{
    m_service->deleteTask(id);
}

void TaskTreeModel::toggleCompleted(const QString &id)
{
    Task t = m_repo->find(id);
    if (t.id.isEmpty()) return;
    m_service->setCompleted(id, !t.completed);
}

void TaskTreeModel::refresh()
{
    beginResetModel();
    endResetModel();
}

void TaskTreeModel::updateTitle(const QString &id, const QString &title)
{
    if (!id.isEmpty()) m_service->updateTitle(id, title);
}

QString TaskTreeModel::taskIdAt(const QModelIndex &index) const
{
    if (!index.isValid()) return QString();
    return static_cast<Node*>(index.internalPointer())->taskId;
}

QModelIndex TaskTreeModel::indexForTask(const QString &taskId) const
{
    if (taskId.isEmpty()) return QModelIndex();
    QString parentId = m_repo->parentIdOf(taskId);
    QString grandParentId = parentId.isEmpty() ? QString() : m_repo->parentIdOf(parentId);
    auto siblings = m_repo->childrenOf(parentId);
    int row = -1;
    for (int i = 0; i < siblings.size(); ++i) {
        if (siblings[i].id == taskId) {
            row = i;
            break;
        }
    }
    if (row < 0) return QModelIndex();

    Node &node = m_nodes[taskId];
    node.taskId = taskId;
    return createIndex(row, 0, &node);
}
