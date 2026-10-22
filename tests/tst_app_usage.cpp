#include <QtTest>
#include <QTemporaryDir>

#include "utils/AppUsageStore.h"

// 使用频次存储：记录/落盘/加载/TTL/淘汰/时间回拨。
// 所有用例用 QTemporaryDir 隔离路径并注入时钟，不触碰用户数据目录、不依赖真实时间。
class TestAppUsage : public QObject {
    Q_OBJECT
private slots:
    void missingFileLoadsEmpty();
    void recordIncrementsCountAndUsesClock();
    void caseInsensitiveKeysMerge();
    void countSaturates();
    void corruptOrUnsupportedFileLoadsEmpty();
    void oversizedFileLoadsEmpty();
    void roundTrip();
    void expiredEntriesDroppedOnLoad();
    void evictionKeepsMostUsed();
    void lastUsedNeverRegresses();
    void clearEmptiesMemoryAndFile();
};

namespace {
qint64 g_now = 1'700'000'000'000; // 注入时钟的当前值，各用例自行调整
qint64 fakeNow() { return g_now; }
}

void TestAppUsage::missingFileLoadsEmpty()
{
    QTemporaryDir dir;
    AppUsageStore store(dir.filePath("app_usage.json"), &fakeNow);
    const AppUsageStore::Usage u = store.find("calc");
    QCOMPARE(u.count, 0u);
    QCOMPARE(u.lastUsed, 0);
}

void TestAppUsage::recordIncrementsCountAndUsesClock()
{
    QTemporaryDir dir;
    AppUsageStore store(dir.filePath("app_usage.json"), &fakeNow);

    g_now = 1000;
    store.record("calc");
    g_now = 2000;
    store.record("calc");

    const AppUsageStore::Usage u = store.find("calc");
    QCOMPARE(u.count, 2u);
    QCOMPARE(u.lastUsed, 2000);
}

void TestAppUsage::caseInsensitiveKeysMerge()
{
    QTemporaryDir dir;
    AppUsageStore store(dir.filePath("app_usage.json"), &fakeNow);

    store.record("C:/Apps/Code.exe");
    store.record("c:/apps/code.exe");

    const AppUsageStore::Usage u = store.find("c:/apps/code.exe");
    QCOMPARE(u.count, 2u);
}

void TestAppUsage::countSaturates()
{
    QTemporaryDir dir;

    // 直接写入一个饱和值太慢，先写入接近上限的值再验证不再递增。
    const QString path = dir.filePath("saturated.json");
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QJsonObject v;
        v.insert("count", int(AppUsageStore::MaxCount));
        v.insert("last", QJsonValue::fromVariant(qint64(1000)));
        QJsonObject root;
        root.insert("version", 1);
        root.insert("entries", QJsonObject{{"calc", v}});
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

    AppUsageStore store(path, &fakeNow);
    store.record("calc");
    QCOMPARE(store.find("calc").count, AppUsageStore::MaxCount);
}

void TestAppUsage::corruptOrUnsupportedFileLoadsEmpty()
{
    QTemporaryDir dir;

    // 损坏的 JSON
    const QString corrupt = dir.filePath("corrupt.json");
    {
        QFile f(corrupt);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
    }
    {
        AppUsageStore store(corrupt, &fakeNow);
        QCOMPARE(store.find("calc").count, 0u);
        // 后续保存能重写为合法文件
        store.record("calc");
        store.saveNow();
        AppUsageStore reloaded(corrupt, &fakeNow);
        QCOMPARE(reloaded.find("calc").count, 1u);
    }

    // 不认识的版本号
    const QString wrongVersion = dir.filePath("v2.json");
    {
        QFile f(wrongVersion);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"version": 2, "entries": {"calc": {"count": 5, "last": 1000}}})");
    }
    AppUsageStore wrongStore(wrongVersion, &fakeNow);
    QCOMPARE(wrongStore.find("calc").count, 0u);
}

void TestAppUsage::oversizedFileLoadsEmpty()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("big.json");
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        // 合法开头 + 大量填充，超过 1 MiB 上限。
        f.write(R"({"version":1,"entries":{})");
        QByteArray pad(int(AppUsageStore::MaxFileSize), ' ');
        f.write(pad);
    }
    AppUsageStore store(path, &fakeNow);
    QCOMPARE(store.find("calc").count, 0u);
}

void TestAppUsage::roundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("app_usage.json");

    g_now = 5000;
    {
        AppUsageStore store(path, &fakeNow);
        store.record("calc");
        store.record("calc");
        store.record("C:/Apps/Code.exe");
        store.saveNow();
    }

    AppUsageStore reloaded(path, &fakeNow);
    QCOMPARE(reloaded.find("calc").count, 2u);
    QCOMPARE(reloaded.find("calc").lastUsed, 5000);
    QCOMPARE(reloaded.find("c:/apps/code.exe").count, 1u);
}

void TestAppUsage::expiredEntriesDroppedOnLoad()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("app_usage.json");
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        const qint64 fresh = 1'700'000'000'000;
        const qint64 stale = fresh - AppUsageStore::TtlMs - 1000;
        QJsonObject root;
        root.insert("version", 1);
        root.insert("entries", QJsonObject{
            {"fresh", QJsonObject{{"count", 3}, {"last", QJsonValue::fromVariant(fresh)}}},
            {"stale", QJsonObject{{"count", 9}, {"last", QJsonValue::fromVariant(stale)}}},
        });
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

    g_now = 1'700'000'000'000;
    AppUsageStore store(path, &fakeNow);
    QCOMPARE(store.find("fresh").count, 3u);
    QCOMPARE(store.find("stale").count, 0u);
}

void TestAppUsage::evictionKeepsMostUsed()
{
    QTemporaryDir dir;
    AppUsageStore store(dir.filePath("app_usage.json"), &fakeNow);

    // 写入上限 + 2 条，保存触发淘汰。
    for (int i = 0; i < AppUsageStore::MaxEntries; ++i) {
        g_now = 1000 + i;
        store.record(QString("app_%1").arg(i, 4, 10, QLatin1Char('0')));
    }
    g_now = 999999;
    store.record("hot_a");
    store.record("hot_a");
    store.record("hot_b");
    store.saveNow();

    AppUsageStore reloaded(dir.filePath("app_usage.json"), &fakeNow);
    // hot_a(2 次) 与 hot_b(1 次但最新) 必须保留；淘汰的是 (count, lastUsed) 最小者。
    QCOMPARE(reloaded.find("hot_a").count, 2u);
    QCOMPARE(reloaded.find("hot_b").count, 1u);
    // 最早的 app_0000 / app_0001 应被淘汰
    QCOMPARE(reloaded.find("app_0000").count, 0u);
    QCOMPARE(reloaded.find("app_0001").count, 0u);
    // 较晚的条目仍保留
    QCOMPARE(reloaded.find(QString("app_%1").arg(AppUsageStore::MaxEntries - 1, 4, 10, QLatin1Char('0'))).count, 1u);
}

void TestAppUsage::lastUsedNeverRegresses()
{
    QTemporaryDir dir;
    AppUsageStore store(dir.filePath("app_usage.json"), &fakeNow);

    g_now = 5000;
    store.record("calc");
    // 系统时间回拨
    g_now = 1000;
    store.record("calc");

    QCOMPARE(store.find("calc").lastUsed, 5000);
    QCOMPARE(store.find("calc").count, 2u);
}

void TestAppUsage::clearEmptiesMemoryAndFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("app_usage.json");
    {
        AppUsageStore store(path, &fakeNow);
        store.record("calc");
        store.saveNow();
        store.clear();
        QCOMPARE(store.find("calc").count, 0u);
    }
    // 落盘后的文件也是空的，重启后不会读回旧数据。
    AppUsageStore reloaded(path, &fakeNow);
    QCOMPARE(reloaded.find("calc").count, 0u);
}

QTEST_GUILESS_MAIN(TestAppUsage)
#include "tst_app_usage.moc"
