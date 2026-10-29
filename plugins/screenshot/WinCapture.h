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

quintptr foregroundWindow();
bool isOwnProcess(quintptr window);
void activateWindow(quintptr window);

// 查找物理坐标下的顶层窗口，排除 TypeGo 自身窗口和桌面外壳。
bool windowAt(const QPoint &physicalPoint, QRect *rect, QString *title);

} // namespace ShotWin