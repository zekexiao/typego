#include "ScreenshotSession.h"

#include "CaptureToolbar.h"
#include "ScreenOverlay.h"
#include "ScrollCaptureSession.h"
#include "WinCapture.h"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWidget>
#include <QtMath>

#include <cmath>
#include <utility>

namespace {

QPoint physicalPoint(const ScreenInfo &screen, const QPointF &logical)
{
    return screen.physical.topLeft()
        + QPoint(qRound((logical.x() - screen.logical.x()) * screen.dpr),
                 qRound((logical.y() - screen.logical.y()) * screen.dpr));
}

QRect clampInside(const QRect &rect, const QRect &bounds)
{
    QRect result = rect;
    if (result.width() > bounds.width()) result.setWidth(bounds.width());
    if (result.height() > bounds.height()) result.setHeight(bounds.height());
    if (result.left() < bounds.left()) result.moveLeft(bounds.left());
    if (result.top() < bounds.top()) result.moveTop(bounds.top());
    if (result.right() > bounds.right()) result.moveRight(bounds.right());
    if (result.bottom() > bounds.bottom()) result.moveBottom(bounds.bottom());
    return result;
}

} // namespace

ScreenshotSession::ScreenshotSession(quintptr previousForeground, Options options, QObject *parent)
    : QObject(parent)
    , m_previousForeground(previousForeground)
    , m_options(options)
{
    m_history.append(QVector<ShotAnnotation>());
}

ScreenshotSession::~ScreenshotSession()
{
    if (!m_finished) {
        if (m_scroll) {
            delete m_scroll;
            m_scroll = nullptr;
        }
        m_toolbar = nullptr;
        for (ScreenOverlay *overlay : std::as_const(m_overlays)) delete overlay;
        m_overlays.clear();
        restoreWindows();
    }
}

void ScreenshotSession::setController(QWidget *controller)
{
    m_controller = controller;
}

void ScreenshotSession::start()
{
    if (m_started || m_finished) return;
    m_started = true;

    QWidget *controllerWindow = m_controller ? m_controller->window() : nullptr;
    const auto widgets = QApplication::topLevelWidgets();
    for (QWidget *widget : widgets) {
        if (!widget || widget == controllerWindow || !widget->isVisible()) continue;
        if (widget->windowType() == Qt::Desktop || widget->testAttribute(Qt::WA_DontShowOnScreen)) continue;
        m_hiddenWindows.append(QPointer<QWidget>(widget));
        widget->hide();
    }
    // 宿主为覆盖层准备的全屏窗口只作为生命周期锚点，真正的覆盖层按屏幕分别创建。
    if (controllerWindow) controllerWindow->hide();

    // 隐藏窗口后要等桌面完成一次重绘，否则可能截到残留的启动器。
    QTimer::singleShot(150, this, &ScreenshotSession::capture);
}

void ScreenshotSession::capture()
{
    if (m_finished) return;
    QRect physicalBounds;
    QString error;
    const QImage desktop = ShotWin::captureVirtualDesktop(&physicalBounds, &error);
    if (desktop.isNull()) {
        fail(error.isEmpty() ? tr("Cannot capture the screen") : error);
        return;
    }
    m_virtualImage = desktop;
    m_virtualPhysical = physicalBounds;

    const QList<ShotWin::MonitorInfo> monitors = ShotWin::monitors();
    for (QScreen *screen : QGuiApplication::screens()) {
        ScreenInfo info;
        info.screen = screen;
        info.logical = screen->geometry();
        info.dpr = screen->devicePixelRatio();
        for (const ShotWin::MonitorInfo &monitor : monitors) {
            if (monitor.device == screen->name()) {
                info.physical = monitor.physical;
                break;
            }
        }
        if (info.physical.isEmpty()) {
            // 监控器名称对不上时按逻辑坐标估算，至少保证单屏仍可用。
            info.physical = QRect(qRound(info.logical.x() * info.dpr), qRound(info.logical.y() * info.dpr),
                                  qRound(info.logical.width() * info.dpr),
                                  qRound(info.logical.height() * info.dpr));
        }
        info.source = QRect(info.physical.topLeft() - m_virtualPhysical.topLeft(), info.physical.size())
                          .intersected(m_virtualImage.rect());
        m_screens.append(info);
        m_virtualLogical = m_virtualLogical.united(info.logical);
    }
    if (m_screens.isEmpty()) {
        fail(tr("No screen is available for capture"));
        return;
    }

    if (m_options.fullscreen) {
        ScreenInfo *target = screenAt(QCursor::pos());
        if (!target) target = &m_screens.first();
        m_selection = target->logical;
    }
    createOverlays();
}

void ScreenshotSession::createOverlays()
{
    for (const ScreenInfo &info : std::as_const(m_screens))
        m_overlays.append(new ScreenOverlay(this, info));

    // 全部窗口都创建完再显示，避免逐个显示时抢焦点。
    for (ScreenOverlay *overlay : std::as_const(m_overlays))
        overlay->start();

    ScreenOverlay *focus = overlayFor(QCursor::pos());
    if (!focus) focus = m_overlays.value(0);
    // 每次激活后再次提到置顶层最上方，避免被其它常驻置顶窗口压住。
    for (ScreenOverlay *overlay : std::as_const(m_overlays)) overlay->forceTopmost();
    if (focus) {
        m_activeOverlay = focus;
        m_activeScreen = screenAt(focus->info().logical.center());
        focus->raise();
        focus->forceTopmost();
        focus->activateWindow();
        focus->setFocus(Qt::OtherFocusReason);
    }
    if (!m_selection.isEmpty()) showToolbar();
    for (ScreenOverlay *overlay : std::as_const(m_overlays)) overlay->update();
}

void ScreenshotSession::restoreWindows()
{
    if (m_restored) return;
    m_restored = true;
    for (const QPointer<QWidget> &window : std::as_const(m_hiddenWindows))
        if (window) window->show();
    m_hiddenWindows.clear();
    // 从其它应用按快捷键截图后，取消或完成后回到原来的前台窗口。
    if (m_previousForeground && !ShotWin::isOwnProcess(m_previousForeground))
        ShotWin::activateWindow(m_previousForeground);
}

void ScreenshotSession::teardown()
{
    endMouseGrab();
    destroyScroll();
    if (m_toolbar) m_toolbar->hide();
    m_toolbar = nullptr;
    m_toolbarOwner = nullptr;
    for (ScreenOverlay *overlay : std::as_const(m_overlays)) {
        overlay->hide();
        overlay->deleteLater();
    }
    m_overlays.clear();
    m_activeOverlay = nullptr;
    m_grabOverlay = nullptr;
    m_activeScreen = nullptr;
    restoreWindows();
}

void ScreenshotSession::fail(const QString &message)
{
    if (m_finished) return;
    m_finished = true;
    teardown();
    emit failed(message);
}

void ScreenshotSession::destroyScroll()
{
    if (!m_scroll) return;
    m_scroll->disconnect(this);
    m_scroll->deleteLater();
    m_scroll = nullptr;
}

void ScreenshotSession::beginScrollCapture()
{
    if (m_finished || m_scroll || m_selection.isEmpty()) return;
    ScreenInfo *screen = screenAt(m_selection.center());
    if (!screen || !screen->logical.contains(m_selection)) {
        fail(tr("Scrolling capture requires the selection to stay on one screen"));
        return;
    }

    if (m_toolbar) {
        m_toolbar->hide();
        m_toolbar = nullptr;
        m_toolbarOwner = nullptr;
    }
    for (ScreenOverlay *overlay : std::as_const(m_overlays)) {
        overlay->hide();
        overlay->deleteLater();
    }
    m_overlays.clear();
    m_activeOverlay = nullptr;
    m_grabOverlay = nullptr;
    m_activeScreen = nullptr;

    m_scroll = new ScrollCaptureSession(m_selection, *screen, this);
    connect(m_scroll, &ScrollCaptureSession::finished, this, &ScreenshotSession::finishScroll);
    connect(m_scroll, &ScrollCaptureSession::cancelled, this, &ScreenshotSession::cancelScroll);
    connect(m_scroll, &ScrollCaptureSession::failed, this, &ScreenshotSession::fail);
    m_scroll->start();
}

void ScreenshotSession::finishScroll(const QImage &image)
{
    if (m_finished) return;
    m_finished = true;
    destroyScroll();
    teardown();
    emit scrollFinished(image);
}

void ScreenshotSession::cancelScroll()
{
    if (m_finished) return;
    m_finished = true;
    destroyScroll();
    teardown();
    emit completed(Finish::Cancel, QImage(), m_selection);
}

void ScreenshotSession::cancel()
{
    finish(Finish::Cancel);
}

void ScreenshotSession::finish(Finish action)
{
    if (m_finished) return;
    QImage image;
    if (action != Finish::Cancel) {
        image = composeImage();
        if (image.isNull()) {
            fail(tr("The selected area is empty"));
            return;
        }
    }
    m_finished = true;
    const QRect selection = m_selection.normalized();
    teardown();
    emit completed(action, image, selection);
}

void ScreenshotSession::showToolbar()
{
    if (m_finished || m_overlays.isEmpty()) return;
    if (m_selection.isEmpty()) {
        hideToolbar();
        return;
    }
    if (!m_toolbar) {
        m_toolbar = new CaptureToolbar;
        connect(m_toolbar, &CaptureToolbar::toolChanged, this, [this](ShotTool tool) {
            setTool(tool);
            if (m_activeOverlay) m_activeOverlay->setFocus(Qt::OtherFocusReason);
        });
        connect(m_toolbar, &CaptureToolbar::colorChanged, this, [this](const QColor &color) {
            setColor(color);
            if (m_activeOverlay) m_activeOverlay->setFocus(Qt::OtherFocusReason);
        });
        connect(m_toolbar, &CaptureToolbar::widthChanged, this, [this](int width) {
            setWidth(width);
            if (m_activeOverlay) m_activeOverlay->setFocus(Qt::OtherFocusReason);
        });
        connect(m_toolbar, &CaptureToolbar::undoRequested, this, &ScreenshotSession::undo);
        connect(m_toolbar, &CaptureToolbar::redoRequested, this, &ScreenshotSession::redo);
        connect(m_toolbar, &CaptureToolbar::scrollRequested, this, [this]() { beginScrollCapture(); });
        connect(m_toolbar, &CaptureToolbar::copyRequested, this, [this]() { finish(Finish::Copy); });
        connect(m_toolbar, &CaptureToolbar::saveRequested, this, [this]() { finish(Finish::Save); });
        connect(m_toolbar, &CaptureToolbar::pinRequested, this, [this]() { finish(Finish::Pin); });
        connect(m_toolbar, &CaptureToolbar::closeRequested, this, [this]() { finish(Finish::Cancel); });
    }

    ScreenOverlay *owner = overlayFor(QPointF(m_selection.center()));
    if (!owner) owner = m_overlays.value(0);
    if (!owner) return;
    if (m_toolbar->parentWidget() != owner) {
        m_toolbar->setParent(owner);
        m_toolbar->show();
    }
    m_toolbarOwner = owner;
    m_toolbar->setTool(m_tool);
    m_toolbar->setColor(m_color);
    m_toolbar->setWidth(m_width);
    updateToolbar();
    positionToolbar();
    m_toolbar->show();
    m_toolbar->raise();
}

void ScreenshotSession::hideToolbar()
{
    if (m_toolbar) m_toolbar->hide();
}

void ScreenshotSession::positionToolbar()
{
    if (!m_toolbar || !m_toolbarOwner) return;
    m_toolbar->adjustSize();
    const QRect selectionLocal = m_selection.translated(-m_toolbarOwner->logicalOrigin());
    const QSize size = m_toolbar->size();
    int x = selectionLocal.right() - size.width() + 1;
    int y = selectionLocal.bottom() + 8;
    if (y + size.height() > m_toolbarOwner->height() - 8)
        y = selectionLocal.top() - size.height() - 8;
    if (y < 8) y = selectionLocal.bottom() + 8;
    x = qBound(8, x, qMax(8, m_toolbarOwner->width() - size.width() - 8));
    y = qBound(8, y, qMax(8, m_toolbarOwner->height() - size.height() - 8));
    m_toolbar->move(x, y);
}

void ScreenshotSession::updateToolbar()
{
    if (!m_toolbar) return;
    m_toolbar->setUndoRedo(m_historyIndex > 0, m_historyIndex + 1 < m_history.size());
}

void ScreenshotSession::refresh()
{
    if (m_finished) return;
    for (ScreenOverlay *overlay : std::as_const(m_overlays)) overlay->update();
    if (m_selection.isEmpty()) hideToolbar();
    else showToolbar();
    setCursorForOverlays();
}

void ScreenshotSession::setCursorForOverlays()
{
    const Qt::CursorShape shape = cursorFor(m_cursor);
    for (ScreenOverlay *overlay : std::as_const(m_overlays)) overlay->setCursor(shape);
}

ScreenInfo *ScreenshotSession::screenAt(const QPointF &globalPosition)
{
    const QPoint point = globalPosition.toPoint();
    for (ScreenInfo &screen : m_screens)
        if (screen.logical.contains(point)) return &screen;
    return nullptr;
}

ScreenOverlay *ScreenshotSession::overlayFor(const QPointF &globalPosition)
{
    ScreenInfo *screen = screenAt(globalPosition);
    if (!screen) return nullptr;
    for (ScreenOverlay *overlay : std::as_const(m_overlays))
        if (overlay->info().logical == screen->logical) return overlay;
    return nullptr;
}

qreal ScreenshotSession::dprAt(const QPointF &globalPosition)
{
    ScreenInfo *screen = screenAt(globalPosition);
    return screen && screen->dpr > 0 ? screen->dpr : 1.0;
}

Qt::CursorShape ScreenshotSession::cursorFor(const QPointF &globalPosition)
{
    if (m_tool == ShotTool::Text) return Qt::IBeamCursor;
    if (m_tool != ShotTool::Select) return Qt::CrossCursor;
    if (m_selection.isEmpty()) return Qt::CrossCursor;
    switch (hitHandle(globalPosition)) {
    case Handle::TopLeft:
    case Handle::BottomRight: return Qt::SizeFDiagCursor;
    case Handle::TopRight:
    case Handle::BottomLeft: return Qt::SizeBDiagCursor;
    case Handle::Top:
    case Handle::Bottom: return Qt::SizeVerCursor;
    case Handle::Left:
    case Handle::Right: return Qt::SizeHorCursor;
    case Handle::None: break;
    }
    // 标注手柄与命中优先于选区拖动，方便直接调整已画的箭头、矩形等。
    if (m_selectedAnnotation >= 0 && m_selectedAnnotation < m_annotations.size()) {
        const int handle = shotAnnotationHandleAt(m_annotations.at(m_selectedAnnotation),
                                                  globalPosition, 8.0);
        if (handle >= 0) {
            if (m_annotations.at(m_selectedAnnotation).tool == ShotTool::Line
                || m_annotations.at(m_selectedAnnotation).tool == ShotTool::Arrow)
                return Qt::SizeAllCursor;
            return handle == 0 || handle == 3 ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor;
        }
    }
    if (shotAnnotationHitTest(m_annotations, globalPosition, 4.0) >= 0) return Qt::OpenHandCursor;
    if (m_selection.contains(globalPosition.toPoint())) return Qt::SizeAllCursor;
    return Qt::CrossCursor;
}

ScreenshotSession::Handle ScreenshotSession::hitHandle(const QPointF &globalPosition) const
{
    if (m_selection.isEmpty()) return Handle::None;
    const int tolerance = 7;
    const QPoint point = globalPosition.toPoint();
    const QRect selection = m_selection;
    auto nearPoint = [&](const QPoint &target) {
        return QRect(target - QPoint(tolerance, tolerance), QSize(tolerance * 2 + 1, tolerance * 2 + 1))
            .contains(point);
    };
    if (nearPoint(selection.topLeft())) return Handle::TopLeft;
    if (nearPoint(selection.topRight())) return Handle::TopRight;
    if (nearPoint(selection.bottomLeft())) return Handle::BottomLeft;
    if (nearPoint(selection.bottomRight())) return Handle::BottomRight;
    if (nearPoint(QPoint(selection.center().x(), selection.top()))) return Handle::Top;
    if (nearPoint(QPoint(selection.center().x(), selection.bottom()))) return Handle::Bottom;
    if (nearPoint(QPoint(selection.left(), selection.center().y()))) return Handle::Left;
    if (nearPoint(QPoint(selection.right(), selection.center().y()))) return Handle::Right;
    return Handle::None;
}

void ScreenshotSession::onPress(ScreenOverlay *overlay, const QPointF &globalPosition, Qt::MouseButton button)
{
    if (m_finished || !overlay) return;
    m_activeOverlay = overlay;
    m_activeScreen = screenAt(globalPosition);
    m_cursor = globalPosition;
    m_hasCursor = true;

    if (button == Qt::RightButton) {
        if (m_tool != ShotTool::Select) setTool(ShotTool::Select);
        else if (!m_selection.isEmpty()) clearSelection();
        else finish(Finish::Cancel);
        return;
    }
    if (button != Qt::LeftButton) return;

    overlay->setFocus(Qt::MouseFocusReason);
    m_dragStart = globalPosition;
    m_dragMode = DragMode::None;

    if (m_tool == ShotTool::Select) {
        if (!m_selection.isEmpty()) {
            const Handle handle = hitHandle(globalPosition);
            if (handle != Handle::None) {
                m_dragMode = DragMode::Resize;
                m_resizeHandle = handle;
                m_originalSelection = m_selection;
                hideToolbar();
                beginMouseGrab(overlay);
                return;
            }
            // 标注编辑：先判已选标注的缩放手柄，再按层级命中已画标注。
            if (m_selectedAnnotation >= 0 && m_selectedAnnotation < m_annotations.size()) {
                const int annotationHandle = shotAnnotationHandleAt(
                    m_annotations.at(m_selectedAnnotation), globalPosition, 8.0);
                if (annotationHandle >= 0) {
                    m_annotationHandle = annotationHandle;
                    m_originalAnnotation = m_annotations.at(m_selectedAnnotation);
                    m_dragMode = DragMode::EditResize;
                    hideToolbar();
                    beginMouseGrab(overlay);
                    return;
                }
            }
            const int index = shotAnnotationHitTest(m_annotations, globalPosition, 4.0);
            if (index >= 0) {
                m_selectedAnnotation = index;
                m_annotationHandle = -1;
                m_originalAnnotation = m_annotations.at(index);
                m_editStart = globalPosition;
                m_dragMode = DragMode::EditMove;
                hideToolbar();
                beginMouseGrab(overlay);
                refresh();
                return;
            }
            m_selectedAnnotation = -1;
            if (m_selection.contains(globalPosition.toPoint())) {
                m_dragMode = DragMode::Move;
                m_originalSelection = m_selection;
                hideToolbar();
                beginMouseGrab(overlay);
                return;
            }
        }
        // 重新框选会丢弃已画标注，先确认。
        if (!confirmDiscardAnnotations()) return;
        m_dragMode = DragMode::Select;
        m_selection = QRect(globalPosition.toPoint(), QSize(0, 0));
        m_annotations.clear();
        m_selectedAnnotation = -1;
        m_annotationHandle = -1;
        m_nextNumber = 1;
        resetHistory();
        hideToolbar();
        beginMouseGrab(overlay);
        for (ScreenOverlay *item : std::as_const(m_overlays)) item->update();
        return;
    }

    if (m_selection.isEmpty()) return;
    if (!m_selection.contains(globalPosition.toPoint())) return;

    if (m_tool == ShotTool::Text) {
        overlay->beginTextInput(globalPosition);
        return;
    }
    if (m_tool == ShotTool::Number) {
        ShotAnnotation annotation;
        annotation.tool = ShotTool::Number;
        annotation.color = m_color;
        annotation.width = m_width;
        annotation.start = globalPosition;
        annotation.number = m_nextNumber++;
        m_annotations.append(annotation);
        pushHistory();
        refresh();
        return;
    }

    m_pending = ShotAnnotation{};
    m_pending.tool = m_tool;
    m_pending.color = m_color;
    m_pending.width = m_width;
    m_pending.start = globalPosition;
    m_pending.end = globalPosition;
    if (m_tool == ShotTool::Pen) m_pending.points.append(globalPosition);
    m_dragMode = DragMode::Annotate;
    beginMouseGrab(overlay);
    for (ScreenOverlay *item : std::as_const(m_overlays)) item->update();
}

void ScreenshotSession::onMove(ScreenOverlay *overlay, const QPointF &globalPosition)
{
    if (m_finished) return;
    m_cursor = globalPosition;
    m_hasCursor = true;
    if (overlay) {
        m_activeOverlay = overlay;
        m_activeScreen = screenAt(globalPosition);
    }

    switch (m_dragMode) {
    case DragMode::Select:
        m_selection = QRect(m_dragStart.toPoint(), globalPosition.toPoint()).normalized()
                          .intersected(m_virtualLogical);
        for (ScreenOverlay *item : std::as_const(m_overlays)) item->update();
        return;
    case DragMode::Move:
        applyMove(globalPosition);
        for (ScreenOverlay *item : std::as_const(m_overlays)) item->update();
        return;
    case DragMode::Resize:
        applyResize(globalPosition);
        for (ScreenOverlay *item : std::as_const(m_overlays)) item->update();
        return;
    case DragMode::Annotate:
        if (m_pending.tool == ShotTool::Pen) {
            if (m_pending.points.isEmpty()
                || QLineF(m_pending.points.last(), globalPosition).length() > 2.0)
                m_pending.points.append(globalPosition);
        } else {
            m_pending.end = globalPosition;
        }
        for (ScreenOverlay *item : std::as_const(m_overlays)) item->update();
        return;
    case DragMode::EditMove:
        if (m_selectedAnnotation >= 0 && m_selectedAnnotation < m_annotations.size()) {
            ShotAnnotation &annotation = m_annotations[m_selectedAnnotation];
            annotation = m_originalAnnotation;
            shotAnnotationTranslate(annotation, globalPosition - m_editStart);
        }
        for (ScreenOverlay *item : std::as_const(m_overlays)) item->update();
        return;
    case DragMode::EditResize:
        if (m_selectedAnnotation >= 0 && m_selectedAnnotation < m_annotations.size())
            shotAnnotationResize(m_annotations[m_selectedAnnotation], m_originalAnnotation,
                                 m_annotationHandle, globalPosition);
        for (ScreenOverlay *item : std::as_const(m_overlays)) item->update();
        return;
    case DragMode::None:
        break;
    }

    if (m_tool == ShotTool::Select && m_selection.isEmpty()) updateHover(globalPosition);
    else {
        m_hoverWindow = {};
        m_hoverTitle.clear();
    }
    for (ScreenOverlay *item : std::as_const(m_overlays)) item->update();
}

void ScreenshotSession::onRelease(ScreenOverlay *overlay, const QPointF &globalPosition)
{
    if (m_finished) return;
    Q_UNUSED(overlay)
    m_cursor = globalPosition;
    m_hasCursor = true;
    endMouseGrab();

    if (m_dragMode == DragMode::Select) {
        m_selection = m_selection.normalized();
        if (m_selection.width() < 3 || m_selection.height() < 3) {
            // 单击悬停窗口即选中该窗口；否则视为放弃本次框选。
            if (!m_hoverWindow.isEmpty()) m_selection = m_hoverWindow;
            else m_selection = {};
        }
        m_hoverWindow = {};
        m_hoverTitle.clear();
        m_dragMode = DragMode::None;
        if (!m_selection.isEmpty()) {
            m_selection = m_selection.intersected(m_virtualLogical);
            if (m_options.autoScroll && m_selection.width() >= 24 && m_selection.height() >= 24) {
                hideToolbar();
                QTimer::singleShot(0, this, &ScreenshotSession::beginScrollCapture);
                return;
            }
        }
        refresh();
        return;
    }
    if (m_dragMode == DragMode::Move || m_dragMode == DragMode::Resize) {
        m_dragMode = DragMode::None;
        refresh();
        return;
    }
    if (m_dragMode == DragMode::EditMove || m_dragMode == DragMode::EditResize) {
        // 松手位置可能比最后一次移动事件更靠近光标，必须用它补齐，
        // 否则快速拖动时标注会停在光标后面一段。
        if (m_selectedAnnotation >= 0 && m_selectedAnnotation < m_annotations.size()) {
            ShotAnnotation &annotation = m_annotations[m_selectedAnnotation];
            if (m_dragMode == DragMode::EditMove) {
                annotation = m_originalAnnotation;
                shotAnnotationTranslate(annotation, globalPosition - m_editStart);
            } else {
                shotAnnotationResize(annotation, m_originalAnnotation, m_annotationHandle,
                                     globalPosition);
            }
        }
        m_dragMode = DragMode::None;
        m_annotationHandle = -1;
        pushHistory();
        refresh();
        return;
    }
    if (m_dragMode == DragMode::Annotate) {
        bool add = true;
        if (m_pending.tool == ShotTool::Pen) add = m_pending.points.size() > 1;
        else if (m_pending.tool == ShotTool::Number) add = true;
        else add = QLineF(m_pending.start, m_pending.end).length() > 2.0;
        if (add) m_annotations.append(m_pending);
        m_pending = ShotAnnotation{};
        m_dragMode = DragMode::None;
        if (add) pushHistory();
        refresh();
        return;
    }
    m_dragMode = DragMode::None;
    refresh();
}

void ScreenshotSession::onLeave(ScreenOverlay *overlay)
{
    Q_UNUSED(overlay)
    if (m_dragMode != DragMode::None) return;
    m_hasCursor = false;
    m_hoverWindow = {};
    m_hoverTitle.clear();
    for (ScreenOverlay *item : std::as_const(m_overlays)) item->update();
}

void ScreenshotSession::onKeyPress(QKeyEvent *event)
{
    if (m_finished || !event) return;
    const int key = event->key();
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    const bool control = modifiers & Qt::ControlModifier;

    if (key == Qt::Key_Escape) {
        if (m_selectedAnnotation >= 0) {
            m_selectedAnnotation = -1;
            m_annotationHandle = -1;
            refresh();
        } else if (m_tool != ShotTool::Select) setTool(ShotTool::Select);
        else if (!m_selection.isEmpty()) clearSelection();
        else finish(Finish::Cancel);
        event->accept();
        return;
    }
    if (key == Qt::Key_Delete || key == Qt::Key_Backspace) {
        if (m_selectedAnnotation >= 0) {
            deleteSelectedAnnotation();
            event->accept();
            return;
        }
    }
    if ((key == Qt::Key_Return || key == Qt::Key_Enter) && !m_selection.isEmpty()) {
        finish(Finish::Copy);
        event->accept();
        return;
    }
    if (control) {
        if (key == Qt::Key_C && !m_selection.isEmpty()) finish(Finish::Copy);
        else if (key == Qt::Key_S && !m_selection.isEmpty()) finish(Finish::Save);
        else if (key == Qt::Key_P && !m_selection.isEmpty()) finish(Finish::Pin);
        else if (key == Qt::Key_Z) {
            if (modifiers & Qt::ShiftModifier) redo();
            else undo();
        } else if (key == Qt::Key_Y) redo();
        else if (key == Qt::Key_A) {
            m_selection = m_virtualLogical;
            m_annotations.clear();
            m_selectedAnnotation = -1;
            m_annotationHandle = -1;
            m_nextNumber = 1;
            resetHistory();
            refresh();
        }
        event->accept();
        return;
    }
    if (modifiers & (Qt::AltModifier | Qt::MetaModifier)) return;

    const int step = (modifiers & Qt::ShiftModifier) ? 10 : 1;
    switch (key) {
    case Qt::Key_Left: nudgeSelection(QPoint(-step, 0)); event->accept(); return;
    case Qt::Key_Right: nudgeSelection(QPoint(step, 0)); event->accept(); return;
    case Qt::Key_Up: nudgeSelection(QPoint(0, -step)); event->accept(); return;
    case Qt::Key_Down: nudgeSelection(QPoint(0, step)); event->accept(); return;
    case Qt::Key_S:
        if (!m_selection.isEmpty()) {
            beginScrollCapture();
            event->accept();
            return;
        }
        break;
    case Qt::Key_V: setTool(ShotTool::Select); event->accept(); return;
    case Qt::Key_R: setTool(ShotTool::Rect); event->accept(); return;
    case Qt::Key_O: setTool(ShotTool::Ellipse); event->accept(); return;
    case Qt::Key_A: setTool(ShotTool::Arrow); event->accept(); return;
    case Qt::Key_L: setTool(ShotTool::Line); event->accept(); return;
    case Qt::Key_P: setTool(ShotTool::Pen); event->accept(); return;
    case Qt::Key_T: setTool(ShotTool::Text); event->accept(); return;
    case Qt::Key_H: setTool(ShotTool::Highlight); event->accept(); return;
    case Qt::Key_M: setTool(ShotTool::Mosaic); event->accept(); return;
    case Qt::Key_B: setTool(ShotTool::Blur); event->accept(); return;
    case Qt::Key_N: setTool(ShotTool::Number); event->accept(); return;
    default:
        break;
    }
    event->ignore();
}

void ScreenshotSession::nudgeSelection(const QPoint &delta)
{
    if (m_finished || m_selection.isEmpty()) return;
    m_selection = clampInside(m_selection.translated(delta), m_virtualLogical);
    refresh();
}

void ScreenshotSession::applyMove(const QPointF &globalPosition)
{
    const QPoint delta = globalPosition.toPoint() - m_dragStart.toPoint();
    m_selection = clampInside(m_originalSelection.translated(delta), m_virtualLogical);
}

void ScreenshotSession::applyResize(const QPointF &globalPosition)
{
    const QPoint point = globalPosition.toPoint();
    QRect rect = m_originalSelection;
    switch (m_resizeHandle) {
    case Handle::TopLeft: rect.setTopLeft(point); break;
    case Handle::Top: rect.setTop(point.y()); break;
    case Handle::TopRight: rect.setTopRight(point); break;
    case Handle::Right: rect.setRight(point.x()); break;
    case Handle::BottomRight: rect.setBottomRight(point); break;
    case Handle::Bottom: rect.setBottom(point.y()); break;
    case Handle::BottomLeft: rect.setBottomLeft(point); break;
    case Handle::Left: rect.setLeft(point.x()); break;
    case Handle::None: break;
    }
    rect = rect.normalized().intersected(m_virtualLogical);
    if (rect.width() >= 1 && rect.height() >= 1) m_selection = rect;
}

void ScreenshotSession::clearSelection()
{
    if (!confirmDiscardAnnotations()) return;
    m_selection = {};
    m_annotations.clear();
    m_selectedAnnotation = -1;
    m_annotationHandle = -1;
    m_pending = ShotAnnotation{};
    m_nextNumber = 1;
    m_hoverWindow = {};
    m_hoverTitle.clear();
    m_dragMode = DragMode::None;
    resetHistory();
    hideToolbar();
    setTool(ShotTool::Select);
    for (ScreenOverlay *overlay : std::as_const(m_overlays)) overlay->update();
}

void ScreenshotSession::updateHover(const QPointF &globalPosition)
{
    m_hoverWindow = {};
    m_hoverTitle.clear();
    ScreenInfo *screen = screenAt(globalPosition);
    if (!screen || screen->physical.isEmpty()) return;
    const QPoint point = physicalPoint(*screen, globalPosition);
    QRect windowRect;
    QString title;
    if (!ShotWin::windowAt(point, &windowRect, &title)) return;

    for (const ScreenInfo &info : std::as_const(m_screens)) {
        if (!info.physical.contains(windowRect.center())) continue;
        const QRect logical(
            QPoint(qRound(info.logical.x() + (windowRect.x() - info.physical.x()) / info.dpr),
                   qRound(info.logical.y() + (windowRect.y() - info.physical.y()) / info.dpr)),
            QSize(qRound(windowRect.width() / info.dpr), qRound(windowRect.height() / info.dpr)));
        if (logical.width() < 40 || logical.height() < 40) return;
        m_hoverWindow = logical;
        m_hoverTitle = title;
        return;
    }
}

void ScreenshotSession::commitText(const QPointF &globalPosition, const QString &text)
{
    if (m_finished || text.trimmed().isEmpty()) return;
    ShotAnnotation annotation;
    annotation.tool = ShotTool::Text;
    annotation.color = m_color;
    annotation.width = m_width;
    annotation.start = globalPosition;
    annotation.text = text;
    m_annotations.append(annotation);
    pushHistory();
    refresh();
}

void ScreenshotSession::setTool(ShotTool tool)
{
    if (m_tool == tool) return;
    m_tool = tool;
    m_selectedAnnotation = -1;
    m_annotationHandle = -1;
    if (m_toolbar) m_toolbar->setTool(tool);
    setCursorForOverlays();
    for (ScreenOverlay *overlay : std::as_const(m_overlays)) overlay->update();
}

void ScreenshotSession::setColor(const QColor &color)
{
    if (!color.isValid() || m_color == color) return;
    m_color = color;
    if (m_toolbar) m_toolbar->setColor(color);
}

void ScreenshotSession::setWidth(int width)
{
    width = qBound(1, width, 40);
    if (m_width == width) return;
    m_width = width;
    if (m_toolbar) m_toolbar->setWidth(width);
}

void ScreenshotSession::undo()
{
    if (m_finished || m_historyIndex <= 0) return;
    restoreHistory(m_historyIndex - 1);
}

void ScreenshotSession::redo()
{
    if (m_finished || m_historyIndex + 1 >= m_history.size()) return;
    restoreHistory(m_historyIndex + 1);
}

void ScreenshotSession::pushHistory()
{
    if (m_historyIndex + 1 < m_history.size()) m_history.resize(m_historyIndex + 1);
    m_history.append(m_annotations);
    // 历史只存矢量数据；超过上限时丢弃最旧的一条。
    if (m_history.size() > 100) m_history.removeFirst();
    m_historyIndex = m_history.size() - 1;
    updateToolbar();
}

void ScreenshotSession::restoreHistory(int index)
{
    if (index < 0 || index >= m_history.size() || index == m_historyIndex) return;
    m_historyIndex = index;
    m_annotations = m_history.at(index);
    m_selectedAnnotation = -1;
    m_annotationHandle = -1;
    m_pending = ShotAnnotation{};
    m_dragMode = DragMode::None;
    refresh();
}

void ScreenshotSession::resetHistory()
{
    m_history.clear();
    m_history.append(m_annotations);
    m_historyIndex = 0;
    updateToolbar();
}

QWidget *ScreenshotSession::dialogParent()
{
    if (m_activeOverlay) return m_activeOverlay;
    if (!m_selection.isEmpty()) {
        if (ScreenOverlay *overlay = overlayFor(QPointF(m_selection.center()))) return overlay;
    }
    return m_overlays.isEmpty() ? nullptr : m_overlays.first();
}

void ScreenshotSession::deleteSelectedAnnotation()
{
    if (m_selectedAnnotation < 0 || m_selectedAnnotation >= m_annotations.size()) return;
    m_annotations.removeAt(m_selectedAnnotation);
    m_selectedAnnotation = -1;
    m_annotationHandle = -1;
    pushHistory();
    refresh();
}

bool ScreenshotSession::confirmDiscardAnnotations()
{
    if (m_annotations.isEmpty()) return true;
    // 覆盖层是置顶全屏窗口，对话框必须以它为父窗口才能盖在它上面。
    QMessageBox box(dialogParent());
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Discard annotations?"));
    box.setText(tr("The current screenshot has annotations."));
    box.setInformativeText(tr("Discard them and start over?"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    box.show();
    box.raise();
    box.activateWindow();
    const bool accepted = box.exec() == QMessageBox::Yes;
    // 关闭对话框后把焦点还给覆盖层，恢复快捷键。
    if (QWidget *owner = dialogParent()) {
        owner->raise();
        owner->activateWindow();
        owner->setFocus(Qt::OtherFocusReason);
    }
    return accepted;
}

void ScreenshotSession::beginMouseGrab(ScreenOverlay *overlay)
{
    if (!overlay) return;
    m_grabOverlay = overlay;
    overlay->grabMouse();
}

void ScreenshotSession::endMouseGrab()
{
    if (m_grabOverlay) {
        m_grabOverlay->releaseMouse();
        m_grabOverlay = nullptr;
    }
}

QImage ScreenshotSession::composeImage(bool includeAnnotations) const
{
    const QRect selection = m_selection.normalized().intersected(m_virtualLogical);
    if (selection.width() < 1 || selection.height() < 1) return {};

    const qreal scale = m_screens.isEmpty() ? 1.0
        : [&]() {
              for (const ScreenInfo &screen : m_screens)
                  if (screen.logical.contains(selection.topLeft())) return screen.dpr;
              return m_screens.first().dpr;
          }();
    const QSize size(qMax(1, qRound(selection.width() * scale)),
                     qMax(1, qRound(selection.height() * scale)));
    QImage base(size, QImage::Format_ARGB32_Premultiplied);
    base.fill(Qt::transparent);

    QPainter painter(&base);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setClipRect(base.rect());

    for (const ScreenInfo &screen : std::as_const(m_screens)) {
        const QRect intersection = selection.intersected(screen.logical);
        if (intersection.isEmpty() || screen.source.isEmpty()) continue;
        const QPointF localPhysical((intersection.x() - screen.logical.x()) * screen.dpr,
                                    (intersection.y() - screen.logical.y()) * screen.dpr);
        const QRectF source(screen.source.topLeft() + localPhysical,
                            QSizeF(intersection.width() * screen.dpr,
                                   intersection.height() * screen.dpr));
        const QRectF target((intersection.x() - selection.x()) * scale,
                            (intersection.y() - selection.y()) * scale,
                            intersection.width() * scale, intersection.height() * scale);
        painter.drawImage(target, m_virtualImage, source);
    }
    painter.end();
    base.setDevicePixelRatio(scale);

    // 贴图需要保留未标注的底图，方便之后继续编辑矢量标注。
    if (!includeAnnotations) return base;
    return composeShotImage(base, m_annotations);
}