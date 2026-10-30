#pragma once

#include <QImage>
#include <QPoint>
#include <QSizeF>
#include <QVector>
#include <QWidget>

#include "Annotation.h"

// 贴图窗口：把截图钉在桌面上，可拖动、滚轮缩放、右键复制/另存/关闭，
// 双击进入标注编辑（修改已画的箭头、矩形等）。
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
    void hideEvent(QHideEvent *event) override;

private:
    QSizeF imageLogicalSize() const;
    const QImage &composedImage();
    void applyZoom(qreal zoom, const QPoint &anchorGlobal);
    void updateWindowSize();
    void copyImage();
    void saveAs();
    void editAnnotations();
    void setAnnotations(const QVector<ShotAnnotation> &annotations);

    QImage m_base;
    QVector<ShotAnnotation> m_annotations;
    QImage m_composed;
    bool m_composedDirty = true;

    QPoint m_dragOffset;
    bool m_dragging = false;
    qreal m_zoom = 1.0;
};