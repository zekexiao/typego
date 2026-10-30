#include "ShotIcons.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

#include <cmath>
#include <functional>

namespace {

constexpr int Canvas = 22;
const QColor Ink(242, 244, 247);

QIcon makeIcon(const std::function<void(QPainter &)> &draw)
{
    // 先在物理像素画布上放大绘制，结束时再设置 DPR。
    // 若提前设置 DPR 又手动 scale，QPainter 会再缩放一次，图标只剩左上角一块。
    QPixmap pixmap(Canvas * 2, Canvas * 2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(2.0, 2.0);
    draw(painter);
    painter.end();
    pixmap.setDevicePixelRatio(2.0);
    return QIcon(pixmap);
}

void drawArrowHead(QPainter &painter, const QPointF &tip, const QPointF &from)
{
    const QPointF delta = tip - from;
    const qreal length = std::hypot(delta.x(), delta.y());
    if (length < 0.001) return;
    const QPointF dir = delta / length;
    const QPointF normal(-dir.y(), dir.x());
    QPolygonF head;
    head << tip << tip - dir * 6 + normal * 2.6 << tip - dir * 6 - normal * 2.6;
    painter.setPen(Qt::NoPen);
    painter.setBrush(Ink);
    painter.drawPolygon(head);
}

} // namespace

QIcon shotToolIcon(ShotTool tool)
{
    return makeIcon([tool](QPainter &p) {
        QPen pen(Ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        switch (tool) {
        case ShotTool::Select: {
            QPainterPath path;
            path.moveTo(5, 3);
            path.lineTo(5, 17);
            path.lineTo(9, 13.4);
            path.lineTo(11.6, 18.6);
            path.lineTo(14, 17.4);
            path.lineTo(11.4, 12.4);
            path.lineTo(16.4, 12.4);
            path.closeSubpath();
            p.setBrush(Ink);
            p.setPen(Qt::NoPen);
            p.drawPath(path);
            break;
        }
        case ShotTool::Rect:
            p.drawRect(QRectF(3.5, 5, 15, 12));
            break;
        case ShotTool::Ellipse:
            p.drawEllipse(QRectF(3.5, 5, 15, 12));
            break;
        case ShotTool::Line:
            p.drawLine(QPointF(4, 17), QPointF(18, 5));
            break;
        case ShotTool::Arrow:
            p.drawLine(QPointF(4, 17), QPointF(16, 7));
            drawArrowHead(p, QPointF(18, 5), QPointF(4, 17));
            break;
        case ShotTool::Pen: {
            QPainterPath path;
            path.moveTo(4, 15);
            path.cubicTo(7, 4, 11, 19, 18, 7);
            p.drawPath(path);
            break;
        }
        case ShotTool::Text: {
            QFont font;
            font.setPixelSize(15);
            font.setBold(true);
            p.setFont(font);
            p.drawText(QRectF(0, 0, Canvas, Canvas), Qt::AlignCenter, QStringLiteral("T"));
            break;
        }
        case ShotTool::Highlight: {
            QColor color(Ink);
            color.setAlpha(90);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRoundedRect(QRectF(3, 8, 16, 7), 2, 2);
            p.setPen(QPen(Ink, 1.2));
            p.drawLine(QPointF(3, 6), QPointF(19, 6));
            break;
        }
        case ShotTool::Mosaic:
            p.setPen(Qt::NoPen);
            p.setBrush(Ink);
            for (int y = 0; y < 3; ++y)
                for (int x = 0; x < 3; ++x)
                    if ((x + y) % 2 == 0) p.drawRect(QRectF(4 + x * 5, 5 + y * 4, 4, 3));
            break;
        case ShotTool::Blur: {
            p.setPen(QPen(Ink, 1.4, Qt::SolidLine, Qt::RoundCap));
            for (int i = 0; i < 3; ++i) {
                const qreal y = 7 + i * 4;
                QPainterPath path;
                path.moveTo(4, y);
                path.cubicTo(8, y - 2, 12, y + 2, 18, y);
                p.drawPath(path);
            }
            break;
        }
        case ShotTool::Number: {
            QFont font;
            font.setPixelSize(13);
            font.setBold(true);
            p.setFont(font);
            p.drawText(QRectF(0, 0, Canvas, Canvas), Qt::AlignCenter, QStringLiteral("1"));
            break;
        }
        }
    });
}

QIcon shotColorIcon(const QColor &color)
{
    return makeIcon([color](QPainter &p) {
        p.setPen(QPen(QColor(255, 255, 255, 150), 1.0));
        p.setBrush(color);
        p.drawEllipse(QRectF(4, 4, 14, 14));
    });
}

QIcon shotWidthIcon(int width)
{
    return makeIcon([width](QPainter &p) {
        p.setPen(QPen(Ink, qMax(1, width / 2), Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(5, 11), QPointF(17, 11));
    });
}

QIcon shotActionIcon(const QString &name)
{
    return makeIcon([name](QPainter &p) {
        QPen pen(Ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        if (name == QLatin1String("undo") || name == QLatin1String("redo")) {
            const bool redo = name == QLatin1String("redo");
            p.save();
            if (redo) {
                p.translate(Canvas, 0);
                p.scale(-1, 1);
            }
            QPainterPath path;
            path.moveTo(7, 7);
            path.cubicTo(15, 5, 18, 10, 16, 15);
            p.drawPath(path);
            QPolygonF head;
            head << QPointF(7, 3.5) << QPointF(7, 10.5) << QPointF(3, 7);
            p.setBrush(Ink);
            p.setPen(Qt::NoPen);
            p.drawPolygon(head);
            p.restore();
        } else if (name == QLatin1String("copy")) {
            p.drawRoundedRect(QRectF(4, 4, 10, 12), 1.5, 1.5);
            p.drawRoundedRect(QRectF(8, 7, 10, 12), 1.5, 1.5);
        } else if (name == QLatin1String("save")) {
            p.drawRoundedRect(QRectF(4, 4, 14, 14), 2, 2);
            p.drawRect(QRectF(7, 4, 8, 5));
            p.drawRect(QRectF(7, 12, 8, 6));
        } else if (name == QLatin1String("pin")) {
            QPainterPath path;
            path.moveTo(11, 3.5);
            path.lineTo(11, 12);
            path.moveTo(7, 9);
            path.lineTo(15, 9);
            path.moveTo(9.5, 14);
            path.lineTo(8, 19);
            path.moveTo(12.5, 14);
            path.lineTo(14, 19);
            p.drawPath(path);
            p.setBrush(Ink);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRectF(7.5, 7, 7, 3), 1.2, 1.2);
        } else if (name == QLatin1String("scroll")) {
            p.drawRoundedRect(QRectF(7.5, 2.5, 7, 13), 1.4, 1.4);
            p.drawLine(QPointF(11, 5), QPointF(11, 12));
            QPolygonF arrow;
            arrow << QPointF(11, 19.5) << QPointF(8, 15.5) << QPointF(14, 15.5);
            p.setPen(Qt::NoPen);
            p.setBrush(Ink);
            p.drawPolygon(arrow);
        } else if (name == QLatin1String("edit")) {
            // 铅笔：用于重新标注贴图。
            QPolygonF tip;
            tip << QPointF(4.5, 16.5) << QPointF(5.5, 12.5) << QPointF(14, 4)
                << QPointF(17.5, 7.5) << QPointF(9, 16) ;
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(tip);
            p.drawLine(QPointF(5.5, 12.5), QPointF(9, 16));
            p.drawLine(QPointF(12.5, 5.5), QPointF(16, 9));
        } else if (name == QLatin1String("delete")) {
            p.drawRoundedRect(QRectF(6, 6.5, 10, 11), 1.5, 1.5);
            p.drawLine(QPointF(4.5, 6), QPointF(17.5, 6));
            p.drawLine(QPointF(9, 3.5), QPointF(13, 3.5));
            p.drawLine(QPointF(9.5, 9.5), QPointF(9.5, 14.5));
            p.drawLine(QPointF(12.5, 9.5), QPointF(12.5, 14.5));
        } else if (name == QLatin1String("close")) {
            p.drawLine(QPointF(6, 6), QPointF(16, 16));
            p.drawLine(QPointF(16, 6), QPointF(6, 16));
        }
    });
}