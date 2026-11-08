#include <QtTest>
#include <QDate>

#include "TodoCalendar.h"

// 月历网格是纯计算：验证周边界、首日对齐、命中与日期回算，不构造控件。
class TstTodoCalendarLayout : public QObject {
    Q_OBJECT
private slots:
    void sizeCoversSixWeeks();
    void gridStartsOnLocalFirstDay();
    void columnFollowsFirstDayOfWeek();
    void cellAtRejectsHeaderAndNavButtons();
    void cellAtMapsGridRows();
    void dateAtRoundTripsWithCellAt();
};

namespace {
// 用周一为首日，与 en_US 之外的默认设置一致，断言不依赖机器 locale。
constexpr Qt::DayOfWeek Monday = Qt::Monday;
constexpr Qt::DayOfWeek Sunday = Qt::Sunday;
}

void TstTodoCalendarLayout::sizeCoversSixWeeks()
{
    const QSize size = TodoCalendarLayout::sizeHint();
    // 宽度 = 7 列 + 两侧边距；高度 = 头部 + 星期行 + 6 周 + 两侧边距。
    QCOMPARE(size.width(), 7 * TodoCalendarLayout::CellWidth + 2 * TodoCalendarLayout::Margin);
    QCOMPARE(size.height(),
             TodoCalendarLayout::HeaderHeight + TodoCalendarLayout::WeekdayHeight
                 + TodoCalendarLayout::GridRows * TodoCalendarLayout::CellHeight
                 + 2 * TodoCalendarLayout::Margin);
    // 网格最后一行的底边不能超出 sizeHint，否则弹层会被裁掉一周。
    const QRect last = TodoCalendarLayout::cellRect(TodoCalendarLayout::GridRows * 7 - 1);
    QVERIFY(last.bottom() + TodoCalendarLayout::Margin <= size.height());
}

void TstTodoCalendarLayout::gridStartsOnLocalFirstDay()
{
    // 2026-11-01 是周日：以周一为首日时网格从 10-26 开始，以周日为首日时从 11-01 开始。
    const QDate november(2026, 11, 1);
    QCOMPARE(TodoCalendarLayout::gridStart(november, Monday), QDate(2026, 10, 26));
    QCOMPARE(TodoCalendarLayout::gridStart(november, Sunday), QDate(2026, 11, 1));
    // 首日就是 1 号时不能倒推出一整周。
    QCOMPARE(TodoCalendarLayout::gridStart(november, Sunday).day(), 1);
    // 无效月份返回无效日期，而不是 invalid 加天数。
    QVERIFY(!TodoCalendarLayout::gridStart(QDate(), Monday).isValid());
}

void TstTodoCalendarLayout::columnFollowsFirstDayOfWeek()
{
    QCOMPARE(TodoCalendarLayout::columnOf(Qt::Monday, Monday), 0);
    QCOMPARE(TodoCalendarLayout::columnOf(Qt::Sunday, Monday), 6);
    QCOMPARE(TodoCalendarLayout::columnOf(Qt::Sunday, Sunday), 0);
    QCOMPARE(TodoCalendarLayout::columnOf(Qt::Saturday, Sunday), 6);
}

void TstTodoCalendarLayout::cellAtRejectsHeaderAndNavButtons()
{
    const QSize size = TodoCalendarLayout::sizeHint();
    // 表头与星期行不算格子。
    QCOMPARE(TodoCalendarLayout::cellAt(QPoint(TodoCalendarLayout::Margin + 2, 2), size), -1);
    QCOMPARE(TodoCalendarLayout::cellAt(
                 QPoint(TodoCalendarLayout::Margin + 2, TodoCalendarLayout::HeaderHeight + 2), size), -1);
    // 翻月按钮的热区下半截与网格同高，必须先于网格判定排除。
    const QRect prev = TodoCalendarLayout::prevRect();
    QCOMPARE(TodoCalendarLayout::cellAt(prev.bottomLeft() + QPoint(2, 0), size), -1);
    const QRect next = TodoCalendarLayout::nextRect(size.width());
    QCOMPARE(TodoCalendarLayout::cellAt(next.center(), size), -1);
    // 右侧留白与底部留白也不算格子。
    QCOMPARE(TodoCalendarLayout::cellAt(QPoint(size.width() - 2, size.height() / 2), size), -1);
    QCOMPARE(TodoCalendarLayout::cellAt(QPoint(size.width() / 2, size.height() - 2), size), -1);
}

void TstTodoCalendarLayout::cellAtMapsGridRows()
{
    const QSize size = TodoCalendarLayout::sizeHint();
    const QRect first = TodoCalendarLayout::cellRect(0);
    QCOMPARE(TodoCalendarLayout::cellAt(first.center(), size), 0);
    const QRect last = TodoCalendarLayout::cellRect(TodoCalendarLayout::GridRows * 7 - 1);
    QCOMPARE(TodoCalendarLayout::cellAt(last.center(), size), TodoCalendarLayout::GridRows * 7 - 1);
    // 第二行第三列 → 7 + 2。
    const QRect third = TodoCalendarLayout::cellRect(9);
    QCOMPARE(TodoCalendarLayout::cellAt(third.center(), size), 9);
}

void TstTodoCalendarLayout::dateAtRoundTripsWithCellAt()
{
    const QDate month(2026, 11, 15);
    const QDate start = TodoCalendarLayout::gridStart(month, Monday);
    const QSize size = TodoCalendarLayout::sizeHint();
    for (int cell = 0; cell < TodoCalendarLayout::GridRows * 7; ++cell) {
        const QPoint center = TodoCalendarLayout::cellRect(cell).center();
        QCOMPARE(TodoCalendarLayout::cellAt(center, size), cell);
        QCOMPARE(TodoCalendarLayout::dateAt(month, Monday, cell), start.addDays(cell));
    }
    // 命中为 -1 时日期也无效，调用方不必额外判断。
    QVERIFY(!TodoCalendarLayout::dateAt(month, Monday, -1).isValid());
}

QTEST_GUILESS_MAIN(TstTodoCalendarLayout)

#include "tst_todo_calendar_layout.moc"
