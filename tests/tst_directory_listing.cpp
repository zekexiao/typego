#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "utils/DirectoryListing.h"

class TstDirectoryListing : public QObject {
    Q_OBJECT
private slots:
    void pathsAndErrors()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(QDir(home.path()).mkdir("Documents"));
        for (const QString &input : {QStringLiteral("Documents"), QStringLiteral("~/Documents"), QStringLiteral("~\\Documents")}) {
            const auto result = DirectoryListing::list(input, home.path(), 10);
            QVERIFY(result.valid);
            QCOMPARE(result.path, home.filePath("Documents"));
        }
        QCOMPARE(DirectoryListing::list(QString(), home.path(), 10).path, home.path());
        const auto missing = DirectoryListing::list("missing", home.path(), 10);
        QVERIFY(!missing.valid);
        QVERIFY(missing.entries.isEmpty());
        QCOMPARE(missing.path, home.filePath("missing"));
        QFile file(home.filePath("file"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(!DirectoryListing::list(file.fileName(), home.path(), 10).valid);
    }
    void boundedSortedResults()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(QDir(home.path()).mkdir("z-folder"));
        for (int i = 199; i >= 0; --i) {
            QFile file(home.filePath(QStringLiteral("%1.txt").arg(i, 3, 10, QLatin1Char('0'))));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const auto result = DirectoryListing::list(home.path(), home.path(), 3);
        QVERIFY(result.valid);
        QVERIFY(result.truncated);
        QCOMPARE(result.entries.size(), 3);
        QCOMPARE(result.entries.at(0).fileName(), "z-folder");
        QCOMPARE(result.entries.at(1).fileName(), "000.txt");
        QCOMPARE(result.entries.at(2).fileName(), "001.txt");
        const auto all = DirectoryListing::list(home.path(), home.path(), 201);
        QCOMPARE(all.entries.size(), 201);
        QVERIFY(!all.truncated);
        QCOMPARE(DirectoryListing::list(home.path(), home.path(), 0).entries.size(), 0);
    }
    void cancelledQueryHasNoResults()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const auto cancel = std::make_shared<std::atomic_bool>(true);
        const auto result = DirectoryListing::list(home.path(), home.path(), 10, cancel);
        QVERIFY(result.cancelled);
        QVERIFY(result.entries.isEmpty());
    }
};
QTEST_GUILESS_MAIN(TstDirectoryListing)
#include "tst_directory_listing.moc"
