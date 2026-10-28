#include <QtTest>

#include <QTemporaryDir>

#include "plugins/PluginAction.h"
#include "plugins/PluginManager.h"
#include "Settings.h"

// 用假插件验证管理器的分发约定，不引入任何真实插件的文件系统与进程副作用。
class RecordingPlugin : public IPlugin
{
public:
    explicit RecordingPlugin(const QString &cmd, QObject *parent = nullptr)
        : IPlugin(parent), m_command(cmd)
    {
    }

    QString command() const override { return m_command; }
    QStringList aliases() const override { return m_aliases; }
    QString description() const override { return QStringLiteral("Demo plugin"); }

    QVariantList query(const QString &args) override
    {
        ++queryCount;
        lastArgs = args;
        return QVariantList();
    }

    QVariantList query(const QString &args, QueryContext context) override
    {
        lastContext = context;
        return query(args);
    }

    bool execute(const QVariantMap &action) override
    {
        ++executeCount;
        lastActionType = action.value(QStringLiteral("type")).toString();
        lastActionPlugin = action.value(QStringLiteral("plugin")).toString();
        return executeResult;
    }

    bool supportsAutoEnter() const override { return autoEnterSupported; }
    void activateCommand(const QString &cmd) override { activatedCommand = cmd; }
    void activateQuery(const QString &args) override { activatedQuery = args; }

    LaunchMode launchMode(const QString &args) const override
    {
        return args.isEmpty() ? LaunchMode::List : LaunchMode::QueryView;
    }

    QString m_command;
    QStringList m_aliases;
    int queryCount = 0;
    int executeCount = 0;
    QString lastArgs;
    QString lastActionType;
    QString lastActionPlugin;
    QString activatedCommand;
    QString activatedQuery;
    QueryContext lastContext = QueryContext::Auto;
    bool executeResult = true;
    bool autoEnterSupported = false;
};

class TstPluginManager : public QObject
{
    Q_OBJECT

private:
    static QVariantMap actionOf(const QString &type, const QString &data = QString(),
                                const QString &plugin = QString())
    {
        QVariantMap action;
        action.insert(QStringLiteral("type"), type);
        if (!data.isEmpty()) action.insert(QStringLiteral("data"), data);
        if (!plugin.isEmpty()) action.insert(QStringLiteral("plugin"), plugin);
        return action;
    }

private slots:
    void initTestCase();
    void registersCommandAliasesAndNormalizesCase();
    void duplicateCommandIsIgnored();
    void unknownCommandYieldsEmptyResults();
    void disabledPluginShortCircuitsQueryAndLaunchMode();
    void autoEnterRequiresPluginAndSetting();
    void activateRoutesToPlugin();
    void parseClassifiesActionTypes();
    void executeActionDelegatesBeforeBuiltin();
    void emptyPayloadActionsAreRejected();
    void metadataDoesNotCreatePlugins();
    void disabledPluginBlocksEveryEntry();
    void conflictingRegistrationIsAtomic();

private:
    QTemporaryDir m_dir;
};

void TstPluginManager::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void TstPluginManager::registersCommandAliasesAndNormalizesCase()
{
    PluginManager manager;
    auto *plugin = new RecordingPlugin(QStringLiteral("Demo"), &manager);
    plugin->m_aliases = {QStringLiteral("D"), QStringLiteral("  ")};
    manager.registerPlugin(plugin);

    // 命令与别名统一小写注册，空白别名被丢弃。
    const QStringList expected{QStringLiteral("demo"), QStringLiteral("d")};
    QCOMPARE(manager.commands(), expected);
    QVERIFY(manager.hasCommand(QStringLiteral("DEMO")));
    QVERIFY(manager.hasCommand(QStringLiteral(" demo ")));
    QVERIFY(!manager.hasCommand(QStringLiteral("nope")));
    QCOMPARE(manager.commandDescription(QStringLiteral("Demo")), QStringLiteral("Demo plugin"));
    QVERIFY(manager.commandDescription(QStringLiteral("nope")).isEmpty());
    QCOMPARE(manager.plugin(QStringLiteral("d")), static_cast<IPlugin *>(plugin));

    // 空指针插件不能把管理器搞坏。
    manager.registerPlugin(nullptr);
    QCOMPARE(manager.commands(), expected);
}

void TstPluginManager::duplicateCommandIsIgnored()
{
    PluginManager manager;
    auto *first = new RecordingPlugin(QStringLiteral("calc"), &manager);
    auto *second = new RecordingPlugin(QStringLiteral("CALC"), &manager);
    first->m_aliases = {QStringLiteral("math")};
    manager.registerPlugin(first);
    manager.registerPlugin(second);

    // 先注册的插件保留命令，冲突命令与重名别名都被跳过。
    const QStringList expected{QStringLiteral("calc"), QStringLiteral("math")};
    QCOMPARE(manager.commands(), expected);
    QCOMPARE(manager.plugin(QStringLiteral("calc")), static_cast<IPlugin *>(first));
}

void TstPluginManager::unknownCommandYieldsEmptyResults()
{
    PluginManager manager;
    QVERIFY(manager.queryCommand(QStringLiteral("nope"), QStringLiteral("x")).isEmpty());
    const IPlugin::LaunchMode mode = manager.launchModeForCommand(QStringLiteral("nope"), QStringLiteral("x"));
    QCOMPARE(int(mode), int(IPlugin::LaunchMode::List));
    QVERIFY(manager.viewForCommand(QStringLiteral("nope")) == nullptr);
    QVERIFY(!manager.autoEnterForCommand(QStringLiteral("nope")));
}

void TstPluginManager::disabledPluginShortCircuitsQueryAndLaunchMode()
{
    PluginManager manager;
    Settings settings(m_dir.filePath(QStringLiteral("disabled.ini")));
    manager.setSettings(&settings);
    auto *plugin = new RecordingPlugin(QStringLiteral("demo"), &manager);
    manager.registerPlugin(plugin);

    settings.setPluginEnabled(QStringLiteral("demo"), false);
    const QVariantList results = manager.queryCommand(QStringLiteral("demo"), QStringLiteral("买焊丝"));
    QCOMPARE(results.size(), 1);
    const QVariantMap item = results.first().toMap();
    QCOMPARE(item.value(QStringLiteral("title")).toString(), QStringLiteral("Plugin disabled"));
    QCOMPARE(item.value(QStringLiteral("action")).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("none"));
    // 禁用时不能把查询打到插件上，否则插件仍会去扫盘或起进程。
    QCOMPARE(plugin->queryCount, 0);
    const IPlugin::LaunchMode disabledMode = manager.launchModeForCommand(QStringLiteral("demo"), QStringLiteral("x"));
    QCOMPARE(int(disabledMode), int(IPlugin::LaunchMode::List));

    settings.setPluginEnabled(QStringLiteral("demo"), true);
    manager.queryCommand(QStringLiteral("demo"), QStringLiteral("买焊丝"), IPlugin::QueryContext::Enter);
    QCOMPARE(plugin->queryCount, 1);
    QCOMPARE(plugin->lastArgs, QStringLiteral("买焊丝"));
    QCOMPARE(int(plugin->lastContext), int(IPlugin::QueryContext::Enter));
    const IPlugin::LaunchMode enabledMode = manager.launchModeForCommand(QStringLiteral("demo"), QStringLiteral("x"));
    QCOMPARE(int(enabledMode), int(IPlugin::LaunchMode::QueryView));

    // 没注入 Settings 时插件按启用处理，查询照常透传。
    manager.setSettings(nullptr);
    manager.queryCommand(QStringLiteral("demo"), QStringLiteral("y"));
    QCOMPARE(plugin->queryCount, 2);
}

void TstPluginManager::autoEnterRequiresPluginAndSetting()
{
    PluginManager manager;
    // 独立 ini 文件，避免用例之间通过配置互相串味。
    Settings settings(m_dir.filePath(QStringLiteral("autoenter.ini")));
    manager.setSettings(&settings);
    auto *plugin = new RecordingPlugin(QStringLiteral("shell"), &manager);
    manager.registerPlugin(plugin);

    // 插件自己不支持时，无论设置如何都不自动进入。
    settings.setPluginAutoEnter(true);
    QVERIFY(!manager.autoEnterForCommand(QStringLiteral("shell")));
    plugin->autoEnterSupported = true;
    QVERIFY(manager.autoEnterForCommand(QStringLiteral("shell")));

    settings.setPluginAutoEnter(false);
    QVERIFY(!manager.autoEnterForCommand(QStringLiteral("shell")));
    settings.setPluginAutoEnter(true);
    settings.setPluginEnabled(QStringLiteral("shell"), false);
    QVERIFY(!manager.autoEnterForCommand(QStringLiteral("shell")));
}

void TstPluginManager::activateRoutesToPlugin()
{
    PluginManager manager;
    auto *plugin = new RecordingPlugin(QStringLiteral("convert"), &manager);
    manager.registerPlugin(plugin);
    // 视图由插件创建，假插件返回空视图；管理器只负责把命令和输入透传过去。
    QVERIFY(manager.viewForCommand(QStringLiteral("convert"), QStringLiteral("10km")) == nullptr);
    QCOMPARE(plugin->activatedCommand, QStringLiteral("convert"));
    QCOMPARE(plugin->activatedQuery, QStringLiteral("10km"));
}

void TstPluginManager::parseClassifiesActionTypes()
{
    const PluginAction::Request launchRequest = PluginAction::parse(actionOf(QStringLiteral("launch"), QStringLiteral("C:/Windows/notepad.exe")));
    QVERIFY(launchRequest.valid);
    QCOMPARE(int(launchRequest.builtin), int(PluginAction::Kind::Launch));
    QCOMPARE(launchRequest.payload, QStringLiteral("C:/Windows/notepad.exe"));

    const PluginAction::Request urlRequest = PluginAction::parse(actionOf(QStringLiteral("openUrl"), QStringLiteral("https://typego.app")));
    QCOMPARE(int(urlRequest.builtin), int(PluginAction::Kind::OpenUrl));

    const PluginAction::Request copyRequest = PluginAction::parse(actionOf(QStringLiteral("copy"), QStringLiteral("42")));
    QCOMPARE(int(copyRequest.builtin), int(PluginAction::Kind::Copy));

    // 未识别的类型不是内置动作，但仍是有效请求，要交给指定插件处理。
    const PluginAction::Request noneRequest = PluginAction::parse(actionOf(QStringLiteral("none"), QString(), QStringLiteral("Calc")));
    QVERIFY(noneRequest.valid);
    QCOMPARE(int(noneRequest.builtin), int(PluginAction::Kind::Invalid));
    QCOMPARE(noneRequest.pluginCommand, QStringLiteral("calc"));

    // type 大小写敏感：界面与插件的约定都是小写字面量。
    const PluginAction::Request upperRequest = PluginAction::parse(actionOf(QStringLiteral("Launch"), QStringLiteral("C:/Windows/notepad.exe")));
    QCOMPARE(int(upperRequest.builtin), int(PluginAction::Kind::Invalid));

    QVERIFY(!PluginAction::parse(QVariantMap()).valid);
    QVariantMap onlyPlugin;
    onlyPlugin.insert(QStringLiteral("plugin"), QStringLiteral("calc"));
    QVERIFY(!PluginAction::parse(onlyPlugin).valid);
}

void TstPluginManager::executeActionDelegatesBeforeBuiltin()
{
    PluginManager manager;
    auto *plugin = new RecordingPlugin(QStringLiteral("calc"), &manager);
    manager.registerPlugin(plugin);

    const QVariantMap action = actionOf(QStringLiteral("none"), QString(), QStringLiteral("CALC"));
    QVERIFY(manager.executeAction(action));
    QCOMPARE(plugin->executeCount, 1);
    QCOMPARE(plugin->lastActionType, QStringLiteral("none"));
    // 透传给插件的是原始 action，不是归一化后的命令。
    QCOMPARE(plugin->lastActionPlugin, QStringLiteral("CALC"));

    // 插件返回 false 时管理器不擅自兜底执行内置动作。
    plugin->executeResult = false;
    QVERIFY(!manager.executeAction(action));
    QCOMPARE(plugin->executeCount, 2);

    // 未知插件命令的 action 走内置分支；这里用无法识别的类型避免起进程和剪贴板。
    QVERIFY(!manager.executeAction(actionOf(QStringLiteral("none"), QString(), QStringLiteral("ghost"))));
    QCOMPARE(plugin->executeCount, 2);
    QVERIFY(!manager.executeAction(QVariantMap()));
}

void TstPluginManager::emptyPayloadActionsAreRejected()
{
    PluginManager manager;
    // data 为空的内置动作必须直接被拒，不能起空进程或写空白到剪贴板。
    QVERIFY(!manager.executeAction(actionOf(QStringLiteral("launch"))));
    QVERIFY(!manager.executeAction(actionOf(QStringLiteral("copy"))));
    QVERIFY(!manager.executeAction(actionOf(QStringLiteral("openUrl"))));
}

void TstPluginManager::metadataDoesNotCreatePlugins()
{
    PluginManager manager;
    Settings settings(m_dir.filePath(QStringLiteral("lazy.ini")));
    settings.setPluginAutoEnter(true);
    manager.setSettings(&settings);
    int created = 0;
    PluginDescriptor descriptor;
    descriptor.id = QStringLiteral("notes");
    descriptor.description = QStringLiteral("Notes");
    descriptor.commands = {{QStringLiteral("open"), QStringLiteral("note"), {QStringLiteral("n")},
                            IPlugin::LaunchMode::FullView, false, true},
                           {QStringLiteral("new"), QStringLiteral("note-new"), {}}};
    QVERIFY(manager.registerPlugin(descriptor, [&created](QObject *parent) {
        ++created;
        return new RecordingPlugin(QStringLiteral("note"), parent);
    }));
    QCOMPARE(manager.pluginIds(), QStringList{QStringLiteral("notes")});
    QCOMPARE(manager.pluginIdForCommand(QStringLiteral(" N ")), QStringLiteral("notes"));
    QCOMPARE(manager.commandDescription(QStringLiteral("note-new")), QStringLiteral("Notes"));
    QCOMPARE(manager.launchModeForCommand(QStringLiteral("n"), {}), IPlugin::LaunchMode::FullView);
    QVERIFY(manager.autoEnterForCommand(QStringLiteral("n")));
    QVERIFY(manager.descriptor(QStringLiteral("notes")));
    QVERIFY(!manager.isInstantiated(QStringLiteral("notes")));
    QCOMPARE(created, 0);
    manager.queryCommand(QStringLiteral("n"), QStringLiteral("test"));
    IPlugin *instance = manager.plugin(QStringLiteral("note-new"));
    QCOMPARE(instance, manager.plugin(QStringLiteral("note")));
    QCOMPARE(instance->parent(), &manager);
    QCOMPARE(created, 1);
}

void TstPluginManager::disabledPluginBlocksEveryEntry()
{
    PluginManager manager;
    Settings settings(m_dir.filePath(QStringLiteral("all-entries.ini")));
    manager.setSettings(&settings);
    int created = 0;
    PluginDescriptor descriptor;
    descriptor.id = QStringLiteral("notes");
    descriptor.commands = {{QStringLiteral("open"), QStringLiteral("note"), {QStringLiteral("n")}}};
    QVERIFY(manager.registerPlugin(descriptor, [&created](QObject *parent) {
        ++created;
        return new RecordingPlugin(QStringLiteral("note"), parent);
    }));
    settings.setPluginEnabled(QStringLiteral("notes"), false);
    QCOMPARE(manager.queryCommand(QStringLiteral("n"), {}).size(), 1);
    QVERIFY(!manager.plugin(QStringLiteral("note")));
    QVERIFY(!manager.viewForCommand(QStringLiteral("n")));
    QVERIFY(!manager.settingsWidgetForCommand(QStringLiteral("note")));
    QVERIFY(!manager.executeAction(actionOf(QStringLiteral("copy"), QStringLiteral("payload"), QStringLiteral("n"))));
    QVERIFY(!manager.executeAction(actionOf(QStringLiteral("copy"), QStringLiteral("payload"), QStringLiteral("ghost"))));
    QCOMPARE(created, 0);
    settings.setPluginEnabled(QStringLiteral("notes"), true);
    auto *instance = static_cast<RecordingPlugin *>(manager.plugin(QStringLiteral("note")));
    settings.setPluginEnabled(QStringLiteral("notes"), false);
    QVERIFY(!manager.executeAction(actionOf(QStringLiteral("none"), {}, QStringLiteral("n"))));
    QVERIFY(!manager.viewForCommand(QStringLiteral("n")));
    QCOMPARE(instance->executeCount, 0);
    QVERIFY(instance->activatedCommand.isEmpty());
}

void TstPluginManager::conflictingRegistrationIsAtomic()
{
    PluginManager manager;
    PluginDescriptor first;
    first.id = QStringLiteral("one");
    first.commands = {{QStringLiteral("open"), QStringLiteral("a"), {}}};
    auto factory = [](QObject *parent) { return new RecordingPlugin(QStringLiteral("a"), parent); };
    QVERIFY(manager.registerPlugin(first, factory));
    PluginDescriptor second;
    second.id = QStringLiteral("two");
    second.commands = {{QStringLiteral("open"), QStringLiteral("b"), {QStringLiteral("A")}}};
    QVERIFY(!manager.registerPlugin(second, factory));
    QVERIFY(!manager.hasCommand(QStringLiteral("b")));
    QVERIFY(!manager.descriptor(QStringLiteral("two")));
    QCOMPARE(manager.pluginIds().size(), 1);
}

QTEST_GUILESS_MAIN(TstPluginManager)

#include "tst_plugin_manager.moc"
