#pragma once

#include <QImage>
#include <QString>
#include <QVector>
#include <QWidget>

#include "Annotation.h"

class CaptureToolbar;
class QLineEdit;

// 贴图编辑窗口：复用截图标注工具，修改贴图上的矢量标注。
// 编辑结果实时回传给贴图窗口，关闭即保留当前标注。
class PinEditor final : public QWidget {
    Q_OBJECT
public:
    PinEditor(const QImage &base, const QVector<ShotAnnotation> &annotations,
              QWidget *parent = nullptr);

signals:
    void annotationsChanged(const QVector<ShotAnnotation> &annotations);
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QSizeF imageLogicalSize() const;
    QPointF toLogical(const QPointF &local) const;
    void setTool(ShotTool tool);
    void undo();
    void redo();
    void commitAnnotation(const ShotAnnotation &annotation);
    void notifyChanged();
    void beginTextInput(const QPointF &logicalPosition);
    void cancelTextInput();
    void commitTextInput();
    QImage composedImage() const;
    void copyImage();
    void saveAs();
    void paintPendingAnnotation(QPainter &painter);

    QImage m_base;
    QVector<ShotAnnotation> m_annotations;
    QVector<ShotAnnotation> m_redo;
    QImage m_composed;
    QRect m_canvasRect;
    qreal m_scale = 1.0;
    CaptureToolbar *m_toolbar = nullptr;

    ShotTool m_tool = ShotTool::Select;
    QColor m_color = QColor(255, 77, 79);
    int m_width = 4;
    int m_nextNumber = 1;

    ShotAnnotation m_pending;
    bool m_hasPending = false;
    QLineEdit *m_textEdit = nullptr;
    QPointF m_textOrigin;
};