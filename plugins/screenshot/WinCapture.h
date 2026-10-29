#pragma once

#include <QImage>
#include <QList>
#include <QRect>
#include <QString>

// Windows 桌面捕获与窗口探测。插件只发布 Windows x64 SDK，因此这里直接使用 Win32，
// 保证多屏物理像素和混合 DPI 下的坐标一致。
namespace ShotWin {

struct MonitorInfo {
    QString device;
    QRect physical;
};

QList<MonitorInfo> monitors();

// 捕获虚拟桌面（所有屏幕的并集）。bounds 返回物理像素坐标。
QImage captureVirtualDesktop(QRect *bounds, QString *error);
// 只捕获指定物理像素区域，用于滚动截图逐帧采样。
QImage captureRect(const QRect &physicalRect, QString *error);
// 让窗口不进入屏幕捕获（Windows 10 2004+），避免长截图的控制条被拍进去。
bool excludeFromCapture(quintptr window);
// 把窗口放到置顶层最上方，压过其它常驻置顶窗口。
void forceTopmost(quintptr window);

quintptr foregroundWindow();
bool isOwnProcess(quintptr window);
void activateWindow(quintptr window);

// 查找物理坐标下的顶层窗口，排除 TypeGo 自身窗口和桌面外壳。
bool windowAt(const QPoint &physicalPoint, QRect *rect, QString *title);

} // namespace ShotWin