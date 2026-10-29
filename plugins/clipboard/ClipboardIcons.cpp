#include "ClipboardIcons.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QSaveFile>

namespace {
constexpr int IconSize = 64;

QString fileName(ClipboardItem::Type type)
{
    switch (type) {
    case ClipboardItem::Type::Text: return QStringLiteral("type-text");
    case ClipboardItem::Type::Files: return QStringLiteral("type-files");
    case ClipboardItem::Type::Image: return QStringLiteral("type-image");
    }
    return QStringLiteral("type-text");
}

QColor background(ClipboardItem::Type type)
{
    switch (type) {
    case ClipboardItem::Type::Text: return QColor(0x5b, 0x6b, 0x7c);
    case ClipboardItem::Type::Files: return QColor(0x3b, 0x82, 0xf6);
    case ClipboardItem::Type::Image: return QColor(0x8b, 0x5c, 0xf6);
    }
    return QColor(0x5b, 0x6b, 0x7c);
}

void drawGlyph(QPainter &painter, ClipboardItem::Type type)
{
    switch (type) {
    case ClipboardItem::Type::Text:
        // T：横杠 + 竖笔。
        painter.drawRect(QRectF(17, 17, 30, 7));
        painter.drawRect(QRectF(28, 24, 8, 24));
        break;
    case ClipboardItem::Type::Files:
        // F：竖笔 + 两段横杠。
        painter.drawRect(QRectF(22, 17, 7, 31));
        painter.drawRect(QRectF(29, 17, 14, 7));
        painter.drawRect(QRectF(29, 28, 11, 6));
        break;
    case ClipboardItem::Type::Image:
        // I：上下横杠 + 竖笔。
        painter.drawRect(QRectF(22, 17, 20, 6));
        painter.drawRect(QRectF(29, 17, 7, 31));
        painter.drawRect(QRectF(22, 42, 20, 6));
        break;
    }
}
} // namespace

QString clipboardTypeIcon(ClipboardItem::Type type, const QString &cacheDir)
{
    if (cacheDir.isEmpty()) return {};
    const QDir iconsDir(QDir(cacheDir).filePath(QStringLiteral("icons")));
    const QString path = iconsDir.filePath(fileName(type) + QStringLiteral(".png"));
    if (QFileInfo::exists(path)) return path;
    if (!QDir().mkpath(iconsDir.absolutePath())) return {};

    QImage image(IconSize, IconSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background(type));
    painter.drawRoundedRect(QRectF(4, 4, 56, 56), 12, 12);
    painter.setBrush(Qt::white);
    drawGlyph(painter, type);
    painter.end();

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit()) return {};
    return path;
}