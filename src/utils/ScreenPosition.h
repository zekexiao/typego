#pragma once
#include <QList>
#include <QRect>

namespace ScreenPosition {
// 屏幕列表首项为主屏；已有负坐标同样是合法位置。
QPoint restore(const QPoint &saved, bool hasSaved, const QSize &size, const QList<QRect> &screens);
}
