// TodoStore 纯逻辑测试：读写往返、字段容错与展示排序。
#include <QtTest>
#include <QTemporaryDir>
#include "TodoStore.h"

class TodoStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void roundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("todos.json"));
        QList<TodoItem> source;
        TodoItem high;
        high.title = QStringLiteral("高优先级");
        high.priority = 2;
        high.due = QDateTime(QDate(2026, 1, 2), QTime(8, 30));
        TodoItem done;
        done.title = QStringLiteral("已完成");
        done.done = true;
        TodoItem plain;
        plain.title = QStringLiteral("普通任务");
        source << high << done << plain;
        {
            TodoStore store(path);
            store.items() = source;
            QVERIFY(store.save());
        }
        TodoStore loaded(path);
        QVERIFY(loaded.load());
        QCOMPARE(loaded.items().size(), 3);
        QCOMPARE(loaded.items().at(0).title, QStringLiteral("高优先级"));
        QCOMPARE(loaded.items().at(0).priority, 2);
        QCOMPARE(loaded.items().at(0).due, QDateTime(QDate(2026, 1, 2), QTime(8, 30)));
        QCOMPARE(loaded.items().at(1).title, QStringLiteral("已完成"));
        QVERIFY(loaded.items().at(1).done);
        QCOMPARE(loaded.items().at(2).title, QStringLiteral("普通任务"));
        QVERIFY(!loaded.items().at(2).due.isValid());
    }

    void toleratesBadFields()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("todos.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        // 越界优先级回落到中，无标题条目被丢弃，缺 items 键当作空列表。
        file.write(QByteArrayLiteral(R"({"items":[{"id":"a","title":"x","priority":9},{"title":" "},{"id":"b","title":"keep"}]})"));
        file.close();
        TodoStore store(path);
        QVERIFY(store.load());
        QCOMPARE(store.items().size(), 2);
        QCOMPARE(store.items().at(0).title, QStringLiteral("x"));
        QCOMPARE(store.items().at(0).priority, 1);
        QCOMPARE(store.items().at(1).title, QStringLiteral("keep"));
    }

    void missingFileLoadsEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        TodoStore store(dir.filePath(QStringLiteral("none.json")));
        QVERIFY(store.load());
        QVERIFY(store.items().isEmpty());
    }

    void corruptFileReportsError()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("bad.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArrayLiteral("{not json"));
        file.close();
        TodoStore store(path);
        QString error;
        QVERIFY(!store.load(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(store.items().isEmpty());
    }

    void sortOrder()
    {
        TodoItem done;
        done.title = QStringLiteral("done");
        done.done = true;
        TodoItem low;
        low.title = QStringLiteral("low");
        low.priority = 0;
        TodoItem high;
        high.title = QStringLiteral("high");
        high.priority = 2;
        TodoItem mediumLater;
        mediumLater.title = QStringLiteral("medium later");
        mediumLater.due = QDateTime(QDate(2026, 3, 1), QTime(9, 0));
        TodoItem mediumSooner;
        mediumSooner.title = QStringLiteral("medium sooner");
        mediumSooner.due = QDateTime(QDate(2026, 2, 1), QTime(9, 0));
        TodoItem noDue;
        noDue.title = QStringLiteral("no due");

        const auto sorted = TodoStore::sorted({done, low, high, mediumLater, mediumSooner, noDue});
        QCOMPARE(sorted.size(), 6);
        QCOMPARE(sorted.at(0).title, QStringLiteral("high"));
        QCOMPARE(sorted.at(1).title, QStringLiteral("medium sooner"));
        QCOMPARE(sorted.at(2).title, QStringLiteral("medium later"));
        QCOMPARE(sorted.at(3).title, QStringLiteral("no due"));
        QCOMPARE(sorted.at(4).title, QStringLiteral("low"));
        QCOMPARE(sorted.at(5).title, QStringLiteral("done"));
    }

    void newIdIsUnique()
    {
        QSet<QString> ids;
        for (int i = 0; i < 100; ++i)
            ids.insert(TodoStore::newId());
        QCOMPARE(ids.size(), 100);
    }
};

QTEST_GUILESS_MAIN(TodoStoreTest)
#include "tst_todo_store.moc"
