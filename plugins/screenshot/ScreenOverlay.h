#pragma once

#include <QPointF>
#include <QRect>
#include <QWidget>

#include "ScreenInfo.h"

class QLineEdit;
class ScreenshotSession;
class ShotAnnotation;

// 单个屏幕上的全屏截图覆盖层。所有屏幕各有一个，选区用全局逻辑坐标共享。
class ScreenOverlay final : public QWidget {
    Q_OBJECT
public:
    ScreenOverlay(ScreenshotSession *session, const ScreenInfo &info);
    ~ScreenOverlay() override;

    const ScreenInfo &info() const { return m_info; }
    QPoint logicalOrigin() const { return m_info.logical.topLeft(); }
    void start();
    // Windows 上把覆盖层提到置顶层最上方，压过其它常驻置顶窗口。
    void forceTopmost();
    void beginTextInput(const QPointF &globalPosition);
    void cancelTextInput();
    bool hasTextInput() const { return m_textEdit != nullptr; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointF toLocal(const QPointF &global) const;
    QRect toLocal(const QRect &global) const;
    void paintDesktop(QPainter &painter);
    void paintAnnotations(QPainter &painter, const QRect &selectionLocal);
    void paintEffect(QPainter &painter, const ShotAnnotation &annotation);
    void paintDim(QPainter &painter, const QRect &selectionLocal);
    void paintSelection(QPainter &painter, const QRect &selectionLocal);
    void paintHandles(QPainter &painter, const QRect &selectionLocal);
    void paintSizeBadge(QPainter &painter, const QRect &selectionLocal);
    void paintGuides(QPainter &painter);
    void paintHover(QPainter &painter);
    void paintMagnifier(QPainter &painter);
    void updateCursorShape();
    void commitTextInput();

    ScreenshotSession *m_session = nullptr;
    ScreenInfo m_info;
    QLineEdit *m_textEdit = nullptr;
    QPointF m_textOrigin;
};