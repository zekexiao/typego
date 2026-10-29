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

int shotToolCount();
ShotTool shotToolAt(int index);