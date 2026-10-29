#pragma once

#include <QImage>
#include <QList>
#include <QSize>
#include <QString>

struct ShotHistoryEntry {
    QString id;
    QString path;
    QString thumbnail;
    qint64 modified = 0;
    QSize size;
};

// 截图历史落盘：原图放插件数据目录，缩略图放可清理的缓存目录。
class ScreenshotStore {
public:
    ScreenshotStore(QString dataDir, QString cacheDir);

    void setHistoryLimit(int limit);
    int historyLimit() const { return m_limit; }

    // 保存一张截图并返回文件路径；失败时写入 error。
    QString record(const QImage &image, QString *error);
    QList<ShotHistoryEntry> entries() const;
    QString ensureThumbnail(const ShotHistoryEntry &entry) const;
    ShotHistoryEntry find(const QString &id) const;
    int clear();
    QString lastError() const { return m_lastError; }

private:
    void prune();

    QString m_historyDir;
    QString m_cacheDir;
    int m_limit = 30;
    QString m_lastError;
};