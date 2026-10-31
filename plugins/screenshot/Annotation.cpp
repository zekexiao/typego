#include "Annotation.h"

#include <QFontMetricsF>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

QPointF normalize(const QPointF &value)
{
    const qreal length = std::hypot(value.x(), value.y());
    return length < 0.0001 ? QPointF(1, 0) : value / length;
}

qreal pointSegmentDistance(const QPointF &point, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const qreal lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
    if (lengthSquared < 0.0001) return QLineF(point, a).length();
    qreal t = ((point.x() - a.x()) * ab.x() + (point.y() - a.y()) * ab.y()) / lengthSquared;
    t = qBound(0.0, t, 1.0);
    const QPointF projection = a + ab * t;
    return QLineF(point, projection).length();
}

QPointF arrowBase(const ShotAnnotation &a, qreal head)
{
    const QPointF dir = normalize(a.end - a.start);
    return a.end - dir * head;
}

qreal arrowHead(const ShotAnnotation &a)
{
    const qreal length = QLineF(a.start, a.end).length();
    return qMin<qreal>(16.0 + a.width * 2.0, length * 0.6);
}

// 可分离盒式模糊，只处理 RGB 通道；读源写目标，避免滑动窗口读到已修改的像素。
void boxBlurRgb(QImage &image, int radius)
{
    if (radius <= 0 || image.isNull()) return;
    const int w = image.width();
    const int h = image.height();
    if (w < 2 || h < 2) return;
    const QImage source = image.convertToFormat(QImage::Format_RGB32);
    QImage horizontal(source.size(), QImage::Format_RGB32);
    QImage output(source.size(), QImage::Format_RGB32);
    const int window = radius * 2 + 1;
    // 水平方向。
    for (int y = 0; y < h; ++y) {
        const auto *src = reinterpret_cast<const QRgb *>(source.scanLine(y));
        auto *dst = reinterpret_cast<QRgb *>(horizontal.scanLine(y));
        int r = 0, g = 0, b = 0;
        for (int i = -radius; i <= radius; ++i) {
            const QRgb pixel = src[qBound(0, i, w - 1)];
            r += qRed(pixel);
            g += qGreen(pixel);
            b += qBlue(pixel);
        }
        for (int x = 0; x < w; ++x) {
            dst[x] = qRgb(r / window, g / window, b / window);
            const QRgb remove = src[qBound(0, x - radius, w - 1)];
            const QRgb add = src[qBound(0, x + radius + 1, w - 1)];
            r += qRed(add) - qRed(remove);
            g += qGreen(add) - qGreen(remove);
            b += qBlue(add) - qBlue(remove);
        }
    }
    // 垂直方向。
    for (int x = 0; x < w; ++x) {
        int r = 0, g = 0, b = 0;
        for (int i = -radius; i <= radius; ++i)
        {
            const QRgb pixel = reinterpret_cast<const QRgb *>(horizontal.scanLine(qBound(0, i, h - 1)))[x];
            r += qRed(pixel);
            g += qGreen(pixel);
            b += qBlue(pixel);
        }
        for (int y = 0; y < h; ++y) {
            auto *dst = reinterpret_cast<QRgb *>(output.scanLine(y));
            dst[x] = qRgb(r / window, g / window, b / window);
            const QRgb remove = reinterpret_cast<const QRgb *>(horizontal.scanLine(qBound(0, y - radius, h - 1)))[x];
            const QRgb add = reinterpret_cast<const QRgb *>(horizontal.scanLine(qBound(0, y + radius + 1, h - 1)))[x];
            r += qRed(add) - qRed(remove);
            g += qGreen(add) - qGreen(remove);
            b += qBlue(add) - qBlue(remove);
        }
    }
    image = output;
}

} // namespace

QFont shotAnnotationFont(const ShotAnnotation &annotation)
{
    QFont font;
    font.setPixelSize(qBound(12, 12 + annotation.width * 3, 48));
    font.setBold(true);
    return font;
}

QRectF shotAnnotationBounds(const ShotAnnotation &annotation)
{
    const qreal pad = annotation.width + 2.0;
    switch (annotation.tool) {
    case ShotTool::Pen: {
        if (annotation.points.isEmpty()) return {};
        QRectF rect(annotation.points.first(), QSizeF(0, 0));
        for (const QPointF &point : annotation.points) rect |= QRectF(point, QSizeF(0, 0));
        return rect.adjusted(-pad, -pad, pad, pad);
    }
    case ShotTool::Text: {
        const QFontMetricsF metrics(shotAnnotationFont(annotation));
        QRectF rect = metrics.boundingRect(QRectF(0, 0, 10000, 10000),
                                           Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                                           annotation.text);
        return rect.translated(annotation.start).adjusted(-pad, -pad, pad, pad);
    }
    case ShotTool::Number: {
        const qreal radius = 9.0 + annotation.width * 2.0;
        return QRectF(annotation.start - QPointF(radius, radius), QSizeF(radius * 2, radius * 2));
    }
    default:
        return annotation.rect().adjusted(-pad, -pad, pad, pad);
    }
}

void paintShotAnnotation(QPainter &painter, const ShotAnnotation &a)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(a.color, a.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    switch (a.tool) {
    case ShotTool::Select:
        break;
    case ShotTool::Rect:
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(a.rect());
        break;
    case ShotTool::Ellipse:
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(a.rect());
        break;
    case ShotTool::Line:
        painter.setPen(pen);
        painter.drawLine(a.start, a.end);
        break;
    case ShotTool::Arrow: {
        const qreal length = QLineF(a.start, a.end).length();
        if (length < 1.0) break;
        const qreal head = arrowHead(a);
        const QPointF dir = normalize(a.end - a.start);
        const QPointF normal(-dir.y(), dir.x());
        const QPointF base = arrowBase(a, head);
        QPolygonF headPolygon;
        headPolygon << a.end << base + normal * head * 0.42 << base - normal * head * 0.42;
        painter.setPen(pen);
        painter.drawLine(a.start, base);
        painter.setPen(Qt::NoPen);
        painter.setBrush(a.color);
        painter.drawPolygon(headPolygon);
        break;
    }
    case ShotTool::Pen: {
        painter.setPen(pen);
        if (a.points.size() == 1) painter.drawPoint(a.points.first());
        else if (a.points.size() > 1) painter.drawPolyline(a.points.constData(), a.points.size());
        break;
    }
    case ShotTool::Highlight: {
        QColor color = a.color;
        color.setAlpha(80);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(a.rect(), 2, 2);
        break;
    }
    case ShotTool::Text: {
        painter.setFont(shotAnnotationFont(a));
        painter.setPen(a.color);
        const QFontMetricsF metrics(shotAnnotationFont(a));
        const QStringList lines = a.text.split(QLatin1Char('\n'));
        qreal y = a.start.y() + metrics.ascent();
        for (const QString &line : lines) {
            painter.drawText(QPointF(a.start.x(), y), line);
            y += metrics.lineSpacing();
        }
        break;
    }
    case ShotTool::Number: {
        const qreal radius = 9.0 + a.width * 2.0;
        painter.setPen(Qt::NoPen);
        painter.setBrush(a.color);
        painter.drawEllipse(a.start, radius, radius);
        QFont font = shotAnnotationFont(a);
        font.setPixelSize(qMax(11, qRound(radius * 1.15)));
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(QRectF(a.start - QPointF(radius, radius), QSizeF(radius * 2, radius * 2)),
                         Qt::AlignCenter, QString::number(a.number));
        break;
    }
    case ShotTool::Mosaic:
    case ShotTool::Blur:
        break;
    }
    painter.restore();
}

void applyShotEffect(QImage &image, const QRect &target, ShotTool tool, int block)
{
    if (image.isNull()) return;
    const QRect area = target.intersected(image.rect());
    if (area.width() < 2 || area.height() < 2) return;
    if (tool == ShotTool::Mosaic) {
        block = qMax(2, block);
        const QSize smallSize(qMax(1, area.width() / block), qMax(1, area.height() / block));
        QImage patch = image.copy(area).scaled(smallSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        patch = patch.scaled(area.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(area.topLeft(), patch);
        return;
    }
    QImage patch = image.copy(area).convertToFormat(QImage::Format_RGB32);
    // 半径随区域面积收敛，避免大区域拖慢交互。
    const int radius = qBound(2, block / 2, 24);
    boxBlurRgb(patch, radius);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(area.topLeft(), patch);
}

QImage composeShotImage(const QImage &base, const QVector<ShotAnnotation> &annotations,
                        const ShotAnnotation *pending)
{
    if (base.isNull()) return {};
    const qreal dpr = base.devicePixelRatio() > 0 ? base.devicePixelRatio() : 1.0;
    QImage image = base.format() == QImage::Format_ARGB32_Premultiplied
        ? base : base.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return {};
    image.setDevicePixelRatio(dpr);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    // 画布按逻辑尺寸裁剪；带 DPR 的图像上 QPainter 已经把逻辑坐标换算成物理像素，
    // 这里再手动 scale(dpr) 会让矢量标注按 DPR 平方放大。
    const QRectF canvas(0, 0, image.width() / dpr, image.height() / dpr);
    painter.setClipRect(canvas);

    auto drawOne = [&](const ShotAnnotation &annotation) {
        if (annotation.isEffect()) {
            // 效果标注直接改像素，目标区域要换算成物理像素。
            const QRectF logical = annotation.rect();
            const QRect target = QRectF(logical.x() * dpr, logical.y() * dpr,
                                        logical.width() * dpr, logical.height() * dpr)
                                     .toAlignedRect().intersected(image.rect());
            if (target.width() < 2 || target.height() < 2) return;
            // 效果标注直接改像素，需要临时结束绘制器。
            painter.end();
            applyShotEffect(image, target, annotation.tool, qMax(3, qRound(8 * dpr)));
            painter.begin(&image);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
            painter.setClipRect(canvas);
            return;
        }
        painter.save();
        paintShotAnnotation(painter, annotation);
        painter.restore();
    };

    for (const ShotAnnotation &annotation : annotations) drawOne(annotation);
    if (pending) drawOne(*pending);
    painter.end();
    image.setDevicePixelRatio(dpr);
    return image;
}

QVector<ShotAnnotation> shotAnnotationsTranslate(const QVector<ShotAnnotation> &annotations,
                                                 const QPointF &delta)
{
    QVector<ShotAnnotation> result = annotations;
    for (ShotAnnotation &annotation : result) {
        annotation.start += delta;
        annotation.end += delta;
        for (QPointF &point : annotation.points) point += delta;
    }
    return result;
}

bool shotAnnotationIsRectTool(ShotTool tool)
{
    return tool == ShotTool::Rect || tool == ShotTool::Ellipse
        || tool == ShotTool::Highlight || tool == ShotTool::Mosaic || tool == ShotTool::Blur;
}

QVector<QPointF> shotAnnotationHandles(const ShotAnnotation &annotation)
{
    QVector<QPointF> points;
    switch (annotation.tool) {
    case ShotTool::Rect:
    case ShotTool::Ellipse:
    case ShotTool::Highlight:
    case ShotTool::Mosaic:
    case ShotTool::Blur: {
        const QRectF rect = annotation.rect();
        points << rect.topLeft() << rect.topRight() << rect.bottomLeft() << rect.bottomRight();
        break;
    }
    case ShotTool::Line:
    case ShotTool::Arrow:
        points << annotation.start << annotation.end;
        break;
    default:
        break;
    }
    return points;
}

int shotAnnotationHandleAt(const ShotAnnotation &annotation, const QPointF &point, qreal tolerance)
{
    const QVector<QPointF> handles = shotAnnotationHandles(annotation);
    for (int index = 0; index < handles.size(); ++index)
        if (QLineF(handles.at(index), point).length() <= tolerance) return index;
    return -1;
}

int shotAnnotationHitTest(const QVector<ShotAnnotation> &annotations, const QPointF &point,
                          qreal padding)
{
    // 后画的标注在上层，优先命中。
    for (int index = annotations.size() - 1; index >= 0; --index) {
        const ShotAnnotation &annotation = annotations.at(index);
        const qreal tolerance = qMax(4.0, annotation.width * 1.3) + padding;
        switch (annotation.tool) {
        case ShotTool::Select:
            continue;
        case ShotTool::Rect:
        case ShotTool::Ellipse: {
            const QRectF rect = annotation.rect();
            const QRectF outer = rect.adjusted(-tolerance, -tolerance, tolerance, tolerance);
            const QRectF inner = rect.adjusted(tolerance, tolerance, -tolerance, -tolerance);
            // 命中边框附近；细小的图形允许点中内部。
            if (outer.contains(point) && !inner.contains(point)) return index;
            if (rect.width() < 12 && rect.height() < 12 && outer.contains(point)) return index;
            break;
        }
        case ShotTool::Line:
        case ShotTool::Arrow:
            if (pointSegmentDistance(point, annotation.start, annotation.end) <= tolerance)
                return index;
            break;
        case ShotTool::Pen: {
            const QVector<QPointF> &points = annotation.points;
            if (points.size() == 1 && QLineF(points.first(), point).length() <= tolerance)
                return index;
            for (int i = 1; i < points.size(); ++i)
                if (pointSegmentDistance(point, points.at(i - 1), points.at(i)) <= tolerance)
                    return index;
            break;
        }
        case ShotTool::Highlight:
        case ShotTool::Mosaic:
        case ShotTool::Blur:
        case ShotTool::Text:
        case ShotTool::Number:
            if (shotAnnotationBounds(annotation).adjusted(-2, -2, 2, 2).contains(point)) return index;
            break;
        }
    }
    return -1;
}

void shotAnnotationTranslate(ShotAnnotation &annotation, const QPointF &delta)
{
    annotation.start += delta;
    annotation.end += delta;
    for (QPointF &point : annotation.points) point += delta;
}

void shotAnnotationResize(ShotAnnotation &annotation, const ShotAnnotation &original, int handle,
                          const QPointF &point)
{
    if (shotAnnotationIsRectTool(annotation.tool)) {
        const QRectF rect = original.rect();
        QPointF topLeft = rect.topLeft();
        QPointF bottomRight = rect.bottomRight();
        switch (handle) {
        case 0: topLeft = point; break;
        case 1: topLeft.setY(point.y()); bottomRight.setX(point.x()); break;
        case 2: topLeft.setX(point.x()); bottomRight.setY(point.y()); break;
        case 3: bottomRight = point; break;
        default: return;
        }
        const QRectF next = QRectF(topLeft, bottomRight).normalized();
        annotation.start = next.topLeft();
        annotation.end = next.bottomRight();
        return;
    }
    if (annotation.tool == ShotTool::Line || annotation.tool == ShotTool::Arrow) {
        if (handle == 0) annotation.start = point;
        else if (handle == 1) annotation.end = point;
    }
}

void paintShotSelection(QPainter &painter, const ShotAnnotation &annotation, qreal lineScale)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal lineWidth = 1.2 * lineScale;
    painter.setPen(QPen(QColor(22, 119, 255), lineWidth, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(shotAnnotationBounds(annotation));

    const qreal handleSize = 9.0 * lineScale;
    for (const QPointF &center : shotAnnotationHandles(annotation)) {
        const QRectF rect(center.x() - handleSize / 2, center.y() - handleSize / 2,
                          handleSize, handleSize);
        painter.setPen(QPen(QColor(22, 119, 255), lineWidth));
        painter.setBrush(QColor(255, 255, 255));
        painter.drawRect(rect);
    }
    painter.restore();
}

int shotToolCount()
{
    return 11;
}

ShotTool shotToolAt(int index)
{
    static const ShotTool tools[] = {
        ShotTool::Select, ShotTool::Rect, ShotTool::Ellipse, ShotTool::Arrow, ShotTool::Line,
        ShotTool::Pen, ShotTool::Text, ShotTool::Highlight, ShotTool::Mosaic, ShotTool::Blur,
        ShotTool::Number,
    };
    return index >= 0 && index < 11 ? tools[index] : ShotTool::Select;
}