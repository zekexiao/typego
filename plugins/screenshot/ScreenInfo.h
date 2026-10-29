#pragma once

#include <QRect>

class QScreen;

// 一个屏幕在 Qt 逻辑坐标和 Windows 物理像素下的位置。
struct ScreenInfo {
    QScreen *screen = nullptr;
    QRect logical;
    QRect physical;
    QRect source; // 在虚拟桌面截图中的裁剪区域
    qreal dpr = 1.0;
};