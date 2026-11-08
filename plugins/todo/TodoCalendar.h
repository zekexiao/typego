#pragma once

#include <QDate>
#include <QRect>
#include <QSize>
#include <QWidget>
#include <functional>

class ThemeManager;

// 月历几何与网格换算：纯计算，不依赖控件状态，便于单元测试。
namespace TodoCalendarLayout {
constexpr int Margin = 8;
constexpr int CellWidth = 38;
constexpr int CellHeight = 28;
constexpr int HeaderHeight = 34;
constexpr int WeekdayHeight = 24;
constexpr int NavSize = 28;
constexpr int GridRows = 6;

QSize sizeHint();
QRect headerRect();
QRect weekdayRect(int column);
QRect prevRect();
QRect nextRect(int width);
QRect cellRect(int cell);
QRect cellInner(const QRect &cell);
// 网格起点：把当月 1 号所在周对齐到本地一周首日。
QDate gridStart(const QDate &month, Qt::DayOfWeek firstDayOfWeek);
// 星期几落在第几列（0 起），首日随本地设置。
int columnOf(Qt::DayOfWeek day, Qt::DayOfWeek firstDayOfWeek);
// 命中哪一格；落在页边、表头或翻月按钮上返回 -1。
int cellAt(const QPoint &pos, const QSize &size);
// 第格对应的日期。
QDate dateAt(const QDate &month, Qt::DayOfWeek firstDayOfWeek, int cell);
} // namespace TodoCalendarLayout

// 主题自绘月历：QCalendarWidget 的内部配色不受样式表控制，深色下不可读；
// 这里用 ThemeManager 的令牌自己画。点某天立即生效，方向键移动、回车选中。
class TodoCalendar : public QWidget {
public:
    explicit TodoCalendar(ThemeManager *theme, QWidget *parent = nullptr);

    void setDate(const QDate &date);
    QDate selectedDate() const { return m_selected; }
    // 供测试与键盘路径复用：按键盘移动选中的日期。
    void moveSelection(int days);
    void pickSelected();
    std::function<void(const QDate &)> picked;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    using ColorFn = QColor (ThemeManager::*)() const;

    QColor themeColor(ColorFn fn, QPalette::ColorRole fallback) const;
    QDate gridStart() const;
    int cellOfSelected() const;
    void drawChevron(QPainter *painter, const QRect &hotspot, bool left) const;

    ThemeManager *m_theme;
    QDate m_month;
    QDate m_selected;
    int m_hoverCell = -1;
    int m_pressedCell = -1;
};
