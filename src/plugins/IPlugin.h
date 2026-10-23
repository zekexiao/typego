#pragma once

#include "PluginTypes.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QWidget;

// 定义命令插件必须实现的查询、执行和视图接口。
class IPlugin : public QObject {
    Q_OBJECT
public:
    enum class LaunchMode {
        List,
        FullView,
        QueryView
    };

    using QueryContext = PluginQueryContext;

    explicit IPlugin(QObject *parent = nullptr) : QObject(parent) {}
    ~IPlugin() override = default;

    virtual QString command() const = 0;
    virtual QStringList aliases() const { return {}; }
    virtual QString description() const = 0;
    virtual QVariantList query(const QString &args) { Q_UNUSED(args) return {}; }
    virtual QVariantList query(const QString &args, QueryContext context)
    {
        Q_UNUSED(context)
        return query(args);
    }
    virtual bool execute(const QVariantMap &action) { Q_UNUSED(action) return false; }
    virtual PluginQueryResult query(const PluginQueryRequest &request)
    {
        QVariantList items = query(request.invocation, request.context);
        if (items.size() > request.limit) items.resize(request.limit);
        return {request.id, std::move(items)};
    }
    virtual QVariantList query(const PluginInvocation &invocation, QueryContext context)
    {
        return query(invocation.arguments, context);
    }
    virtual PluginExecutionResult execute(const PluginInvocation &invocation)
    {
        const bool ok = execute(invocation.action);
        return {ok ? PluginExecutionResult::Status::Succeeded : PluginExecutionResult::Status::Failed,
                ok ? PluginExecutionResult::HostAction::Hide : PluginExecutionResult::HostAction::Keep};
    }
    virtual void cancelQuery() {}
    virtual LaunchMode launchMode(const QString &args) const
    {
        Q_UNUSED(args)
        return LaunchMode::List;
    }
    virtual bool supportsAutoEnter() const { return false; }
    virtual void activateCommand(const QString &command) { Q_UNUSED(command) }
    virtual void activateQuery(const QString &args) { Q_UNUSED(args) }

    // 旧页面接口由 createView 适配；新插件应使用明确的父对象创建界面。
    virtual QWidget *viewUrl() { return nullptr; }
    virtual QWidget *createView(QWidget *parent);
    virtual void activate(const PluginInvocation &invocation)
    {
        activateCommand(invocation.keyword);
        activateQuery(invocation.arguments);
    }
    virtual void deactivate() {}
    virtual PluginLeaveDecision prepareToLeave(PluginLeaveReason reason, quint64 token)
    {
        Q_UNUSED(reason)
        Q_UNUSED(token)
        return PluginLeaveDecision::Ready;
    }
    virtual void releaseInactiveResources() {}
    virtual bool hasSettingsWidget() const { return false; }
    virtual QWidget *settingsWidget() { return nullptr; }

signals:
    void viewCloseRequested();
    void queryFinished(const PluginQueryResult &result);
    // 异步完成必须回传原始令牌，宿主会丢弃已失效的操作。
    void executionFinished(quint64 operationId, const PluginExecutionResult &result);
    void leaveFinished(quint64 token, bool accepted);
};
