#pragma once
#include <QDateTime>
#include <QString>
#include <QList>

// 一层结构的待办事项：只有标题、完成状态、优先级和可选截止时间。
struct TodoItem {
    int priority = 1; // 0 低 / 1 中 / 2 高
    QString id;
    QString title;
    QDateTime due; // 无效值表示不设时间
    bool done = false;
};

// 待办数据的读写与展示排序；纯 Qt Core 逻辑，供界面与单元测试共用。
class TodoStore {
public:
    explicit TodoStore(QString filePath);

    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;

    const QList<TodoItem> &items() const { return m_items; }
    QList<TodoItem> &items() { return m_items; }

    // 展示顺序：未完成在前，优先级高的在前，有截止时间的按时间升序且排在无时间之前。
    static QList<TodoItem> sorted(const QList<TodoItem> &items);
    static QString newId();

private:
    QString m_filePath;
    QList<TodoItem> m_items;
};
