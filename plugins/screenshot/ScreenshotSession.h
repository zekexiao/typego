#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QVector>
#include <Qt>

#include "Annotation.h"
#include "ScreenInfo.h"

class CaptureToolbar;
class QKeyEvent;
class QWidget;
class ScreenOverlay;
class ScrollCaptureSession;
class ShotAnnotation;

// 一次截图会话：隐藏 TypeGo 窗口、捕获桌面、管理选区/标注，最后输出图像。
class ScreenshotSession final : public QObject {
    Q_OBJECT
public:
    enum class Finish { Copy, Save, Pin, Cancel };

    struct Options {
        bool fullscreen = false;
        bool magnifier = true;
        int magnifierZoom = 8;
        bool autoScroll = false;
    };

    ScreenshotSession(quintptr previousForeground, Options options, QObject *parent = nullptr);
    ~ScreenshotSession() override;

    void setController(QWidget *controller);
    void start();
    void cancel();

    // 供覆盖层读取状态和回传输入。
    const QImage &desktopImage() const { return m_virtualImage; }
    const QList<ScreenInfo> &screens() const { return m_screens; }
    const QRect &virtualRect() const { return m_virtualLogical; }
    const QRect &selection() const { return m_selection; }
    const QVector<ShotAnnotation> &annotations() const { return m_annotations; }
    ShotTool tool() const { return m_tool; }
    QColor color() const { return m_color; }
    int width() const { return m_width; }
    bool magnifierEnabled() const { return m_options.magnifier; }
    int magnifierZoom() const { return m_options.magnifierZoom; }
    QPointF cursor() const { return m_cursor; }
    bool hasCursor() const { return m_hasCursor; }
    bool draggingSelection() const { return m_dragMode == DragMode::Select; }
    const QRect &hoverWindow() const { return m_hoverWindow; }
    const QString &hoverTitle() const { return m_hoverTitle; }
    int selectedAnnotation() const { return m_selectedAnnotation; }
    ScreenInfo *screenAt(const QPointF &globalPosition);
    ScreenInfo *activeScreen() const { return m_activeScreen; }
    ScreenOverlay *activeOverlay() const { return m_activeOverlay; }
    Qt::CursorShape cursorFor(const QPointF &globalPosition);
    const ShotAnnotation &pendingAnnotation() const { return m_pending; }
    bool hasPendingAnnotation() const { return m_dragMode == DragMode::Annotate; }

    void onPress(ScreenOverlay *overlay, const QPointF &globalPosition, Qt::MouseButton button);
    void onMove(ScreenOverlay *overlay, const QPointF &globalPosition);
    void onRelease(ScreenOverlay *overlay, const QPointF &globalPosition);
    void onLeave(ScreenOverlay *overlay);
    void onKeyPress(QKeyEvent *event);
    void commitText(const QPointF &globalPosition, const QString &text);

    void setTool(ShotTool tool);
    void setColor(const QColor &color);
    void setWidth(int width);
    void undo();
    void redo();
    void pushHistory();
    void restoreHistory(int index);
    void resetHistory();
    void deleteSelectedAnnotation();
    void finish(Finish action);

    QImage composeImage(bool includeAnnotations = true) const;

signals:
    void completed(ScreenshotSession::Finish action, const QImage &image, const QRect &selection);
    void scrollFinished(const QImage &image);
    void failed(const QString &message);

private:
    enum class Handle { None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };
    enum class DragMode { None, Select, Move, Resize, Annotate, EditMove, EditResize };

    void capture();
    void createOverlays();
    void beginScrollCapture();
    void finishScroll(const QImage &image);
    void cancelScroll();
    void destroyScroll();
    void refresh();
    void restoreWindows();
    void teardown();
    void fail(const QString &message);
    void showToolbar();
    void hideToolbar();
    void positionToolbar();
    void updateToolbar();
    void updateHover(const QPointF &globalPosition);
    void clearSelection();
    Handle hitHandle(const QPointF &globalPosition) const;
    void nudgeSelection(const QPoint &delta);
    void applyResize(const QPointF &globalPosition);
    void applyMove(const QPointF &globalPosition);
    void beginMouseGrab(ScreenOverlay *overlay);
    void endMouseGrab();
    ScreenOverlay *overlayFor(const QPointF &globalPosition);
    qreal dprAt(const QPointF &globalPosition);
    void setCursorForOverlays();

    QWidget *m_controller = nullptr;
    QList<QPointer<QWidget>> m_hiddenWindows;
    quintptr m_previousForeground = 0;
    Options m_options;

    QImage m_virtualImage;
    QRect m_virtualPhysical;
    QRect m_virtualLogical;
    QList<ScreenInfo> m_screens;

    QList<ScreenOverlay *> m_overlays;
    ScrollCaptureSession *m_scroll = nullptr;
    CaptureToolbar *m_toolbar = nullptr;
    QPointer<ScreenOverlay> m_toolbarOwner;
    ScreenOverlay *m_activeOverlay = nullptr;
    ScreenOverlay *m_grabOverlay = nullptr;
    ScreenInfo *m_activeScreen = nullptr;

    QRect m_selection;
    QVector<ShotAnnotation> m_annotations;
    // 标注历史采用快照，移动、缩放、改字、删除都能统一撤销。
    QVector<QVector<ShotAnnotation>> m_history;
    int m_historyIndex = 0;
    int m_selectedAnnotation = -1;
    int m_annotationHandle = -1;
    ShotAnnotation m_originalAnnotation;
    QPointF m_editStart;
    ShotAnnotation m_pending;
    QRect m_hoverWindow;
    QString m_hoverTitle;
    QPointF m_cursor;
    bool m_hasCursor = false;

    ShotTool m_tool = ShotTool::Select;
    QColor m_color = QColor(255, 77, 79);
    int m_width = 4;
    int m_nextNumber = 1;

    DragMode m_dragMode = DragMode::None;
    QPointF m_dragStart;
    QRect m_originalSelection;
    Handle m_resizeHandle = Handle::None;

    bool m_started = false;
    bool m_restored = false;
    bool m_finished = false;
};