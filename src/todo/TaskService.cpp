#include "TaskService.h"
#include <QUuid>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

TaskService::TaskService(QObject *parent)
    : QObject(parent)
    , m_repo(new TaskRepository(this))
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    path += "/todo.json";
    m_storage = std::make_unique<JsonTaskStorage>(path);
    m_storage->load(m_repo);
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
    m_repo->removeRecursive(id);
    persist();
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
    Task t = m_repo->find(id);
    if (t.id.isEmpty()) return;

    // 设置自身状态
    t.completed = completed;
    t.updatedTime = QDateTime::currentDateTime();
    m_repo->update(t);

    // 递归设置子任务状态
    auto children = m_repo->childrenOf(id);
    for (const auto &child : children) {
        setCompletedRecursive(child.id, completed);
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

void TaskService::persist()
{
    m_storage->save(m_repo);
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
