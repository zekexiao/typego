#include "ScreenshotStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>

#include <algorithm>

namespace {
constexpr int ThumbnailSize = 160;
const QString FilePrefix = QStringLiteral("shot_");
}

ScreenshotStore::ScreenshotStore(QString dataDir, QString cacheDir)
    : m_historyDir(QDir(dataDir).filePath(QStringLiteral("history")))
    , m_cacheDir(QDir(cacheDir).filePath(QStringLiteral("thumbnails")))
{
}

void ScreenshotStore::setHistoryLimit(int limit)
{
    m_limit = qBound(0, limit, 500);
    prune();
}

QString ScreenshotStore::record(const QImage &image, QString *error)
{
    m_lastError.clear();
    if (image.isNull()) {
        m_lastError = QStringLiteral("The screenshot is empty");
        if (error) *error = m_lastError;
        return {};
    }
    if (!QDir().mkpath(m_historyDir)) {
        m_lastError = QStringLiteral("Cannot create the screenshot history directory");
        if (error) *error = m_lastError;
        return {};
    }
    const QString name = FilePrefix
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    // 同一毫秒内连续截图时追加序号，避免覆盖上一张。
    QString path = QDir(m_historyDir).filePath(name + QStringLiteral(".png"));
    for (int index = 2; QFileInfo::exists(path); ++index)
        path = QDir(m_historyDir).filePath(name + QStringLiteral("_") + QString::number(index)
                                             + QStringLiteral(".png"));
    QImageWriter writer(path, "png");
    if (!writer.write(image)) {
        m_lastError = writer.errorString();
        if (error) *error = m_lastError;
        return {};
    }
    prune();
    return path;
}

QList<ShotHistoryEntry> ScreenshotStore::entries() const
{
    QList<ShotHistoryEntry> result;
    const QDir dir(m_historyDir);
    QFileInfoList files = dir.entryInfoList({QStringLiteral("*.png")}, QDir::Files);
    std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.lastModified() > b.lastModified();
    });
    result.reserve(files.size());
    for (const QFileInfo &file : files) {
        QImageReader reader(file.absoluteFilePath());
        const QSize size = reader.size();
        if (!size.isValid()) continue;
        ShotHistoryEntry entry;
        entry.id = file.fileName();
        entry.path = file.absoluteFilePath();
        entry.modified = file.lastModified().toMSecsSinceEpoch();
        entry.size = size;
        entry.thumbnail = QDir(m_cacheDir).filePath(file.completeBaseName() + QStringLiteral(".png"));
        result.append(entry);
    }
    return result;
}

QString ScreenshotStore::ensureThumbnail(const ShotHistoryEntry &entry) const
{
    if (entry.thumbnail.isEmpty()) return {};
    if (QFileInfo::exists(entry.thumbnail)) return entry.thumbnail;
    QImageReader reader(entry.path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) return {};
    image = image.scaled(ThumbnailSize, ThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!QDir().mkpath(m_cacheDir)) return {};
    if (!image.save(entry.thumbnail, "png")) return {};
    return entry.thumbnail;
}

ShotHistoryEntry ScreenshotStore::find(const QString &id) const
{
    for (const ShotHistoryEntry &entry : entries())
        if (entry.id == id) return entry;
    return {};
}

int ScreenshotStore::clear()
{
    int removed = 0;
    QDir history(m_historyDir);
    const QFileInfoList files = history.entryInfoList({QStringLiteral("*.png")}, QDir::Files);
    for (const QFileInfo &file : files)
        if (QFile::remove(file.absoluteFilePath())) ++removed;
    QDir cache(m_cacheDir);
    for (const QFileInfo &file : cache.entryInfoList({QStringLiteral("*.png")}, QDir::Files))
        QFile::remove(file.absoluteFilePath());
    return removed;
}

void ScreenshotStore::prune()
{
    const QDir dir(m_historyDir);
    QFileInfoList files = dir.entryInfoList({QStringLiteral("*.png")}, QDir::Files);
    std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.lastModified() > b.lastModified();
    });
    for (int index = m_limit; index < files.size(); ++index) {
        const QFileInfo &file = files.at(index);
        QFile::remove(QDir(m_cacheDir).filePath(file.completeBaseName() + QStringLiteral(".png")));
        QFile::remove(file.absoluteFilePath());
    }
}