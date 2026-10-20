#include <QtTest>

#include "utils/Version.h"

// 版本比较是自动更新的入口条件，比较错一次就会让用户卡在旧版本或收到假提示。
class TstVersion : public QObject
{
    Q_OBJECT

private slots:
    void parseStopsLeadingVAndDots();
    void parseKeepsNonNumericSegmentAsZero();
    void isNewerHandlesDifferentSegmentCount();
    void isNewerRejectsOlderOrEqual();
    void isNewerComparesTwoDigitMinor();
};

void TstVersion::parseStopsLeadingVAndDots()
{
    QCOMPARE(Version::parse(QStringLiteral("v1.2.3")), (QList<int>{1, 2, 3}));
    QCOMPARE(Version::parse(QStringLiteral("V2.0")), (QList<int>{2, 0}));
    QCOMPARE(Version::parse(QStringLiteral(" 1.2 ")), (QList<int>{1, 2}));
    QCOMPARE(Version::parse(QStringLiteral("1.2.3.4")), (QList<int>{1, 2, 3, 4}));
}

void TstVersion::parseKeepsNonNumericSegmentAsZero()
{
    // GitHub Release 标签可能被写成 v1.2-rc1，无法解析的段按 0 处理而不是崩溃。
    QCOMPARE(Version::parse(QStringLiteral("1.2-rc1")), (QList<int>{1, 0}));
    QCOMPARE(Version::parse(QStringLiteral("")), (QList<int>{0}));
}

void TstVersion::isNewerHandlesDifferentSegmentCount()
{
    // 缺位段按 0 补齐：1.2 与 1.2.0 视为同一版本，避免重复提示同一个更新。
    QVERIFY(Version::isNewer(QStringLiteral("1.2.1"), QStringLiteral("1.2")));
    QVERIFY(Version::isNewer(QStringLiteral("1.3"), QStringLiteral("1.2.9")));
    QVERIFY(!Version::isNewer(QStringLiteral("1.2"), QStringLiteral("1.2.1")));
    QVERIFY(!Version::isNewer(QStringLiteral("1.2"), QStringLiteral("1.2.0")));
    QVERIFY(!Version::isNewer(QStringLiteral("1.2.0"), QStringLiteral("1.2")));
}

void TstVersion::isNewerRejectsOlderOrEqual()
{
    QVERIFY(!Version::isNewer(QStringLiteral("1.1"), QStringLiteral("1.2")));
    QVERIFY(!Version::isNewer(QStringLiteral("1.2"), QStringLiteral("1.2")));
    QVERIFY(!Version::isNewer(QStringLiteral("v1.2"), QStringLiteral("1.2")));
    QVERIFY(!Version::isNewer(QStringLiteral("0.9"), QStringLiteral("1.0")));
    QVERIFY(Version::isNewer(QStringLiteral("v2"), QStringLiteral("1.99.99")));
}

void TstVersion::isNewerComparesTwoDigitMinor()
{
    // 按字符串比较会把 1.10 判成比 1.9 旧，这里必须是数值逐段比较。
    QVERIFY(Version::isNewer(QStringLiteral("1.10"), QStringLiteral("1.9")));
    QVERIFY(!Version::isNewer(QStringLiteral("1.9"), QStringLiteral("1.10")));
}

QTEST_GUILESS_MAIN(TstVersion)

#include "tst_version.moc"
