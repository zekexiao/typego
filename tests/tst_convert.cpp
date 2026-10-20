#include <QtTest>

#include "plugins/ConvertPlugin.h"

// 换算视图的解析规则藏在 ConvertPlugin.cpp 的匿名命名空间里，
// 这里先锁住插件对外的两条契约：输入非空时切到整屏换算视图，列表项只作占位提示。
class TstConvert : public QObject
{
    Q_OBJECT

private slots:
    void launchModeDependsOnInput();
    void listItemCarriesNoAction();
    void queryKeepsHintForEmptyInput();
};

void TstConvert::launchModeDependsOnInput()
{
    ConvertPlugin plugin;
    QCOMPARE(plugin.command(), QStringLiteral("convert"));
    QCOMPARE(plugin.launchMode(QString()), IPlugin::LaunchMode::List);
    QCOMPARE(plugin.launchMode(QStringLiteral("   ")), IPlugin::LaunchMode::List);
    QCOMPARE(plugin.launchMode(QStringLiteral("1mile")), IPlugin::LaunchMode::QueryView);
}

void TstConvert::listItemCarriesNoAction()
{
    ConvertPlugin plugin;
    const QVariantList results = plugin.query(QStringLiteral("10km"));
    QCOMPARE(results.size(), 1);
    const QVariantMap item = results.first().toMap();
    QCOMPARE(item.value(QStringLiteral("title")).toString(), QStringLiteral("Convert 10km"));
    // 空白和多余字符不参与标题，避免界面显示成 "Convert  10km "。
    QCOMPARE(plugin.query(QStringLiteral("  10km  ")).first().toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("Convert 10km"));
    const QVariantMap action = item.value(QStringLiteral("action")).toMap();
    QCOMPARE(action.value(QStringLiteral("type")).toString(), QStringLiteral("none"));
    QCOMPARE(action.value(QStringLiteral("plugin")).toString(), QStringLiteral("convert"));
}

void TstConvert::queryKeepsHintForEmptyInput()
{
    ConvertPlugin plugin;
    const QVariantMap item = plugin.query(QString()).first().toMap();
    QCOMPARE(item.value(QStringLiteral("title")).toString(), QStringLiteral("Enter a value to convert"));
    QCOMPARE(item.value(QStringLiteral("subtitle")).toString(), QStringLiteral("Examples: 1mile, 10km, 25cm"));
    // 换算插件不消费回车，activateQuery 只记录当前输入。
    QVERIFY(!plugin.supportsAutoEnter());
    QVERIFY(plugin.execute(QVariantMap()));
}

QTEST_GUILESS_MAIN(TstConvert)

#include "tst_convert.moc"
