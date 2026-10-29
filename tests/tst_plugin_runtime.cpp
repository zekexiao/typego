#include <QtTest>
#include <QTemporaryDir>
#include "Settings.h"
#include "plugins/PluginManager.h"

// 仅记录协议与令牌，不创建窗口、进程，也不访问真实用户数据。
class RuntimePlugin final : public IPlugin {
public:
    explicit RuntimePlugin(QObject *parent) : IPlugin(parent) {}
    QString command() const override { return QStringLiteral("demo"); }
    QString description() const override { return QStringLiteral("Demo"); }
    PluginQueryResult query(const PluginQueryRequest &request) override
    {
        lastQuery = request;
        return {request.id, {}, true};
    }
    PluginExecutionResult execute(const PluginInvocation &call) override
    {
        lastInvocation = call;
        ++executions;
        return executionResult;
    }
    PluginLeaveDecision prepareToLeave(PluginLeaveReason reason, quint64 token) override
    {
        leaveReason = reason;
        leaveToken = token;
        ++preparations;
        return leaveDecision;
    }
    void cancelQuery() override { ++cancellations; }
    void activate(const PluginInvocation &) override { ++activations; }

    PluginQueryRequest lastQuery;
    PluginInvocation lastInvocation;
    PluginExecutionResult executionResult{PluginExecutionResult::Status::Succeeded,
                                          PluginExecutionResult::HostAction::Keep};
    PluginLeaveDecision leaveDecision = PluginLeaveDecision::Ready;
    PluginLeaveReason leaveReason = PluginLeaveReason::Navigate;
    quint64 leaveToken = 0;
    int executions = 0;
    int preparations = 0;
    int cancellations = 0;
    int activations = 0;
};

class TstPluginRuntime : public QObject {
    Q_OBJECT
private:
    static PluginDescriptor descriptor(const QString &id)
    {
        PluginDescriptor result;
        result.id = id;
        result.commands = {{QStringLiteral("open"), id, {}}};
        return result;
    }
    static RuntimePlugin *add(PluginManager &manager, const QString &id)
    {
        if (!manager.registerPlugin(descriptor(id), [](QObject *parent) { return new RuntimePlugin(parent); })) return nullptr;
        return static_cast<RuntimePlugin *>(manager.plugin(id));
    }
private slots:
    void commandsDeliverStableIdentity();
    void pendingActionsRequireMatchingPluginAndToken();
    void disabledActionsNeverResumeAfterReenable();
    void sameTextQueriesHaveDifferentTokens();
    void switchingPluginRejectsOldResults();
    void refusedDisablePreservesEnabledState();
    void pendingLeaveMergesAndRejectsOldTokens();
    void shutdownWaitsWithoutCreatingUnusedPlugins();
    void backgroundPluginsStartOnlyWhenEnabled();
    void failedFactoryCanRetryWithoutRecursion();
};

void TstPluginRuntime::commandsDeliverStableIdentity()
{
    PluginManager manager;
    auto metadata = descriptor(QStringLiteral("notes"));
    metadata.commands = {{QStringLiteral("open"), QStringLiteral("note"), {QStringLiteral("n")}},
                         {QStringLiteral("new"), QStringLiteral("note-new"), {}}};
    QVERIFY(manager.registerPlugin(metadata, [](QObject *parent) { return new RuntimePlugin(parent); }));
    manager.startQuery(QStringLiteral(" N "), QStringLiteral("hello"));
    auto *plugin = static_cast<RuntimePlugin *>(manager.plugin(QStringLiteral("note")));
    QCOMPARE(plugin->lastQuery.invocation.pluginId, QStringLiteral("notes"));
    QCOMPARE(plugin->lastQuery.invocation.commandId, QStringLiteral("open"));
    QCOMPARE(plugin->lastQuery.invocation.arguments, QStringLiteral("hello"));
    const auto result = manager.invoke(QStringLiteral("note-new"), QStringLiteral("title"), PluginInvocation::Source::Hotkey);
    QCOMPARE(result.status, PluginExecutionResult::Status::Succeeded);
    QCOMPARE(result.hostAction, PluginExecutionResult::HostAction::Keep);
    QCOMPARE(plugin->lastInvocation.commandId, QStringLiteral("new"));
    QCOMPARE(plugin->lastInvocation.source, PluginInvocation::Source::Hotkey);
}

void TstPluginRuntime::pendingActionsRequireMatchingPluginAndToken()
{
    PluginManager manager;
    auto *first = add(manager, QStringLiteral("first"));
    auto *second = add(manager, QStringLiteral("second"));
    first->executionResult.status = PluginExecutionResult::Status::Pending;
    QSignalSpy finished(&manager, &PluginManager::actionFinished);
    const auto pending = manager.invoke(QStringLiteral("first"), {}, PluginInvocation::Source::Search);
    QVERIFY(pending.operationId);
    QCOMPARE(pending.status, PluginExecutionResult::Status::Pending);
    const PluginExecutionResult completed{PluginExecutionResult::Status::Succeeded, PluginExecutionResult::HostAction::Keep};
    emit second->executionFinished(pending.operationId, completed);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(finished.size(), 0);
    emit first->executionFinished(pending.operationId, completed);
    QTRY_COMPARE(finished.size(), 1);
    emit first->executionFinished(pending.operationId, completed);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(finished.size(), 1);
}

void TstPluginRuntime::disabledActionsNeverResumeAfterReenable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Settings settings(dir.filePath(QStringLiteral("settings.ini")));
    PluginManager manager;
    manager.setSettings(&settings);
    auto *plugin = add(manager, QStringLiteral("demo"));
    plugin->executionResult.status = PluginExecutionResult::Status::Pending;
    const auto pending = manager.invoke(QStringLiteral("demo"), {}, PluginInvocation::Source::Search);
    bool disabled = false;
    manager.setPluginEnabled(QStringLiteral("demo"), false, [&](bool accepted) { disabled = accepted; });
    QVERIFY(disabled);
    manager.setPluginEnabled(QStringLiteral("demo"), true, [](bool) {});
    QSignalSpy finished(&manager, &PluginManager::actionFinished);
    emit plugin->executionFinished(pending.operationId, {PluginExecutionResult::Status::Succeeded});
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(finished.size(), 0);
}

void TstPluginRuntime::sameTextQueriesHaveDifferentTokens()
{
    PluginManager manager;
    auto *plugin = add(manager, QStringLiteral("demo"));
    const auto old = manager.startQuery(QStringLiteral("demo"), QStringLiteral("same"));
    manager.cancelQuery();
    const auto current = manager.startQuery(QStringLiteral("demo"), QStringLiteral("same"), IPlugin::QueryContext::Auto, 1);
    QVERIFY(old.id != current.id);
    QSignalSpy results(&manager, &PluginManager::queryFinished);
    emit plugin->queryFinished({old.id, {QStringLiteral("old")}});
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(results.size(), 0);
    emit plugin->queryFinished({current.id, {QStringLiteral("new"), QStringLiteral("extra")}});
    QTRY_COMPARE(results.size(), 1);
    const auto accepted = qvariant_cast<PluginQueryResult>(results.first().first());
    QCOMPARE(accepted.items, QVariantList{QStringLiteral("new")});
    emit plugin->queryFinished({current.id, {QStringLiteral("duplicate")}});
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(results.size(), 1);
}

void TstPluginRuntime::switchingPluginRejectsOldResults()
{
    PluginManager manager;
    auto *first = add(manager, QStringLiteral("first"));
    auto *second = add(manager, QStringLiteral("second"));
    manager.startQuery(QStringLiteral("first"), QStringLiteral("a"));
    const auto current = manager.startQuery(QStringLiteral("second"), QStringLiteral("b"));
    QVERIFY(first->cancellations);
    QSignalSpy results(&manager, &PluginManager::queryFinished);
    emit first->queryFinished({current.id, {QStringLiteral("wrong plugin")}});
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(results.size(), 0);
    emit second->queryFinished({current.id, {QStringLiteral("correct")}});
    QTRY_COMPARE(results.size(), 1);
}

void TstPluginRuntime::refusedDisablePreservesEnabledState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Settings settings(dir.filePath(QStringLiteral("settings.ini")));
    PluginManager manager;
    manager.setSettings(&settings);
    auto *plugin = add(manager, QStringLiteral("demo"));
    plugin->leaveDecision = PluginLeaveDecision::Refused;
    bool accepted = true;
    manager.setPluginEnabled(QStringLiteral("demo"), false, [&](bool value) { accepted = value; });
    QVERIFY(!accepted);
    QVERIFY(settings.isPluginEnabled(QStringLiteral("demo")));
    QVERIFY(manager.isEnabled(QStringLiteral("demo")));
    QCOMPARE(plugin->leaveReason, PluginLeaveReason::Disable);
}

void TstPluginRuntime::pendingLeaveMergesAndRejectsOldTokens()
{
    PluginManager manager;
    auto *plugin = add(manager, QStringLiteral("demo"));
    plugin->leaveDecision = PluginLeaveDecision::Pending;
    int completed = 0;
    auto callback = [&](bool accepted) { if (accepted) ++completed; };
    manager.requestLeave(QStringLiteral("demo"), PluginLeaveReason::Navigate, callback);
    manager.requestLeave(QStringLiteral("demo"), PluginLeaveReason::Navigate, callback);
    QCOMPARE(plugin->preparations, 1);
    QVERIFY(!manager.isEnabled(QStringLiteral("demo")));
    emit plugin->leaveFinished(plugin->leaveToken + 1, true);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCOMPARE(completed, 0);
    emit plugin->leaveFinished(plugin->leaveToken, true);
    QTRY_COMPARE(completed, 2);
    QVERIFY(manager.isEnabled(QStringLiteral("demo")));
}

void TstPluginRuntime::shutdownWaitsWithoutCreatingUnusedPlugins()
{
    PluginManager manager;
    auto *plugin = add(manager, QStringLiteral("used"));
    int unusedCreated = 0;
    QVERIFY(manager.registerPlugin(descriptor(QStringLiteral("unused")), [&](QObject *parent) {
        ++unusedCreated;
        return new RuntimePlugin(parent);
    }));
    plugin->leaveDecision = PluginLeaveDecision::Pending;
    int completed = 0;
    bool accepted = true;
    manager.requestShutdown([&](bool value) { ++completed; accepted = value; });
    QCOMPARE(completed, 0);
    QCOMPARE(plugin->leaveReason, PluginLeaveReason::Shutdown);
    QVERIFY(!manager.plugin(QStringLiteral("unused")));
    emit plugin->leaveFinished(plugin->leaveToken, false);
    QTRY_COMPARE(completed, 1);
    QVERIFY(!accepted);
    QVERIFY(manager.isEnabled(QStringLiteral("unused")));
    manager.requestShutdown([&](bool value) { ++completed; accepted = value; });
    emit plugin->leaveFinished(plugin->leaveToken, true);
    QTRY_COMPARE(completed, 2);
    QVERIFY(accepted);
    QCOMPARE(unusedCreated, 0);
}

void TstPluginRuntime::backgroundPluginsStartOnlyWhenEnabled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Settings settings(dir.filePath(QStringLiteral("settings.ini")));
    PluginManager manager;
    manager.setSettings(&settings);
    int backgroundCreated = 0;
    auto background = descriptor(QStringLiteral("monitor"));
    background.background = true;
    QVERIFY(manager.registerPlugin(background, [&](QObject *parent) {
        ++backgroundCreated;
        return new RuntimePlugin(parent);
    }));
    int normalCreated = 0;
    QVERIFY(manager.registerPlugin(descriptor(QStringLiteral("normal")), [&](QObject *parent) {
        ++normalCreated;
        return new RuntimePlugin(parent);
    }));
    manager.startBackgroundPlugins();
    QCOMPARE(backgroundCreated, 1);
    QCOMPARE(normalCreated, 0);
    QVERIFY(manager.isInstantiated(QStringLiteral("monitor")));
    QVERIFY(!manager.isInstantiated(QStringLiteral("normal")));
    auto *monitor = static_cast<RuntimePlugin *>(manager.plugin(QStringLiteral("monitor")));
    QCOMPARE(monitor->activations, 1);
    // 禁用后不再启动，重新启用时通过 activate 恢复后台工作。
    manager.setPluginEnabled(QStringLiteral("monitor"), false, [](bool) {});
    manager.startBackgroundPlugin(QStringLiteral("monitor"));
    QCOMPARE(monitor->activations, 1);
    manager.setPluginEnabled(QStringLiteral("monitor"), true, [](bool) {});
    manager.startBackgroundPlugin(QStringLiteral("monitor"));
    QCOMPARE(monitor->activations, 2);
}

void TstPluginRuntime::failedFactoryCanRetryWithoutRecursion()
{
    PluginManager manager;
    int attempts = 0;
    QVERIFY(manager.registerPlugin(descriptor(QStringLiteral("demo")), [&](QObject *parent) -> IPlugin * {
        ++attempts;
        if (manager.plugin(QStringLiteral("demo"))) return nullptr;
        return attempts == 1 ? nullptr : new RuntimePlugin(parent);
    }));
    QVERIFY(!manager.plugin(QStringLiteral("demo")));
    QVERIFY(manager.plugin(QStringLiteral("demo")));
    QCOMPARE(attempts, 2);
}

QTEST_GUILESS_MAIN(TstPluginRuntime)
#include "tst_plugin_runtime.moc"
