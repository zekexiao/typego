#include "ClipboardStore.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <algorithm>
#include <utility>

namespace {
// 单条上限：避免把超大文本或图片整块写进历史，拖慢落盘与查询。
constexpr qsizetype MaxTextChars = 200000;
constexpr int MaxFilesPerItem = 64;
constexpr qint64 MaxImagePixels = 25000000;
constexpr int MaxItemsLimit = 200;
// 结果图标按 32px 绘制，缩略图取 64px 兼顾高 DPI。
constexpr int ThumbnailSize = 64;

QByteArray hashOf(const QByteArray &prefix, const QByteArray &data)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(prefix);
    hash.addData(data);
    return hash.result();
}

QString typeName(ClipboardItem::Type type)
{
    switch (type) {
    case ClipboardItem::Type::Text: return QStringLiteral("text");
    case ClipboardItem::Type::Image: return QStringLiteral("image");
    case ClipboardItem::Type::Files: return QStringLiteral("files");
    }
    return QStringLiteral("text");
}

bool parseType(const QString &value, ClipboardItem::Type *type)
{
    if (value == QLatin1String("text")) *type = ClipboardItem::Type::Text;
    else if (value == QLatin1String("image")) *type = ClipboardItem::Type::Image;
    else if (value == QLatin1String("files")) *type = ClipboardItem::Type::Files;
    else return false;
    return true;
}
} // namespace

ClipboardStore::ClipboardStore(QString dataDir, QString cacheDir)
    : m_dataDir(std::move(dataDir)), m_imageDir(QDir(m_dataDir).filePath(QStringLiteral("images"))),
      m_thumbDir(cacheDir.isEmpty() ? QString() : QDir(cacheDir).filePath(QStringLiteral("thumbnails")))
{
}

QString ClipboardStore::dataPath(const QString &name) const
{
    return QDir(m_dataDir).filePath(name);
}

void ClipboardStore::setMaxItems(int maxItems)
{
    m_maxItems = qBound(1, maxItems, MaxItemsLimit);
    if (m_items.size() > m_maxItems) trim();

}

void ClipboardStore::load()
{
    m_items.clear();
    QFile file(dataPath(QStringLiteral("history.json")));
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            m_lastError = QStringLiteral("Cannot read clipboard history index");
            return;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        const QJsonArray array = document.object().value(QStringLiteral("items")).toArray();
        if (error.error == QJsonParseError::NoError) {
            m_items.reserve(array.size());
            for (const auto &value : array) {
                const QJsonObject object = value.toObject();
                ClipboardItem item;
                item.id = object.value(QStringLiteral("id")).toString();
                item.timestamp = object.value(QStringLiteral("time")).toVariant().toLongLong();
                item.hash = QByteArray::fromHex(object.value(QStringLiteral("hash")).toString().toLatin1());
                if (item.id.isEmpty() || item.id != QFileInfo(item.id).fileName()
                    || item.hash.isEmpty() || !parseType(object.value(QStringLiteral("type")).toString(), &item.type))
                    continue;
                if (item.type == ClipboardItem::Type::Text) {
                    item.text = object.value(QStringLiteral("text")).toString();
                    if (item.text.isEmpty()) continue;
                } else if (item.type == ClipboardItem::Type::Files) {
                    for (const auto &path : object.value(QStringLiteral("files")).toArray())
                        item.files.append(path.toString());
                    if (item.files.isEmpty()) continue;
                } else {
                    item.imageName = object.value(QStringLiteral("image")).toString();
                    item.imageSize = QSize(object.value(QStringLiteral("width")).toInt(),
                                           object.value(QStringLiteral("height")).toInt());
                    // 索引只允许引用本目录下的图片文件，避免被篡改的路径越界读写。
                    if (item.imageName.isEmpty() || item.imageName != QFileInfo(item.imageName).fileName()
                        || !QFileInfo::exists(imagePath(item))) continue;
                }
                m_items.append(std::move(item));
            }
        } else {
            m_lastError = QStringLiteral("Cannot parse clipboard history index");
        }
    }
    if (m_items.size() > m_maxItems) trim();
    cleanOrphanImages();
    cleanOrphanThumbs();
}

bool ClipboardStore::save()
{
    if (m_dataDir.isEmpty()) {
        m_lastError = QStringLiteral("Clipboard data directory is unavailable");
        return false;
    }
    QDir().mkpath(m_dataDir);
    QJsonArray array;
    for (const auto &item : std::as_const(m_items)) {
        QJsonObject object{
            {QStringLiteral("id"), item.id},
            {QStringLiteral("type"), typeName(item.type)},
            {QStringLiteral("time"), double(item.timestamp)},
            {QStringLiteral("hash"), QString::fromLatin1(item.hash.toHex())}};
        if (item.type == ClipboardItem::Type::Text) object.insert(QStringLiteral("text"), item.text);
        else if (item.type == ClipboardItem::Type::Files) object.insert(QStringLiteral("files"), QJsonArray::fromStringList(item.files));
        else {
            object.insert(QStringLiteral("image"), item.imageName);
            object.insert(QStringLiteral("width"), item.imageSize.width());
            object.insert(QStringLiteral("height"), item.imageSize.height());
        }
        array.append(object);
    }
    QSaveFile file(dataPath(QStringLiteral("history.json")));
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QStringLiteral("Cannot open clipboard history index for writing");
        return false;
    }
    const QByteArray data = QJsonDocument(QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("items"), array}}).toJson(QJsonDocument::Compact);
    if (file.write(data) != data.size() || !file.commit()) {
        m_lastError = QStringLiteral("Cannot save clipboard history index");
        return false;
    }
    m_lastError.clear();
    return true;
}

void ClipboardStore::clear()
{
    for (const auto &item : std::as_const(m_items)) removeImage(item);
    m_items.clear();
    if (QDir(m_imageDir).exists()) QDir(m_imageDir).removeRecursively();
    if (!m_thumbDir.isEmpty() && QDir(m_thumbDir).exists()) QDir(m_thumbDir).removeRecursively();
}

const ClipboardItem *ClipboardStore::find(const QString &id) const
{
    const auto it = std::find_if(m_items.cbegin(), m_items.cend(), [&id](const ClipboardItem &item) { return item.id == id; });
    return it == m_items.cend() ? nullptr : &*it;
}

QString ClipboardStore::imagePath(const ClipboardItem &item) const
{
    return QDir(m_imageDir).filePath(item.imageName);
}

QString ClipboardStore::thumbnailPath(const ClipboardItem &item) const
{
    if (m_thumbDir.isEmpty() || item.type != ClipboardItem::Type::Image || item.id.isEmpty()) return {};
    return QDir(m_thumbDir).filePath(QStringLiteral("thumb-") + item.id + QStringLiteral(".png"));
}

bool ClipboardStore::writeThumbnail(const ClipboardItem &item, const QImage &image)
{
    const QString path = thumbnailPath(item);
    if (path.isEmpty() || image.isNull()) return false;
    const QImage thumbnail = image.scaled(ThumbnailSize, ThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (thumbnail.isNull() || !QDir().mkpath(m_thumbDir)) return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && thumbnail.save(&file, "PNG") && file.commit();
}

bool ClipboardStore::ensureThumbnail(const ClipboardItem &item)
{
    const QString path = thumbnailPath(item);
    if (path.isEmpty()) return false;
    if (QFileInfo::exists(path)) return true;
    return writeThumbnail(item, QImage(imagePath(item)));
}

QString ClipboardStore::newId(qint64 now)
{
    const QString base = QString::number(now, 36);
    QString id;
    do {
        id = base + QLatin1Char('-') + QString::number(++m_sequence, 36);
    } while (std::any_of(m_items.cbegin(), m_items.cend(), [&id](const ClipboardItem &item) { return item.id == id; }));
    return id;
}

bool ClipboardStore::touchByHash(const QByteArray &hash, qint64 now)
{
    const auto it = std::find_if(m_items.begin(), m_items.end(), [&hash](const ClipboardItem &item) { return item.hash == hash; });
    if (it == m_items.end()) return false;
    it->timestamp = now;
    if (it != m_items.begin()) m_items.move(int(it - m_items.begin()), 0);
    return true;
}

bool ClipboardStore::captureText(const QString &text, qint64 now)
{
    m_lastError.clear();
    if (text.isEmpty() || text.size() > MaxTextChars || text.trimmed().isEmpty()) return false;
    const QByteArray hash = hashOf("text", text.toUtf8());
    if (touchByHash(hash, now)) return true;
    ClipboardItem item;
    item.id = newId(now);
    item.type = ClipboardItem::Type::Text;
    item.text = text;
    item.hash = hash;
    item.timestamp = now;
    m_items.prepend(std::move(item));
    trim();
    return true;
}

bool ClipboardStore::captureFiles(const QStringList &files, qint64 now)
{
    m_lastError.clear();
    QStringList paths;
    for (const QString &file : files) {
        if (file.isEmpty()) continue;
        paths.append(file);
        if (paths.size() >= MaxFilesPerItem) break;
    }
    if (paths.isEmpty()) return false;
    const QByteArray hash = hashOf("files", paths.join(QLatin1Char('\n')).toUtf8());
    if (touchByHash(hash, now)) return true;
    ClipboardItem item;
    item.id = newId(now);
    item.type = ClipboardItem::Type::Files;
    item.files = std::move(paths);
    item.hash = hash;
    item.timestamp = now;
    m_items.prepend(std::move(item));
    trim();
    return true;
}

bool ClipboardStore::captureImage(const QImage &image, qint64 now)
{
    m_lastError.clear();
    if (image.isNull() || qint64(image.width()) * qint64(image.height()) > MaxImagePixels) return false;
    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        m_lastError = QStringLiteral("Cannot encode clipboard image");
        return false;
    }
    const QByteArray hash = hashOf("image", png);
    // 相同图片已存在时不重复编码落盘，只更新顺序。
    if (touchByHash(hash, now)) return true;

    ClipboardItem item;
    item.id = newId(now);
    item.type = ClipboardItem::Type::Image;
    item.imageName = item.id + QStringLiteral(".png");
    item.imageSize = image.size();
    item.hash = hash;
    item.timestamp = now;
    if (!QDir().mkpath(m_imageDir)) {
        m_lastError = QStringLiteral("Cannot create clipboard image directory");
        return false;
    }
    QSaveFile file(imagePath(item));
    if (!file.open(QIODevice::WriteOnly) || file.write(png) != png.size() || !file.commit()) {
        m_lastError = QStringLiteral("Cannot save clipboard image");
        return false;
    }
    // 缩略图失败不影响历史，查询时会重试。
    writeThumbnail(item, image);
    m_items.prepend(std::move(item));
    trim();
    return true;
}

bool ClipboardStore::touch(const QString &id, qint64 now)
{
    const auto it = std::find_if(m_items.begin(), m_items.end(), [&id](const ClipboardItem &item) { return item.id == id; });
    if (it == m_items.end()) return false;
    it->timestamp = now;
    if (it != m_items.begin()) m_items.move(int(it - m_items.begin()), 0);
    return true;
}

void ClipboardStore::trim()
{
    while (m_items.size() > m_maxItems) removeImage(m_items.takeLast());
}

void ClipboardStore::removeImage(const ClipboardItem &item)
{
    if (item.type != ClipboardItem::Type::Image || item.imageName.isEmpty()) return;
    QFile::remove(imagePath(item));
    QFile::remove(thumbnailPath(item));
}

void ClipboardStore::cleanOrphanImages()
{
    QDir imageDir(m_imageDir);
    if (!imageDir.exists()) return;
    QSet<QString> referenced;
    for (const auto &item : std::as_const(m_items))
        if (item.type == ClipboardItem::Type::Image) referenced.insert(item.imageName);
    for (const QFileInfo &entry : imageDir.entryInfoList(QDir::Files))
        if (!referenced.contains(entry.fileName())) QFile::remove(entry.absoluteFilePath());
}

void ClipboardStore::cleanOrphanThumbs()
{
    QDir thumbDir(m_thumbDir);
    if (m_thumbDir.isEmpty() || !thumbDir.exists()) return;
    QSet<QString> referenced;
    for (const auto &item : std::as_const(m_items))
        if (item.type == ClipboardItem::Type::Image)
            referenced.insert(QFileInfo(thumbnailPath(item)).fileName());
    for (const QFileInfo &entry : thumbDir.entryInfoList(QDir::Files))
        if (!referenced.contains(entry.fileName())) QFile::remove(entry.absoluteFilePath());
}
