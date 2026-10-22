#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QTimer>

#include <functional>

// 记录应用被成功启动的次数与最近使用时间，用于搜索结果同档内的频次排序。
// 纯逻辑类：构造函数注入文件路径与时钟，测试可用 QTemporaryDir 隔离，不触碰用户数据目录。
// 只在 GUI 线程读写，无需锁；所有落盘失败静默降级为日志，绝不影响启动。
class AppUsageStore {
public:
    struct Usage {
        quint32 count = 0;    // 成功启动次数，到达上限后饱和
        qint64 lastUsed = 0;  // UTC 毫秒时间戳，取历史最大，系统时间回拨不倒退
    };

    // 计数饱和上限：再高频的使用也不需要更精细的区分度。
    static constexpr quint32 MaxCount = 9999;
    // 条目总数上限，超出时按 (count, lastUsed) 最小者淘汰，避免文件无限增长。
    static constexpr int MaxEntries = 1024;
    // 过期阈值：180 天未使用的条目在加载时丢弃，已卸载应用的陈旧路径由此自然清理。
    static constexpr qint64 TtlMs = qint64(180) * 24 * 60 * 60 * 1000;
    // 文件大小上限，超过视为损坏不解析，防止异常文件拖慢启动。
    static constexpr qint64 MaxFileSize = 1024 * 1024;

    explicit AppUsageStore(QString filePath,
                           std::function<qint64()> now = &QDateTime::currentMSecsSinceEpoch);
    ~AppUsageStore();

    // 启动成功后记录一次。空路径忽略；path 内部转小写后作为键（Windows 路径大小写不敏感）。
    // 只改内存并置脏，2 秒防抖后落盘；连续启动多个应用只写一次文件。
    void record(const QString &path);

    // 查询某个小写路径的使用数据；无记录时返回默认 Usage{0, 0}。
    // 调用方应复用 AppMatcher::Entry::lowerPath，避免每次按键重复小写化分配。
    Usage find(const QString &lowerPath) const { return m_entries.value(lowerPath); }

    // 立即落盘（防抖定时器触发与析构时调用）。先清理过期条目并执行上限淘汰，
    // 再用 QSaveFile 原子写入；失败只记日志，内存数据保持有效，下次仍会重试。
    void saveNow();

    // 清空全部使用记录并立即落盘（写入空的合法文件）。
    // 用于设置页的「清空应用使用记录」入口。
    void clear();

private:
    void load();
    // 淘汰超出上限的条目：保留 (count, lastUsed) 降序、路径升序的前 MaxEntries 条。
    void evictOverflow();

    QString m_filePath;
    std::function<qint64()> m_now;
    QHash<QString, Usage> m_entries;
    bool m_dirty = false;
    QTimer m_saveTimer;
};
