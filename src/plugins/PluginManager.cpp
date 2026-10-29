#include "PluginManager.h"
#include "../Settings.h"
#include "PluginAction.h"
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QProcess>
#include <QSet>
#include <QUrl>

namespace {
QString normalized(const QString &value) { return value.trimmed().toLower(); }
}

PluginManager::PluginManager(QObject *parent) : QObject(parent) {}
PluginManager::~PluginManager()
{
    // 在路由节点析构前断开实例信号，避免子对象销毁访问失效的节点。
    for (auto &item : m_entries) {
        IPlugin *instance = item.second.instance;
        if (!instance) continue;
        disconnect(instance, nullptr, this, nullptr);
        if (instance->parent() == this) delete instance;
    }
}
void PluginManager::setSettings(Settings *settings) { m_settings = settings; }

bool PluginManager::registerPlugin(PluginDescriptor descriptor, Factory factory)
{
    descriptor.id = normalized(descriptor.id);
    if (descriptor.id.isEmpty() || descriptor.commands.isEmpty() || !factory
        || m_entries.count(descriptor.id)) return false;
    QSet<QString> keywords;
    QSet<QString> commandIds;
    for (PluginCommand &command : descriptor.commands) {
        command.id = normalized(command.id);
        command.keyword = normalized(command.keyword);
        if (command.id.isEmpty() || command.keyword.isEmpty() || commandIds.contains(command.id)) return false;
        commandIds.insert(command.id);
        for (QString &alias : command.aliases) alias = normalized(alias);
        command.aliases.removeAll(QString());
        command.aliases.removeDuplicates();
        QStringList names{command.keyword};
        names.append(command.aliases);
        for (const QString &name : names) {
            bool valid = !name.isEmpty();
            for (const QChar ch : name) valid = valid && !ch.isSpace();
            if (!valid || keywords.contains(name) || m_routes.contains(name)) {
                qWarning() << "Plugin command conflict or invalid command:" << name;
                return false;
            }
            keywords.insert(name);
        }
    }
    // 整体校验通过后才发布路由，失败不会留下半注册插件。
    const QString id = descriptor.id;
    auto &entry = m_entries[id];
    entry.descriptor = std::move(descriptor);
    entry.factory = std::move(factory);
    m_ids.append(id);
    for (int index = 0; index < entry.descriptor.commands.size(); ++index) {
        const PluginCommand &command = entry.descriptor.commands.at(index);
        QStringList names{command.keyword};
        names.append(command.aliases);
        for (const QString &name : names) {
            m_routes.insert(name, Route{&entry, index});
            m_commands.append(name);
        }
    }
    return true;
}

void PluginManager::registerPlugin(IPlugin *plugin)
{
    if (!plugin) return;
    PluginDescriptor descriptor;
    descriptor.id = plugin->command();
    descriptor.description = plugin->description();
    descriptor.hasSettings = plugin->hasSettingsWidget();
    descriptor.commands.append(PluginCommand{plugin->command(), plugin->command(), plugin->aliases()});
    const QString id = normalized(descriptor.id);
    if (!registerPlugin(std::move(descriptor), [guard = QPointer<IPlugin>(plugin)](QObject *) {
        return guard.data();
    })) return;
    Entry &entry = m_entries.at(id);
    entry.legacy = true;
    entry.instance = plugin;
    connectInstance(entry);
}

const PluginManager::Route *PluginManager::route(const QString &command) const
{
    const auto it = m_routes.constFind(normalized(command));
    return it == m_routes.cend() ? nullptr : &it.value();
}

bool PluginManager::entryEnabled(const Entry &entry) const
{
    return !m_shuttingDown && !entry.leaveToken
        && (!entry.descriptor.configurable || !m_settings || m_settings->isPluginEnabled(entry.descriptor.id));
}

IPlugin *PluginManager::ensureInstance(Entry &entry)
{
    if (!entryEnabled(entry)) return nullptr;
    if (entry.instance) return entry.instance;
    if (entry.creating) return nullptr;
    entry.creating = true;
    IPlugin *instance = entry.factory(this);
    entry.creating = false;
    if (!instance) return nullptr;
    instance->setParent(this);
    entry.instance = instance;
    connectInstance(entry);
    emit pluginCreated(entry.descriptor.id, instance);
    return instance;
}

QStringList PluginManager::pluginIds() const { return m_ids; }
const PluginDescriptor *PluginManager::descriptor(const QString &id) const
{
    const auto it = m_entries.find(normalized(id));
    return it == m_entries.end() ? nullptr : &it->second.descriptor;
}
QString PluginManager::pluginIdForCommand(const QString &command) const
{
    const Route *r = route(command);
    return r ? r->entry->descriptor.id : QString();
}
bool PluginManager::isEnabled(const QString &command) const
{
    const Route *r = route(command);
    return r && entryEnabled(*r->entry);
}
bool PluginManager::isInstantiated(const QString &id) const
{
    const auto it = m_entries.find(normalized(id));
    return it != m_entries.end() && !it->second.instance.isNull();
}
void PluginManager::startBackgroundPlugins()
{
    for (const QString &id : m_ids) startBackgroundPlugin(id);
}
void PluginManager::startBackgroundPlugin(const QString &id)
{
    const auto it = m_entries.find(normalized(id));
    if (it == m_entries.end() || !it->second.descriptor.background) return;
    // 禁用状态不初始化，也不恢复已停止的后台监听。
    if (IPlugin *instance = ensureInstance(it->second))
        instance->activate(invocation(it->second.descriptor.commands.first().keyword));
}
IPlugin *PluginManager::plugin(const QString &cmd)
{
    const Route *r = route(cmd);
    return r ? ensureInstance(*r->entry) : nullptr;
}
QVariantList PluginManager::queryCommand(const QString &cmd, const QString &args)
{
    return queryCommand(cmd, args, IPlugin::QueryContext::Auto);
}
QVariantList PluginManager::queryCommand(const QString &cmd, const QString &args, IPlugin::QueryContext context)
{
    return startQuery(cmd, args, context).items;
}

void PluginManager::cancelQuery()
{
    const Route *r = route(m_query.invocation.keyword);
    m_query.id = 0;
    m_queryPending = false;
    if (r && r->entry->instance) r->entry->instance->cancelQuery();
}

PluginQueryResult PluginManager::startQuery(const QString &cmd, const QString &args,
                                          IPlugin::QueryContext context, int limit)
{
    cancelQuery();
    if (!hasCommand(cmd)) return {};
    m_query = {++m_nextQuery, invocation(cmd, args), context, qBound(1, limit, 1000)};
    if (!isEnabled(cmd)) {
        return {m_query.id, {QVariantMap{{"title", tr("Plugin disabled")},
                            {"subtitle", normalized(cmd) + tr(" plugin has been disabled in settings")},
                            {"action", QVariantMap{{"type", "none"}}}}}};
    }
    IPlugin *instance = plugin(cmd);
    if (!instance) return {m_query.id, {}};
    PluginQueryResult result = instance->query(m_query);
    result.id = m_query.id;
    if (result.items.size() > m_query.limit) result.items.resize(m_query.limit);
    m_queryPending = result.pending;
    return result;
}
bool PluginManager::executeAction(const QVariantMap &action)
{
    return executeActionResult(action).status != PluginExecutionResult::Status::Failed;
}

PluginExecutionResult PluginManager::executeActionResult(const QVariantMap &action)
{
    const PluginAction::Request request = PluginAction::parse(action);
    if (!request.valid) return {};
    if (!request.pluginCommand.isEmpty()) {
        // 指定插件的动作不能因禁用或不存在而回退成内置动作。
        PluginInvocation call = invocation(request.pluginCommand);
        call.action = action;
        return executeInvocation(std::move(call));
    }
    bool ok = false;
    switch (request.builtin) {
    case PluginAction::Kind::Launch:
        ok = QProcess::startDetached(request.payload, QStringList());
        break;
    case PluginAction::Kind::OpenUrl:
        ok = QDesktopServices::openUrl(QUrl(request.payload));
        break;
    case PluginAction::Kind::Copy: {
        QClipboard *clipboard = QGuiApplication::clipboard();
        if (clipboard) {
            clipboard->setText(request.payload);
            ok = true;
        }
        break;
    }
    case PluginAction::Kind::Invalid:
        break;
    }
    return {ok ? PluginExecutionResult::Status::Succeeded : PluginExecutionResult::Status::Failed,
            ok ? PluginExecutionResult::HostAction::Hide : PluginExecutionResult::HostAction::Keep};
}
QStringList PluginManager::commands() const { return m_commands; }
bool PluginManager::hasCommand(const QString &cmd) const { return route(cmd) != nullptr; }
QString PluginManager::commandDescription(const QString &cmd) const
{
    const Route *r = route(cmd);
    return r ? r->entry->descriptor.description : QString();
}
QWidget *PluginManager::viewForCommand(const QString &cmd) { return viewForCommand(cmd, QString()); }
QWidget *PluginManager::viewForCommand(const QString &cmd, const QString &args)
{
    IPlugin *instance = plugin(cmd);
    if (!instance) return nullptr;
    instance->activate(invocation(cmd, args));
    return instance->viewUrl();
}
IPlugin::LaunchMode PluginManager::launchModeForCommand(const QString &cmd, const QString &args) const
{
    const Route *r = route(cmd);
    if (!r || !entryEnabled(*r->entry)) return IPlugin::LaunchMode::List;
    if (r->entry->legacy && r->entry->instance) return r->entry->instance->launchMode(args);
    const PluginCommand &command = r->entry->descriptor.commands.at(r->commandIndex);
    return command.requiresArguments && args.trimmed().isEmpty() ? IPlugin::LaunchMode::List : command.launchMode;
}
QWidget *PluginManager::settingsWidgetForCommand(const QString &cmd)
{
    IPlugin *instance = plugin(cmd);
    return instance ? instance->settingsWidget() : nullptr;
}
bool PluginManager::autoEnterForCommand(const QString &cmd) const
{
    const Route *r = route(cmd);
    if (!r || !entryEnabled(*r->entry) || !m_settings || !m_settings->pluginAutoEnter()) return false;
    return r->entry->legacy && r->entry->instance ? r->entry->instance->supportsAutoEnter()
        : r->entry->descriptor.commands.at(r->commandIndex).supportsAutoEnter;
}

const PluginCommand *PluginManager::commandDescriptor(const QString &cmd) const
{
    const Route *r = route(cmd);
    return r ? &r->entry->descriptor.commands.at(r->commandIndex) : nullptr;
}

bool PluginManager::supportsAutoEnter(const QString &cmd) const
{
    const Route *r = route(cmd);
    if (!r || !entryEnabled(*r->entry)) return false;
    return r->entry->legacy && r->entry->instance ? r->entry->instance->supportsAutoEnter()
        : commandDescriptor(cmd)->supportsAutoEnter;
}

PluginInvocation PluginManager::invocation(const QString &cmd, const QString &args,
                                            PluginInvocation::Source source) const
{
    const Route *r = route(cmd);
    if (!r) return {};
    return {r->entry->descriptor.id, r->entry->descriptor.commands.at(r->commandIndex).id,
            normalized(cmd), args, source};
}

PluginExecutionResult PluginManager::invoke(const QString &cmd, const QString &args,
                                           PluginInvocation::Source source)
{
    const PluginCommand *command = commandDescriptor(cmd);
    if (!command || !isEnabled(cmd)) return {};
    if (!command->directActivation && launchModeForCommand(cmd, args) != IPlugin::LaunchMode::List) {
        return {PluginExecutionResult::Status::Succeeded, PluginExecutionResult::HostAction::OpenView};
    }
    return executeInvocation(invocation(cmd, args, source));
}

PluginExecutionResult PluginManager::executeInvocation(PluginInvocation call)
{
    IPlugin *instance = plugin(call.keyword);
    if (!instance) return {};
    call.operationId = ++m_nextOperation;
    m_pendingActions.insert(call.operationId, call);
    PluginExecutionResult result = instance->execute(call);
    result.operationId = call.operationId;
    if (result.status != PluginExecutionResult::Status::Pending) m_pendingActions.remove(call.operationId);
    return result;
}

void PluginManager::connectInstance(Entry &entry)
{
    Entry *record = &entry;
    connect(entry.instance, &IPlugin::queryFinished, this, [this, record](const PluginQueryResult &result) {
        if (!m_queryPending || !result.id || result.id != m_query.id
            || m_query.invocation.pluginId != record->descriptor.id || !entryEnabled(*record)) return;
        PluginQueryResult accepted = result;
        if (accepted.items.size() > m_query.limit) accepted.items.resize(m_query.limit);
        m_queryPending = accepted.pending;
        emit queryFinished(accepted);
    }, Qt::QueuedConnection);
    connect(entry.instance, &IPlugin::executionFinished, this,
            [this, record](quint64 token, const PluginExecutionResult &result) {
        const auto it = m_pendingActions.find(token);
        if (it == m_pendingActions.end() || it->pluginId != record->descriptor.id
            || result.status == PluginExecutionResult::Status::Pending) return;
        const PluginInvocation call = it.value();
        m_pendingActions.erase(it);
        if (!entryEnabled(*record)) return;
        PluginExecutionResult completed = result;
        completed.operationId = token;
        emit actionFinished(call, completed);
    }, Qt::QueuedConnection);
    connect(entry.instance, &IPlugin::leaveFinished, this, [this, record](quint64 token, bool accepted) {
        finishLeave(*record, token, accepted);
    }, Qt::QueuedConnection);
}

void PluginManager::requestLeave(const QString &cmd, PluginLeaveReason reason,
                                 std::function<void(bool)> completed)
{
    const Route *r = route(cmd);
    if (!r || !r->entry->instance) { completed(true); return; }
    Entry &entry = *r->entry;
    if (entry.leaveToken) {
        // 相同离开请求合并；不同意图留待当前操作结束后重试。
        if (entry.leaveReason == reason) entry.leaveCallbacks.append(std::move(completed));
        else completed(false);
        return;
    }
    entry.leaveToken = ++m_nextOperation;
    entry.leaveReason = reason;
    entry.leaveCallbacks.append(std::move(completed));
    const quint64 token = entry.leaveToken;
    const auto decision = entry.instance->prepareToLeave(reason, token);
    if (decision != PluginLeaveDecision::Pending) finishLeave(entry, token, decision == PluginLeaveDecision::Ready);
}

void PluginManager::finishLeave(Entry &entry, quint64 token, bool accepted)
{
    if (!token || entry.leaveToken != token) return;
    entry.leaveToken = 0;
    auto callbacks = std::move(entry.leaveCallbacks);
    entry.leaveCallbacks.clear();
    for (auto &callback : callbacks) callback(accepted);
}

void PluginManager::setPluginEnabled(const QString &id, bool enabled, std::function<void(bool)> completed)
{
    const auto it = m_entries.find(normalized(id));
    if (!m_settings || it == m_entries.end() || !it->second.descriptor.configurable) { completed(false); return; }
    if (m_settings->isPluginEnabled(it->first) == enabled) { completed(true); return; }
    if (enabled) {
        m_settings->setPluginEnabled(it->first, true);
        completed(true);
        return;
    }
    const QString key = it->first;
    requestLeave(it->second.descriptor.commands.first().keyword, PluginLeaveReason::Disable,
                 [this, key, completed = std::move(completed)](bool accepted) {
        if (accepted && m_settings) {
            Entry &entry = m_entries.at(key);
            if (m_query.invocation.pluginId == key) cancelQuery();
            if (entry.instance) entry.instance->cancelQuery();
            m_settings->setPluginEnabled(key, false);
            for (auto it = m_pendingActions.begin(); it != m_pendingActions.end();) {
                if (it->pluginId == key) it = m_pendingActions.erase(it);
                else ++it;
            }
            emit pluginDisabled(key);
        }
        completed(accepted);
    });
}

void PluginManager::requestShutdown(std::function<void(bool)> completed)
{
    m_shutdownCallbacks.append(std::move(completed));
    if (m_shuttingDown) return;
    m_shuttingDown = true;
    m_shutdownIndex = 0;
    continueShutdown();
}

void PluginManager::continueShutdown()
{
    if (m_shutdownIndex == m_ids.size()) {
        cancelQuery();
        m_pendingActions.clear();
        for (auto &item : m_entries) if (item.second.instance) item.second.instance->cancelQuery();
        auto callbacks = std::move(m_shutdownCallbacks);
        m_shutdownCallbacks.clear();
        m_shuttingDown = false;
        for (auto &callback : callbacks) callback(true);
        return;
    }
    const auto &descriptor = m_entries.at(m_ids.at(m_shutdownIndex++)).descriptor;
    requestLeave(descriptor.commands.first().keyword, PluginLeaveReason::Shutdown, [this](bool accepted) {
        if (accepted) { continueShutdown(); return; }
        m_shuttingDown = false;
        auto callbacks = std::move(m_shutdownCallbacks);
        m_shutdownCallbacks.clear();
        for (auto &callback : callbacks) callback(false);
    });
}
