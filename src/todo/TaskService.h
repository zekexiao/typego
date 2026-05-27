#pragma once

#include <QObject>
#include <QDateTime>
#include "TaskRepository.h"
#include "JsonTaskStorage.h"

/**
 * @brief 任务业务逻辑层
 */
class TaskService : public QObject {
    Q_OBJECT
public:
    explicit TaskService(QObject *parent = nullptr);

    TaskRepository *repository() const { return m_repo; }

    /// 创建任务
    Q_INVOKABLE QString createTask(const QString &title, const QString &parentId = QString());

    /// 创建子任务
    Q_INVOKABLE QString createSubTask(const QString &parentId, const QString &title);

    /// 删除任务（递归删除子任务）
    Q_INVOKABLE void deleteTask(const QString &id);

    /// 切换完成状态（递归影响子任务）
    Q_INVOKABLE void setCompleted(const QString &id, bool completed);

    /// 更新标题
    Q_INVOKABLE void updateTitle(const QString &id, const QString &title);

    /// 更新备注
    Q_INVOKABLE void updateNote(const QString &id, const QString &note);

    /// 保存到磁盘
    Q_INVOKABLE void persist();

private:
    void updateParentCompletion(const QString &parentId);

    TaskRepository *m_repo;
    JsonTaskStorage *m_storage;
};
