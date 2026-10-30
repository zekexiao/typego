#include "CaptureToolbar.h"

#include "ShotIcons.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

const QColor kColors[] = {
    QColor(0, 0, 0),       QColor(255, 255, 255), QColor(255, 77, 79),
    QColor(255, 169, 64),  QColor(255, 214, 102), QColor(82, 196, 26),
    QColor(19, 194, 194),  QColor(22, 119, 255),  QColor(146, 84, 222),
};

const int kWidths[] = {2, 4, 6, 10};

} // namespace

CaptureToolbar::CaptureToolbar(QWidget *parent, Mode mode)
    : QWidget(parent)
    , m_mode(mode)
{
    setObjectName(QStringLiteral("shotToolbar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setStyleSheet(QStringLiteral(
        "QWidget#shotToolbar { background: rgba(26, 28, 32, 238); border: 1px solid rgba(255,255,255,38); border-radius: 10px; }"
        "QToolButton { background: transparent; border: 1px solid transparent; border-radius: 6px; padding: 2px; }"
        "QToolButton:hover { background: rgba(255,255,255,30); }"
        "QToolButton:pressed { background: rgba(255,255,255,55); }"
        "QToolButton:checked { background: #1677ff; }"
        "QToolButton:disabled { opacity: 0.35; }"
        "QToolButton[selected=\"true\"] { border: 2px solid #ffffff; }"
        "QLabel#shotHint { color: #c8ccd4; font-size: 11px; }"
        "QFrame#shotDivider { background: rgba(255,255,255,36); max-width: 1px; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 6);
    root->setSpacing(4);

    auto *tools = new QHBoxLayout;
    tools->setSpacing(2);
    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(true);
    for (int index = 0; index < shotToolCount(); ++index) {
        const ShotTool tool = shotToolAt(index);
        QToolButton *button = makeToolButton(tool);
        m_toolGroup->addButton(button, index);
        m_toolButtons.append(button);
        tools->addWidget(button);
        if (tool == ShotTool::Ellipse || tool == ShotTool::Mosaic || tool == ShotTool::Number) {
            auto *divider = new QFrame(this);
            divider->setObjectName(QStringLiteral("shotDivider"));
            divider->setFrameShape(QFrame::VLine);
            divider->setFixedHeight(20);
            tools->addWidget(divider);
        }
    }
    tools->addStretch();
    root->addLayout(tools);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(4);

    for (const QColor &color : kColors) {
        auto *button = new QToolButton(this);
        button->setFixedSize(22, 22);
        button->setIconSize(QSize(18, 18));
        button->setIcon(shotColorIcon(color));
        button->setCheckable(false);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(color.name());
        connect(button, &QToolButton::clicked, this, [this, button, color]() {
            selectColor(button);
            emit colorChanged(color);
        });
        m_colorButtons.append(button);
        actions->addWidget(button);
    }

    auto *divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("shotDivider"));
    divider->setFrameShape(QFrame::VLine);
    divider->setFixedHeight(22);
    actions->addWidget(divider);

    for (int width : kWidths) {
        auto *button = new QToolButton(this);
        button->setFixedSize(24, 24);
        button->setIconSize(QSize(20, 20));
        button->setIcon(shotWidthIcon(width));
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tr("Stroke %1 px").arg(width));
        connect(button, &QToolButton::clicked, this, [this, button, width]() {
            selectWidth(button);
            emit widthChanged(width);
        });
        m_widthButtons.append(button);
        actions->addWidget(button);
    }

    auto *divider2 = new QFrame(this);
    divider2->setObjectName(QStringLiteral("shotDivider"));
    divider2->setFrameShape(QFrame::VLine);
    divider2->setFixedHeight(22);
    actions->addWidget(divider2);

    auto addAction = [this, actions](const QString &name, const QString &tip) {
        auto *button = new QToolButton(this);
        button->setFixedSize(28, 26);
        button->setIconSize(QSize(20, 20));
        button->setIcon(shotActionIcon(name));
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tip);
        actions->addWidget(button);
        return button;
    };
    m_undo = addAction(QStringLiteral("undo"), tr("Undo (Ctrl+Z)"));
    m_redo = addAction(QStringLiteral("redo"), tr("Redo (Ctrl+Y)"));
    connect(m_undo, &QToolButton::clicked, this, &CaptureToolbar::undoRequested);
    connect(m_redo, &QToolButton::clicked, this, &CaptureToolbar::redoRequested);

    auto *divider3 = new QFrame(this);
    divider3->setObjectName(QStringLiteral("shotDivider"));
    divider3->setFrameShape(QFrame::VLine);
    divider3->setFixedHeight(22);
    actions->addWidget(divider3);

    QToolButton *scroll = addAction(QStringLiteral("scroll"), tr("Scrolling capture (S)"));
    QToolButton *copy = addAction(QStringLiteral("copy"), tr("Copy (Enter / Ctrl+C)"));
    QToolButton *save = addAction(QStringLiteral("save"), tr("Save as (Ctrl+S)"));
    QToolButton *pin = addAction(QStringLiteral("pin"), tr("Pin to screen (Ctrl+P)"));
    QToolButton *close = addAction(QStringLiteral("close"),
        m_mode == Mode::Pin ? tr("Done (Esc)") : tr("Cancel (Esc)"));
    if (m_mode == Mode::Pin) {
        scroll->hide();
        pin->hide();
    }
    connect(scroll, &QToolButton::clicked, this, &CaptureToolbar::scrollRequested);
    connect(copy, &QToolButton::clicked, this, &CaptureToolbar::copyRequested);
    connect(save, &QToolButton::clicked, this, &CaptureToolbar::saveRequested);
    connect(pin, &QToolButton::clicked, this, &CaptureToolbar::pinRequested);
    connect(close, &QToolButton::clicked, this, &CaptureToolbar::closeRequested);
    actions->addStretch();
    root->addLayout(actions);

    setTool(ShotTool::Select);
    selectColor(m_colorButtons.value(2));
    selectWidth(m_widthButtons.value(1));
    m_undo->setEnabled(false);
    m_redo->setEnabled(false);
    adjustSize();
}

CaptureToolbar::~CaptureToolbar() = default;

QToolButton *CaptureToolbar::makeToolButton(ShotTool tool)
{
    auto *button = new QToolButton(this);
    button->setCheckable(true);
    button->setFixedSize(28, 28);
    button->setIconSize(QSize(20, 20));
    button->setIcon(shotToolIcon(tool));
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    QString name;
    QString shortcut;
    switch (tool) {
    case ShotTool::Select: name = tr("Select"); shortcut = QStringLiteral("V"); break;
    case ShotTool::Rect: name = tr("Rectangle"); shortcut = QStringLiteral("R"); break;
    case ShotTool::Ellipse: name = tr("Ellipse"); shortcut = QStringLiteral("O"); break;
    case ShotTool::Arrow: name = tr("Arrow"); shortcut = QStringLiteral("A"); break;
    case ShotTool::Line: name = tr("Line"); shortcut = QStringLiteral("L"); break;
    case ShotTool::Pen: name = tr("Pen"); shortcut = QStringLiteral("P"); break;
    case ShotTool::Text: name = tr("Text"); shortcut = QStringLiteral("T"); break;
    case ShotTool::Highlight: name = tr("Highlight"); shortcut = QStringLiteral("H"); break;
    case ShotTool::Mosaic: name = tr("Mosaic"); shortcut = QStringLiteral("M"); break;
    case ShotTool::Blur: name = tr("Blur"); shortcut = QStringLiteral("B"); break;
    case ShotTool::Number: name = tr("Step number"); shortcut = QStringLiteral("N"); break;
    }
    button->setToolTip(QStringLiteral("%1 (%2)").arg(name, shortcut));
    connect(button, &QToolButton::clicked, this, [this, tool]() { emit toolChanged(tool); });
    return button;
}

void CaptureToolbar::setTool(ShotTool tool)
{
    for (int index = 0; index < m_toolButtons.size(); ++index) {
        if (shotToolAt(index) != tool) continue;
        QSignalBlocker blocker(m_toolButtons.at(index));
        m_toolButtons.at(index)->setChecked(true);
        return;
    }
}

void CaptureToolbar::setColor(const QColor &color)
{
    for (int index = 0; index < m_colorButtons.size() && index < 9; ++index)
        if (kColors[index] == color) {
            selectColor(m_colorButtons.at(index));
            return;
        }
}

void CaptureToolbar::setWidth(int width)
{
    for (int index = 0; index < m_widthButtons.size() && index < 4; ++index)
        if (kWidths[index] == width) {
            selectWidth(m_widthButtons.at(index));
            return;
        }
}

void CaptureToolbar::setUndoRedo(bool canUndo, bool canRedo)
{
    if (m_undo) m_undo->setEnabled(canUndo);
    if (m_redo) m_redo->setEnabled(canRedo);
}

void CaptureToolbar::selectColor(QToolButton *button)
{
    if (!button) return;
    for (QToolButton *item : std::as_const(m_colorButtons)) {
        item->setProperty("selected", item == button);
        item->style()->unpolish(item);
        item->style()->polish(item);
    }
}

void CaptureToolbar::selectWidth(QToolButton *button)
{
    if (!button) return;
    for (QToolButton *item : std::as_const(m_widthButtons)) {
        item->setProperty("selected", item == button);
        item->style()->unpolish(item);
        item->style()->polish(item);
    }
}