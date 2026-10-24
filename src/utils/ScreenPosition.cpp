#include "ScreenPosition.h"

QPoint ScreenPosition::restore(const QPoint &saved, bool hasSaved, const QSize &size, const QList<QRect> &screens)
{
    if (screens.isEmpty()) return saved;
    auto clamp = [&](const QPoint &point, const QRect &screen) {
        return QPoint(qBound(screen.left(), point.x(), qMax(screen.left(), screen.right() - size.width() + 1)),
                      qBound(screen.top(), point.y(), qMax(screen.top(), screen.bottom() - size.height() + 1)));
    };
    if (hasSaved) {
        const QRect icon(saved, size);
        for (const QRect &screen : screens) if (screen.contains(icon)) return saved;
        for (const QRect &screen : screens) if (screen.intersects(icon)) return clamp(saved, screen);
    }
    const QRect &primary = screens.first();
    return clamp(QPoint(primary.right() - size.width() - 8, primary.bottom() - size.height() - 8), primary);
}
