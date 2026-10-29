#include <QtTest>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <QTemporaryDir>

#include "ScreenshotStore.h"

// 只验证截图历史的落盘、缩略图和淘汰规则，不访问真实用户目录或屏幕。
class TstScreenshotStore : public QObject {
    Q_OBJECT
private slots:
    void recordSavesImageAndThumbnail();
    void historyLimitEvictsOldest();
    void clearRemovesFiles();
    void emptyImageIsRejected();
};

void TstScreenshotStore::recordSavesImageAndThumbnail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ScreenshotStore store(dir.filePath(QStringLiteral("data")), dir.filePath(QStringLiteral("cache")));
    store.setHistoryLimit(10);
    QImage image(200, 100, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QString error;
    const QString path = store.record(image, &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    QVERIFY(QFileInfo::exists(path));

    const QList<ShotHistoryEntry> entries = store.entries();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().size, QSize(200, 100));
    QCOMPARE(store.find(entries.first().id).path, path);

    const QString thumbnail = store.ensureThumbnail(entries.first());
    QVERIFY(!thumbnail.isEmpty());
    QVERIFY(QFileInfo::exists(thumbnail));
    // 保持宽高比，长边缩到 160px。
    QCOMPARE(QImage(thumbnail).size(), QSize(160, 80));
    // 缩略图存在后复用，不重复生成。
    QCOMPARE(store.ensureThumbnail(entries.first()), thumbnail);
}

void TstScreenshotStore::historyLimitEvictsOldest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ScreenshotStore store(dir.filePath(QStringLiteral("data")), dir.filePath(QStringLiteral("cache")));
    store.setHistoryLimit(2);
    QSet<QString> paths;
    for (int index = 0; index < 3; ++index) {
        QImage image(32, 32, QImage::Format_ARGB32);
        image.fill(index == 0 ? Qt::red : index == 1 ? Qt::green : Qt::blue);
        QString error;
        const QString path = store.record(image, &error);
        QVERIFY2(!path.isEmpty(), qPrintable(error));
        QVERIFY(!paths.contains(path));
        paths.insert(path);
    }
    const QList<ShotHistoryEntry> entries = store.entries();
    QCOMPARE(entries.size(), 2);
}

void TstScreenshotStore::clearRemovesFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ScreenshotStore store(dir.filePath(QStringLiteral("data")), dir.filePath(QStringLiteral("cache")));
    QImage image(40, 40, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    const QString path = store.record(image, nullptr);
    QVERIFY(!path.isEmpty());
    QVERIFY(!store.ensureThumbnail(store.entries().first()).isEmpty());
    QCOMPARE(store.clear(), 1);
    QVERIFY(store.entries().isEmpty());
    QVERIFY(!QFileInfo::exists(path));
}

void TstScreenshotStore::emptyImageIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ScreenshotStore store(dir.filePath(QStringLiteral("data")), dir.filePath(QStringLiteral("cache")));
    QString error;
    QVERIFY(store.record(QImage(), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(TstScreenshotStore)
#include "tst_screenshot_store.moc"