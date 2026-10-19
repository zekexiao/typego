#pragma once

#include "IPlugin.h"
#include "../todo/TaskService.h"

class QWidget;
class TodoView;

// 提供任务管理命令和任务视图的插件。
class TodoPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit TodoPlugin(QObject *parent = nullptr);

    QString command() const override { return QStringLiteral("todo"); }
    QString description() const override { return tr("Task Management"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;
    LaunchMode launchMode(const QString &args) const override;
    bool supportsAutoEnter() const override { return true; }
    QWidget *viewUrl() override;

private:
    TaskService *m_service;
    TodoView *m_view = nullptr;
};

