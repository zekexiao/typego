#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QPluginLoader>
#include <QSettings>
#include <QTemporaryDir>
#include <limits>
#include "Settings.h"
#include "plugins/ExternalPluginRegistry.h"
#include "plugins/PluginManager.h"

namespace Api = TypeGo::PluginApi;

class TstPluginSdk : public QObject {
    Q_OBJECT
private:
    static PluginPaths paths(const QTemporaryDir &dir)
    {
        return {dir.filePath(QStringLiteral("packages")), dir.filePath(QStringLiteral("data")),
                dir.filePath(QStringLiteral("cache")), dir.filePath(QStringLiteral("temp"))};
    }
    static bool install(const PluginPaths &paths, const QString &id = QStringLiteral("org.typego.example"))
    {
        const QString root = QDir(paths.packages).filePath(id);
        return QDir().mkpath(root) && QFile::copy(QStringLiteral(TYPEGO_EXAMPLE_PLUGIN), QDir(root).filePath(QStringLiteral("plugin.dll")));
    }
    static QJsonObject metadata() { return QPluginLoader(QStringLiteral(TYPEGO_EXAMPLE_PLUGIN)).metaData(); }
    static QJsonObject changed(const QString &key, const QJsonValue &value)
    {
        auto root = metadata();
        auto data = root.value(QStringLiteral("MetaData")).toObject();
        data.insert(key, value);
        root.insert(QStringLiteral("MetaData"), data);
        return root;
    }
private slots:
    void metadataMatchesConsumerToolchain();
    void rejectsIncompatibleAndMalformedMetadata();
    void parametersAllowOnlyBoundedScalarValues();
    void independentPackageIsLazyAndExecutesOwnedAction();
    void conflictAndMisnamedPackagesDoNotCreateInstances();
    void asyncSearchCancelsAndFinishesOnce();
    void asyncExecutionRejectsDuplicatesAndShutdownWaits();
    void disabledPackageDoesNotLoad();
    void malformedResultsAndLateCallbacksAreRejected();
    void inlineCompletionsAndRefusedSaveRespectLifecycle();
};

void TstPluginSdk::metadataMatchesConsumerToolchain()
{
    const auto result = parsePluginManifest(metadata());
    QVERIFY2(!result.error, qPrintable(result.error.message));
    QCOMPARE(result.manifest.descriptor.id, QStringLiteral("org.typego.example"));
    QCOMPARE(result.manifest.descriptor.commands.size(), 3);
    QCOMPARE(result.manifest.abi, Api::nativeAbi());
}

void TstPluginSdk::rejectsIncompatibleAndMalformedMetadata()
{
    QCOMPARE(parsePluginManifest(changed(QStringLiteral("apiMajor"), 2)).error.code, Api::ErrorCode::Incompatible);
    QCOMPARE(parsePluginManifest(changed(QStringLiteral("apiMinor"), 1)).error.code, Api::ErrorCode::Incompatible);
    QCOMPARE(parsePluginManifest(changed(QStringLiteral("abi"), QStringLiteral("other-toolchain"))).error.code, Api::ErrorCode::Incompatible);
    QCOMPARE(parsePluginManifest(changed(QStringLiteral("requires"), QJsonArray{QStringLiteral("unknown")})).error.code, Api::ErrorCode::Incompatible);
    QCOMPARE(parsePluginManifest(changed(QStringLiteral("id"), QStringLiteral("../escape"))).error.code, Api::ErrorCode::InvalidArgument);
    QCOMPARE(parsePluginManifest(changed(QStringLiteral("version"), QStringLiteral("next"))).error.code, Api::ErrorCode::InvalidArgument);
    auto commands = metadata().value(QStringLiteral("MetaData")).toObject().value(QStringLiteral("commands")).toArray();
    commands.append(commands.first());
    QVERIFY(parsePluginManifest(changed(QStringLiteral("commands"), commands)).error);
    auto root = metadata();
    root.insert(QStringLiteral("IID"), QStringLiteral("other-interface"));
    QCOMPARE(parsePluginManifest(root).error.code, Api::ErrorCode::Incompatible);
}

void TstPluginSdk::parametersAllowOnlyBoundedScalarValues()
{
    QVERIFY(validPluginParameters({{QStringLiteral("text"), QStringLiteral("hello")}, {QStringLiteral("number"), 42}}));
    QVERIFY(!validPluginParameters({{QStringLiteral("object"), QVariantMap{{QStringLiteral("nested"), 1}}}}));
    QVERIFY(!validPluginParameters({{QStringLiteral("bytes"), QByteArray(4, 'x')}}));
    QVERIFY(!validPluginParameters({{QStringLiteral("text"), QString(Api::MaxParameterBytes, QLatin1Char('x'))}}));
    QVERIFY(!validPluginParameters({{QStringLiteral("nan"), std::numeric_limits<double>::quiet_NaN()}}));
}

void TstPluginSdk::independentPackageIsLazyAndExecutesOwnedAction()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto locations = paths(dir);
    QVERIFY(install(locations));
    PluginManager manager;
    ExternalPluginRegistry registry(manager, locations);
    QCOMPARE(registry.discover(), 1);
    QVERIFY(!manager.isInstantiated(QStringLiteral("org.typego.example")));
    QVERIFY(!QFileInfo::exists(locations.data));
    QVERIFY(manager.hasCommand(QStringLiteral("sdk-example")));
    const auto query = manager.startQuery(QStringLiteral("sdk-example"), QStringLiteral("customer text"));
    QCOMPARE(query.items.size(), 1);
    const auto item = query.items.first().toMap();
    QCOMPARE(item.value(QStringLiteral("title")).toString(), QStringLiteral("customer text"));
    const auto action = item.value(QStringLiteral("action")).toMap();
    QCOMPARE(action.value(QStringLiteral("plugin")).toString(), QStringLiteral("sdk-example"));
    const auto executed = manager.executeActionResult(action);
    QCOMPARE(executed.status, PluginExecutionResult::Status::Succeeded);
    QSettings config(QDir(locations.data).filePath(QStringLiteral("org.typego.example/config.ini")), QSettings::IniFormat);
    QCOMPARE(config.value(QStringLiteral("lastText")).toString(), QStringLiteral("customer text"));
    QVERIFY(registry.diagnostics().isEmpty());
}

void TstPluginSdk::conflictAndMisnamedPackagesDoNotCreateInstances()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto locations = paths(dir);
    QVERIFY(install(locations));
    QVERIFY(install(locations, QStringLiteral("wrong-name")));
    PluginManager manager;
    PluginDescriptor reserved;
    reserved.id = QStringLiteral("reserved");
    reserved.commands = {{QStringLiteral("search"), QStringLiteral("sdk-demo"), {}}};
    QVERIFY(manager.registerPlugin(reserved, [](QObject *) -> IPlugin * { return nullptr; }));
    ExternalPluginRegistry registry(manager, locations);
    QCOMPARE(registry.discover(), 0);
    QCOMPARE(registry.diagnostics().size(), 2);
    QVERIFY(!manager.isInstantiated(QStringLiteral("org.typego.example")));
    QVERIFY(!QFileInfo::exists(locations.data));
}

void TstPluginSdk::asyncSearchCancelsAndFinishesOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto locations = paths(dir);
    QVERIFY(install(locations));
    PluginManager manager;
    ExternalPluginRegistry registry(manager, locations);
    QCOMPARE(registry.discover(), 1);
    QSignalSpy finished(&manager, &PluginManager::queryFinished);
    QVERIFY(manager.startQuery(QStringLiteral("sdk-demo"), QStringLiteral("async")).pending);
    const auto current = manager.startQuery(QStringLiteral("sdk-demo"), QStringLiteral("new"));
    QVERIFY(!current.pending);
    QCoreApplication::processEvents();
    QCOMPARE(finished.size(), 0);
    const auto pending = manager.startQuery(QStringLiteral("sdk-demo"), QStringLiteral("async"));
    QTRY_COMPARE(finished.size(), 1);
    const auto result = qvariant_cast<PluginQueryResult>(finished.first().first());
    QCOMPARE(result.id, pending.id);
    QCOMPARE(result.items.first().toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Asynchronous result"));
}

void TstPluginSdk::asyncExecutionRejectsDuplicatesAndShutdownWaits()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto locations = paths(dir);
    QVERIFY(install(locations));
    PluginManager manager;
    ExternalPluginRegistry registry(manager, locations);
    QCOMPARE(registry.discover(), 1);
    const auto job = manager.invoke(QStringLiteral("sdk-job"), {}, PluginInvocation::Source::Hotkey);
    QCOMPARE(job.status, PluginExecutionResult::Status::Pending);
    const auto duplicate = manager.invoke(QStringLiteral("sdk-job"), {}, PluginInvocation::Source::Hotkey);
    QCOMPARE(duplicate.status, PluginExecutionResult::Status::Failed);
    QVERIFY(duplicate.error.startsWith(QStringLiteral("[6]")));
    QSignalSpy finished(&manager, &PluginManager::actionFinished);
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(qvariant_cast<PluginExecutionResult>(finished.first().at(1)).operationId, job.operationId);
    QCOMPARE(manager.invoke(QStringLiteral("sdk-job"), {}, PluginInvocation::Source::Hotkey).status, PluginExecutionResult::Status::Pending);
    int shutdowns = 0;
    manager.requestShutdown([&](bool accepted) { if (accepted) ++shutdowns; });
    QCOMPARE(shutdowns, 0);
    QTRY_COMPARE(shutdowns, 1);
}

void TstPluginSdk::disabledPackageDoesNotLoad()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto locations = paths(dir);
    QVERIFY(install(locations));
    Settings settings(dir.filePath(QStringLiteral("settings.ini")));
    settings.setPluginEnabled(QStringLiteral("org.typego.example"), false);
    PluginManager manager;
    manager.setSettings(&settings);
    ExternalPluginRegistry registry(manager, locations);
    QCOMPARE(registry.discover(), 1);
    manager.startQuery(QStringLiteral("sdk-demo"), QStringLiteral("test"));
    QCOMPARE(manager.invoke(QStringLiteral("sdk-job"), {}, PluginInvocation::Source::Hotkey).status, PluginExecutionResult::Status::Failed);
    QVERIFY(!manager.isInstantiated(QStringLiteral("org.typego.example")));
    QVERIFY(!QFileInfo::exists(locations.data));
}

void TstPluginSdk::malformedResultsAndLateCallbacksAreRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto locations = paths(dir);
    const QString package = QDir(locations.packages).filePath(QStringLiteral("org.typego.contract"));
    QVERIFY(QDir().mkpath(package));
    QVERIFY(QFile::copy(QStringLiteral(TYPEGO_CONTRACT_PLUGIN), QDir(package).filePath(QStringLiteral("plugin.dll"))));
    PluginManager manager;
    ExternalPluginRegistry registry(manager, locations);
    QCOMPARE(registry.discover(), 1);
    for (const QString &argument : {QStringLiteral("wrong-id"), QStringLiteral("duplicate"),
             QStringLiteral("invalid-parameters"), QStringLiteral("oversized")}) {
        const auto result = manager.startQuery(QStringLiteral("contract"), argument);
        QCOMPARE(result.items.size(), 1);
        QCOMPARE(result.items.first().toMap().value(QStringLiteral("errorCode")).toInt(), static_cast<int>(Api::ErrorCode::InvalidArgument));
        QVERIFY(!result.pending);
    }
    QSignalSpy finished(&manager, &PluginManager::queryFinished);
    QVERIFY(manager.startQuery(QStringLiteral("contract"), QStringLiteral("late")).pending);
    manager.startQuery(QStringLiteral("contract"), QStringLiteral("current"));
    QCoreApplication::processEvents();
    QCOMPARE(finished.size(), 0);
    const auto pending = manager.startQuery(QStringLiteral("contract"), QStringLiteral("inline"));
    QVERIFY(pending.pending);
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(qvariant_cast<PluginQueryResult>(finished.first().first()).id, pending.id);
}

void TstPluginSdk::inlineCompletionsAndRefusedSaveRespectLifecycle()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto locations = paths(dir);
    const QString package = QDir(locations.packages).filePath(QStringLiteral("org.typego.contract"));
    QVERIFY(QDir().mkpath(package));
    QVERIFY(QFile::copy(QStringLiteral(TYPEGO_CONTRACT_PLUGIN), QDir(package).filePath(QStringLiteral("plugin.dll"))));
    Settings settings(dir.filePath(QStringLiteral("settings.ini")));
    PluginManager manager;
    manager.setSettings(&settings);
    ExternalPluginRegistry registry(manager, locations);
    QCOMPARE(registry.discover(), 1);
    auto action = [](const QString &id) { return QVariantMap{{QStringLiteral("type"), QStringLiteral("external")},
        {QStringLiteral("plugin"), QStringLiteral("contract")}, {QStringLiteral("actionId"), id}}; };
    QSignalSpy finished(&manager, &PluginManager::actionFinished);
    const auto pending = manager.executeActionResult(action(QStringLiteral("inline")));
    QCOMPARE(pending.status, PluginExecutionResult::Status::Pending);
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(qvariant_cast<PluginExecutionResult>(finished.first().at(1)).operationId, pending.operationId);
    QCOMPARE(manager.executeActionResult(action(QStringLiteral("dirty"))).status, PluginExecutionResult::Status::Succeeded);
    bool accepted = true;
    manager.setPluginEnabled(QStringLiteral("org.typego.contract"), false, [&](bool value) { accepted = value; });
    QVERIFY(!accepted);
    QVERIFY(settings.isPluginEnabled(QStringLiteral("org.typego.contract")));
    int shutdowns = 0;
    manager.requestShutdown([&](bool value) { if (value) ++shutdowns; });
    QCOMPARE(shutdowns, 0);
    QCOMPARE(manager.executeActionResult(action(QStringLiteral("save"))).status, PluginExecutionResult::Status::Succeeded);
    manager.requestShutdown([&](bool value) { if (value) ++shutdowns; });
    QCOMPARE(shutdowns, 0);
    QTRY_COMPARE(shutdowns, 1);
}

QTEST_GUILESS_MAIN(TstPluginSdk)
#include "tst_plugin_sdk.moc"
