#pragma once

#include <QImage>
#include <QPoint>
#include <QWidget>

// 贴图窗口：把截图钉在桌面上，可拖动、滚轮缩放、复制或另存。
class PinWindow final : public QWidget {
    Q_OBJECT
public:
    PinWindow(const QImage &image, const QPoint &position, QWidget *parent = nullptr);

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

private:
    void applyZoom(qreal zoom);
    void updateWindowSize();
    void saveAs();

    QImage m_image;
    QPoint m_dragOffset;
    bool m_dragging = false;
    qreal m_zoom = 1.0;
};