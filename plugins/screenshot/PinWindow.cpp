#include "PinWindow.h"

#include "PinEditor.h"
#include "ShotIcons.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <QtMath>

namespace {
const qreal kMinZoom = 0.1;
const qreal kMaxZoom = 6.0;
const int kCornerHit = 10;
}

PinWindow::PinWindow(const QImage &base, const QVector<ShotAnnotation> &annotations,
                     const QPoint &position, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_base(base)
    , m_annotations(annotations)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setWindowTitle(tr("Pinned screenshot"));
    updateWindowSize();
    buildHoverBar();
    updateHoverBarGeometry();

    QScreen *screen = QGuiApplication::screenAt(position);
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect available = screen->availableGeometry();
        QPoint target = position;
        if (target.x() + width() > available.right()) target.setX(available.right() - width());
        if (target.y() + height() > available.bottom()) target.setY(available.bottom() - height());
        if (target.x() < available.left()) target.setX(available.left());
        if (target.y() < available.top()) target.setY(available.top());
        move(target);
    } else {
        move(position);
    }
}

QSizeF PinWindow::imageLogicalSize() const
{
    const qreal dpr = m_base.devicePixelRatio() > 0 ? m_base.devicePixelRatio() : 1.0;
    return QSizeF(m_base.width() / dpr, m_base.height() / dpr);
}

const QImage &PinWindow::composedImage()
{
    if (m_composedDirty || m_composed.isNull()) {
        m_composed = composeShotImage(m_base, m_annotations);
        m_composedDirty = false;
    }
    return m_composed;
}

void PinWindow::updateWindowSize()
{
    if (m_base.isNull()) return;
    const QSizeF logical = imageLogicalSize();
    resize(qMax(1, qRound(logical.width() * m_zoom)),
           qMax(1, qRound(logical.height() * m_zoom)));
}

void PinWindow::buildHoverBar()
{
    m_bar = new QWidget(this);
    m_bar->setObjectName(QStringLiteral("pinBar"));
    m_bar->setAttribute(Qt::WA_StyledBackground, true);
    m_bar->setStyleSheet(QStringLiteral(
        "QWidget#pinBar { background: rgba(26, 28, 32, 220); border: 1px solid rgba(255,255,255,30);"
        " border-radius: 8px; }"
        "QToolButton { background: transparent; border: none; border-radius: 5px; padding: 0; }"
        "QToolButton:hover { background: rgba(255,255,255,32); }"
        "QToolButton:pressed { background: rgba(255,255,255,55); }"
        "QToolButton:checked { background: #1677ff; }"));

    auto *row = new QHBoxLayout(m_bar);
    row->setContentsMargins(4, 3, 4, 3);
    row->setSpacing(2);
    auto addButton = [this, row](const QString &action) {
        auto *button = new QToolButton(m_bar);
        button->setIcon(shotActionIcon(action));
        button->setIconSize(QSize(16, 16));
        button->setFixedSize(26, 26);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        row->addWidget(button);
        return button;
    };

    QToolButton *edit = addButton(QStringLiteral("edit"));
    edit->setToolTip(tr("Edit annotations (double-click)"));
    connect(edit, &QToolButton::clicked, this, &PinWindow::editAnnotations);

    QToolButton *copy = addButton(QStringLiteral("copy"));
    copy->setToolTip(tr("Copy (Ctrl+C)"));
    connect(copy, &QToolButton::clicked, this, &PinWindow::copyImage);

    QToolButton *save = addButton(QStringLiteral("save"));
    save->setToolTip(tr("Save as... (Ctrl+S)"));
    connect(save, &QToolButton::clicked, this, &PinWindow::saveAs);

    m_topmostButton = addButton(QStringLiteral("pin"));
    m_topmostButton->setCheckable(true);
    m_topmostButton->setChecked(windowFlags() & Qt::WindowStaysOnTopHint);
    m_topmostButton->setToolTip(tr("Always on top"));
    connect(m_topmostButton, &QToolButton::toggled, this, &PinWindow::setTopmost);

    QToolButton *remove = addButton(QStringLiteral("delete"));
    remove->setToolTip(tr("Close"));
    connect(remove, &QToolButton::clicked, this, &QWidget::close);

    m_bar->adjustSize();
    m_bar->hide();
}

void PinWindow::updateHoverBarGeometry()
{
    if (!m_bar) return;
    m_bar->adjustSize();
    const int margin = 6;
    const int x = qMax(margin, width() - m_bar->width() - margin);
    const int y = height() >= m_bar->height() + margin * 2
        ? margin : qMax(0, (height() - m_bar->height()) / 2);
    m_bar->move(x, y);
}

void PinWindow::setBarVisible(bool visible)
{
    if (!m_bar) return;
    // 贴图比操作条还窄时隐藏按钮条，右键菜单和双击仍然可用。
    const bool fits = width() >= m_bar->sizeHint().width() + 12;
    m_bar->setVisible(visible && fits);
    if (m_bar->isVisible()) m_bar->raise();
}

void PinWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(rect(), composedImage());

    painter.setPen(QPen(QColor(22, 119, 255, 180), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    // 悬停时在四角提示可拖拽缩放。
    if (m_hovered && width() > 28 && height() > 28) {
        painter.setPen(QPen(QColor(22, 119, 255), 1.2));
        painter.setBrush(QColor(255, 255, 255, 230));
        const QPoint corners[] = {QPoint(0, 0), QPoint(width() - 1, 0),
                                  QPoint(0, height() - 1), QPoint(width() - 1, height() - 1)};
        for (const QPoint &point : corners) {
            painter.drawRect(QRect(point - QPoint(3, 3), QSize(7, 7)));
        }
    }
}

PinWindow::Corner PinWindow::cornerAt(const QPoint &local) const
{
    if (width() <= 28 || height() <= 28) return Corner::None;
    const QRect topLeft(0, 0, kCornerHit, kCornerHit);
    const QRect topRight(width() - kCornerHit, 0, kCornerHit, kCornerHit);
    const QRect bottomLeft(0, height() - kCornerHit, kCornerHit, kCornerHit);
    const QRect bottomRight(width() - kCornerHit, height() - kCornerHit, kCornerHit, kCornerHit);
    if (topLeft.contains(local)) return Corner::TopLeft;
    if (topRight.contains(local)) return Corner::TopRight;
    if (bottomLeft.contains(local)) return Corner::BottomLeft;
    if (bottomRight.contains(local)) return Corner::BottomRight;
    return Corner::None;
}

void PinWindow::updateCursorShape(const QPoint &local)
{
    const Corner corner = m_drag == DragMode::Move ? Corner::None : cornerAt(local);
    switch (corner) {
    case Corner::TopLeft:
    case Corner::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        return;
    case Corner::TopRight:
    case Corner::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        return;
    case Corner::None:
        break;
    }
    setCursor(m_drag == DragMode::Move ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
}

void PinWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    activateWindow();
    setFocus();
    const QPoint local = event->position().toPoint();
    const Corner corner = cornerAt(local);
    if (corner != Corner::None) {
        const QRect frame = frameGeometry();
        m_resizeCorner = corner;
        switch (corner) {
        case Corner::TopLeft:
            m_resizeAnchor = frame.bottomRight();
            m_anchorAtLeft = false;
            m_anchorAtTop = false;
            break;
        case Corner::TopRight:
            m_resizeAnchor = frame.bottomLeft();
            m_anchorAtLeft = true;
            m_anchorAtTop = false;
            break;
        case Corner::BottomLeft:
            m_resizeAnchor = frame.topRight();
            m_anchorAtLeft = false;
            m_anchorAtTop = true;
            break;
        case Corner::BottomRight:
            m_resizeAnchor = frame.topLeft();
            m_anchorAtLeft = true;
            m_anchorAtTop = true;
            break;
        case Corner::None:
            break;
        }
        m_drag = DragMode::Resize;
        event->accept();
        return;
    }
    m_drag = DragMode::Move;
    m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
}

void PinWindow::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint local = event->position().toPoint();
    if (m_drag == DragMode::Move) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    if (m_drag == DragMode::Resize) {
        const QSizeF logical = imageLogicalSize();
        const QPoint cursor = event->globalPosition().toPoint();
        const qreal zoomX = qAbs(cursor.x() - m_resizeAnchor.x()) / qMax(1.0, logical.width());
        const qreal zoomY = qAbs(cursor.y() - m_resizeAnchor.y()) / qMax(1.0, logical.height());
        const qreal zoom = qBound(kMinZoom, qMax(zoomX, zoomY), kMaxZoom);
        if (!qFuzzyCompare(zoom, m_zoom)) {
            m_zoom = zoom;
            updateWindowSize();
            const int left = m_anchorAtLeft ? m_resizeAnchor.x() : m_resizeAnchor.x() - width();
            const int top = m_anchorAtTop ? m_resizeAnchor.y() : m_resizeAnchor.y() - height();
            move(left, top);
        }
        event->accept();
        return;
    }
    updateCursorShape(local);
    event->accept();
}

void PinWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    m_drag = DragMode::None;
    m_resizeCorner = Corner::None;
    updateCursorShape(event->position().toPoint());
    QWidget::mouseReleaseEvent(event);
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    m_drag = DragMode::None;
    editAnnotations();
    event->accept();
}

void PinWindow::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) return;
    applyZoom(m_zoom * (delta > 0 ? 1.1 : 0.9), event->globalPosition().toPoint());
    event->accept();
}

void PinWindow::applyZoom(qreal zoom, const QPoint &anchorGlobal)
{
    const qreal target = qBound(kMinZoom, zoom, kMaxZoom);
    if (qFuzzyCompare(target, m_zoom)) return;
    const QPoint local = mapFromGlobal(anchorGlobal);
    const qreal factor = target / m_zoom;
    m_zoom = target;
    updateWindowSize();
    // 保持光标（或中心点）下的内容不漂移。
    const QPoint newLocal(qRound(local.x() * factor), qRound(local.y() * factor));
    move(anchorGlobal - newLocal);
}

void PinWindow::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    setBarVisible(true);
    update();
    QWidget::enterEvent(event);
}

void PinWindow::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
    // 鼠标移到按钮条上时父窗口也会收到离开事件，延后确认再隐藏。
    QTimer::singleShot(200, this, [this]() {
        if (!m_bar || m_drag != DragMode::None) return;
        const QPoint global = QCursor::pos();
        const bool insideBar = m_bar->isVisible()
            && m_bar->rect().contains(m_bar->mapFromGlobal(global));
        if (insideBar || frameGeometry().contains(global)) return;
        m_hovered = false;
        setBarVisible(false);
        update();
    });
}

void PinWindow::resizeEvent(QResizeEvent *event)
{
    updateHoverBarGeometry();
    setBarVisible(m_hovered);
    QWidget::resizeEvent(event);
}

void PinWindow::hideEvent(QHideEvent *event)
{
    // 截图前宿主会隐藏所有顶层窗口；编辑窗口随贴图一起关闭，避免恢复时错位。
    if (PinEditor *editor = findChild<PinEditor *>(QString(), Qt::FindDirectChildrenOnly))
        editor->close();
    QWidget::hideEvent(event);
}

void PinWindow::setTopmost(bool on)
{
    const bool visible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
    if (visible) {
        show();
        raise();
    }
    if (m_topmostButton && m_topmostButton->isChecked() != on) m_topmostButton->setChecked(on);
}

void PinWindow::editAnnotations()
{
    if (QPointer<PinEditor> existing = findChild<PinEditor *>(QString(), Qt::FindDirectChildrenOnly)) {
        existing->raise();
        existing->activateWindow();
        return;
    }
    auto *editor = new PinEditor(m_base, m_annotations, this);
    connect(editor, &PinEditor::annotationsChanged, this, &PinWindow::setAnnotations);
    connect(editor, &PinEditor::closed, this, [this]() {
        if (m_hovered) setBarVisible(true);
    });
    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    QPoint target = frameGeometry().center() - QPoint(editor->width() / 2, editor->height() / 2);
    if (screen) {
        const QRect available = screen->availableGeometry();
        target.setX(qBound(available.left(), target.x(),
                           qMax(available.left(), available.right() - editor->width())));
        target.setY(qBound(available.top(), target.y(),
                           qMax(available.top(), available.bottom() - editor->height())));
    }
    editor->move(target);
    editor->show();
    editor->raise();
    editor->activateWindow();
    setBarVisible(false);
}

void PinWindow::setAnnotations(const QVector<ShotAnnotation> &annotations)
{
    m_annotations = annotations;
    m_composedDirty = true;
    update();
}

void PinWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *edit = menu.addAction(tr("Edit annotations"));
    QAction *copy = menu.addAction(tr("Copy"));
    QAction *save = menu.addAction(tr("Save as..."));
    menu.addSeparator();
    QAction *topmost = menu.addAction(tr("Always on top"));
    topmost->setCheckable(true);
    topmost->setChecked(windowFlags() & Qt::WindowStaysOnTopHint);
    menu.addSeparator();
    QAction *closeAction = menu.addAction(tr("Close"));

    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == edit) editAnnotations();
    else if (chosen == copy) copyImage();
    else if (chosen == save) saveAs();
    else if (chosen == topmost) setTopmost(topmost->isChecked());
    else if (chosen == closeAction) close();
}

void PinWindow::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    const bool control = event->modifiers() & Qt::ControlModifier;
    if (key == Qt::Key_Escape || key == Qt::Key_Delete) {
        close();
        return;
    }
    if (control) {
        if (key == Qt::Key_C) {
            copyImage();
            return;
        }
        if (key == Qt::Key_S) {
            saveAs();
            return;
        }
        if (key == Qt::Key_E) {
            editAnnotations();
            return;
        }
        if (key == Qt::Key_W) {
            close();
            return;
        }
        if (key == Qt::Key_0) {
            applyZoom(1.0, mapToGlobal(rect().center()));
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void PinWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    emit closed();
}

void PinWindow::copyImage()
{
    QGuiApplication::clipboard()->setImage(composedImage());
}

void PinWindow::saveAs()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (directory.isEmpty()) directory = QDir::homePath();
    const QString defaultPath = QDir(directory).filePath(
        QStringLiteral("Screenshot_%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
    QString path = QFileDialog::getSaveFileName(this, tr("Save screenshot"), defaultPath,
                                                tr("PNG image (*.png);;JPEG image (*.jpg *.jpeg)"));
    if (path.isEmpty()) return;
    if (!path.contains(QLatin1Char('.'))) path += QStringLiteral(".png");
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QByteArray format = suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")
        ? QByteArrayLiteral("jpg") : QByteArrayLiteral("png");
    if (!composedImage().save(path, format.constData()))
        QMessageBox::warning(this, tr("Save failed"), tr("Cannot save the image to %1").arg(path));
}