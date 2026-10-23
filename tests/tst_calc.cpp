#include <QtTest>

#include <cmath>

#include "plugins/CalcPlugin.h"

// CalcPlugin 的递归下降解析器是纯逻辑，query() 返回的 QVariantList 直接喂给界面，
// 因此这里断言「界面拿到的第一个条目」，不触碰剪贴板和窗口。
class TstCalc : public QObject
{
    Q_OBJECT

private:
    static QVariantMap firstItem(CalcPlugin &plugin, const QString &expr)
    {
        const QVariantList results = plugin.query(expr);
        if (results.isEmpty()) return {};
        return results.first().toMap();
    }

    static bool isError(const QVariantMap &item)
    {
        return item.value(QStringLiteral("title")).toString() == QStringLiteral("Expression Error");
    }

    // 结果显示保留 15 位有效数字，所以按相对容差比较，不做位级断言。
    void expectValue(const QString &expr, double expected)
    {
        CalcPlugin plugin;
        const QVariantMap item = firstItem(plugin, expr);
        QVERIFY2(!isError(item), qPrintable(QStringLiteral("%1 -> %2")
            .arg(expr, item.value(QStringLiteral("subtitle")).toString())));
        const QString text = item.value(QStringLiteral("title")).toString();
        const double actual = text.toDouble();
        const double tolerance = 1e-12 * qMax(1.0, std::fabs(expected));
        QVERIFY2(std::fabs(actual - expected) <= tolerance, qPrintable(QStringLiteral("%1 = %2")
            .arg(expr, text)));
        // 标题字符串必须是自身格式化结果，防止出现 "3.00000001" 这类脏显示。
        QCOMPARE(QString::number(actual, 'g', 15), text);
    }

    void expectError(const QString &expr, const QString &expectedReason)
    {
        CalcPlugin plugin;
        const QVariantMap item = firstItem(plugin, expr);
        QVERIFY2(isError(item), qPrintable(QStringLiteral("%1 应解析失败").arg(expr)));
        const QString detail = item.value(QStringLiteral("subtitle")).toString();
        QVERIFY2(detail.contains(expectedReason), qPrintable(QStringLiteral("%1 -> %2").arg(expr, detail)));
        QCOMPARE(item.value(QStringLiteral("action")).toMap().value(QStringLiteral("type")).toString(),
                 QStringLiteral("none"));
    }

private slots:
    void emptyInputReturnsHint();
    void basicArithmetic();
    void operatorPrecedence();
    void powerAndUnarySign();
    void functionsAndConstants();
    void numberFormats();
    void reportsErrorsWithPosition();
    void resultIsCopiedToActionData();
    void executeRejectsUselessActions();
};

void TstCalc::emptyInputReturnsHint()
{
    CalcPlugin plugin;
    const QVariantMap item = firstItem(plugin, QString());
    QCOMPARE(item.value(QStringLiteral("title")).toString(), QStringLiteral("Enter an expression to calculate"));
    QCOMPARE(item.value(QStringLiteral("action")).toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("none"));
    // 只有空白也按空输入处理，避免用户敲空格时看到解析错误。
    QCOMPARE(firstItem(plugin, QStringLiteral("   ")).value(QStringLiteral("title")).toString(),
             item.value(QStringLiteral("title")).toString());
    QCOMPARE(plugin.command(), QStringLiteral("calc"));
}

void TstCalc::basicArithmetic()
{
    expectValue(QStringLiteral("1+2"), 3.0);
    expectValue(QStringLiteral("10-4"), 6.0);
    expectValue(QStringLiteral("6*7"), 42.0);
    expectValue(QStringLiteral("9/2"), 4.5);
    expectValue(QStringLiteral("10%3"), 1.0);
    expectValue(QStringLiteral("0.1+0.2"), 0.3);
}

void TstCalc::operatorPrecedence()
{
    expectValue(QStringLiteral("2+3*4"), 14.0);
    expectValue(QStringLiteral("(2+3)*4"), 20.0);
    expectValue(QStringLiteral("2+3*4-6/3"), 12.0);
    expectValue(QStringLiteral("((1+2)*(3+4))"), 21.0);
    // 幂运算右结合，2^3^2 应为 2^(3^2)=512，而不是 (2^3)^2=64。
    expectValue(QStringLiteral("2^3^2"), 512.0);
}

void TstCalc::powerAndUnarySign()
{
    expectValue(QStringLiteral("-2^2"), -4.0);   // 一元负号在幂之前求值
    expectValue(QStringLiteral("(-2)^2"), 4.0);
    expectValue(QStringLiteral("2^-1"), 0.5);
    expectValue(QStringLiteral("+5"), 5.0);
    expectValue(QStringLiteral("--5"), 5.0);
    expectValue(QStringLiteral("1- -1"), 2.0);
}

void TstCalc::functionsAndConstants()
{
    expectValue(QStringLiteral("sqrt(16)"), 4.0);
    expectValue(QStringLiteral("SQRT(16)"), 4.0);          // 函数名大小写不敏感
    expectValue(QStringLiteral("abs(-3.5)"), 3.5);
    expectValue(QStringLiteral("floor(2.7)"), 2.0);
    expectValue(QStringLiteral("ceil(2.1)"), 3.0);
    expectValue(QStringLiteral("min(3,7)"), 3.0);
    expectValue(QStringLiteral("max(3,7)"), 7.0);
    expectValue(QStringLiteral("pow(2,10)"), 1024.0);
    expectValue(QStringLiteral("log(8,2)"), 3.0);          // 双参数是按底数取对数
    expectValue(QStringLiteral("log(100)"), std::log(100.0)); // 单参数时是自然对数
    expectValue(QStringLiteral("cos(0)"), 1.0);
    expectValue(QStringLiteral("sqrt(3^2+4^2)"), 5.0);
    expectValue(QStringLiteral("pi"), 3.14159265358979323846);
    expectValue(QStringLiteral("e"), 2.71828182845904523536);
}

void TstCalc::numberFormats()
{
    expectValue(QStringLiteral("1e3"), 1000.0);
    expectValue(QStringLiteral("1.5e-2"), 0.015);
    expectValue(QStringLiteral(".5+1"), 1.5);
    expectValue(QStringLiteral("1/3"), 1.0 / 3.0);
    // 数字中间的空格不会被吞掉拼成一个大数，必须当错误报出来。
    expectError(QStringLiteral("1 000"), QStringLiteral("Unexpected token"));
}

void TstCalc::reportsErrorsWithPosition()
{
    expectError(QStringLiteral("1/0"), QStringLiteral("Division by zero"));
    expectError(QStringLiteral("10%0"), QStringLiteral("Division by zero"));
    expectError(QStringLiteral("(1+2"), QStringLiteral("Expected ')'"));
    expectError(QStringLiteral("1+2x"), QStringLiteral("Unexpected token"));
    expectError(QStringLiteral("foo(1)"), QStringLiteral("Unknown function"));
    expectError(QStringLiteral("sqrt(1,2)"), QStringLiteral("expects one argument"));
    expectError(QStringLiteral("min(1)"), QStringLiteral("expects two arguments"));
    expectError(QStringLiteral("1e"), QStringLiteral("Invalid exponent"));
    expectError(QStringLiteral("sqrt()"), QStringLiteral("expects one argument"));
    expectError(QStringLiteral("abc"), QStringLiteral("Expected '(' after function name"));
    expectError(QStringLiteral("+"), QStringLiteral("Expected number, constant, or function"));
}

void TstCalc::resultIsCopiedToActionData()
{
    CalcPlugin plugin;
    const QVariantMap item = firstItem(plugin, QStringLiteral("2+2"));
    const QVariantMap action = item.value(QStringLiteral("action")).toMap();
    QCOMPARE(action.value(QStringLiteral("type")).toString(), QStringLiteral("copy"));
    QCOMPARE(action.value(QStringLiteral("plugin")).toString(), QStringLiteral("calc"));
    // 复制的内容必须和标题一致，否则用户看到的数和粘到剪贴板的数不同。
    QCOMPARE(action.value(QStringLiteral("data")).toString(), item.value(QStringLiteral("title")).toString());
    QCOMPARE(item.value(QStringLiteral("subtitle")).toString(), QStringLiteral("2+2 = 4"));
}

void TstCalc::executeRejectsUselessActions()
{
    CalcPlugin plugin;
    PluginInvocation none;
    none.action.insert(QStringLiteral("type"), QStringLiteral("none"));
    QCOMPARE(plugin.execute(none).status, PluginExecutionResult::Status::Failed);
    PluginInvocation emptyCopy;
    emptyCopy.action.insert(QStringLiteral("type"), QStringLiteral("copy"));
    // copy 但内容为空时不能动剪贴板（此处未创建 GUI，能返回 Failed 即为正确路径）。
    QCOMPARE(plugin.execute(emptyCopy).status, PluginExecutionResult::Status::Failed);
}

QTEST_GUILESS_MAIN(TstCalc)

#include "tst_calc.moc"
