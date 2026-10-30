#include "Annotation.h"

#include <QFontMetricsF>
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
    painter.setClipRect(image.rect());

    auto drawOne = [&](const ShotAnnotation &annotation) {
        if (annotation.isEffect()) {
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
            painter.setClipRect(image.rect());
            return;
        }
        painter.save();
        painter.scale(dpr, dpr);
        paintShotAnnotation(painter, annotation);
        painter.restore();
    };

    for (const ShotAnnotation &annotation : annotations) drawOne(annotation);
    if (pending) drawOne(*pending);
    painter.end();
    image.setDevicePixelRatio(dpr);
    return image;
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