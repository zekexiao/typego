#pragma once

#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QSizeF>
#include <QVector>
#include <QWidget>

#include "Annotation.h"

class QToolButton;
class QHideEvent;

// 贴图窗口：把截图钉在桌面上，可拖动、滚轮或拖角缩放、继续标注、复制、另存和删除。
class PinWindow final : public QWidget {
    Q_OBJECT
public:
    PinWindow(const QImage &base, const QVector<ShotAnnotation> &annotations,
              const QPoint &position, QWidget *parent = nullptr);

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class DragMode { None, Move, Resize };
    enum class Corner { None, TopLeft, TopRight, BottomLeft, BottomRight };

    void buildHoverBar();
    void updateBarGeometry();
    QSizeF imageLogicalSize() const;
    const QImage &composedImage();
    void applyZoom(qreal zoom, const QPoint &anchorGlobal);
    void updateWindowSize();
    void copyImage();
    void saveAs();
    void editAnnotations();
    void setTopmost(bool on);
    void setAnnotations(const QVector<ShotAnnotation> &annotations);
    Corner cornerAt(const QPoint &local) const;
    void updateCursorShape(const QPoint &local);

    QImage m_base;
    QVector<ShotAnnotation> m_annotations;
    QImage m_composed;
    bool m_composedDirty = true;

    QWidget *m_bar = nullptr;
    QToolButton *m_topmostButton = nullptr;

    QPoint m_dragOffset;
    DragMode m_drag = DragMode::None;
    Corner m_resizeCorner = Corner::None;
    QPoint m_resizeAnchor;
    bool m_anchorAtLeft = true;
    bool m_anchorAtTop = true;

    qreal m_zoom = 1.0;
};