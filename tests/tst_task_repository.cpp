#include <QtTest>

#include "todo/TaskRepository.h"

// TaskRepository 只管树结构和查询，落盘由 JsonTaskStorage 负责。
// 这里覆盖 TodoView 依赖的排序、删除语义和移动环路保护。
class TstTaskRepository : public QObject
{
    Q_OBJECT

private:
    static Task makeTask(const QString &id, const QString &parentId = QString(),
                         const QDateTime &createdTime = QDateTime())
    {
        Task t;
        t.id = id;
        t.parentId = parentId;
        t.title = QStringLiteral("任务") + id;
        t.createdTime = createdTime.isValid() ? createdTime : QDateTime(QDate(2026, 1, 1), QTime(0, 0));
        t.updatedTime = t.createdTime;
        return t;
    }

    static QStringList childIds(const TaskRepository &repo, const QString &parentId)
    {
        QStringList ids;
        for (const Task &t : repo.childrenOf(parentId)) ids.append(t.id);
        return ids;
    }

private slots:
    void addFindAndExists();
    void childrenSortedByCreatedTimeThenId();
    void depthCountsFromOneAtRoot();
    void removePromotesChildren();
    void removeRecursiveDropsWholeSubtree();
    void moveRejectsCyclesAndUnknownParents();
    void indexOfUsesSiblingOrder();
    void saveLoadRoundTrip();
    void signalsReportMutations();
};

void TstTaskRepository::addFindAndExists()
{
    TaskRepository repo;
    repo.add(makeTask(QStringLiteral("a")));
    QVERIFY(repo.exists(QStringLiteral("a")));
    QCOMPARE(repo.find(QStringLiteral("a")).title, QStringLiteral("任务a"));
    // 未命中返回空 Task，上层普遍用 id.isEmpty() 判空，这里必须保持不变。
    QVERIFY(repo.find(QStringLiteral("missing")).id.isEmpty());
    QVERIFY(!repo.exists(QStringLiteral("missing")));
    QCOMPARE(repo.tasks().size(), 1);
    // 同 id 覆盖而不是追加。
    Task updated = makeTask(QStringLiteral("a"));
    updated.completed = true;
    repo.add(updated);
    QCOMPARE(repo.tasks().size(), 1);
    QVERIFY(repo.find(QStringLiteral("a")).completed);
}

void TstTaskRepository::childrenSortedByCreatedTimeThenId()
{
    TaskRepository repo;
    const QDateTime early(QDate(2026, 1, 1), QTime(9, 0));
    const QDateTime late(QDate(2026, 1, 2), QTime(9, 0));
    // 插入顺序故意打乱，childrenOf 必须按创建时间排，界面才不会因为落盘顺序跳动。
    repo.add(makeTask(QStringLiteral("late"), QStringLiteral("p"), late));
    repo.add(makeTask(QStringLiteral("z"), QStringLiteral("p"), early));
    repo.add(makeTask(QStringLiteral("b"), QStringLiteral("p"), early));
    repo.add(makeTask(QStringLiteral("other"), QStringLiteral("q"), early));
    // 大括号里的逗号会被 QCOMPARE 宏当成参数分隔符，先落到具量变量再断言。
    const QStringList expected{QStringLiteral("b"), QStringLiteral("z"), QStringLiteral("late")};
    QCOMPARE(childIds(repo, QStringLiteral("p")), expected);
    QCOMPARE(childIds(repo, QStringLiteral("q")), QStringList{QStringLiteral("other")});
    QVERIFY(childIds(repo, QStringLiteral("missing")).isEmpty());
}

void TstTaskRepository::depthCountsFromOneAtRoot()
{
    TaskRepository repo;
    repo.add(makeTask(QStringLiteral("r")));
    repo.add(makeTask(QStringLiteral("c"), QStringLiteral("r")));
    repo.add(makeTask(QStringLiteral("g"), QStringLiteral("c")));
    QCOMPARE(repo.depthOf(QStringLiteral("r")), 1);
    QCOMPARE(repo.depthOf(QStringLiteral("c")), 2);
    QCOMPARE(repo.depthOf(QStringLiteral("g")), 3);
    // 不存在的 id 也返回 1，调用方必须先用 exists 过滤。
    QCOMPARE(repo.depthOf(QStringLiteral("missing")), 1);
}

void TstTaskRepository::removePromotesChildren()
{
    TaskRepository repo;
    repo.add(makeTask(QStringLiteral("top")));
    repo.add(makeTask(QStringLiteral("mid"), QStringLiteral("top")));
    repo.add(makeTask(QStringLiteral("leaf"), QStringLiteral("mid")));
    repo.remove(QStringLiteral("mid"));
    QVERIFY(!repo.exists(QStringLiteral("mid")));
    QCOMPARE(repo.parentIdOf(QStringLiteral("leaf")), QStringLiteral("top"));
    QVERIFY(repo.exists(QStringLiteral("top")));
    // 删除不存在的 id 不应影响其它任务。
    repo.remove(QStringLiteral("nope"));
    QCOMPARE(repo.tasks().size(), 2);
}

void TstTaskRepository::removeRecursiveDropsWholeSubtree()
{
    TaskRepository repo;
    repo.add(makeTask(QStringLiteral("a")));
    repo.add(makeTask(QStringLiteral("b"), QStringLiteral("a")));
    repo.add(makeTask(QStringLiteral("c"), QStringLiteral("b")));
    repo.add(makeTask(QStringLiteral("d"), QStringLiteral("x")));
    repo.removeRecursive(QStringLiteral("a"));
    QCOMPARE(repo.tasks().size(), 1);
    QVERIFY(repo.exists(QStringLiteral("d")));
    QVERIFY(!repo.exists(QStringLiteral("b")));
    QVERIFY(!repo.exists(QStringLiteral("c")));
}

void TstTaskRepository::moveRejectsCyclesAndUnknownParents()
{
    TaskRepository repo;
    repo.add(makeTask(QStringLiteral("root")));
    repo.add(makeTask(QStringLiteral("child"), QStringLiteral("root")));
    repo.add(makeTask(QStringLiteral("grand"), QStringLiteral("child")));

    QVERIFY(repo.canMoveTo(QStringLiteral("root"), QString()));   // 移到顶层
    QVERIFY(repo.moveTo(QStringLiteral("root"), QString()));
    QVERIFY(repo.find(QStringLiteral("root")).parentId.isEmpty());

    // 不能把父任务挂到自己的后代下，否则 depthOf 会无限循环。
    QVERIFY(!repo.canMoveTo(QStringLiteral("root"), QStringLiteral("grand")));
    QVERIFY(!repo.moveTo(QStringLiteral("root"), QStringLiteral("grand")));
    QVERIFY(!repo.canMoveTo(QStringLiteral("root"), QStringLiteral("root")));
    QVERIFY(!repo.canMoveTo(QStringLiteral("root"), QStringLiteral("missing")));
    // 同层兄弟之间移动是允许的。
    repo.add(makeTask(QStringLiteral("sibling")));
    QVERIFY(repo.canMoveTo(QStringLiteral("child"), QStringLiteral("sibling")));
    QVERIFY(repo.moveTo(QStringLiteral("child"), QStringLiteral("sibling")));
    QCOMPARE(repo.parentIdOf(QStringLiteral("child")), QStringLiteral("sibling"));
    QVERIFY(!repo.moveTo(QStringLiteral("missing"), QStringLiteral("root")));
}

void TstTaskRepository::indexOfUsesSiblingOrder()
{
    TaskRepository repo;
    const QDateTime early(QDate(2026, 1, 1), QTime(8, 0));
    const QDateTime late(QDate(2026, 1, 1), QTime(9, 0));
    repo.add(makeTask(QStringLiteral("first"), QString(), early));
    repo.add(makeTask(QStringLiteral("second"), QString(), late));
    QCOMPARE(repo.indexOf(QStringLiteral("first")), 0);
    QCOMPARE(repo.indexOf(QStringLiteral("second")), 1);
    QCOMPARE(repo.indexOf(QStringLiteral("missing")), -1);
}

void TstTaskRepository::saveLoadRoundTrip()
{
    TaskRepository repo;
    Task task = makeTask(QStringLiteral("t1"));
    task.note = QStringLiteral("备注：带中文、引号\"和换行\n");
    task.completed = true;
    task.priority = 2;
    task.dueTime = QDateTime(QDate(2026, 10, 20), QTime(15, 30));
    repo.add(task);
    repo.add(makeTask(QStringLiteral("t2"), QStringLiteral("t1")));

    TaskRepository reloaded;
    reloaded.load(repo.save());
    QCOMPARE(reloaded.tasks().size(), 2);
    const Task copy = reloaded.find(QStringLiteral("t1"));
    QCOMPARE(copy.note, task.note);
    QCOMPARE(copy.completed, true);
    QCOMPARE(copy.priority, 2);
    QCOMPARE(copy.dueTime, task.dueTime);
    QCOMPARE(copy.createdTime, task.createdTime);
    QCOMPARE(reloaded.parentIdOf(QStringLiteral("t2")), QStringLiteral("t1"));

    // 缺 id 的条目被丢弃，不能生成一个空 key 的任务。
    QJsonArray dirty;
    QJsonObject withoutId;
    withoutId.insert(QStringLiteral("title"), QStringLiteral("无 id"));
    dirty.append(withoutId);
    dirty.append(repo.save().first());
    TaskRepository filtered;
    filtered.load(dirty);
    QCOMPARE(filtered.tasks().size(), 1);
}

void TstTaskRepository::signalsReportMutations()
{
    TaskRepository repo;
    QSignalSpy added(&repo, &TaskRepository::taskAdded);
    QSignalSpy updated(&repo, &TaskRepository::taskUpdated);
    QSignalSpy removed(&repo, &TaskRepository::taskRemoved);
    QSignalSpy moved(&repo, &TaskRepository::taskMoved);

    repo.add(makeTask(QStringLiteral("s")));
    QCOMPARE(added.count(), 1);
    repo.update(makeTask(QStringLiteral("s")));
    QCOMPARE(updated.count(), 1);
    repo.update(makeTask(QStringLiteral("ghost")));   // 不存在的任务不触发信号
    QCOMPARE(updated.count(), 1);
    repo.add(makeTask(QStringLiteral("p")));
    repo.moveTo(QStringLiteral("s"), QStringLiteral("p"));
    QCOMPARE(moved.count(), 1);
    repo.remove(QStringLiteral("s"));
    QCOMPARE(removed.count(), 1);
    repo.remove(QStringLiteral("s"));                 // 重复删除不广播
    QCOMPARE(removed.count(), 1);
    // QSignalSpy 的每一项是一次信号的全部参数，取第一个参数才是 id。
    QCOMPARE(added.takeFirst().first().toString(), QStringLiteral("s"));
}

QTEST_GUILESS_MAIN(TstTaskRepository)

#include "tst_task_repository.moc"
