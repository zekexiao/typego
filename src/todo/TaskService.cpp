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
    m_storage = new JsonTaskStorage(path);
    m_storage->load(m_repo);
}

QString TaskService::createTask(const QString &title, const QString &parentId)
{
    Task t;
    t.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.parentId = parentId;
    t.title = title.isEmpty() ? tr("New Task") : title;
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
    Task t = m_repo->find(id);
    if (t.id.isEmpty()) return;

    // 设置自身状态
    t.completed = completed;
    t.updatedTime = QDateTime::currentDateTime();
    m_repo->update(t);

    // 递归设置子任务状态
    auto children = m_repo->childrenOf(id);
    for (const auto &child : children) {
        setCompleted(child.id, completed);
    }

    // 更新父任务的完成状态
    if (!t.parentId.isEmpty()) {
        updateParentCompletion(t.parentId);
    }

    persist();
}

void TaskService::updateTitle(const QString &id, const QString &title)
{
    Task t = m_repo->find(id);
    if (t.id.isEmpty()) return;
    t.title = title;
    t.updatedTime = QDateTime::currentDateTime();
    m_repo->update(t);
    persist();
}

void TaskService::updateNote(const QString &id, const QString &note)
{
    Task t = m_repo->find(id);
    if (t.id.isEmpty()) return;
    t.note = note;
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
