#include <QtTest>

#include <QFile>

#include "todo/JsonTaskStorage.h"
#include "todo/TaskRepository.h"

// todo.json 是用户唯一的数据文件，读写行为出问题就是丢数据。
// 所有用例都在 QTemporaryDir 里跑，绝不碰用户 AppData。
class TstJsonTaskStorage : public QObject
{
    Q_OBJECT

private:
    static Task makeTask(const QString &id, const QString &title)
    {
        Task t;
        t.id = id;
        t.title = title;
        t.createdTime = QDateTime(QDate(2026, 10, 20), QTime(10, 0));
        t.updatedTime = t.createdTime;
        return t;
    }

    QString writeRaw(const QString &name, const QByteArray &content) const
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return {};
        file.write(content);
        file.close();
        return path;
    }

private slots:
    void initTestCase();
    void missingFileIsNotAnError();
    void saveThenLoadRoundTrip();
    void createsMissingParentDirectory();
    void rejectsNonArrayDocument();
    void skipsUnusableEntries();
    void writesHumanReadableUtf8();
    void saveFailureIsReported();

private:
    QTemporaryDir m_dir;
};

void TstJsonTaskStorage::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void TstJsonTaskStorage::missingFileIsNotAnError()
{
    // 首次启动没有 todo.json 属于正常情况，必须返回 true，否则 TaskService 会永久拒绝保存。
    JsonTaskStorage storage(m_dir.filePath(QStringLiteral("absent.json")));
    TaskRepository repo;
    QVERIFY(storage.load(&repo));
    QVERIFY(repo.tasks().isEmpty());
}

void TstJsonTaskStorage::saveThenLoadRoundTrip()
{
    const QString path = m_dir.filePath(QStringLiteral("roundtrip.json"));
    TaskRepository source;
    source.add(makeTask(QStringLiteral("a"), QStringLiteral("买焊丝")));
    Task child = makeTask(QStringLiteral("b"), QStringLiteral("排产"));
    child.parentId = QStringLiteral("a");
    child.completed = true;
    child.priority = 1;
    child.note = QStringLiteral("多行\n备注");
    child.dueTime = QDateTime(QDate(2026, 11, 1), QTime(9, 30));
    source.add(child);

    JsonTaskStorage storage(path);
    QVERIFY(storage.save(&source));

    TaskRepository target;
    QVERIFY(storage.load(&target));
    QCOMPARE(target.tasks().size(), 2);
    const Task reloaded = target.find(QStringLiteral("b"));
    QCOMPARE(reloaded.title, QStringLiteral("排产"));
    QCOMPARE(reloaded.parentId, QStringLiteral("a"));
    QCOMPARE(reloaded.note, QStringLiteral("多行\n备注"));
    QCOMPARE(reloaded.completed, true);
    QCOMPARE(reloaded.priority, 1);
    QCOMPARE(reloaded.dueTime, child.dueTime);
    QCOMPARE(reloaded.createdTime, child.createdTime);
}

void TstJsonTaskStorage::createsMissingParentDirectory()
{
    // 安装后数据目录不存在也要能保存，不能静默丢一次写入。
    const QString path = m_dir.filePath(QStringLiteral("nested/deeper/todo.json"));
    TaskRepository repo;
    repo.add(makeTask(QStringLiteral("x"), QStringLiteral("深度目录")));
    JsonTaskStorage storage(path);
    QVERIFY(storage.save(&repo));
    QVERIFY(QFile::exists(path));

    TaskRepository reloaded;
    QVERIFY(storage.load(&reloaded));
    QCOMPARE(reloaded.find(QStringLiteral("x")).title, QStringLiteral("深度目录"));
}

void TstJsonTaskStorage::rejectsNonArrayDocument()
{
    const QString path = writeRaw(QStringLiteral("object.json"), QByteArrayLiteral("{\"tasks\":[]}"));
    QVERIFY(!path.isEmpty());
    JsonTaskStorage storage(path);
    TaskRepository repo;
    QVERIFY(!storage.load(&repo));
}

void TstJsonTaskStorage::skipsUnusableEntries()
{
    // 数组里混入非对象或无 id 的条目时，只丢这些条目，其它任务照常加载。
    const QString path = writeRaw(QStringLiteral("mixed.json"),
        QByteArrayLiteral("[1, \"text\", {}, {\"title\":\"无id\"}, {\"id\":\"ok\",\"title\":\"可用\"}]"));
    QVERIFY(!path.isEmpty());
    JsonTaskStorage storage(path);
    TaskRepository repo;
    QVERIFY(storage.load(&repo));
    QCOMPARE(repo.tasks().size(), 1);
    QCOMPARE(repo.find(QStringLiteral("ok")).title, QStringLiteral("可用"));
}

void TstJsonTaskStorage::writesHumanReadableUtf8()
{
    const QString path = m_dir.filePath(QStringLiteral("utf8.json"));
    TaskRepository repo;
    repo.add(makeTask(QStringLiteral("c"), QStringLiteral("焊接夹具验收")));
    QVERIFY(JsonTaskStorage(path).save(&repo));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray raw = file.readAll();
    file.close();
    // 直接落 UTF-8 原字符，用户手改文件或排查数据时能看到中文标题。
    QVERIFY(raw.contains(QString::fromUtf8("焊接夹具验收").toUtf8()));
    QVERIFY(!raw.contains("\\u"));
}

void TstJsonTaskStorage::saveFailureIsReported()
{
    // 目标位置是个已存在的目录，写入必然失败，返回值必须是 false 而不是假装成功。
    QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("blocked.json")));
    TaskRepository repo;
    repo.add(makeTask(QStringLiteral("d"), QStringLiteral("写不进")));
    QVERIFY(!JsonTaskStorage(m_dir.filePath(QStringLiteral("blocked.json"))).save(&repo));
}

QTEST_GUILESS_MAIN(TstJsonTaskStorage)

#include "tst_json_task_storage.moc"
