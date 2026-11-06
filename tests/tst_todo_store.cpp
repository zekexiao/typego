#include <QtTest>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "utils/TodoStore.h"

// 只验证待办存储与排序规则；用 QTemporaryDir 隔离，不碰用户数据目录。
class TstTodoStore : public QObject {
    Q_OBJECT
private slots:
    void missingFileLoadsAsEmpty();
    void roundTripPreservesFields();
    void invalidFieldsFallBack();
    void sortedFollowsPriorityDueAndCompletion();
    void tombstonesHiddenAndCleaned();
    void corruptedFileIsBackedUpOnSave();
    void saveFailsWithoutDirectory();
    void dueHelpersParseWallClock();

private:
    static TodoItem item(const QString &title, qint64 createdAt = 1000);
    static void writeJson(const QString &path, const QJsonArray &items);
};

TodoItem TstTodoStore::item(const QString &title, qint64 createdAt)
{
    TodoItem entry;
    entry.id = TodoStore::newId();
    entry.title = title;
    entry.createdAt = createdAt;
    entry.updatedAt = createdAt;
    return entry;
}

void TstTodoStore::writeJson(const QString &path, const QJsonArray &items)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("items"), items);
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void TstTodoStore::missingFileLoadsAsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    TodoStore store(dir.filePath(QStringLiteral("todo/todo.json")));
    QString error;
    QVERIFY(store.load(&error));
    QVERIFY(error.isEmpty());
    QVERIFY(store.items().isEmpty());
}

void TstTodoStore::roundTripPreservesFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("todo.json"));

    TodoItem plain = item(QStringLiteral("Plain"));
    plain.priority = 0;
    TodoItem dated = item(QStringLiteral("Dated"), 2000);
    dated.priority = 2;
    dated.due = QStringLiteral("2026-11-20T18:00");
    TodoItem dayOnly = item(QStringLiteral("DayOnly"), 3000);
    dayOnly.done = true;
    dayOnly.completedAt = 4000;
    dayOnly.due = QStringLiteral("2026-11-21");
    // 墓碑必须是近期时间，否则加载时会被 30 天清理规则物理删除。
    TodoItem tombstone = item(QStringLiteral("Gone"), 5000);
    const qint64 tombstoneAt = QDateTime::currentMSecsSinceEpoch() - 1000;
    tombstone.deletedAt = tombstoneAt;

    {
        TodoStore store(path);
        store.items() = {plain, dated, dayOnly, tombstone};
        QVERIFY(store.save());
    }
    TodoStore reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.items().size(), 4);
    // 墓碑也必须完整保留，同步端需要删改依据。
    QCOMPARE(reloaded.items().at(3).deletedAt, tombstoneAt);
    QCOMPARE(reloaded.items().at(1).due, QStringLiteral("2026-11-20T18:00"));
    QCOMPARE(reloaded.items().at(2).completedAt, qint64(4000));
    QCOMPARE(reloaded.items().at(0).priority, 0);
}

void TstTodoStore::invalidFieldsFallBack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("todo.json"));
    QJsonArray items;
    // 非法优先级、缺失 id、空标题、垃圾截止时间各占一条。
    items.append(QJsonObject{{QStringLiteral("title"), QStringLiteral("Bad priority")},
                             {QStringLiteral("priority"), 9}});
    items.append(QJsonObject{{QStringLiteral("title"), QStringLiteral("No id")}});
    items.append(QJsonObject{{QStringLiteral("title"), QStringLiteral("   ")}});
    items.append(QJsonObject{{QStringLiteral("title"), QStringLiteral("Bad due")},
                             {QStringLiteral("due"), QStringLiteral("yesterday")}});
    writeJson(path, items);

    TodoStore store(path);
    QVERIFY(store.load());
    QCOMPARE(store.items().size(), 3);
    QCOMPARE(store.items().at(0).priority, 1);
    QVERIFY(!store.items().at(0).id.isEmpty());
    QVERIFY(store.items().at(2).due.isEmpty());
}

void TstTodoStore::sortedFollowsPriorityDueAndCompletion()
{
    QList<TodoItem> items;
    TodoItem high = item(QStringLiteral("High"), 1000);
    high.priority = 2;
    TodoItem lowNoDue = item(QStringLiteral("Low"), 2000);
    lowNoDue.priority = 0;
    TodoItem midDated = item(QStringLiteral("Dated"), 3000);
    midDated.due = QStringLiteral("2026-11-20");
    TodoItem midLater = item(QStringLiteral("Later"), 4000);
    midLater.due = QStringLiteral("2026-11-21T09:00");
    TodoItem midPlain = item(QStringLiteral("Plain"), 5000);
    TodoItem doneFirst = item(QStringLiteral("Done1"), 6000);
    doneFirst.done = true;
    doneFirst.completedAt = 1000;
    TodoItem doneLast = item(QStringLiteral("Done2"), 7000);
    doneLast.done = true;
    doneLast.completedAt = 2000;
    TodoItem gone = item(QStringLiteral("Gone"), 8000);
    gone.deletedAt = 9000;
    items = {midPlain, doneLast, gone, midLater, doneFirst, lowNoDue, high, midDated};

    const QList<TodoItem> sorted = TodoStore::sorted(items);
    QStringList order;
    for (const auto &entry : sorted) order << entry.title;
    // 优先级降序 → 截止升序且无截止靠后 → 创建时间升序；已完成按完成时间倒序；墓碑不出现。
    QCOMPARE(order.join(QLatin1Char('|')),
             QStringLiteral("High|Dated|Later|Plain|Low|Done2|Done1"));
}

void TstTodoStore::tombstonesHiddenAndCleaned()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("todo.json"));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QJsonArray items;
    items.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("a")},
                             {QStringLiteral("title"), QStringLiteral("Old tombstone")},
                             {QStringLiteral("deletedAt"), now - TodoStore::TombstoneRetentionMs - 1000}});
    items.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("b")},
                             {QStringLiteral("title"), QStringLiteral("Fresh tombstone")},
                             {QStringLiteral("deletedAt"), now - 1000}});
    items.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("c")},
                             {QStringLiteral("title"), QStringLiteral("Alive")}});
    writeJson(path, items);

    TodoStore store(path);
    QVERIFY(store.load());
    // 过期墓碑物理清除，新墓碑保留在存储里供同步，排序则一律跳过。
    QCOMPARE(store.items().size(), 2);
    QVERIFY(TodoStore::sorted(store.items()).isEmpty() == false);
    QCOMPARE(TodoStore::sorted(store.items()).size(), 1);
    QCOMPARE(TodoStore::sorted(store.items()).first().title, QStringLiteral("Alive"));
}

void TstTodoStore::corruptedFileIsBackedUpOnSave()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("todo.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{not json");
    file.close();

    TodoStore store(path);
    QString error;
    QVERIFY(!store.load(&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(store.items().isEmpty());

    // 首次保存先备份坏文件，再写入内存态；数据不被静默销毁。
    store.items().append(item(QStringLiteral("Recovered")));
    QVERIFY(store.save());
    QVERIFY(QFile::exists(path + QStringLiteral(".bak")));
    QFile backup(path + QStringLiteral(".bak"));
    QVERIFY(backup.open(QIODevice::ReadOnly));
    QCOMPARE(backup.readAll(), QByteArray("{not json"));

    TodoStore reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.items().size(), 1);
    QCOMPARE(reloaded.items().first().title, QStringLiteral("Recovered"));
    // 备份一次即可，后续保存不再重复覆盖。
    QVERIFY(store.save());
}

void TstTodoStore::saveFailsWithoutDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    TodoStore store(dir.filePath(QStringLiteral("missing/todo.json")));
    store.items().append(item(QStringLiteral("Test")));
    QString error;
    QVERIFY(!store.save(&error));
    QVERIFY(!error.isEmpty());
}

void TstTodoStore::dueHelpersParseWallClock()
{
    QCOMPARE(TodoStore::makeDue(QDate(2026, 11, 20)), QStringLiteral("2026-11-20"));
    QCOMPARE(TodoStore::makeDue(QDate(2026, 11, 20), QTime(18, 5)), QStringLiteral("2026-11-20T18:05"));
    QVERIFY(TodoStore::makeDue(QDate()).isEmpty());

    QVERIFY(!TodoStore::dueHasTime(QStringLiteral("2026-11-20")));
    QVERIFY(TodoStore::dueHasTime(QStringLiteral("2026-11-20T18:05")));
    QVERIFY(!TodoStore::dueHasTime(QString()));

    // 纯日期视为当天 0 点；带时间按本地墙钟还原。
    const QDateTime dayOnly = TodoStore::dueDateTime(QStringLiteral("2026-11-20"));
    QCOMPARE(dayOnly, QDateTime(QDate(2026, 11, 20), QTime(0, 0)));
    const QDateTime withTime = TodoStore::dueDateTime(QStringLiteral("2026-11-20T18:05"));
    QCOMPARE(withTime, QDateTime(QDate(2026, 11, 20), QTime(18, 5)));
    QVERIFY(!TodoStore::dueDateTime(QString()).isValid());
    QVERIFY(!TodoStore::dueDateTime(QStringLiteral("garbage")).isValid());
}

QTEST_GUILESS_MAIN(TstTodoStore)

#include "tst_todo_store.moc"
