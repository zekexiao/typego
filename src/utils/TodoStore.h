#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QList>

// 一层结构的待办事项：只有标题、完成状态、优先级和可选截止时间。
// 刻意不做子任务、关联和分组，条目之间没有任何引用关系，便于将来整项同步。
struct TodoItem {
    QString id;              // 创建时生成的 UUID，恢复与同步的稳定锚点，永不变更
    QString title;           // 单行文字，trim 后非空，超长在加载与提交时截断
    bool done = false;
    qint64 completedAt = 0;  // epoch ms；0 表示未完成，已完成组按它倒序展示
    int priority = 1;        // 0 低 / 1 中 / 2 高，加载时非法值回退中
    QString due;             // 可选截止，本地墙钟："YYYY-MM-DD" 或 "YYYY-MM-DDTHH:mm"；空=无截止。
                             // 不存 UTC：纯日期跨时区会整体偏移一天，墙钟对个人待办语义也更直观
    qint64 createdAt = 0;
    qint64 updatedAt = 0;    // 每次修改都刷新，将来按 id 整项 LWW 合并
    qint64 deletedAt = 0;    // 墓碑：>0 表示已删除，界面不展示，供同步与恢复；加载时清理过期墓碑
};

// 待办数据的读写、排序与日期解析；纯 Qt Core 逻辑，界面与单元测试共用。
class TodoStore {
public:
    static constexpr int MaxTitleLength = 512;
    // 已删除条目保留 30 天供同步，过期在加载时物理清除。
    static constexpr qint64 TombstoneRetentionMs = 30 * 24 * 3600 * 1000LL;

    explicit TodoStore(QString filePath);

    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr);

    const QList<TodoItem> &items() const { return m_items; }
    QList<TodoItem> &items() { return m_items; }

    // 展示顺序：未完成在前（优先级降序 → 截止升序且无截止靠后 → 创建时间升序），
    // 已完成按完成时间倒序；墓碑一律不参与展示。
    static QList<TodoItem> sorted(const QList<TodoItem> &items);
    static QString newId();

    // 由日期与可选时间生成标准截止字符串；日期无效返回空串。
    static QString makeDue(const QDate &date, const QTime &time = QTime());
    static bool dueHasTime(const QString &due);
    // 截止时间的墙钟解释：纯日期视为当天 0 点；空串或无法解析返回无效 QDateTime。
    static QDateTime dueDateTime(const QString &due);

private:
    QString m_filePath;
    QList<TodoItem> m_items;
    bool m_unreadable = false; // 原文件解析失败过，首次保存前先备份再覆盖
};
