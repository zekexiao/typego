#include "AppUsageStore.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QDebug>

#include <algorithm>

namespace {
// 落盘防抖间隔：连续启动多个应用只写一次文件；进程被强杀最多丢失这段时间的记录，可接受。
constexpr int SaveDebounceMs = 2000;
constexpr int FormatVersion = 1;
}

AppUsageStore::AppUsageStore(QString filePath, std::function<qint64()> now)
    : m_filePath(std::move(filePath))
    , m_now(std::move(now))
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SaveDebounceMs);
    QObject::connect(&m_saveTimer, &QTimer::timeout, [this]() { saveNow(); });
    load();
}

AppUsageStore::~AppUsageStore()
{
    // 进程退出时把防抖窗口内的记录落盘；saveNow 幂等，无脏数据时直接返回。
    saveNow();
}

void AppUsageStore::record(const QString &path)
{
    if (path.isEmpty()) return;

    const QString key = path.toLower();
    const qint64 now = m_now();
    Usage &u = m_entries[key];
    if (u.count < MaxCount) ++u.count;
    // 用户回拨系统时间时最近使用时间不倒退，避免排序突然乱掉。
    u.lastUsed = qMax(u.lastUsed, now);

    m_dirty = true;
    m_saveTimer.start();
}

void AppUsageStore::load()
{
    m_entries.clear();

    QFileInfo info(m_filePath);
    if (!info.exists()) return;
    if (info.size() > MaxFileSize) {
        qWarning() << "AppUsageStore: file too large, treating as empty:" << m_filePath;
        return;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "AppUsageStore: cannot open" << m_filePath << file.errorString();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();
    if (!doc.isObject() || root.value(QStringLiteral("version")).toInt() != FormatVersion) {
        // 损坏或版本不认识的文件按空数据处理，下一次保存会重写为合法内容。
        qWarning() << "AppUsageStore: invalid or unsupported file, treating as empty:" << m_filePath;
        return;
    }

    const qint64 now = m_now();
    const QJsonObject entries = root.value(QStringLiteral("entries")).toObject();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        const QJsonObject v = it.value().toObject();
        Usage u;
        u.count = quint32(qBound(0, v.value(QStringLiteral("count")).toInt(), int(MaxCount)));
        u.lastUsed = v.value(QStringLiteral("last")).toVariant().toLongLong();
        // 加载时丢弃过期条目，陈旧路径随 TTL 自然淘汰。
        if (now - u.lastUsed > TtlMs) continue;
        m_entries.insert(it.key(), u);
    }
    evictOverflow();
}

void AppUsageStore::evictOverflow()
{
    if (m_entries.size() <= MaxEntries) return;

    // 按 (count, lastUsed) 降序、路径升序排出确定性顺序，截断到上限。
    QStringList keys = m_entries.keys();
    std::sort(keys.begin(), keys.end(), [this](const QString &a, const QString &b) {
        const Usage ua = m_entries.value(a);
        const Usage ub = m_entries.value(b);
        if (ua.count != ub.count) return ua.count > ub.count;
        if (ua.lastUsed != ub.lastUsed) return ua.lastUsed > ub.lastUsed;
        return a < b;
    });
    for (int i = MaxEntries; i < keys.size(); ++i) m_entries.remove(keys.at(i));
}

void AppUsageStore::saveNow()
{
    m_saveTimer.stop();
    if (!m_dirty) return;

    // 保存前清理过期条目并执行上限淘汰；只影响排序，静默执行。
    const qint64 now = m_now();
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (now - it.value().lastUsed > TtlMs) it = m_entries.erase(it);
        else ++it;
    }
    evictOverflow();

    QJsonObject entries;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        QJsonObject v;
        v.insert(QStringLiteral("count"), int(it.value().count));
        v.insert(QStringLiteral("last"), QJsonValue::fromVariant(it.value().lastUsed));
        entries.insert(it.key(), v);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), FormatVersion);
    root.insert(QStringLiteral("entries"), entries);

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0
        || !file.commit()) {
        // 保存失败不弹窗不阻塞：内存数据仍然有效，下一次启动会重试。
        qWarning() << "AppUsageStore: save failed:" << m_filePath << file.errorString();
        return;
    }
    m_dirty = false;
}
