#include "TaskService.h"
#include <QUuid>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

TaskService::TaskService(QObject *parent)
    : TaskService(defaultStoragePath(), parent)
{
}

TaskService::TaskService(const QString &storagePath, QObject *parent)
    : QObject(parent)
    , m_repo(new TaskRepository(this))
    , m_storage(std::make_unique<JsonTaskStorage>(storagePath))
{
    m_storageLoaded = m_storage->load(m_repo);
}

QString TaskService::defaultStoragePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/todo.json");
}

TaskService::~TaskService() = default;

QString TaskService::createTask(const QString &title, const QString &parentId)
{
    const QString normalizedTitle = title.trimmed();
    if (normalizedTitle.isEmpty()) return QString();
    if (!parentId.isEmpty() && !m_repo->exists(parentId)) return QString();

    Task t;
    t.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.parentId = parentId;
    t.title = normalizedTitle;
    t.completed = false;
    t.priority = 0;
    t.createdTime = QDateTime::currentDateTime();
    t.updatedTime = t.createdTime;
    m_repo->add(t);
    persist();
    return t.id;
}

QString TaskService::createSubTask(const QString &parentId, const QString &title)
{
    if (!m_repo->exists(parentId)) return QString();
    if (m_repo->depthOf(parentId) >= 5) return QString();
    return createTask(title, parentId);
}

void TaskService::deleteTask(const QString &id)
{
    if (!m_repo->exists(id)) return;
    m_deletedTasks.clear();
    const QStringList ids = m_repo->subtreeIds(id);
    m_deletedTasks.reserve(ids.size());
    for (const QString &taskId : ids) m_deletedTasks.append(m_repo->find(taskId));
    m_repo->removeRecursive(id);
    persist();
}

bool TaskService::undoDelete()
{
    if (m_deletedTasks.isEmpty()) return false;
    for (const Task &task : m_deletedTasks) m_repo->add(task);
    if (!persist()) return false;
    m_deletedTasks.clear();
    return true;
}

bool TaskService::updateDetails(const QString &id, const QString &title, const QString &note, int priority, bool completed)
{
    Task task = m_repo->find(id);
    if (task.id.isEmpty() || title.trimmed().isEmpty()) return false;
    task.title = title.trimmed();
    task.note = note;
    task.priority = qBound(0, priority, 2);
    task.updatedTime = QDateTime::currentDateTime();
    m_repo->update(task);
    if (task.completed != completed) {
        setCompletedRecursive(id, completed);
        if (!task.parentId.isEmpty()) updateParentCompletion(task.parentId);
    }
    return persist();
}

void TaskService::setCompleted(const QString &id, bool completed)
{
    setCompletedRecursive(id, completed);

    const Task task = m_repo->find(id);
    if (!task.parentId.isEmpty()) {
        updateParentCompletion(task.parentId);
    }
    persist();
}

void TaskService::setCompletedRecursive(const QString &id, bool completed)
{
    const QStringList ids = m_repo->subtreeIds(id);
    const QDateTime now = QDateTime::currentDateTime();
    for (const QString &taskId : ids) {
        Task task = m_repo->find(taskId);
        task.completed = completed;
        task.updatedTime = now;
        m_repo->update(task);
    }
}

void TaskService::updateTitle(const QString &id, const QString &title)
{
    const QString normalizedTitle = title.trimmed();
    if (normalizedTitle.isEmpty()) return;
    Task t = m_repo->find(id);
    if (t.id.isEmpty()) return;
    if (t.title == normalizedTitle) return;
    t.title = normalizedTitle;
    t.updatedTime = QDateTime::currentDateTime();
    m_repo->update(t);
    persist();
}

void TaskService::updateNote(const QString &id, const QString &note)
{
    Task t = m_repo->find(id);
    if (t.id.isEmpty()) return;
    if (t.note == note) return;
    t.note = note;
    t.updatedTime = QDateTime::currentDateTime();
    m_repo->update(t);
    persist();
}

void TaskService::updatePriority(const QString &id, int priority)
{
    priority = qBound(0, priority, 2);
    Task t = m_repo->find(id);
    if (t.id.isEmpty() || t.priority == priority) return;
    t.priority = priority;
    t.updatedTime = QDateTime::currentDateTime();
    m_repo->update(t);
    persist();
}

void TaskService::updateDueTime(const QString &id, const QDateTime &dueTime)
{
    Task t = m_repo->find(id);
    if (t.id.isEmpty() || t.dueTime == dueTime) return;
    t.dueTime = dueTime;
    t.updatedTime = QDateTime::currentDateTime();
    m_repo->update(t);
    persist();
}

bool TaskService::persist()
{
    // 读取失败时保留原始文件，避免下一次编辑用空仓库覆盖已有任务。
    if (!m_storageLoaded) {
        emit storageError(tr("Existing task data could not be loaded. The original file has been kept; changes have not been saved."));
        return false;
    }
    if (!m_storage->save(m_repo)) {
        emit storageError(tr("Cannot save tasks. Check available disk space and folder permissions, then save again before closing TypeGo."));
        return false;
    }
    return true;
}

void TaskService::updateParentCompletion(const QString &parentId)
{
    if (parentId.isEmpty()) return;
    Task parent = m_repo->find(parentId);
    if (parent.id.isEmpty()) return;

    auto children = m_repo->childrenOf(parentId);
    if (children.isEmpty()) return;

    bool allCompleted = true;
    for (const auto &c : children) {
        if (!c.completed) {
            allCompleted = false;
            break;
        }
    }

    if (parent.completed != allCompleted) {
        parent.completed = allCompleted;
        parent.updatedTime = QDateTime::currentDateTime();
        m_repo->update(parent);
        // 继续向上更新
        if (!parent.parentId.isEmpty()) {
            updateParentCompletion(parent.parentId);
        }
    }
}
