#pragma once

#include <QObject>
#include <QHash>
#include <QList>
#include "Task.h"

/**
 * @brief 任务仓库，内存中管理任务树
 */
class TaskRepository : public QObject {
    Q_OBJECT
public:
    explicit TaskRepository(QObject *parent = nullptr);

    /// 获取所有任务
    QList<Task> tasks() const;

    /// 获取某个父任务的子任务
    QList<Task> childrenOf(const QString &parentId) const;

    /// 根据 id 查找任务
    Task find(const QString &id) const;

    /// 判断任务是否存在
    bool exists(const QString &id) const;

    /// 获取任务在同级中的索引位置
    int indexOf(const QString &id) const;

    /// 获取任务的嵌套深度（根任务=1）
    int depthOf(const QString &id) const;

    /// 获取父任务 id
    QString parentIdOf(const QString &id) const;

    /// 添加任务
    void add(const Task &task);

    /// 更新任务
    void update(const Task &task);

    /// 删除任务（不删子任务，子任务提升为同级）
    void remove(const QString &id);

    /// 递归删除任务及其所有子任务
    void removeRecursive(const QString &id);

    /// 移动任务到新的父任务下
    bool moveTo(const QString &id, const QString &newParentId);

    /// 判断移动是否会造成循环
    bool canMoveTo(const QString &id, const QString &newParentId) const;

    /// 从 JSON 加载
    void load(const QJsonArray &data);

    /// 保存为 JSON
    QJsonArray save() const;

signals:
    void taskAdded(const QString &id);
    void taskRemoved(const QString &id);
    void taskUpdated(const QString &id);
    void taskMoved(const QString &id);

private:
    void collectDescendants(const QString &id, QStringList &out) const;

    QHash<QString, Task> m_tasks;
};
