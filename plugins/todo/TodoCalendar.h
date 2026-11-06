#pragma once

#include <QWidget>
#include <QDate>
#include <functional>

class ThemeManager;

// 主题自绘月历：QCalendarWidget 的内部配色不受样式表控制，深色下不可读；
// 这里用 ThemeManager 的令牌自己画，行为对齐「点某天立即生效」。
class TodoCalendar : public QWidget {
public:
    explicit TodoCalendar(ThemeManager *theme, QWidget *parent = nullptr);

    void setDate(const QDate &date);
    QDate date() const;
    std::function<void(const QDate &)> picked;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    using ColorFn = QColor (ThemeManager::*)() const;

    QColor themeColor(ColorFn fn, QPalette::ColorRole fallback) const;
    QRect prevRect() const;
    QRect nextRect() const;
    QDate gridStart() const;
    QRect cellRect(int cell) const;
    static QRect cellInner(const QRect &rect);
    int cellAt(const QPoint &pos) const;
    void drawChevron(QPainter *painter, const QRect &hotspot, bool left) const;

    ThemeManager *m_theme;
    QDate m_month;
    QDate m_selected;
    int m_hoverCell = -1;
    int m_pressedCell = -1;
};
