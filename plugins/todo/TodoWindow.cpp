#include "TodoWindow.h"
#include "ThemeManager.h"

#include <QCalendarWidget>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStringList>
#include <QTimeEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>

namespace {
constexpr int RowHeight = 44;        // 任务行高
constexpr int HeaderHeight = 28;     // 已完成分组头行高
constexpr int RowHPadding = 16;      // 行内水平留白
constexpr int CheckBoxSize = 18;     // 勾选框边长
constexpr int TitleFontSize = 14;    // 标题字号
constexpr int MetaFontSize = 13;     // 元信息与分组头字号
constexpr int TitleMetaGap = 12;     // 勾选框与标题间距
constexpr int EditorHeight = 32;     // 行内编辑器高度

using ColorFn = QColor (ThemeManager::*)() const;

// 行内圆角矩形：与主窗口结果列表的画法一致（水平内收 3px）。
QRect rowBackground(const QRect &row)
{
    return row.adjusted(3, 1, -3, -1);
}

QRect checkBoxRect(const QRect &row)
{
    const QRect background = rowBackground(row);
    QRect rect(QPoint(), QSize(CheckBoxSize, CheckBoxSize));
    rect.moveLeft(background.left() + RowHPadding - 3);
    rect.moveTop(background.center().y() - CheckBoxSize / 2);
    return rect;
}

// 标题区（编辑器几何也用它）：勾选框右侧到行尾。
QRect titleRect(const QRect &row)
{
    const QRect background = rowBackground(row);
    QRect rect = background;
    rect.setLeft(checkBoxRect(row).right() + TitleMetaGap);
    rect.setRight(background.right() - RowHPadding + 3);
    return rect;
}
} // namespace

// 列表委托：勾选框、标题、行尾元信息全部自绘，保持与结果列表同一套视觉语言。
class TodoRowDelegate final : public QStyledItemDelegate {
public:
    using Toggler = std::function<void(const QString &id)>;
    using Committer = std::function<void(const QString &id, const QString &text)>;

    TodoRowDelegate(QListWidget *list, ThemeManager *theme, Toggler toggle, Committer commit, QObject *parent)
        : QStyledItemDelegate(parent), m_list(list), m_theme(theme),
          m_toggle(std::move(toggle)), m_commit(std::move(commit)) {}

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return {option.rect.width(), index.data(TodoWindow::HeaderRole).toBool() ? HeaderHeight : RowHeight};
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        auto *editor = new QLineEdit(parent);
        editor->setMaxLength(TodoStore::MaxTitleLength);
        editor->setStyleSheet(QStringLiteral("min-height: 0; padding: 4px 8px;"));
        QFont font = editor->font();
        font.setPixelSize(TitleFontSize);
        editor->setFont(font);
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *line = qobject_cast<QLineEdit *>(editor))
            line->setText(index.data(Qt::DisplayRole).toString());
    }

    void setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const override
    {
        auto *line = qobject_cast<QLineEdit *>(editor);
        if (line && m_commit) m_commit(index.data(TodoWindow::IdRole).toString(), line->text());
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        const QRect rect = titleRect(option.rect);
        editor->setGeometry(rect.left(), rect.center().y() - EditorHeight / 2,
                            rect.width(), EditorHeight);
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonRelease && !index.data(TodoWindow::HeaderRole).toBool()) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (checkBoxRect(option.rect).contains(mouse->pos())) {
                if (m_toggle) m_toggle(index.data(TodoWindow::IdRole).toString());
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QRect row = option.rect;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        if (index.data(TodoWindow::HeaderRole).toBool()) {
            // 分组头：上方有未完成条目时画分隔线，再跟一行小字。
            if (index.row() > 0) {
                painter->setPen(QPen(color(option, &ThemeManager::border, QPalette::Mid), 1));
                const int y = row.top() + 6;
                painter->drawLine(row.left() + RowHPadding, y, row.right() - RowHPadding, y);
            }
            painter->setPen(secondary(option));
            QFont font = option.font;
            font.setPixelSize(MetaFontSize);
            painter->setFont(font);
            painter->drawText(row.adjusted(RowHPadding, 8, -RowHPadding, 0),
                              Qt::AlignVCenter | Qt::AlignLeft, index.data(Qt::DisplayRole).toString());
            painter->restore();
            return;
        }

        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = !selected && isHovered(index);
        const QRect background = rowBackground(row);
        if (selected || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? color(option, &ThemeManager::selection, QPalette::Highlight)
                                       : color(option, &ThemeManager::hover, QPalette::Mid));
            painter->drawRoundedRect(background, 12, 12);
        }

        // 勾选框：勾选态实心强调填充加白色圆头对勾，未完成态只描边。
        const QRect box = checkBoxRect(row);
        const bool done = index.data(TodoWindow::DoneRole).toBool();
        if (done) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(color(option, &ThemeManager::accentFill, QPalette::Highlight));
            painter->drawRoundedRect(box, 6, 6);
            QPen checkPen(Qt::white, 1.6);
            checkPen.setCapStyle(Qt::RoundCap);
            checkPen.setJoinStyle(Qt::RoundJoin);
            painter->setPen(checkPen);
            painter->drawPolyline(QPolygonF{QPointF(box.left() + 4.5, box.top() + 9.5),
                                            QPointF(box.left() + 7.5, box.top() + 12.5),
                                            QPointF(box.left() + 13.5, box.top() + 5.5)});
        } else {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(color(option, &ThemeManager::border, QPalette::Mid), 1.4));
            painter->drawRoundedRect(box, 6, 6);
        }

        // 编辑态只画底和勾选框，标题与元信息交给行内编辑器。
        if (!(option.state & QStyle::State_Editing)) {
            const QRect textRect = titleRect(row);
            const QString title = index.data(Qt::DisplayRole).toString();
            QFont titleFont = option.font;
            titleFont.setPixelSize(TitleFontSize);
            titleFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
            titleFont.setStrikeOut(done);

            // 元信息靠右且最多占文本区四成，超出部分从左侧省略，标题优先保完整。
            const QString meta = index.data(TodoWindow::MetaRole).toString();
            const QString metaText = meta.isEmpty() ? QString() : QFontMetrics(metaFontOf(option)).elidedText(
                meta, Qt::ElideLeft, textRect.width() * 2 / 5);
            const int metaAdvance = metaText.isEmpty()
                ? 0 : QFontMetrics(metaFontOf(option)).horizontalAdvance(metaText) + TitleMetaGap;
            const int titleWidth = qMax(0, textRect.width() - metaAdvance);

            painter->setFont(titleFont);
            painter->setPen(done ? secondary(option)
                                 : selected ? color(option, &ThemeManager::accentText, QPalette::HighlightedText)
                                            : color(option, &ThemeManager::text, QPalette::Text));
            painter->drawText(QRect(textRect.left(), textRect.top(), titleWidth, textRect.height()),
                              Qt::AlignVCenter | Qt::AlignLeft,
                              QFontMetrics(titleFont).elidedText(title, Qt::ElideMiddle, titleWidth));

            if (!metaText.isEmpty()) {
                painter->setFont(metaFontOf(option));
                painter->setPen(index.data(TodoWindow::OverdueRole).toBool()
                                    ? color(option, &ThemeManager::error, QPalette::BrightText)
                                    : secondary(option));
                painter->drawText(QRect(textRect.right() - metaAdvance + TitleMetaGap, textRect.top(),
                                        metaAdvance - TitleMetaGap, textRect.height()),
                                  Qt::AlignVCenter | Qt::AlignRight, metaText);
            }
        }
        painter->restore();
    }

private:
    QFont metaFontOf(const QStyleOptionViewItem &option) const
    {
        QFont font = option.font;
        font.setPixelSize(MetaFontSize);
        return font;
    }

    QColor color(const QStyleOptionViewItem &option, ColorFn fn, QPalette::ColorRole fallback) const
    {
        // 主题未注入时退回系统调色板，保证截图外的极端场景仍可读。
        return m_theme ? (m_theme->*fn)() : option.palette.color(fallback);
    }

    QColor secondary(const QStyleOptionViewItem &option) const
    {
        return color(option, &ThemeManager::textSecondary, QPalette::PlaceholderText);
    }

    bool isHovered(const QModelIndex &index) const
    {
        if (!m_list || !m_list->underMouse()) return false;
        return m_list->indexAt(m_list->viewport()->mapFromGlobal(QCursor::pos())) == index;
    }

    QListWidget *m_list;
    ThemeManager *m_theme;
    Toggler m_toggle;
    Committer m_commit;
};

TodoWindow::TodoWindow(const QString &storageDirectory, ThemeManager *theme, QWidget *parent)
    : QWidget(parent), m_store(storageDirectory + QStringLiteral("/todo.json")), m_theme(theme)
{
    // 目录不存在时保存会失败，先建好。
    QDir().mkpath(storageDirectory);
    QString error;
    if (!m_store.load(&error))
        qWarning("Todo: cannot load store: %s", qPrintable(error));
    buildLayout();
    rebuild();
    if (m_theme)
        connect(m_theme, &ThemeManager::paletteChanged, this, [this]() { m_list->viewport()->update(); });
}

void TodoWindow::buildLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(12);

    // 顶部输入框即添加入口：回车入库并保持焦点，可连续录入。
    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("todoInput"));
    m_input->setPlaceholderText(tr("Add a todo"));
    m_input->setMaxLength(TodoStore::MaxTitleLength);
    connect(m_input, &QLineEdit::returnPressed, this, &TodoWindow::addItem);
    m_input->installEventFilter(this);
    layout->addWidget(m_input);

    // 列表容器只负责描边与圆角，行内容由委托自绘，需要鼠标跟踪做悬停反馈。
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("todoList"));
    m_list->setMouseTracking(true);
    m_list->viewport()->setMouseTracking(true);
    m_list->setUniformItemSizes(false);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &TodoWindow::showContextMenu);
    m_list->setItemDelegate(new TodoRowDelegate(m_list, m_theme,
        [this](const QString &id) { toggleDone(id); },
        [this](const QString &id, const QString &text) { commitEditById(id, text); }, m_list));
    m_list->installEventFilter(this);
    m_list->viewport()->installEventFilter(this);
    layout->addWidget(m_list, 1);

    // 空状态覆盖在列表上，与列表同尺寸居中。
    m_empty = new QLabel(tr("No todos yet"), m_list);
    m_empty->setObjectName(QStringLiteral("todoEmpty"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_empty->hide();

    // 页脚：左侧未完成计数，右侧唯一动作「清除已完成」。
    auto *footerRow = new QHBoxLayout;
    footerRow->setContentsMargins(4, 0, 4, 0);
    footerRow->setSpacing(8);
    m_footer = new QLabel(this);
    m_footer->setObjectName(QStringLiteral("todoFooter"));
    footerRow->addWidget(m_footer);
    footerRow->addStretch(1);
    m_clearButton = new QPushButton(tr("Clear completed"), this);
    m_clearButton->setObjectName(QStringLiteral("todoClear"));
    m_clearButton->setCursor(Qt::PointingHandCursor);
    connect(m_clearButton, &QPushButton::clicked, this, &TodoWindow::clearDone);
    footerRow->addWidget(m_clearButton);
    layout->addLayout(footerRow);
}

// 事件过滤：输入框 Esc 清空；列表视口鼠标移动刷新悬停；列表按键 Return 编辑、Del 删除。
bool TodoWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input) {
        if (event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                m_input->clear();
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }
    if (watched == m_list) {
        if (event->type() == QEvent::Resize) {
            placeEmptyLabel();
            return QWidget::eventFilter(watched, event);
        }
        // 编辑器打开时按键会直达编辑器，不会到达列表，这里无需再判编辑态。
        if (event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                if (m_list->currentIndex().isValid()) m_list->edit(m_list->currentIndex());
                return true;
            }
            if (key->key() == Qt::Key_Delete) {
                if (m_list->currentIndex().isValid())
                    removeItem(m_list->currentIndex().data(TodoWindow::IdRole).toString());
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }
    if (watched == m_list->viewport()) {
        // 纯移动不触发重绘，主动刷新让委托重算悬停底色。
        if (event->type() == QEvent::MouseMove || event->type() == QEvent::Leave)
            m_list->viewport()->update();
        return QWidget::eventFilter(watched, event);
    }
    return QWidget::eventFilter(watched, event);
}

void TodoWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_input->setFocus();
    placeEmptyLabel();
}

TodoItem *TodoWindow::find(const QString &id)
{
    for (auto &item : m_store.items())
        if (item.id == id) return &item;
    return nullptr;
}

// 用存储数据整体重建列表；条目数少，排序重建的开销可以忽略。
void TodoWindow::rebuild()
{
    const QString currentId = m_list->currentItem()
        ? m_list->currentItem()->data(TodoWindow::IdRole).toString() : QString();
    m_list->clear();

    const auto items = TodoStore::sorted(m_store.items());
    int doneCount = 0;
    for (const auto &item : items)
        if (item.done) ++doneCount;

    int row = 0;
    int firstSelectable = -1;
    bool headerWritten = false;
    for (const auto &item : items) {
        if (item.done && !headerWritten) {
            // 已完成条目前插入分组头，把列表自然分成两段，不需要筛选控件。
            auto *header = new QListWidgetItem(tr("%n completed", "", doneCount), m_list);
            header->setFlags(Qt::NoItemFlags);
            header->setData(TodoWindow::HeaderRole, true);
            ++row;
            headerWritten = true;
        }
        auto *entry = new QListWidgetItem(item.title, m_list);
        entry->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
        entry->setData(TodoWindow::IdRole, item.id);
        entry->setData(TodoWindow::DoneRole, item.done);

        bool overdue = false;
        QStringList parts;
        if (!item.done) {
            if (item.priority > 1) parts << tr("High");
            if (item.priority < 1) parts << tr("Low");
            const QString due = dueText(item, &overdue);
            if (!due.isEmpty()) parts << due;
            if (overdue) parts << tr("Overdue");
        }
        entry->setData(TodoWindow::MetaRole, parts.join(QStringLiteral(" · ")));
        entry->setData(TodoWindow::OverdueRole, overdue);
        if (item.id == currentId) m_list->setCurrentRow(row);
        if (firstSelectable < 0) firstSelectable = row;
        ++row;
    }
    // 未还原选中时选第一个任务行，键盘导航立即可用。
    if (!m_list->currentItem() && firstSelectable >= 0)
        m_list->setCurrentRow(firstSelectable);
    m_empty->setVisible(items.isEmpty());
    placeEmptyLabel();
    updateFooter();
}

void TodoWindow::persist()
{
    QString error;
    if (!m_store.save(&error))
        qWarning("Todo: cannot save store: %s", qPrintable(error));
}

void TodoWindow::addItem()
{
    QString title = m_input->text().trimmed();
    if (title.isEmpty()) return;
    if (title.size() > TodoStore::MaxTitleLength) title.truncate(TodoStore::MaxTitleLength);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    TodoItem item;
    item.id = TodoStore::newId();
    item.title = title;
    item.createdAt = now;
    item.updatedAt = now;
    m_store.items().append(item);
    persist();
    rebuild();
    m_input->clear();
    m_input->setFocus();
}

// 行内编辑提交；此时视图仍在关闭编辑器的调用栈里，重建延迟到下一轮事件循环。
void TodoWindow::commitEditById(const QString &id, const QString &text)
{
    QString title = text.trimmed();
    if (title.size() > TodoStore::MaxTitleLength) title.truncate(TodoStore::MaxTitleLength);
    if (TodoItem *item = find(id); item && !title.isEmpty() && title != item->title) {
        item->title = title;
        item->updatedAt = QDateTime::currentMSecsSinceEpoch();
        persist();
    }
    QTimer::singleShot(0, this, &TodoWindow::rebuild);
}

void TodoWindow::toggleDone(const QString &id)
{
    if (TodoItem *item = find(id)) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        item->done = !item->done;
        item->completedAt = item->done ? now : 0;
        item->updatedAt = now;
        persist();
        // 从委托的鼠标事件里调用，重建延迟到下一轮事件循环，避免动到正在处理的模型。
        QTimer::singleShot(0, this, &TodoWindow::rebuild);
    }
}

void TodoWindow::removeItem(const QString &id)
{
    if (TodoItem *item = find(id)) {
        // 打墓碑而不是物理删除，给将来的同步与恢复留依据。
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        item->deletedAt = now;
        item->updatedAt = now;
        persist();
        rebuild();
    }
}

void TodoWindow::setPriority(const QString &id, int priority)
{
    if (TodoItem *item = find(id)) {
        item->priority = priority;
        item->updatedAt = QDateTime::currentMSecsSinceEpoch();
        persist();
        rebuild();
    }
}

void TodoWindow::setDue(const QString &id, const QString &due)
{
    if (TodoItem *item = find(id)) {
        item->due = due;
        item->updatedAt = QDateTime::currentMSecsSinceEpoch();
        persist();
        rebuild();
    }
}

// 日期选择弹层：日历上点某天立即生效；时间拨到最小值即纯日期。
void TodoWindow::pickDue(const QString &id, const QRect &rowRect)
{
    QDialog picker(this, Qt::Popup);
    picker.setObjectName(QStringLiteral("todoDatePicker"));
    auto *layout = new QVBoxLayout(&picker);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    auto *calendar = new QCalendarWidget(&picker);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar->setNavigationBarVisible(true);
    layout->addWidget(calendar);
    auto *timeEdit = new QTimeEdit(&picker);
    timeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    timeEdit->setAccelerated(true);
    timeEdit->setSpecialValueText(tr("No time"));
    layout->addWidget(timeEdit);

    if (TodoItem *item = find(id)) {
        if (const QDateTime due = TodoStore::dueDateTime(item->due); due.isValid()) {
            calendar->setSelectedDate(due.date());
            if (TodoStore::dueHasTime(item->due)) timeEdit->setTime(due.time());
        }
    }

    connect(calendar, &QCalendarWidget::clicked, this, [&picker, timeEdit, id, this](const QDate &date) {
        // 0 点即「不设时间」，按纯日期入库。
        const QTime time = timeEdit->time() == QTime(0, 0) ? QTime() : timeEdit->time();
        setDue(id, TodoStore::makeDue(date, time));
        picker.accept();
    });

    picker.adjustSize();
    // 锚定在任务行下方；下方放不下就翻到行上方，再夹回屏幕内。
    QPoint pos = m_list->viewport()->mapToGlobal(rowRect.bottomLeft());
    pos.setY(pos.y() + 4);
    if (const QRect avail = screen()->availableGeometry(); pos.y() + picker.height() > avail.bottom())
        pos.setY(qMax(avail.top(), pos.y() - rowRect.height() - picker.height() - 8));
    pos.setX(qMin(qMax(pos.x(), screen()->availableGeometry().left() + 8),
                  screen()->availableGeometry().right() - picker.width() - 8));
    picker.move(pos);
    picker.exec();
}

void TodoWindow::clearDone()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    for (auto &item : m_store.items()) {
        if (item.done && item.deletedAt <= 0) {
            item.deletedAt = now;
            item.updatedAt = now;
            changed = true;
        }
    }
    if (!changed) return;
    persist();
    rebuild();
}

void TodoWindow::showContextMenu(const QPoint &pos)
{
    // 键盘 Menu 键触发时坐标不可用，回退到当前行中心。
    QModelIndex index = m_list->indexAt(pos);
    if (!index.isValid() || index.data(TodoWindow::HeaderRole).toBool())
        index = m_list->currentIndex();
    if (!index.isValid() || index.data(TodoWindow::HeaderRole).toBool()) return;
    const QString id = index.data(TodoWindow::IdRole).toString();
    TodoItem *item = find(id);
    if (!item) return;

    QMenu menu(this);
    auto *priorityMenu = menu.addMenu(tr("Priority"));
    // 行内只显示非默认优先级减少噪音，菜单里三档都可切。
    auto *high = priorityMenu->addAction(tr("High"));
    auto *medium = priorityMenu->addAction(tr("Medium"));
    auto *low = priorityMenu->addAction(tr("Low"));
    high->setCheckable(true); high->setChecked(item->priority == 2);
    medium->setCheckable(true); medium->setChecked(item->priority == 1);
    low->setCheckable(true); low->setChecked(item->priority == 0);
    connect(high, &QAction::triggered, this, [this, id]() { setPriority(id, 2); });
    connect(medium, &QAction::triggered, this, [this, id]() { setPriority(id, 1); });
    connect(low, &QAction::triggered, this, [this, id]() { setPriority(id, 0); });

    auto *dueMenu = menu.addMenu(tr("Due date"));
    const QDate today = QDate::currentDate();
    connect(dueMenu->addAction(tr("Today")), &QAction::triggered,
            this, [this, id, today]() { setDue(id, TodoStore::makeDue(today)); });
    connect(dueMenu->addAction(tr("Tomorrow")), &QAction::triggered,
            this, [this, id, today]() { setDue(id, TodoStore::makeDue(today.addDays(1))); });
    dueMenu->addAction(tr("Pick a date…"), this, [this, id, index]() { pickDue(id, m_list->visualRect(index)); });
    dueMenu->addSeparator();
    auto *clear = dueMenu->addAction(tr("Clear due"));
    clear->setEnabled(!item->due.isEmpty());
    connect(clear, &QAction::triggered, this, [this, id]() { setDue(id, QString()); });

    menu.addSeparator();
    connect(menu.addAction(tr("Delete")), &QAction::triggered, this, [this, id]() { removeItem(id); });
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

void TodoWindow::updateFooter()
{
    int active = 0;
    int done = 0;
    for (const auto &item : m_store.items()) {
        if (item.deletedAt > 0) continue;
        if (item.done) ++done; else ++active;
    }
    m_footer->setText(tr("%n pending", "", active));
    m_clearButton->setEnabled(done > 0);
}

void TodoWindow::placeEmptyLabel()
{
    m_empty->setGeometry(0, 0, m_list->width(), m_list->height());
}

// 截止时间展示：今天/明天用词，一周内用星期，更远用月/日；带时间则补 HH:mm。
QString TodoWindow::dayLabel(const QDate &date, const QDate &today)
{
    if (date == today) return TodoWindow::tr("Today");
    if (date == today.addDays(1)) return TodoWindow::tr("Tomorrow");
    const qint64 delta = today.daysTo(date);
    if (delta > 1 && delta < 7) return QLocale::system().dayName(date.dayOfWeek(), QLocale::ShortFormat);
    return QLocale::system().toString(date, QStringLiteral("M/d"));
}

QString TodoWindow::dueText(const TodoItem &item, bool *overdue) const
{
    const QDateTime due = TodoStore::dueDateTime(item.due);
    if (!due.isValid()) return {};
    const QDate today = QDate::currentDate();
    if (overdue) {
        // 纯日期过完当天才算逾期，带时间过点即逾期。
        *overdue = !item.done && (TodoStore::dueHasTime(item.due)
                                      ? due < QDateTime::currentDateTime()
                                      : due.date() < today);
    }
    const QString day = dayLabel(due.date(), today);
    return TodoStore::dueHasTime(item.due)
        ? day + QLatin1Char(' ') + due.time().toString(QStringLiteral("HH:mm"))
        : day;
}
