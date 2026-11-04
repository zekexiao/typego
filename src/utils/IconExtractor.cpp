#include "IconExtractor.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>

#include <qt_windows.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <cstring>

#ifndef IO_REPARSE_TAG_APPEXECLINK
#define IO_REPARSE_TAG_APPEXECLINK 0x8000001B
#endif

namespace {

constexpr int kIconSize = 32;
// 缓存名版本号：提取算法更新后旧缓存自动失效
constexpr int kCacheVersion = 3;

QImage hBitmapToImage(HBITMAP hBmp)
{
    BITMAP bmp;
    if (!GetObject(hBmp, sizeof(BITMAP), &bmp) || bmp.bmWidth <= 0 || bmp.bmHeight <= 0)
        return QImage();

    QImage image(bmp.bmWidth, bmp.bmHeight, QImage::Format_ARGB32_Premultiplied);
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.bmWidth;
    bmi.bmiHeader.biHeight = -bmp.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(nullptr);
    const int lines = GetDIBits(screenDC, hBmp, 0, bmp.bmHeight, image.bits(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, screenDC);
    return lines == bmp.bmHeight ? image : QImage();
}

QImage hIconToImage(HICON hIcon)
{
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo))
        return QImage();

    BITMAP bmp;
    if (!GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp)) {
        DeleteObject(iconInfo.hbmMask);
        DeleteObject(iconInfo.hbmColor);
        return QImage();
    }

    const int width = bmp.bmWidth;
    const int height = bmp.bmHeight;

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HBITMAP hDIB = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hDIB) {
        DeleteDC(memDC);
        DeleteObject(iconInfo.hbmMask);
        DeleteObject(iconInfo.hbmColor);
        return QImage();
    }

    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, hDIB));
    DrawIconEx(memDC, 0, 0, hIcon, width, height, 0, nullptr, DI_NORMAL);
    SelectObject(memDC, oldBmp);

    QImage image(width, height, QImage::Format_ARGB32);
    memcpy(image.bits(), bits, width * height * 4);

    DeleteObject(hDIB);
    DeleteDC(memDC);
    DeleteObject(iconInfo.hbmMask);
    DeleteObject(iconInfo.hbmColor);
    return image;
}

bool isReparsePoint(const QString &path)
{
    const DWORD attrs = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()));
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT);
}

// 从应用执行别名的 reparse point 解析 AUMID。
// 数据布局：ReparseTag(4) + ReparseDataLength(2) + Reserved(2) +
// StringCount(4) + 以 NUL 分隔的字符串（包族名、AUMID、目标 exe、...）。
QString appExecLinkAumid(const QString &path)
{
    HANDLE h = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return QString();

    QByteArray buf(16 * 1024, 0);
    DWORD ret = 0;
    QString aumid;
    if (DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, nullptr, 0, buf.data(), (DWORD)buf.size(), &ret, nullptr)) {
        const unsigned tag = *reinterpret_cast<const unsigned *>(buf.constData());
        if (tag == IO_REPARSE_TAG_APPEXECLINK) {
            // 第一个字符串是包族名，第二个是 AUMID
            const wchar_t *p = reinterpret_cast<const wchar_t *>(buf.constData() + 12);
            p += wcslen(p) + 1;
            aumid = QString::fromWCharArray(p);
        }
    }
    CloseHandle(h);
    return aumid;
}

// 经 shell:AppsFolder\<AUMID> 项目提取 Store 应用图标。
// 开始菜单“所有应用”列表用的是同一来源；对别名路径直接提取只会得到通用图标。
QImage appsFolderIconByAumid(const QString &aumid)
{
    if (aumid.isEmpty())
        return QImage();

    // 注意：AppsFolder 项目路径必须用反斜杠，正斜杠会让 Shell 解析失败
    const QString itemPath = QStringLiteral("shell:AppsFolder\\") + aumid;
    IShellItemImageFactory *factory = nullptr;
    if (FAILED(SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(itemPath.utf16()), nullptr, IID_PPV_ARGS(&factory))))
        return QImage();

    HBITMAP hBmp = nullptr;
    const HRESULT hr = factory->GetImage(SIZE{kIconSize, kIconSize}, SIIGBF_ICONONLY, &hBmp);
    factory->Release();
    if (FAILED(hr) || !hBmp)
        return QImage();

    QImage image = hBitmapToImage(hBmp);
    DeleteObject(hBmp);
    return image;
}

QImage appsFolderIcon(const QString &aliasPath)
{
    return appsFolderIconByAumid(appExecLinkAumid(aliasPath));
}

// URL/协议路径（内置“设置”与“浏览器”项）没有本地文件可提取，
// 用各自在 Shell 里的真实来源取图标。实现放在 fileIcon/shellItemIcon 之后。
QImage protocolIcon(const QString &path);

// Shell 项目工厂兜底：处理虚拟路径和其余 Shell 能识别的对象
QImage shellItemIcon(const QString &path)
{
    IShellItemImageFactory *factory = nullptr;
    if (FAILED(SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr, IID_PPV_ARGS(&factory))))
        return QImage();

    HBITMAP hBmp = nullptr;
    const HRESULT hr = factory->GetImage(SIZE{kIconSize, kIconSize}, SIIGBF_ICONONLY, &hBmp);
    factory->Release();
    if (FAILED(hr) || !hBmp)
        return QImage();

    QImage image = hBitmapToImage(hBmp);
    DeleteObject(hBmp);
    return image;
}

// SHGetFileInfo：普通 exe/lnk 及已存在文件的主要来源
QImage fileIcon(const QString &path)
{
    SHFILEINFOW sfi = {};
    if (!SHGetFileInfoW(reinterpret_cast<LPCWSTR>(path.utf16()), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON))
        return QImage();

    QImage image = hIconToImage(sfi.hIcon);
    DestroyIcon(sfi.hIcon);
    return image;
}

// URL/协议路径（内置“设置”与“浏览器”项）没有本地文件可提取，
// 用各自在 Shell 里的真实来源取图标。
QImage protocolIcon(const QString &path)
{
    // 设置：固定的系统应用 AUMID
    if (path.startsWith(QStringLiteral("ms-settings:"))) {
        return appsFolderIconByAumid(
            QStringLiteral("windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel"));
    }

    // 浏览器：取默认浏览器的可执行文件再提取其图标
    if (path.startsWith(QStringLiteral("http://")) || path.startsWith(QStringLiteral("https://"))) {
        wchar_t exe[MAX_PATH];
        DWORD len = MAX_PATH;
        // ASSOCSTR_EXECUTABLE：直接返回命令行里的可执行文件路径
        if (AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, L"http", L"open", exe, &len) == S_OK) {
            const QString browser = QDir::toNativeSeparators(QString::fromWCharArray(exe));
            if (QFileInfo(browser).isAbsolute()) {
                QImage image = fileIcon(browser);
                if (!image.isNull())
                    return image;
                return shellItemIcon(browser);
            }
        }
    }
    return QImage();
}

QImage extractImage(const QString &path)
{
    // 应用执行别名（reparse point）：传统 API 只会“成功”返回通用图标，
    // 必须直接走 AppsFolder；Shell 工厂作后备。
    if (isReparsePoint(path)) {
        QImage image = appsFolderIcon(path);
        if (image.isNull())
            image = shellItemIcon(path);
        return image;
    }

    QImage image = fileIcon(path);
    if (!image.isNull())
        return image;

    return shellItemIcon(path);
}

QString cachePathFor(const QString &path, const QString &cacheDir)
{
    QDir(cacheDir).mkpath(QStringLiteral("."));
    return QDir(cacheDir).filePath(QStringLiteral("%1-%2.png").arg(QString::number(qHash(path), 16)).arg(kCacheVersion));
}

bool saveNormalized(const QImage &source, const QString &cachePath)
{
    if (source.isNull())
        return false;
    QImage image = source;
    if (image.width() != kIconSize || image.height() != kIconSize)
        image = image.scaled(kIconSize, kIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image.save(cachePath, "PNG");
}

} // namespace

namespace IconExtractor {

QString extract(const QString &path, const QString &cacheDir)
{
    if (path.isEmpty())
        return QString();

    const QString cachePath = cachePathFor(path, cacheDir);
    if (QFileInfo::exists(cachePath))
        return cachePath;

#ifdef Q_OS_WIN
    // 线程池线程需要自行初始化 COM（SHCreateItemFromParsingName 依赖）
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const auto uninitialize = qScopeGuard([initialized]() {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });

    // 裸命令名（如 calc）解析成完整路径；URL/协议路径没有本地文件可提取
    QString resolved = path;
    if (!QFileInfo(resolved).isAbsolute() && !resolved.contains(QLatin1Char(':'))) {
        const QString executable = QStandardPaths::findExecutable(resolved);
        if (!executable.isEmpty())
            resolved = executable;
    }
    if (!QFileInfo(resolved).isAbsolute()) {
        // URL/协议路径（ms-settings:、http/https）走协议专属来源
        if (saveNormalized(protocolIcon(path), cachePath))
            return cachePath;
        return QString();
    }

    // findExecutable 等来源可能返回正斜杠路径，而 SHGetFileInfo /
    // SHCreateItemFromParsingName 只接受反斜杠，必须先转成原生格式。
    resolved = QDir::toNativeSeparators(resolved);

    const QImage image = extractImage(resolved);
    if (saveNormalized(image, cachePath))
        return cachePath;
#else
    Q_UNUSED(cacheDir)
#endif
    return QString();
}

} // namespace IconExtractor
