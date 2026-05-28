#pragma once

#include "IPlugin.h"
#include "../todo/TaskService.h"
#include "../todo/TaskTreeModel.h"

/**
 * @brief TODO 任务管理插件
 *
 * 命令: todo
 * 示例: todo
 */
class TodoPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit TodoPlugin(QObject *parent = nullptr);

    QString command() const override { return QString::fromUtf8(u8"todo"); }
    QString description() const override { return tr("Task Management"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;

    TaskTreeModel *treeModel() const { return m_model; }

private:
    TaskService *m_service;
    TaskTreeModel *m_model;
};
