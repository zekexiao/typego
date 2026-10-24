#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "utils/PinStorage.h"

class TstPinStorage : public QObject {
    Q_OBJECT
private slots:
    void roundTripAndEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("pins.json");
        PinStorage store(path);
        QVERIFY(store.load());
        QVERIFY(store.missing());
        QVERIFY(store.save({{"Calculator", "calc", ""}}));
        PinStorage reopened(path);
        QVERIFY(reopened.load());
        QCOMPARE(reopened.items().first().path, "calc");
        QVERIFY(reopened.save({}));
        PinStorage empty(path);
        QVERIFY(empty.load());
        QVERIFY(!empty.missing());
        QVERIFY(empty.items().isEmpty());
    }
    void malformedDataIsPreserved()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("pins.json");
        for (const QByteArray &bytes : {QByteArray("broken"), QByteArray("[{\"name\":\"missing path\"}]")}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(bytes), bytes.size());
            file.close();
            PinStorage store(path);
            QVERIFY(!store.load());
            QVERIFY(!store.save({{"New", "new.exe", ""}}));
            QVERIFY(!store.error().isEmpty());
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), bytes);
        }
    }
    void failedSaveKeepsPreviousPinsAndCanRetry()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("pins.json");
        const QString backup = dir.filePath("saved.json");
        PinStorage store(path);
        QVERIFY(store.load());
        QVERIFY(store.save({{"Old", "old.exe", ""}}));
        QVERIFY(QFile::rename(path, backup));
        // 同名目录使原子提交失败，不修改权限或真实用户文件。
        QVERIFY(QDir().mkdir(path));
        QVERIFY(!store.save({{"New", "new.exe", ""}}));
        QCOMPARE(store.items().first().path, "old.exe");
        QVERIFY(!store.error().isEmpty());
        QVERIFY(QDir().rmdir(path));
        QVERIFY(QFile::rename(backup, path));
        QVERIFY(store.save({{"New", "new.exe", ""}}));
        PinStorage reopened(path);
        QVERIFY(reopened.load());
        QCOMPARE(reopened.items().first().path, "new.exe");
    }
};
QTEST_GUILESS_MAIN(TstPinStorage)
#include "tst_pin_storage.moc"
