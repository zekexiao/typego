#include <QtTest>

#include <QFile>

#include "todo/JsonTaskStorage.h"
#include "todo/TaskService.h"

// TaskService 是待办的业务规则层：递归完成、父子状态同步、撤销删除、
// 以及「读不到就别覆盖」的数据保护。这里全部走临时目录里的存储文件。
class TstTaskService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void createTaskNormalizesTitle();
    void createTaskRejectsUnknownParent();
    void changesSurviveNewInstance();
    void completingParentCascadesToChildren();
    void parentFollowsChildrenCompletion();
    void subTaskDepthIsLimited();
    void deleteRemovesSubtreeAndUndoRestoresIt();
    void undoStackKeepsOnlyLastDelete();
    void updateDetailsClampsPriorityAndRejectsEmptyTitle();
    void updateTitleSkipsNoOpWrites();
    void unknownIdsDoNotCrash();
    void unreadableStorageBlocksWrites();
    void largeSubtreeCompletionAndDeletion();

private:
    QString path() const { return m_dir.filePath(QStringLiteral("todo.json")); }
    TaskRepository *repo() const { return m_service->repository(); }
    int taskCount() const { return repo()->tasks().size(); }

    QString create(const QString &title) { return m_service->createTask(title); }
    QString createChild(const QString &parentId, const QString &title)
    {
        return m_service->createSubTask(parentId, title);
    }

    QTemporaryDir m_dir;
    TaskService *m_service = nullptr;
};

void TstTaskService::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void TstTaskService::cleanupTestCase()
{
    delete m_service;
    m_service = nullptr;
}

void TstTaskService::init()
{
    QFile::remove(path());
    delete m_service;
    m_service = new TaskService(path());
}

void TstTaskService::createTaskNormalizesTitle()
{
    const QString id = create(QStringLiteral("  买焊丝  "));
    QVERIFY(!id.isEmpty());
    QCOMPARE(repo()->find(id).title, QStringLiteral("买焊丝"));
    QVERIFY(!repo()->find(id).completed);
    QCOMPARE(repo()->find(id).priority, 0);
    QVERIFY(repo()->find(id).createdTime.isValid());
    QVERIFY(repo()->find(id).parentId.isEmpty());

    QVERIFY(create(QStringLiteral("   ")).isEmpty());
    QVERIFY(create(QString()).isEmpty());
    QCOMPARE(taskCount(), 1);
}

void TstTaskService::createTaskRejectsUnknownParent()
{
    QVERIFY(m_service->createTask(QStringLiteral("子"), QStringLiteral("no-such-parent")).isEmpty());
    QVERIFY(createChild(QStringLiteral("no-such-parent"), QStringLiteral("子")).isEmpty());
    QCOMPARE(taskCount(), 0);
}

void TstTaskService::changesSurviveNewInstance()
{
    const QString id = create(QStringLiteral("排产"));
    m_service->updateNote(id, QStringLiteral("下周一前"));
    m_service->updatePriority(id, 1);
    const QDateTime due(QDate(2026, 11, 2), QTime(9, 0));
    m_service->updateDueTime(id, due);

    TaskService reopened(path());
    const Task task = reopened.repository()->find(id);
    QCOMPARE(task.title, QStringLiteral("排产"));
    QCOMPARE(task.note, QStringLiteral("下周一前"));
    QCOMPARE(task.priority, 1);
    QCOMPARE(task.dueTime, due);
}

void TstTaskService::completingParentCascadesToChildren()
{
    const QString parent = create(QStringLiteral("验收"));
    const QString child = createChild(parent, QStringLiteral("尺寸"));
    const QString grand = createChild(child, QStringLiteral("焊缝"));

    m_service->setCompleted(parent, true);
    QVERIFY(repo()->find(parent).completed);
    QVERIFY(repo()->find(child).completed);
    QVERIFY(repo()->find(grand).completed);

    m_service->setCompleted(parent, false);
    QVERIFY(!repo()->find(grand).completed);

    // 完成状态要落盘，重启后不能又变回未完成。
    TaskService reopened(path());
    QVERIFY(!reopened.repository()->find(parent).completed);
}

void TstTaskService::parentFollowsChildrenCompletion()
{
    const QString parent = create(QStringLiteral("发货"));
    const QString first = createChild(parent, QStringLiteral("打包"));
    const QString second = createChild(parent, QStringLiteral("贴单"));

    m_service->setCompleted(first, true);
    QVERIFY(!repo()->find(parent).completed);      // 还有一个未完成，父任务保持未完成

    m_service->setCompleted(second, true);
    QVERIFY(repo()->find(parent).completed);       // 全部完成时父任务自动打勾

    m_service->setCompleted(second, false);
    QVERIFY(!repo()->find(parent).completed);      // 反向也要同步，否则勾会赖在父任务上

    // 多层向上冒泡：孙子全完成后父与祖父依次完成。
    const QString grand = createChild(first, QStringLiteral("复核"));
    m_service->setCompleted(second, true);
    m_service->setCompleted(grand, true);
    QVERIFY(repo()->find(first).completed);
    QVERIFY(repo()->find(parent).completed);
}

void TstTaskService::subTaskDepthIsLimited()
{
    QString current = create(QStringLiteral("第1层"));
    QVERIFY(!current.isEmpty());
    for (int level = 2; level <= 5; ++level) {
        const QString child = createChild(current, QStringLiteral("第%1层").arg(level));
        QVERIFY2(!child.isEmpty(), qPrintable(QStringLiteral("第%1层应可创建").arg(level)));
        current = child;
    }
    QCOMPARE(repo()->depthOf(current), 5);
    QVERIFY(createChild(current, QStringLiteral("第6层")).isEmpty());
    QCOMPARE(taskCount(), 5);
}

void TstTaskService::deleteRemovesSubtreeAndUndoRestoresIt()
{
    const QString parent = create(QStringLiteral("夹具"));
    const QString child = createChild(parent, QStringLiteral("定位销"));
    createChild(child, QStringLiteral("螺纹孔"));
    const QString unrelated = create(QStringLiteral("其它"));

    m_service->deleteTask(parent);
    QVERIFY(!repo()->exists(parent));
    QVERIFY(!repo()->exists(child));
    QCOMPARE(taskCount(), 1);
    QVERIFY(repo()->exists(unrelated));
    QVERIFY(m_service->canUndoDelete());

    QVERIFY(m_service->undoDelete());
    QVERIFY(repo()->exists(parent));
    QVERIFY(repo()->exists(child));
    QCOMPARE(repo()->parentIdOf(child), parent);
    QCOMPARE(taskCount(), 4);
    QVERIFY(!m_service->canUndoDelete());
    QVERIFY(!m_service->undoDelete());             // 撤销栈已空时返回 false

    // 删除后重新打开，恢复结果必须已经落盘。
    TaskService reopened(path());
    QCOMPARE(reopened.repository()->tasks().size(), 4);
}

void TstTaskService::undoStackKeepsOnlyLastDelete()
{
    const QString first = create(QStringLiteral("第一批"));
    const QString second = create(QStringLiteral("第二批"));
    m_service->deleteTask(first);
    m_service->deleteTask(second);
    QVERIFY(m_service->undoDelete());
    QVERIFY(repo()->exists(second));               // 只恢复最后一次删除
    QVERIFY(!repo()->exists(first));
}

void TstTaskService::updateDetailsClampsPriorityAndRejectsEmptyTitle()
{
    const QString id = create(QStringLiteral("报价"));
    QVERIFY(m_service->updateDetails(id, QStringLiteral("报价单"), QStringLiteral("备注"), 5, false));
    QCOMPARE(repo()->find(id).priority, 2);        // 上界夹到「紧急」

    QVERIFY(m_service->updateDetails(id, QStringLiteral("报价单"), QString(), -3, false));
    QCOMPARE(repo()->find(id).priority, 0);

    QVERIFY(!m_service->updateDetails(id, QStringLiteral("  "), QString(), 1, false));
    QVERIFY(!m_service->updateDetails(QStringLiteral("missing"), QStringLiteral("标题"), QString(), 1, false));
    QCOMPARE(repo()->find(id).title, QStringLiteral("报价单"));

    // updateDetails 同时负责完成状态，且继续递归子任务。
    const QString child = createChild(id, QStringLiteral("子项"));
    QVERIFY(m_service->updateDetails(id, QStringLiteral("报价单"), QString(), 0, true));
    QVERIFY(repo()->find(id).completed);
    QVERIFY(repo()->find(child).completed);
}

void TstTaskService::updateTitleSkipsNoOpWrites()
{
    const QString id = create(QStringLiteral("周报"));
    const QDateTime before = repo()->find(id).updatedTime;

    m_service->updateTitle(id, QStringLiteral("周报"));
    QCOMPARE(repo()->find(id).updatedTime, before);   // 同标题不刷新更新时间
    m_service->updateTitle(id, QStringLiteral("  周报  "));
    QCOMPARE(repo()->find(id).updatedTime, before);
    m_service->updateTitle(id, QString());            // 空标题一律忽略
    QCOMPARE(repo()->find(id).title, QStringLiteral("周报"));
    m_service->updateTitle(QStringLiteral("missing"), QStringLiteral("改名"));
    QCOMPARE(taskCount(), 1);

    m_service->updateTitle(id, QStringLiteral("月报"));
    QCOMPARE(repo()->find(id).title, QStringLiteral("月报"));
    QVERIFY(repo()->find(id).updatedTime >= before);
}

void TstTaskService::unknownIdsDoNotCrash()
{
    m_service->deleteTask(QStringLiteral("missing"));
    m_service->setCompleted(QStringLiteral("missing"), true);
    m_service->updateNote(QStringLiteral("missing"), QStringLiteral("x"));
    m_service->updatePriority(QStringLiteral("missing"), 1);
    m_service->updateDueTime(QStringLiteral("missing"), QDateTime());
    QCOMPARE(taskCount(), 0);
    QVERIFY(m_service->persist());
}

void TstTaskService::unreadableStorageBlocksWrites()
{
    // 坏文件必须保持原样：读不懂就停止写入，等用户自己修，不能拿空仓库覆盖。
    const QByteArray broken = QByteArrayLiteral("{\"not\":\"an array\"}");
    QFile file(path());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(broken);
    file.close();

    TaskService service(path());
    QSignalSpy errors(&service, &TaskService::storageError);
    QVERIFY(!service.persist());                    // 读不懂时拒绝写入
    QCOMPARE(errors.count(), 1);

    const QString id = service.createTask(QStringLiteral("新任务"));
    QVERIFY(!id.isEmpty());                         // 内存里照常可用
    QVERIFY(service.repository()->exists(id));      // 降级是静默的，不弹窗打断输入
    QCOMPARE(errors.count(), 2);                    // 每次编辑都提示一次保存失败

    QFile check(path());
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), broken);              // 磁盘内容一字未改
}

void TstTaskService::largeSubtreeCompletionAndDeletion()
{
    QJsonArray tasks;
    Task root;
    root.id = QStringLiteral("root");
    root.title = root.id;
    tasks.append(root.toJson());
    for (int i = 0; i < 5000; ++i) {
        Task child;
        child.id = QString::number(i);
        child.title = child.id;
        child.parentId = root.id;
        tasks.append(child.toJson());
    }
    Task other;
    other.id = QStringLiteral("other");
    other.title = other.id;
    tasks.append(other.toJson());
    repo()->load(tasks);
    QSignalSpy updates(repo(), &TaskRepository::taskUpdated);
    m_service->setCompleted(root.id, true);
    QCOMPARE(updates.count(), 5001);
    for (int i = 0; i < 5000; ++i) QVERIFY(repo()->find(QString::number(i)).completed);
    QVERIFY(!repo()->find(other.id).completed);
    m_service->deleteTask(root.id);
    QCOMPARE(taskCount(), 1);
    QVERIFY(repo()->exists(other.id));
    QVERIFY(m_service->undoDelete());
    QCOMPARE(taskCount(), 5002);
    TaskService reopened(path());
    QVERIFY(reopened.repository()->find(QStringLiteral("4999")).completed);
}

QTEST_GUILESS_MAIN(TstTaskService)

#include "tst_task_service.moc"
