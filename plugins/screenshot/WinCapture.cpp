#include "WinCapture.h"

#include <QGuiApplication>
#include <QProcess>
#include <QStringList>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#endif

namespace ShotWin {

#ifdef Q_OS_WIN

namespace {

QString windowTitleOf(HWND window)
{
    const int length = GetWindowTextLengthW(window);
    if (length <= 0) return {};
    QString title(length, Qt::Uninitialized);
    const int copied = GetWindowTextW(window, reinterpret_cast<wchar_t *>(title.data()), length + 1);
    if (copied <= 0) return {};
    title.resize(copied);
    return title;
}

bool isShellWindow(HWND window)
{
    wchar_t className[128] = {};
    if (GetClassNameW(window, className, 128) <= 0) return false;
    const QString name = QString::fromWCharArray(className);
    return name == QLatin1String("Progman") || name == QLatin1String("WorkerW")
        || name == QLatin1String("Shell_TrayWnd") || name == QLatin1String("Shell_SecondaryTrayWnd");
}

bool isCloaked(HWND window)
{
    DWORD cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0;
}

} // namespace

QList<MonitorInfo> monitors()
{
    QList<MonitorInfo> result;
    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
            MONITORINFOEXW info = {};
            info.cbSize = sizeof(info);
            if (!GetMonitorInfoW(monitor, &info)) return TRUE;
            auto *list = reinterpret_cast<QList<MonitorInfo> *>(data);
            list->append({QString::fromWCharArray(info.szDevice),
                          QRect(info.rcMonitor.left, info.rcMonitor.top,
                                info.rcMonitor.right - info.rcMonitor.left,
                                info.rcMonitor.bottom - info.rcMonitor.top)});
            return TRUE;
        }, reinterpret_cast<LPARAM>(&result));
    return result;
}

QImage captureVirtualDesktop(QRect *bounds, QString *error)
{
    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width <= 0 || height <= 0) {
        if (error) *error = QStringLiteral("Cannot read the virtual screen bounds");
        return {};
    }

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        if (error) *error = QStringLiteral("Cannot access the screen device context");
        return {};
    }
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = memoryDc ? CreateCompatibleBitmap(screenDc, width, height) : nullptr;
    if (!memoryDc || !bitmap) {
        if (memoryDc) DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        if (error) *error = QStringLiteral("Cannot allocate the screen bitmap");
        return {};
    }

    const HGDIOBJ previous = SelectObject(memoryDc, bitmap);
    const BOOL copied = BitBlt(memoryDc, 0, 0, width, height, screenDc, x, y, SRCCOPY | CAPTUREBLT);
    SelectObject(memoryDc, previous);

    QImage image;
    if (copied) {
        image = QImage(width, height, QImage::Format_RGB32);
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        // 负高度表示自上而下，和 QImage 扫描线顺序一致。
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        const int lines = GetDIBits(memoryDc, bitmap, 0, height, image.bits(), &info, DIB_RGB_COLORS);
        if (lines != height) image = QImage();
    }

    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    if (image.isNull()) {
        if (error) *error = QStringLiteral("Screen capture failed");
        return {};
    }
    if (bounds) *bounds = QRect(x, y, width, height);
    return image;
}

quintptr foregroundWindow()
{
    return reinterpret_cast<quintptr>(GetForegroundWindow());
}

bool isOwnProcess(quintptr window)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(reinterpret_cast<HWND>(window), &processId);
    return processId == GetCurrentProcessId();
}

void activateWindow(quintptr window)
{
    HWND handle = reinterpret_cast<HWND>(window);
    if (!handle || !IsWindow(handle)) return;
    if (IsIconic(handle)) ShowWindow(handle, SW_RESTORE);
    SetForegroundWindow(handle);
}

bool windowAt(const QPoint &physicalPoint, QRect *rect, QString *title)
{
    // 覆盖层自己是鼠标下的顶层窗口，因此不能只看 WindowFromPoint；
    // 按 z 序枚举顶层窗口，取第一个不属于本进程、且包含光标的窗口。
    struct Search {
        POINT point;
        HWND found = nullptr;
    } search{{physicalPoint.x(), physicalPoint.y()}, nullptr};

    EnumWindows([](HWND window, LPARAM data) -> BOOL {
        auto *search = reinterpret_cast<Search *>(data);
        if (!window || !IsWindowVisible(window) || IsIconic(window) || isCloaked(window)
            || isOwnProcess(reinterpret_cast<quintptr>(window)) || isShellWindow(window))
            return TRUE;
        RECT bounds = {};
        if (!GetWindowRect(window, &bounds)) return TRUE;
        if (search->point.x < bounds.left || search->point.x >= bounds.right
            || search->point.y < bounds.top || search->point.y >= bounds.bottom)
            return TRUE;
        if (bounds.right - bounds.left < 32 || bounds.bottom - bounds.top < 32) return TRUE;
        search->found = window;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&search));

    if (!search.found) return false;
    RECT bounds = {};
    if (!GetWindowRect(search.found, &bounds)) return false;
    const QRect area(bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top);
    if (rect) *rect = area;
    if (title) *title = windowTitleOf(search.found);
    return true;
}

#else

QList<MonitorInfo> monitors() { return {}; }
QImage captureVirtualDesktop(QRect *, QString *error)
{
    if (error) *error = QStringLiteral("Screen capture is only supported on Windows");
    return {};
}
quintptr foregroundWindow() { return 0; }
bool isOwnProcess(quintptr) { return false; }
void activateWindow(quintptr) {}
bool windowAt(const QPoint &, QRect *, QString *) { return false; }

#endif

} // namespace ShotWin