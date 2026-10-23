#include <QtTest>

#include "plugins/ConvertPlugin.h"
#include "plugins/UnitConversion.h"

// 插件契约：输入非空时切到整屏换算视图，列表项只作占位提示；
// 解析与换算规则已下沉到 UnitConversion（typego_core），这里直接锁行为。
class TstConvert : public QObject
{
    Q_OBJECT

private slots:
    void launchModeDependsOnInput();
    void listItemCarriesNoAction();
    void queryKeepsHintForEmptyInput();
    void parsesCommonUnits();
    void rejectsUnknownUnitAndGarbage();
    void convertsThroughMeters();
    void formatsWithoutNegativeZero();
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
    QCOMPARE(plugin.execute(PluginInvocation{}).status, PluginExecutionResult::Status::Succeeded);
}

void TstConvert::parsesCommonUnits()
{
    double value = 0.0;
    UnitConversion::LengthUnit unit;
    // 数值容差比较，字符串 key 精确比较。
    QVERIFY(UnitConversion::parseLengthQuery(QStringLiteral("10km"), &value, &unit));
    QCOMPARE(value, 10.0);
    QCOMPARE(unit.key, QStringLiteral("km"));
    QVERIFY(UnitConversion::parseLengthQuery(QStringLiteral(" 1 mile "), &value, &unit));
    QCOMPARE(value, 1.0);
    QCOMPARE(unit.key, QStringLiteral("mi"));
    QVERIFY(UnitConversion::parseLengthQuery(QStringLiteral("-2.5Inch"), &value, &unit));
    QCOMPARE(value, -2.5);
    QCOMPARE(unit.key, QStringLiteral("in"));
}

void TstConvert::rejectsUnknownUnitAndGarbage()
{
    double value = 0.0;
    UnitConversion::LengthUnit unit;
    QVERIFY(!UnitConversion::parseLengthQuery(QStringLiteral("10kg"), &value, &unit));
    QVERIFY(!UnitConversion::parseLengthQuery(QStringLiteral("abc"), &value, &unit));
    QVERIFY(!UnitConversion::parseLengthQuery(QStringLiteral("10"), &value, &unit));
    QVERIFY(!UnitConversion::parseLengthQuery(QString(), &value, &unit));
}

void TstConvert::convertsThroughMeters()
{
    double value = 0.0;
    UnitConversion::LengthUnit unit;
    QVERIFY(UnitConversion::parseLengthQuery(QStringLiteral("1mi"), &value, &unit));
    const double meters = value * unit.factorToMeter;
    // 1 英里 = 1609.344 米 = 160934.4 厘米。
    QCOMPARE(UnitConversion::formatNumber(meters * 100.0), QStringLiteral("160934.4"));
}

void TstConvert::formatsWithoutNegativeZero()
{
    QCOMPARE(UnitConversion::formatNumber(0.0), QStringLiteral("0"));
    QCOMPARE(UnitConversion::formatNumber(-0.0), QStringLiteral("0"));
    QCOMPARE(UnitConversion::formatNumber(2.5), QStringLiteral("2.5"));
}

QTEST_GUILESS_MAIN(TstConvert)

#include "tst_convert.moc"
