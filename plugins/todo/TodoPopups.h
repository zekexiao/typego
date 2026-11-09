#pragma once

#include <QDate>
#include <QFrame>
#include <QTime>
#include <functional>

class QButtonGroup;
class QPushButton;
class QTimeEdit;
class ThemeManager;
class TodoCalendar;

// 弹出层的统一摆放：优先贴锚点上方，放不下再翻到下方，最后夹回屏幕内。
// anchor 是锚点矩形的全局坐标，popup 会被移动到相邻位置。
void placePopupNear(QWidget *popup, const QRect &anchor);

// 优先级弹层：四档旗标竖排一行一项，当前档位用底色和勾选态标出。
// 不用原生 QMenu：它会把勾选指示和图标挤进同一列，样式表改过之后看不出当前档位。
class TodoPriorityPopup final : public QFrame {
    Q_OBJECT
public:
    TodoPriorityPopup(ThemeManager *theme, int current, QWidget *parent = nullptr);

signals:
    void picked(int level);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void choose(int level);

    QButtonGroup *m_group = nullptr;
    QList<QPushButton *> m_buttons;
};

// 截止时间弹层：主题自绘月历 + 清除按钮与时间输入。
// 回调在关闭弹层前触发，调用方拿到结果后可以立即落盘并重建列表。
class TodoDuePopup final : public QFrame {
    Q_OBJECT
public:
    TodoDuePopup(ThemeManager *theme, const QDate &date, const QTime &time, QWidget *parent = nullptr);

    // 选中日期与时间（time 无效表示纯日期）。
    std::function<void(const QDate &date, const QTime &time)> duePicked;
    std::function<void()> dueCleared;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    TodoCalendar *m_calendar = nullptr;
    QTimeEdit *m_time = nullptr;
};

// 删除确认弹层：与优先级、截止时间用同一套弹层与摆位，不再弹原生消息框。
// 原生消息框会脱离行尾图标跳到屏幕中央，连续删多条时鼠标要来回跑。
class TodoDeletePopup final : public QFrame {
    Q_OBJECT
public:
    explicit TodoDeletePopup(const QString &title, QWidget *parent = nullptr);

    std::function<void()> confirmed;

protected:
    void keyPressEvent(QKeyEvent *event) override;
};
