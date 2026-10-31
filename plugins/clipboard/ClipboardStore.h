#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

// 单条剪贴板历史。图片只保存磁盘文件名，内存中不保留原始像素。
struct ClipboardItem {
    enum class Type { Text, Image, Files };

    QString id;
    Type type = Type::Text;
    QString text;
    QStringList files;
    QString imageName;
    QSize imageSize;
    QByteArray hash;
    qint64 timestamp = 0;
};

// 历史内容与落盘都在数据目录内完成；条目数量与单条大小都有上限。
// 相同内容只保留一条，再次出现时置顶而不是重复记录。
// 索引只保存元数据，文本正文单独放在 texts 目录下，
// 这样每次复制重写索引的开销与历史正文的总大小无关。
class ClipboardStore {
public:
    ClipboardStore(QString dataDir, QString cacheDir);

    // 读取索引并清理已损坏或超出上限的条目。
    // 索引本身损坏时保留磁盘上的原始文件和图片，只记录错误，不做任何清理。
    void load();
    bool save();
    void clear();

    int maxItems() const { return m_maxItems; }
    void setMaxItems(int maxItems);

    const QList<ClipboardItem> &items() const { return m_items; }
    const ClipboardItem *find(const QString &id) const;
    QString imagePath(const ClipboardItem &item) const;
    // 缩略图放在可清理的缓存目录，缺失时由 ensureThumbnail 重建。
    QString thumbnailPath(const ClipboardItem &item) const;
    bool ensureThumbnail(const ClipboardItem &item);
    QString cacheDir() const { return m_cacheDir; }
    QString lastError() const { return m_lastError; }
    // 索引损坏时为真；save() 会先把原文件改名备份再重建索引。
    bool indexCorrupted() const { return m_indexCorrupted; }

    // 返回 true 表示历史内容有变化，需要调用 save()。
    bool captureText(const QString &text, qint64 now);
    bool captureFiles(const QStringList &files, qint64 now);
    bool captureImage(const QImage &image, qint64 now);
    // 把已有条目移到最前，复制回剪贴板后调用。
    bool touch(const QString &id, qint64 now);

private:
    bool touchByHash(const QByteArray &hash, qint64 now);
    void trim();
    void removePayload(const ClipboardItem &item);
    void cleanOrphanImages();
    void cleanOrphanThumbs();
    void cleanOrphanTexts();
    bool writeThumbnail(const ClipboardItem &item, const QImage &image);
    // 文本正文单独存文件；id 已经过 fileName 校验，拼不出目录穿越的路径。
    QString textPath(const ClipboardItem &item) const;
    QString readTextPayload(const QString &id) const;
    bool writeTextPayload(const ClipboardItem &item) const;
    bool backupCorruptedIndex();
    QString newId(qint64 now);
    QString dataPath(const QString &name) const;

    QString m_dataDir;
    QString m_imageDir;
    QString m_textDir;
    QString m_cacheDir;
    QString m_thumbDir;
    QList<ClipboardItem> m_items;
    QString m_lastError;
    int m_maxItems = 50;
    quint64 m_sequence = 0;
    bool m_indexCorrupted = false;
};