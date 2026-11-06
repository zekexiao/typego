#include "TodoCalendar.h"
#include "ThemeManager.h"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

namespace {
// 月历弹层几何常量。
constexpr int CalMargin = 8;
constexpr int CalCellWidth = 38;
constexpr int CalCellHeight = 28;
constexpr int CalHeaderHeight = 34;
constexpr int CalWeekdayHeight = 24;
constexpr int CalNavSize = 28;
}

TodoCalendar::TodoCalendar(ThemeManager *theme, QWidget *parent)
    : QWidget(parent), m_theme(theme)
{
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

QDate TodoCalendar::date() const
{
    return m_selected;
}

QSize TodoCalendar::sizeHint() const
{
    return {7 * CalCellWidth + 2 * CalMargin,
            CalHeaderHeight + CalWeekdayHeight + 6 * CalCellHeight + 2 * CalMargin};
}

void TodoCalendar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font = this->font();
    font.setPixelSize(13);

    // 头部：左右翻月热区 + 居中年月。
    painter.setFont(font);
    painter.setPen(themeColor(&ThemeManager::text, QPalette::Text));
    painter.drawText(QRect(0, 0, width(), CalHeaderHeight), Qt::AlignCenter,
                     QLocale::system().toString(m_month, QStringLiteral("yyyy MMMM")));
    drawChevron(&painter, prevRect(), true);
    drawChevron(&painter, nextRect(), false);

    // 星期表头随本地一周首日。
    painter.setPen(themeColor(&ThemeManager::textSecondary, QPalette::PlaceholderText));
    const QLocale locale;
    const int firstDow = static_cast<int>(locale.firstDayOfWeek());
    for (int col = 0; col < 7; ++col) {
        const auto day = static_cast<Qt::DayOfWeek>((firstDow - 1 + col) % 7 + 1);
        painter.drawText(QRect(CalMargin + col * CalCellWidth, CalHeaderHeight, CalCellWidth, CalWeekdayHeight),
                         Qt::AlignCenter, locale.dayName(day, QLocale::ShortFormat));
    }

    // 六周网格：非本月置灰，今天描边，选中实心，悬停淡底。
    const QDate today = QDate::currentDate();
    for (int cell = 0; cell < 42; ++cell) {
        const QDate date = gridStart().addDays(cell);
        const QRect rect = cellRect(cell);
        const bool otherMonth = date.month() != m_month.month();
        const bool selected = date == m_selected;
        const bool hovered = cell == m_hoverCell && !selected && !otherMonth;
        if (selected) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(themeColor(&ThemeManager::accentFill, QPalette::Highlight));
            painter.drawRoundedRect(cellInner(rect), 8, 8);
        } else if (hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(themeColor(&ThemeManager::hover, QPalette::Mid));
            painter.drawRoundedRect(cellInner(rect), 8, 8);
        } else if (date == today) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(themeColor(&ThemeManager::accent, QPalette::Highlight), 1.2));
            painter.drawRoundedRect(cellInner(rect), 8, 8);
        }
        painter.setFont(font);
        painter.setPen(selected ? Qt::white
                        : otherMonth ? themeColor(&ThemeManager::placeholder, QPalette::PlaceholderText)
                                     : themeColor(&ThemeManager::text, QPalette::Text));
        painter.drawText(rect, Qt::AlignCenter, QString::number(date.day()));
    }
}

void TodoCalendar::mousePressEvent(QMouseEvent *event)
{
    // 先处理翻月热区；命中网格则记住按下的格子，抬起来才选。
    if (prevRect().contains(event->pos())) {
        m_month = m_month.addMonths(-1);
        m_hoverCell = -1;
        update();
        return;
    }
    if (nextRect().contains(event->pos())) {
        m_month = m_month.addMonths(1);
        m_hoverCell = -1;
        update();
        return;
    }
    m_pressedCell = cellAt(event->pos());
    QWidget::mousePressEvent(event);
}

void TodoCalendar::mouseReleaseEvent(QMouseEvent *event)
{
    const int cell = cellAt(event->pos());
    if (m_pressedCell >= 0 && cell == m_pressedCell) {
        m_selected = gridStart().addDays(cell);
        update();
        if (picked) picked(m_selected);
    }
    m_pressedCell = -1;
    QWidget::mouseReleaseEvent(event);
}

void TodoCalendar::mouseMoveEvent(QMouseEvent *event)
{
    const int cell = cellAt(event->pos());
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

QColor TodoCalendar::themeColor(ColorFn fn, QPalette::ColorRole fallback) const
{
    return m_theme ? (m_theme->*fn)() : palette().color(fallback);
}

QRect TodoCalendar::prevRect() const
{
    return {CalMargin, 3, CalNavSize, CalHeaderHeight - 6};
}

QRect TodoCalendar::nextRect() const
{
    return {width() - CalMargin - CalNavSize, 3, CalNavSize, CalHeaderHeight - 6};
}

QDate TodoCalendar::gridStart() const
{
    const QDate first = QDate(m_month.year(), m_month.month(), 1);
    const int back = (first.dayOfWeek() - static_cast<int>(QLocale::system().firstDayOfWeek()) + 7) % 7;
    return first.addDays(-back);
}

QRect TodoCalendar::cellRect(int cell) const
{
    return {CalMargin + (cell % 7) * CalCellWidth,
            CalHeaderHeight + CalWeekdayHeight + (cell / 7) * CalCellHeight,
            CalCellWidth, CalCellHeight};
}

QRect TodoCalendar::cellInner(const QRect &rect)
{
    return QRect(rect.center() - QPoint(12, 12), QSize(24, 24));
}

int TodoCalendar::cellAt(const QPoint &pos) const
{
    if (pos.x() < CalMargin || pos.y() < CalHeaderHeight + CalWeekdayHeight) return -1;
    const int col = (pos.x() - CalMargin) / CalCellWidth;
    const int row = (pos.y() - CalHeaderHeight - CalWeekdayHeight) / CalCellHeight;
    if (col < 0 || col > 6 || row < 0 || row > 5) return -1;
    return row * 7 + col;
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
