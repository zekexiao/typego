#pragma once

#include "IPlugin.h"
#include "../todo/TaskService.h"
#include "../todo/TaskTreeModel.h"

class QWidget;

// 提供任务管理命令和任务视图的插件。
class TodoPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit TodoPlugin(QObject *parent = nullptr);

    QString command() const override { return QStringLiteral("todo"); }
    QString description() const override { return tr("Task Management"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;
    QWidget *viewUrl() override;

    TaskTreeModel *treeModel() const { return m_model; }

private:
    TaskService *m_service;
    TaskTreeModel *m_model;
};

