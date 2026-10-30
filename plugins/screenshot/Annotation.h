#pragma once

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

// 截图标注工具。坐标统一使用全局逻辑坐标，渲染时再按屏幕 DPI 映射。
enum class ShotTool {
    Select,
    Rect,
    Ellipse,
    Arrow,
    Line,
    Pen,
    Text,
    Highlight,
    Mosaic,
    Blur,
    Number,
};

struct ShotAnnotation {
    ShotTool tool = ShotTool::Rect;
    QColor color = QColor(255, 77, 79);
    int width = 3;
    QPointF start;
    QPointF end;
    QVector<QPointF> points;
    QString text;
    int number = 0;

    QRectF rect() const { return QRectF(start, end).normalized(); }
    bool isEffect() const { return tool == ShotTool::Mosaic || tool == ShotTool::Blur; }
};

QFont shotAnnotationFont(const ShotAnnotation &annotation);
QRectF shotAnnotationBounds(const ShotAnnotation &annotation);

// 绘制单个矢量标注；Mosaic / Blur 由调用方先做像素处理，不经过这里。
void paintShotAnnotation(QPainter &painter, const ShotAnnotation &annotation);

// 对目标图像中的区域做马赛克或模糊处理。block 为马赛克块大小（像素）。
void applyShotEffect(QImage &image, const QRect &target, ShotTool tool, int block);

// 把原图与矢量标注合成为物理像素图像，标注坐标基于原图逻辑像素。
// pending 非空时一并绘制，用于拖拽中的实时预览；贴图、贴图编辑和截图导出共用同一实现。
QImage composeShotImage(const QImage &base, const QVector<ShotAnnotation> &annotations,
                        const ShotAnnotation *pending = nullptr);

// 标注命中与几何调整：坐标为标注坐标（原图逻辑像素），贴图和贴图编辑器共用。
// 返回最上层命中的标注下标，未命中返回 -1；padding 为额外的逻辑像素容差。
int shotAnnotationHitTest(const QVector<ShotAnnotation> &annotations, const QPointF &point,
                          qreal padding = 0);
// 返回可缩放标注的手柄中心（矩形四角或直线/箭头两端）。
QVector<QPointF> shotAnnotationHandles(const ShotAnnotation &annotation);
// 返回命中的手柄下标，未命中返回 -1。
int shotAnnotationHandleAt(const ShotAnnotation &annotation, const QPointF &point, qreal tolerance);
bool shotAnnotationIsRectTool(ShotTool tool);
void shotAnnotationTranslate(ShotAnnotation &annotation, const QPointF &delta);
// 按原标注和手柄拖拽点更新尺寸。
void shotAnnotationResize(ShotAnnotation &annotation, const ShotAnnotation &original, int handle,
                          const QPointF &point);
// 绘制选中框和手柄；lineScale 为屏幕像素到标注坐标的换算（1 / 显示缩放）。
void paintShotSelection(QPainter &painter, const ShotAnnotation &annotation, qreal lineScale);

int shotToolCount();
ShotTool shotToolAt(int index);