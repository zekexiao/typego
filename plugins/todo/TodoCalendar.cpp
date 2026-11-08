#include "TodoCalendar.h"
#include "ThemeManager.h"

#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

namespace TodoCalendarLayout {

QSize sizeHint()
{
    return {7 * CellWidth + 2 * Margin,
            HeaderHeight + WeekdayHeight + GridRows * CellHeight + 2 * Margin};
}

QRect headerRect()
{
    return {0, 0, 7 * CellWidth + 2 * Margin, HeaderHeight};
}

QRect weekdayRect(int column)
{
    return {Margin + column * CellWidth, HeaderHeight, CellWidth, WeekdayHeight};
}

QRect prevRect()
{
    return {Margin, 3, NavSize, HeaderHeight - 6};
}

QRect nextRect(int width)
{
    return {width - Margin - NavSize, 3, NavSize, HeaderHeight - 6};
}

QRect cellRect(int cell)
{
    return {Margin + (cell % 7) * CellWidth,
            HeaderHeight + WeekdayHeight + (cell / 7) * CellHeight,
            CellWidth, CellHeight};
}

QRect cellInner(const QRect &cell)
{
    return QRect(cell.center() - QPoint(12, 12), QSize(24, 24));
}

QDate gridStart(const QDate &month, Qt::DayOfWeek firstDayOfWeek)
{
    if (!month.isValid()) return QDate();
    const QDate first(month.year(), month.month(), 1);
    const int back = (first.dayOfWeek() - static_cast<int>(firstDayOfWeek) + 7) % 7;
    return first.addDays(-back);
}

int columnOf(Qt::DayOfWeek day, Qt::DayOfWeek firstDayOfWeek)
{
    return (static_cast<int>(day) - static_cast<int>(firstDayOfWeek) + 7) % 7;
}

int cellAt(const QPoint &pos, const QSize &size)
{
    // 先排除翻月热区，否则它们的下半截会落进网格命中区。
    if (prevRect().contains(pos) || nextRect(size.width()).contains(pos)) return -1;
    if (pos.x() < Margin || pos.x() >= Margin + 7 * CellWidth) return -1;
    if (pos.y() < HeaderHeight + WeekdayHeight) return -1;
    const int col = (pos.x() - Margin) / CellWidth;
    const int row = (pos.y() - HeaderHeight - WeekdayHeight) / CellHeight;
    if (row < 0 || row >= GridRows) return -1;
    return row * 7 + col;
}

QDate dateAt(const QDate &month, Qt::DayOfWeek firstDayOfWeek, int cell)
{
    const QDate start = gridStart(month, firstDayOfWeek);
    return start.isValid() && cell >= 0 ? start.addDays(cell) : QDate();
}

} // namespace TodoCalendarLayout

TodoCalendar::TodoCalendar(ThemeManager *theme, QWidget *parent)
    : QWidget(parent), m_theme(theme)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    const QDate today = QDate::currentDate();
    m_month = today;
    m_selected = today;
}

void TodoCalendar::setDate(const QDate &date)
{
    const QDate today = QDate::currentDate();
    m_month = date.isValid() ? date : today;
    m_selected = date.isValid() ? date : today;
    update();
}

void TodoCalendar::moveSelection(int days)
{
    const QDate moved = m_selected.addDays(days);
    if (!moved.isValid()) return;
    m_selected = moved;
    // 跨月时月份跟着选中日期走，否则方向键会移出可见网格。
    m_month = moved;
    m_hoverCell = -1;
    update();
}

void TodoCalendar::pickSelected()
{
    if (picked) picked(m_selected);
}

QSize TodoCalendar::sizeHint() const
{
    return TodoCalendarLayout::sizeHint();
}

QDate TodoCalendar::gridStart() const
{
    return TodoCalendarLayout::gridStart(m_month, QLocale::system().firstDayOfWeek());
}

int TodoCalendar::cellOfSelected() const
{
    const QDate start = gridStart();
    if (!start.isValid() || !m_selected.isValid()) return -1;
    return static_cast<int>(start.daysTo(m_selected));
}

void TodoCalendar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font = this->font();
    font.setPixelSize(13);

    // 头部与星期表头都按同一个 locale 渲染，避免年月与列顺序错位。
    const QLocale locale = QLocale::system();

    // 头部：左右翻月热区 + 居中年月。
    painter.setFont(font);
    painter.setPen(themeColor(&ThemeManager::text, QPalette::Text));
    painter.drawText(TodoCalendarLayout::headerRect(), Qt::AlignCenter,
                     locale.toString(m_month, QStringLiteral("yyyy MMMM")));
    const QRect prev = TodoCalendarLayout::prevRect();
    const QRect next = TodoCalendarLayout::nextRect(width());
    drawChevron(&painter, prev, true);
    drawChevron(&painter, next, false);

    // 星期表头随本地一周首日。
    painter.setPen(themeColor(&ThemeManager::textSecondary, QPalette::PlaceholderText));
    const Qt::DayOfWeek firstDay = locale.firstDayOfWeek();
    for (int col = 0; col < 7; ++col) {
        const auto day = static_cast<Qt::DayOfWeek>(((static_cast<int>(firstDay) - 1 + col) % 7) + 1);
        painter.drawText(TodoCalendarLayout::weekdayRect(col), Qt::AlignCenter,
                         locale.dayName(day, QLocale::ShortFormat));
    }

    // 六周网格：非本月置灰，今天描边，选中实心，悬停淡底。
    const QDate today = QDate::currentDate();
    const QDate start = gridStart();
    for (int cell = 0; cell < TodoCalendarLayout::GridRows * 7; ++cell) {
        const QDate date = start.addDays(cell);
        const QRect rect = TodoCalendarLayout::cellRect(cell);
        const bool otherMonth = date.month() != m_month.month();
        const bool selected = date == m_selected;
        const bool hovered = cell == m_hoverCell && !selected && !otherMonth;
        if (selected) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(themeColor(&ThemeManager::accentFill, QPalette::Highlight));
            painter.drawRoundedRect(TodoCalendarLayout::cellInner(rect), 8, 8);
        } else if (hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(themeColor(&ThemeManager::hover, QPalette::Mid));
            painter.drawRoundedRect(TodoCalendarLayout::cellInner(rect), 8, 8);
        } else if (date == today) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(themeColor(&ThemeManager::accent, QPalette::Highlight), 1.2));
            painter.drawRoundedRect(TodoCalendarLayout::cellInner(rect), 8, 8);
        }
        painter.setFont(font);
        painter.setPen(selected ? Qt::white
                        : otherMonth ? themeColor(&ThemeManager::placeholder, QPalette::PlaceholderText)
                                     : themeColor(&ThemeManager::text, QPalette::Text));
        painter.drawText(rect, Qt::AlignCenter, QString::number(date.day()));
    }

    // 键盘焦点落在哪一格：选中格的日期下补一条短下划线，避免只有实心底色时看不出焦点。
    if (hasFocus() && m_selected.month() == m_month.month()) {
        const int cell = cellOfSelected();
        if (cell >= 0) {
            const QRect box = TodoCalendarLayout::cellInner(TodoCalendarLayout::cellRect(cell));
            painter.setPen(QPen(themeColor(&ThemeManager::accent, QPalette::Highlight), 1.4));
            painter.drawLine(QPointF(box.left() + 6, box.bottom() + 3),
                             QPointF(box.right() - 6, box.bottom() + 3));
        }
    }
}

void TodoCalendar::mousePressEvent(QMouseEvent *event)
{
    // 先处理翻月热区；命中网格则记住按下的格子，抬起来才选。
    if (TodoCalendarLayout::prevRect().contains(event->pos())) {
        m_month = m_month.addMonths(-1);
        m_hoverCell = -1;
        update();
        return;
    }
    if (TodoCalendarLayout::nextRect(width()).contains(event->pos())) {
        m_month = m_month.addMonths(1);
        m_hoverCell = -1;
        update();
        return;
    }
    m_pressedCell = TodoCalendarLayout::cellAt(event->pos(), size());
    QWidget::mousePressEvent(event);
}

void TodoCalendar::mouseReleaseEvent(QMouseEvent *event)
{
    const int cell = TodoCalendarLayout::cellAt(event->pos(), size());
    if (m_pressedCell >= 0 && cell == m_pressedCell) {
        setDate(TodoCalendarLayout::dateAt(m_month, QLocale::system().firstDayOfWeek(), cell));
        if (picked) picked(m_selected);
    }
    m_pressedCell = -1;
    QWidget::mouseReleaseEvent(event);
}

void TodoCalendar::mouseMoveEvent(QMouseEvent *event)
{
    const int cell = TodoCalendarLayout::cellAt(event->pos(), size());
    if (cell != m_hoverCell) {
        m_hoverCell = cell;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void TodoCalendar::leaveEvent(QEvent *event)
{
    m_hoverCell = -1;
    update();
    QWidget::leaveEvent(event);
}

// 键盘路径：方向键按周/天移动，翻页键按月，回车或空格确认。
void TodoCalendar::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left: moveSelection(-1); return;
    case Qt::Key_Right: moveSelection(1); return;
    case Qt::Key_Up: moveSelection(-7); return;
    case Qt::Key_Down: moveSelection(7); return;
    case Qt::Key_PageUp: moveSelection(-28); return;
    case Qt::Key_PageDown: moveSelection(28); return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        pickSelected();
        return;
    case Qt::Key_Home: setDate(QDate::currentDate()); return;
    default: break;
    }
    QWidget::keyPressEvent(event);
}

QColor TodoCalendar::themeColor(ColorFn fn, QPalette::ColorRole fallback) const
{
    return m_theme ? (m_theme->*fn)() : palette().color(fallback);
}

void TodoCalendar::drawChevron(QPainter *painter, const QRect &hotspot, bool left) const
{
    QPen pen(themeColor(&ThemeManager::textSecondary, QPalette::PlaceholderText), 1.4);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    const qreal tipX = left ? hotspot.center().x() - 2 : hotspot.center().x() + 2;
    const qreal wingX = left ? tipX + 5 : tipX - 5;
    painter->drawPolyline(QPolygonF{QPointF(wingX, hotspot.center().y() - 4),
                                    QPointF(tipX, hotspot.center().y()),
                                    QPointF(wingX, hotspot.center().y() + 4)});
}
