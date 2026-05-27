#include "TaskRepository.h"
#include <QUuid>
#include <QJsonArray>

TaskRepository::TaskRepository(QObject *parent)
    : QObject(parent)
{
}

QList<Task> TaskRepository::tasks() const
{
    return m_tasks.values();
}

QList<Task> TaskRepository::childrenOf(const QString &parentId) const
{
    QList<Task> result;
    for (const auto &t : m_tasks) {
        if (t.parentId == parentId) {
            result.append(t);
        }
    }
    // 按创建时间排序，时间相同则按 id 稳定排序
    std::stable_sort(result.begin(), result.end(), [](const Task &a, const Task &b) {
        if (a.createdTime != b.createdTime)
            return a.createdTime < b.createdTime;
        return a.id < b.id;
    });
    return result;
}

Task TaskRepository::find(const QString &id) const
{
    return m_tasks.value(id, Task());
}

bool TaskRepository::exists(const QString &id) const
{
    return m_tasks.contains(id);
}

int TaskRepository::indexOf(const QString &id) const
{
    Task t = find(id);
    if (t.id.isEmpty()) return -1;
    auto siblings = childrenOf(t.parentId);
    for (int i = 0; i < siblings.size(); ++i) {
        if (siblings[i].id == id) return i;
    }
    return -1;
}

int TaskRepository::depthOf(const QString &id) const
{
    int depth = 0;
    QString current = id;
    while (!current.isEmpty()) {
        depth++;
        current = parentIdOf(current);
    }
    return depth;
}

QString TaskRepository::parentIdOf(const QString &id) const
{
    return m_tasks.value(id).parentId;
}

void TaskRepository::add(const Task &task)
{
    m_tasks[task.id] = task;
    emit taskAdded(task.id);
}

void TaskRepository::update(const Task &task)
{
    if (!m_tasks.contains(task.id)) return;
    m_tasks[task.id] = task;
    emit taskUpdated(task.id);
}

void TaskRepository::remove(const QString &id)
{
    if (!m_tasks.contains(id)) return;
    // 子任务提升为同级
    QString parentId = m_tasks[id].parentId;
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        if (it.value().parentId == id) {
            it.value().parentId = parentId;
        }
    }
    m_tasks.remove(id);
    emit taskRemoved(id);
}

void TaskRepository::removeRecursive(const QString &id)
{
    if (!m_tasks.contains(id)) return;
    QStringList desc;
    collectDescendants(id, desc);
    for (const QString &d : desc) {
        m_tasks.remove(d);
    }
    m_tasks.remove(id);
    emit taskRemoved(id);
}

bool TaskRepository::moveTo(const QString &id, const QString &newParentId)
{
    if (!m_tasks.contains(id)) return false;
    if (!canMoveTo(id, newParentId)) return false;
    m_tasks[id].parentId = newParentId;
    m_tasks[id].updatedTime = QDateTime::currentDateTime();
    emit taskMoved(id);
    return true;
}

bool TaskRepository::canMoveTo(const QString &id, const QString &newParentId) const
{
    if (id == newParentId) return false;
    if (newParentId.isEmpty()) return true;
    if (!m_tasks.contains(newParentId)) return false;
    // 检查是否会把父任务移到自己的子任务下（循环）
    QString current = newParentId;
    while (!current.isEmpty()) {
        if (current == id) return false;
        current = parentIdOf(current);
    }
    return true;
}

void TaskRepository::load(const QJsonArray &data)
{
    m_tasks.clear();
    for (const auto &v : data) {
        Task t = Task::fromJson(v.toObject());
        if (!t.id.isEmpty()) {
            m_tasks[t.id] = t;
        }
    }
}

QJsonArray TaskRepository::save() const
{
    QJsonArray arr;
    for (const auto &t : m_tasks) {
        arr.append(t.toJson());
    }
    return arr;
}

void TaskRepository::collectDescendants(const QString &id, QStringList &out) const
{
    for (const auto &t : m_tasks) {
        if (t.parentId == id) {
            out.append(t.id);
            collectDescendants(t.id, out);
        }
    }
}
