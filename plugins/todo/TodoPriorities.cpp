#include "TodoPriorities.h"
#include "ThemeManager.h"

#include <QCoreApplication>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPen>

namespace TodoPriority {
namespace {
// 翻译上下文固定在 TodoWindow，与列表、提示里的同类文案共用一条译文。
// 这里必须逐个写纯字面量：lupdate 只认字面量 token，换成一个参数就提不出文案。
}

QString label(int level)
{
    if (level >= Urgent) return QCoreApplication::translate("TodoWindow", "Urgent");
    if (level == High) return QCoreApplication::translate("TodoWindow", "High");
    if (level <= Low) return QCoreApplication::translate("TodoWindow", "Low");
    return QCoreApplication::translate("TodoWindow", "Medium");
}

QString metaLabel(int level)
{
    // 中档是新建条目的默认值，行尾每行都写「中」只是噪音。
    return level == Medium ? QString() : label(level);
}

QColor color(ThemeManager *theme, int level)
{
    // 主题未注入时退回固定色，保证截图外的极端场景仍可读。
    if (level >= Urgent) return theme ? theme->error() : QColor(0xc42647);
    if (level == High) return theme ? theme->accent() : QColor(0x0088ff);
    if (level == Medium) return theme ? theme->textSecondary() : QColor(0x71717a);
    return theme ? theme->placeholder() : QColor(0xa1a1aa);
}

void drawFlag(QPainter *painter, const QRect &hotspot, const QColor &ink)
{
    // 字形固定 16px，与图标热区解耦：调用方传入多大的热区都只画中间那一面旗。
    QRect glyph(QPoint(), QSize(16, 16));
    glyph.moveCenter(hotspot.center());
    QPen pen(ink, 1.4);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(ink);
    const qreal poleX = glyph.left() + 3.5;
    painter->drawLine(QPointF(poleX, glyph.top() + 2), QPointF(poleX, glyph.bottom() - 1));
    painter->drawRoundedRect(QRectF(poleX, glyph.top() + 2, 9, 5.5), 1.5, 1.5);
}

QIcon icon(ThemeManager *theme, int level)
{
    // 在物理 32x32 画布上按 16x16 逻辑坐标绘制，最后再设 DPR：
    // 提前设 DPR 会让 QPainter 只剩左上四分之一可用，旗标被裁成一个小方块。
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    drawFlag(&painter, QRect(0, 0, 16, 16), color(theme, level));
    painter.end();
    pixmap.setDevicePixelRatio(2);
    return QIcon(pixmap);
}
} // namespace TodoPriority
