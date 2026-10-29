#pragma once

#include "PluginDescriptor.h"
#include <QHash>
#include <QPointer>
#include <functional>
#include <map>

class Settings;
class QWidget;

// 元信息、命令路由与运行实例分开；命令查找仍为 O(1)。
class PluginManager : public QObject {
    Q_OBJECT
public:
    using Factory = std::function<IPlugin *(QObject *)>;

    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager() override;
    void setSettings(Settings *settings);
    bool registerPlugin(PluginDescriptor descriptor, Factory factory);
    // 兼容已有插件与测试，新增插件优先使用元信息和工厂。
    void registerPlugin(IPlugin *plugin);

    QStringList pluginIds() const;
    const PluginDescriptor *descriptor(const QString &id) const;
    QString pluginIdForCommand(const QString &command) const;
    bool isEnabled(const QString &command) const;
    bool isInstantiated(const QString &id) const;
    // 后台插件在启动和重新启用时初始化；普通插件仍保持首次使用才加载。
    void startBackgroundPlugins();
    void startBackgroundPlugin(const QString &id);
    bool supportsAutoEnter(const QString &cmd) const;
    const PluginCommand *commandDescriptor(const QString &cmd) const;
    PluginInvocation invocation(const QString &cmd, const QString &args = {},
                                PluginInvocation::Source source = PluginInvocation::Source::Search) const;
    PluginExecutionResult invoke(const QString &cmd, const QString &args, PluginInvocation::Source source);
    PluginExecutionResult executeActionResult(const QVariantMap &action);
    void requestLeave(const QString &cmd, PluginLeaveReason reason, std::function<void(bool)> completed);
    void setPluginEnabled(const QString &id, bool enabled, std::function<void(bool)> completed);
    void requestShutdown(std::function<void(bool)> completed);

    QVariantList queryCommand(const QString &cmd, const QString &args);
    QVariantList queryCommand(const QString &cmd, const QString &args, IPlugin::QueryContext context);
    PluginQueryResult startQuery(const QString &cmd, const QString &args,
                                 IPlugin::QueryContext context = IPlugin::QueryContext::Auto, int limit = 200);
    void cancelQuery();
    bool executeAction(const QVariantMap &action);
    QStringList commands() const;
    bool hasCommand(const QString &cmd) const;
    QString commandDescription(const QString &cmd) const;
    QWidget *viewForCommand(const QString &cmd);
    QWidget *viewForCommand(const QString &cmd, const QString &args);
    IPlugin::LaunchMode launchModeForCommand(const QString &cmd, const QString &args) const;
    QWidget *settingsWidgetForCommand(const QString &cmd);
    bool autoEnterForCommand(const QString &cmd) const;
    // 显式请求运行实例才会调用工厂，禁用插件不会创建。
    IPlugin *plugin(const QString &cmd);

signals:
    void pluginCreated(const QString &id, IPlugin *plugin);
    void pluginDisabled(const QString &id);
    void actionFinished(const PluginInvocation &invocation, const PluginExecutionResult &result);
    void queryFinished(const PluginQueryResult &result);

private:
    struct Entry {
        PluginDescriptor descriptor;
        Factory factory;
        QPointer<IPlugin> instance;
        bool creating = false;
        bool legacy = false;
        quint64 leaveToken = 0;
        PluginLeaveReason leaveReason = PluginLeaveReason::Navigate;
        QList<std::function<void(bool)>> leaveCallbacks;
    };
    struct Route {
        Entry *entry = nullptr;
        int commandIndex = 0;
    };
    const Route *route(const QString &command) const;
    IPlugin *ensureInstance(Entry &entry);
    bool entryEnabled(const Entry &entry) const;
    void connectInstance(Entry &entry);
    void finishLeave(Entry &entry, quint64 token, bool accepted);
    PluginExecutionResult executeInvocation(PluginInvocation invocation);
    void continueShutdown();

    // map 的节点地址稳定，哈希路由可以直接引用，避免查询时二次按 ID 查找。
    std::map<QString, Entry> m_entries;
    QHash<QString, Route> m_routes;
    QStringList m_ids;
    QStringList m_commands;
    QPointer<Settings> m_settings;
    quint64 m_nextOperation = 0;
    QHash<quint64, PluginInvocation> m_pendingActions;
    bool m_shuttingDown = false;
    int m_shutdownIndex = 0;
    QList<std::function<void(bool)>> m_shutdownCallbacks;
    quint64 m_nextQuery = 0;
    PluginQueryRequest m_query;
    bool m_queryPending = false;
};
