#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <QTemporaryDir>

#include "ClipboardIcons.h"
#include "ClipboardStore.h"

// 只验证历史存储与缩略图规则，不访问系统剪贴板或真实用户目录。
class TstClipboardStore : public QObject {
    Q_OBJECT
private slots:
    void imageCaptureCreatesThumbnail();
    void duplicatesMoveToTopAndPersist();
    void evictionRemovesImageAndThumbnail();
    void clearRemovesImages();
    void tamperedIndexIsIgnored();
    void typeIconsAreGenerated();
    void corruptedIndexIsBackedUpNotOverwritten();
    void indexStaysSmallWhenTextsAreLarge();
    void legacyInlineTextIndexStillLoads();
};

void TstClipboardStore::imageCaptureCreatesThumbnail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ClipboardStore store(dir.filePath(QStringLiteral("data")), dir.filePath(QStringLiteral("cache")));
    store.setMaxItems(10);
    QImage image(200, 100, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QVERIFY(store.captureImage(image, 1000));
    QVERIFY(store.save());
    QCOMPARE(store.items().size(), 1);
    const ClipboardItem &item = store.items().first();
    QVERIFY(QFileInfo::exists(store.imagePath(item)));
    const QString thumbnail = store.thumbnailPath(item);
    QVERIFY(QFileInfo::exists(thumbnail));
    // 保持宽高比，长边缩到 64px。
    QCOMPARE(QImage(thumbnail).size(), QSize(64, 32));
}

void TstClipboardStore::duplicatesMoveToTopAndPersist()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString data = dir.filePath(QStringLiteral("data"));
    ClipboardStore store(data, dir.filePath(QStringLiteral("cache")));
    store.setMaxItems(10);
    QVERIFY(store.captureText(QStringLiteral("hello"), 1000));
    QVERIFY(store.save());
    QVERIFY(store.captureText(QStringLiteral("world"), 2000));
    QVERIFY(store.captureText(QStringLiteral("hello"), 3000));
    QVERIFY(store.save());
    QCOMPARE(store.items().size(), 2);
    QCOMPARE(store.items().first().text, QStringLiteral("hello"));
    QCOMPARE(store.items().first().timestamp, 3000);

    ClipboardStore reloaded(data, dir.filePath(QStringLiteral("cache")));
    reloaded.setMaxItems(10);
    reloaded.load();
    QCOMPARE(reloaded.items().size(), 2);
    QCOMPARE(reloaded.items().first().text, QStringLiteral("hello"));
    QCOMPARE(reloaded.items().last().text, QStringLiteral("world"));
}

void TstClipboardStore::evictionRemovesImageAndThumbnail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ClipboardStore store(dir.filePath(QStringLiteral("data")), dir.filePath(QStringLiteral("cache")));
    store.setMaxItems(2);
    QImage image(40, 40, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    QVERIFY(store.captureImage(image, 1000));
    const QString imagePath = store.imagePath(store.items().first());
    const QString thumbnail = store.thumbnailPath(store.items().first());
    QVERIFY(QFileInfo::exists(imagePath));
    QVERIFY(QFileInfo::exists(thumbnail));
    image.fill(Qt::green);
    QVERIFY(store.captureImage(image, 2000));
    image.fill(Qt::yellow);
    QVERIFY(store.captureImage(image, 3000));
    QVERIFY(store.save());
    QCOMPARE(store.items().size(), 2);
    QVERIFY(!QFileInfo::exists(imagePath));
    QVERIFY(!QFileInfo::exists(thumbnail));
}

void TstClipboardStore::clearRemovesImages()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString data = dir.filePath(QStringLiteral("data"));
    ClipboardStore store(data, dir.filePath(QStringLiteral("cache")));
    QImage image(40, 40, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    QVERIFY(store.captureImage(image, 1000));
    QVERIFY(store.captureText(QStringLiteral("text"), 2000));
    const QString imagePath = store.imagePath(store.items().last());
    store.clear();
    QVERIFY(store.items().isEmpty());
    QVERIFY(!QFileInfo::exists(imagePath));
    QVERIFY(store.save());
    ClipboardStore reloaded(data, dir.filePath(QStringLiteral("cache")));
    reloaded.load();
    QVERIFY(reloaded.items().isEmpty());
}

void TstClipboardStore::tamperedIndexIsIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString data = dir.filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(data));
    QFile index(QDir(data).filePath(QStringLiteral("history.json")));
    QVERIFY(index.open(QIODevice::WriteOnly));
    index.write("{\"version\":1,\"items\":["
                "{\"id\":\"../escape\",\"type\":\"text\",\"time\":1,\"hash\":\"aa\",\"text\":\"x\"},"
                "{\"id\":\"safe\",\"type\":\"image\",\"time\":2,\"hash\":\"bb\","
                "\"image\":\"../escape.png\",\"width\":10,\"height\":10}]}");
    index.close();
    ClipboardStore store(data, dir.filePath(QStringLiteral("cache")));
    store.load();
    QVERIFY(store.items().isEmpty());
}

void TstClipboardStore::typeIconsAreGenerated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString cache = dir.filePath(QStringLiteral("cache"));
    QSet<QString> paths;
    for (const auto type : {ClipboardItem::Type::Text, ClipboardItem::Type::Files, ClipboardItem::Type::Image}) {
        const QString path = clipboardTypeIcon(type, cache);
        QVERIFY(!path.isEmpty());
        QVERIFY(QFileInfo::exists(path));
        const QImage icon(path);
        QCOMPARE(icon.size(), QSize(64, 64));
        QVERIFY(icon.pixelColor(32, 32).alpha() > 0);
        paths.insert(path);
    }
    // 三种类型各自缓存一份，重复请求复用同一文件；无缓存目录时不生成。
    QCOMPARE(paths.size(), 3);
    QCOMPARE(clipboardTypeIcon(ClipboardItem::Type::Text, cache), clipboardTypeIcon(ClipboardItem::Type::Text, cache));
    QCOMPARE(clipboardTypeIcon(ClipboardItem::Type::Text, QString()), QString());
}

void TstClipboardStore::corruptedIndexIsBackedUpNotOverwritten()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString data = dir.filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(data));
    // 已经存在的图片条目：索引损坏时不能被当成孤儿删掉。
    const QString imageDir = QDir(data).filePath(QStringLiteral("images"));
    QVERIFY(QDir().mkpath(imageDir));
    const QString imagePath = QDir(imageDir).filePath(QStringLiteral("keep.png"));
    QImage image(10, 10, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(imagePath, "PNG"));

    const QString indexPath = QDir(data).filePath(QStringLiteral("history.json"));
    QFile index(indexPath);
    QVERIFY(index.open(QIODevice::WriteOnly));
    index.write("{not json");
    index.close();

    ClipboardStore store(data, dir.filePath(QStringLiteral("cache")));
    store.load();
    QVERIFY(store.indexCorrupted());
    QVERIFY(!store.lastError().isEmpty());
    // 索引读不出来时磁盘上的历史文件要原样保留。
    QVERIFY(QFileInfo::exists(imagePath));
    QVERIFY(QFileInfo::exists(indexPath));

    QVERIFY(store.captureText(QStringLiteral("new"), 1000));
    QVERIFY(store.save());
    // 损坏的索引改名备份，新索引正常写出来。
    QVERIFY(QFileInfo::exists(QDir(data).filePath(QStringLiteral("history.corrupt.json"))));
    QVERIFY(!store.indexCorrupted());

    ClipboardStore reloaded(data, dir.filePath(QStringLiteral("cache")));
    reloaded.load();
    QCOMPARE(reloaded.items().size(), 1);
    QCOMPARE(reloaded.items().first().text, QStringLiteral("new"));
}

void TstClipboardStore::indexStaysSmallWhenTextsAreLarge()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString data = dir.filePath(QStringLiteral("data"));
    ClipboardStore store(data, dir.filePath(QStringLiteral("cache")));
    store.setMaxItems(50);
    const QString bigText(50000, QLatin1Char('x'));
    for (int i = 0; i < 20; ++i) {
        QVERIFY(store.captureText(bigText + QString::number(i), 1000 + i));
    }
    QVERIFY(store.save());
    // 索引只存元数据，正文单独放文件，重写索引的开销与正文大小无关。
    const QFileInfo indexInfo(QDir(data).filePath(QStringLiteral("history.json")));
    QVERIFY(indexInfo.exists());
    QVERIFY(indexInfo.size() < 20000);

    ClipboardStore reloaded(data, dir.filePath(QStringLiteral("cache")));
    reloaded.load();
    QCOMPARE(reloaded.items().size(), 20);
    // 正文完整落盘，只是多了一个序号后缀。
    QCOMPARE(reloaded.items().first().text.size(), bigText.size() + 2);
    QVERIFY(reloaded.items().first().text.startsWith(bigText));
}

void TstClipboardStore::legacyInlineTextIndexStillLoads()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString data = dir.filePath(QStringLiteral("data"));
    QVERIFY(QDir().mkpath(data));
    QFile index(QDir(data).filePath(QStringLiteral("history.json")));
    QVERIFY(index.open(QIODevice::WriteOnly));
    index.write("{\"version\":1,\"items\":["
                "{\"id\":\"legacy\",\"type\":\"text\",\"time\":1,\"hash\":\"aabb\",\"text\":\"old text\"}]}");
    index.close();
    ClipboardStore store(data, dir.filePath(QStringLiteral("cache")));
    store.load();
    QCOMPARE(store.items().size(), 1);
    QCOMPARE(store.items().first().text, QStringLiteral("old text"));
}

QTEST_GUILESS_MAIN(TstClipboardStore)
#include "tst_clipboard_store.moc"