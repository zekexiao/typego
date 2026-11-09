#include "TodoPopups.h"
#include "TodoCalendar.h"
#include "TodoPriorities.h"
#include "ThemeManager.h"

#include <QButtonGroup>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace {
// 弹层与锚点之间留 6px，避开行底描边，弹出时不会贴着文字。
constexpr int PopupGap = 6;
// 屏幕边缘最小留白，避免弹层贴到屏幕边框上。
constexpr int ScreenMargin = 8;
} // namespace

void placePopupNear(QWidget *popup, const QRect &anchor)
{
    popup->adjustSize();
    // 以锚点所在屏幕为准：多屏时主屏几何会把弹层拽到另一块屏上。
    const QScreen *target = QGuiApplication::screenAt(anchor.center());
    const QRect avail = (target ? target : QGuiApplication::primaryScreen())->availableGeometry();
    const QSize size = popup->size();
    // 默认贴在锚点上方（列表最后一行的弹层不会盖住输入框），上方放不下再翻到下方。
    int y = anchor.top() - size.height() - PopupGap;
    if (y < avail.top() + ScreenMargin) y = anchor.bottom() + PopupGap;
    int x = anchor.center().x() - size.width() / 2;
    x = qBound(avail.left() + ScreenMargin, x, avail.right() - size.width() - ScreenMargin);
    y = qBound(avail.top() + ScreenMargin, y, avail.bottom() - size.height() - ScreenMargin);
    popup->move(x, y);
}

TodoPriorityPopup::TodoPriorityPopup(ThemeManager *theme, int current, QWidget *parent)
    : QFrame(parent, Qt::Popup)
{
    // 与原生菜单一致：弹层本身不带圆角，避免无边框窗口四角露出底色残影。
    setObjectName(QStringLiteral("todoPriorityPopup"));
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
    // 四档从左到右由高到低，与旗标颜色深浅对应，一眼能挑出目标档。
    for (const int level : {TodoPriority::Urgent, TodoPriority::High,
                            TodoPriority::Medium, TodoPriority::Low}) {
        auto *button = new QPushButton(TodoPriority::label(level), this);
        button->setObjectName(QStringLiteral("todoPriorityChoice"));
        // 旗标图标与列表行尾、菜单项共用同一份画法，档位色一眼对应。
        button->setIcon(TodoPriority::icon(theme, level));
        button->setIconSize(QSize(16, 16));
        // 档位色只体现在文字上：底色留给选中态，避免两套颜色互相打架。
        const QColor ink = TodoPriority::color(theme, level);
        button->setStyleSheet(QStringLiteral("color: %1;").arg(ink.name(QColor::HexRgb)));
        button->setCheckable(true);
        button->setChecked(level == current);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        m_group->addButton(button, level);
        m_buttons.append(button);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, level]() { choose(level); });
    }
}

void TodoPriorityPopup::choose(int level)
{
    // 先回调再收起：调用方只把重建推到下一轮事件循环，不会动到本弹层。
    emit picked(level);
    close();
}

void TodoPriorityPopup::keyPressEvent(QKeyEvent *event)
{
    // Esc 或再按一次旗标位置都由上层关闭；这里补一个 Tab 在四档间移动。
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QFrame::keyPressEvent(event);
}

TodoDuePopup::TodoDuePopup(ThemeManager *theme, const QDate &date, const QTime &time, QWidget *parent)
    : QFrame(parent, Qt::Popup)
{
    setObjectName(QStringLiteral("todoDatePicker"));
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    m_calendar = new TodoCalendar(theme, this);
    if (date.isValid()) m_calendar->setDate(date);
    layout->addWidget(m_calendar);

    auto *timeRow = new QHBoxLayout;
    timeRow->setSpacing(8);
    auto *clearButton = new QPushButton(tr("Clear due"), this);
    clearButton->setObjectName(QStringLiteral("todoClear"));
    clearButton->setCursor(Qt::PointingHandCursor);
    clearButton->setFocusPolicy(Qt::NoFocus);
    timeRow->addWidget(clearButton);
    timeRow->addStretch(1);
    m_time = new QTimeEdit(this);
    m_time->setObjectName(QStringLiteral("todoDueTime"));
    m_time->setDisplayFormat(QStringLiteral("HH:mm"));
    m_time->setAccelerated(true);
    m_time->setSpecialValueText(tr("No time"));
    if (time.isValid()) m_time->setTime(time);
    timeRow->addWidget(m_time);
    layout->addLayout(timeRow);

    // 点某天立即生效并关闭；0 点即「不设时间」，按纯日期入库。
    m_calendar->picked = [this](const QDate &picked) {
        const QTime chosen = m_time->time() == QTime(0, 0) ? QTime() : m_time->time();
        if (duePicked) duePicked(picked, chosen);
        close();
    };
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        if (dueCleared) dueCleared();
        close();
    });
}

void TodoDuePopup::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QFrame::keyPressEvent(event);
}

TodoDeletePopup::TodoDeletePopup(const QString &title, QWidget *parent)
    : QFrame(parent, Qt::Popup)
{
    setObjectName(QStringLiteral("todoDeletePopup"));
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(10);

    auto *question = new QLabel(tr("Delete \"%1\"?").arg(title), this);
    question->setObjectName(QStringLiteral("todoDeleteQuestion"));
    // 长标题不能把弹层拉到整行宽，超过定宽换行；完整内容本来就在列表里。
    question->setMaximumWidth(320);
    question->setWordWrap(true);
    layout->addWidget(question);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addStretch(1);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    cancel->setObjectName(QStringLiteral("todoClear"));
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFocusPolicy(Qt::NoFocus);
    buttons->addWidget(cancel);
    auto *confirm = new QPushButton(tr("Delete"), this);
    confirm->setObjectName(QStringLiteral("danger"));
    confirm->setCursor(Qt::PointingHandCursor);
    confirm->setFocusPolicy(Qt::NoFocus);
    buttons->addWidget(confirm);
    layout->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QWidget::close);
    connect(confirm, &QPushButton::clicked, this, [this]() {
        if (confirmed) confirmed();
        close();
    });
}

void TodoDeletePopup::keyPressEvent(QKeyEvent *event)
{
    // 删除打的是墓碑且不可撤销，只允许 Esc 退出；确认必须真的点一下。
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QFrame::keyPressEvent(event);
}
