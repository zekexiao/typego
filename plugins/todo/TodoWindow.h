#pragma once

#include "utils/TodoStore.h"

#include <QList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class ThemeManager;

// 待办主窗口：顶部输入即添加，列表内勾选、行内编辑，行尾图标改优先级与截止时间。
// 所有变更即时落盘，没有未保存状态；布局规范见 DESIGN.md。
class TodoWindow : public QWidget {
    Q_OBJECT
public:
    // 列表条目携带的数据角色，委托绘制与编辑回调共用。
    enum Role {
        IdRole = Qt::UserRole,        // 任务 id
        DoneRole,                     // 完成状态
        PriorityRole,                 // 优先级（0 低 / 1 中 / 2 高 / 3 紧急），供图标提示
        MetaRole,                     // 行尾元信息（优先级、截止），已组装好
        OverdueRole,                  // 元信息是否整体转错误色
        HeaderRole,                   // 分组头行（已完成 · N）
        DueFullRole,                  // 完整截止日期，供行尾图标提示用
    };

    TodoWindow(const QString &storageDirectory, ThemeManager *theme, QWidget *parent = nullptr);

    // 优先级弹层选中后回调；做成公开槽，弹层在委托的鼠标事件栈里也能安全打开。
    void setPriority(const QString &id, int priority);
    // 列出未完成条目的行矩形，供截图夹具定位行尾图标。
    QList<QRect> rowRects() const;

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildLayout();
    void rebuild();
    void persist();
    void addItem();
    void commitEditById(const QString &id, const QString &text);
    void toggleDone(const QString &id);
    void pickPriority(const QString &id, const QRect &rowRect);
    void removeItem(const QString &id);
    void setDue(const QString &id, const QString &due);
    void pickDue(const QString &id, const QRect &rowRect);
    void clearDone();
    void updateFooter();
    void placeEmptyLabel();

    TodoItem *find(const QString &id);
    // 截止时间的展示文案；overdue 返回是否未完成且已过截止。
    QString dueText(const TodoItem &item, bool *overdue) const;
    // 完整截止文案（含年份），只用于提示，不占行内宽度。
    QString dueFullText(const TodoItem &item) const;
    static QString dayLabel(const QDate &date, const QDate &today);

    TodoStore m_store;
    ThemeManager *m_theme = nullptr;
    QLineEdit *m_input = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_empty = nullptr;
    QLabel *m_footer = nullptr;
    QPushButton *m_clearButton = nullptr;
};
