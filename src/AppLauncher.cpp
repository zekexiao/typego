#include "AppLauncher.h"
#include "Settings.h"
#include "utils/PinyinConverter.h"
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QDebug>
#include <QFileIconProvider>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

AppLauncher::AppLauncher(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_iconCacheDir = QDir::tempPath() + "/TypeGo/icons";
    QDir().mkpath(m_iconCacheDir);

    rebuildApps();
    if (m_settings) {
        connect(m_settings, &Settings::hideUninstallEntriesChanged, this, &AppLauncher::rebuildApps);
        connect(m_settings, &Settings::customScanDirsChanged, this, &AppLauncher::rebuildApps);
        connect(m_settings, &Settings::customScanDirsRecursiveChanged, this, &AppLauncher::rebuildApps);
    }
    // 仅首次运行时创建默认固定项，已保存的空列表必须保持为空。
    if (!loadPins()) {
        pinApp(QStringLiteral("calc"), tr("Calculator"), QString());
        pinApp(QStringLiteral("taskmgr"), tr("Task Manager"), QString());
    }
}

static AppItem makeAppItem(const QString &name, const QString &path, const QString &desc, const QString &iconPath = "")
{
    AppItem item;
    item.name = name;
    item.path = path;
    item.desc = desc;
    item.lowerName = name.toLower();
    item.lowerDesc = desc.toLower();
    item.iconPath = iconPath;
    item.pinyin = PinyinConverter::instance().toPinyin(name);
    item.pinyinAcronym = PinyinConverter::instance().toAcronym(name);
    return item;
}

void AppLauncher::addBuiltinApps()
{
    QString winDir = qEnvironmentVariable("SystemRoot");
    m_apps.append(makeAppItem(tr("Notepad"), "notepad", tr("Windows Notepad"), extractIcon(winDir + "\\System32\\notepad.exe")));
    m_apps.append(makeAppItem(tr("Calculator"), "calc", tr("Windows Calculator"), extractIcon(winDir + "\\System32\\calc.exe")));
    m_apps.append(makeAppItem(tr("Paint"), "mspaint", tr("Windows Paint"), extractIcon(winDir + "\\System32\\mspaint.exe")));
    m_apps.append(makeAppItem(tr("Command Prompt"), "cmd", tr("CMD Command Line"), extractIcon(winDir + "\\System32\\cmd.exe")));
    m_apps.append(makeAppItem(tr("File Explorer"), "explorer", tr("File Explorer"), extractIcon(winDir + "\\explorer.exe")));
    m_apps.append(makeAppItem(tr("Task Manager"), "taskmgr", tr("Windows Task Manager"), extractIcon(winDir + "\\System32\\taskmgr.exe")));
    m_apps.append(makeAppItem(tr("Settings"), "ms-settings:", tr("Windows Settings"), extractIcon(QStringLiteral("ms-settings:"))));
    m_apps.append(makeAppItem(tr("Browser"), "https://www.bing.com", tr("Open Bing Search"), extractIcon(QStringLiteral("https://www.bing.com"))));
}

void AppLauncher::rebuildApps()
{
    m_apps.clear();
    m_appPaths.clear();
    addBuiltinApps();
    scanStartMenu();
    scanCustomDirs();
    emit appsChanged();
}

static QPixmap hIconToPixmap(HICON hIcon)
{
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return QPixmap();

    BITMAP bmp;
    if (!GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp)) {
        DeleteObject(iconInfo.hbmMask);
        DeleteObject(iconInfo.hbmColor);
        return QPixmap();
    }

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;

    HDC screenDC = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(screenDC);
    ReleaseDC(NULL, screenDC);

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
        return QPixmap();
    }

    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hDIB);
    DrawIconEx(memDC, 0, 0, hIcon, width, height, 0, NULL, DI_NORMAL);
    SelectObject(memDC, oldBmp);

    QImage image(width, height, QImage::Format_ARGB32);
    memcpy(image.bits(), bits, width * height * 4);

    DeleteObject(hDIB);
    DeleteDC(memDC);
    DeleteObject(iconInfo.hbmMask);
    DeleteObject(iconInfo.hbmColor);

    return QPixmap::fromImage(image);
}

QString AppLauncher::extractIcon(const QString &filePath)
{
    QString cacheName = QString::number(qHash(filePath), 16) + ".png";
    QString cachePath = m_iconCacheDir + "/" + cacheName;

    if (QFile::exists(cachePath)) {
        return cachePath;
    }

#ifdef Q_OS_WIN
    // 先尝试 QFileIconProvider（对已存在的文件路径效果好）
    QFileInfo fi(filePath);
    if (fi.exists()) {
        QFileIconProvider provider;
        QIcon icon = provider.icon(fi);
        if (!icon.isNull()) {
            QPixmap pixmap = icon.pixmap(QSize(32, 32));
            if (!pixmap.isNull()) {
                if (pixmap.width() != 32 || pixmap.height() != 32) {
                    pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                if (pixmap.save(cachePath, "PNG")) return cachePath;
            }
        }
    }

    // 使用 SHGetFileInfo 获取系统图标（支持 URL 协议和特殊路径）
    SHFILEINFOW sfi = {};
    DWORD flags = SHGFI_ICON | SHGFI_LARGEICON;
    if (SHGetFileInfoW((LPCWSTR)filePath.utf16(), 0, &sfi, sizeof(sfi), flags)) {
        HICON hIcon = sfi.hIcon;
        if (hIcon) {
            QPixmap pixmap = hIconToPixmap(hIcon);
            DestroyIcon(hIcon);
            if (!pixmap.isNull()) {
                if (pixmap.width() != 32 || pixmap.height() != 32) {
                    pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                if (pixmap.save(cachePath, "PNG")) return cachePath;
            }
        }
    }
#endif
    return "";
}

void AppLauncher::scanStartMenu()
{
#ifdef Q_OS_WIN
    QStringList dirs;

    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, path))) {
        dirs << QString::fromWCharArray(path);
    }
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, path))) {
        QString p = QString::fromWCharArray(path);
        if (!dirs.contains(p)) dirs << p;
    }

    for (const QString &dirPath : dirs) {
        scanDirectory(dirPath, tr("Start Menu"), true, 0, 8);
    }
#endif
}

void AppLauncher::scanCustomDirs()
{
    if (!m_settings) return;
    const bool recursive = m_settings->customScanDirsRecursive();
    for (const QString &dirPath : m_settings->customScanDirs()) {
        scanDirectory(dirPath, tr("Custom Directory"), recursive, 0, 3);
    }
}

void AppLauncher::scanDirectory(const QString &dirPath, const QString &desc, bool recursive, int depth, int maxDepth)
{
    QDir dir(dirPath);
    if (!dir.exists()) return;

    const QFileInfoList entries = dir.entryInfoList(QStringList() << "*.lnk" << "*.exe",
                                                    QDir::Files | QDir::NoDotAndDotDot,
                                                    QDir::Name);
    for (const QFileInfo &fi : entries) {
        const QString name = fi.completeBaseName();
        const QString filePath = fi.absoluteFilePath();
        if (shouldHideEntry(name, filePath)) continue;

        const QString pathKey = filePath.toLower();
        if (m_appPaths.contains(pathKey)) continue;
        m_appPaths.insert(pathKey);

        m_apps.append(makeAppItem(name, filePath, desc, extractIcon(filePath)));
    }

    if (!recursive || depth >= maxDepth) return;
    const QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &subdir : subdirs) {
        scanDirectory(subdir.absoluteFilePath(),
                      desc + QStringLiteral(" / ") + subdir.fileName(),
                      true,
                      depth + 1,
                      maxDepth);
    }
}

bool AppLauncher::shouldHideEntry(const QString &name, const QString &path) const
{
    if (!m_settings || !m_settings->hideUninstallEntries()) return false;
    const QString text = (name + QStringLiteral(" ") + path).toLower();
    return text.contains(QStringLiteral("uninstall"))
        || text.contains(QStringLiteral("unins"))
        || text.contains(QStringLiteral("uninstaller"))
        || text.contains(QStringLiteral("remove "))
        || text.contains(QStringLiteral("卸载"));
}

QVariantList AppLauncher::search(const QString &query)
{
    QVariantList results;
    results.reserve(m_apps.size());
    QString q = query.trimmed().toLower();

    for (const auto &app : m_apps) {
        bool match = q.isEmpty();
        if (!match) {
            if (app.lowerName.contains(q)) match = true;
            else if (app.lowerDesc.contains(q)) match = true;
            else if (app.pinyin.contains(q)) match = true;
            else if (app.pinyinAcronym.contains(q)) match = true;
        }
        if (match) {
            QVariantMap m;
            m["name"] = app.name;
            m["path"] = app.path;
            m["desc"] = app.desc;
            m["icon"] = app.iconPath;
            results.append(m);
        }
    }
    return results;
}

bool AppLauncher::launch(const QString &path)
{
    if (path.isEmpty()) return false;

    qDebug() << "Launching:" << path;

    if (path.startsWith("http://") || path.startsWith("https://")) {
        return QDesktopServices::openUrl(QUrl(path));
    }

    if (path.startsWith("ms-settings:")) {
        return QDesktopServices::openUrl(QUrl(path));
    }

    QFileInfo fi(path);
    if (fi.isDir()) {
        return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }

    if (fi.suffix().toLower() == "lnk") {
#ifdef Q_OS_WIN
        HINSTANCE result = ShellExecuteW(nullptr, L"open", (LPCWSTR)path.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        return (INT_PTR)result > 32;
#else
        return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
    }

    return QProcess::startDetached(path, QStringList());
}

QString AppLauncher::pinFilePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/pins.json";
}

bool AppLauncher::loadPins()
{
    QString path = pinFilePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return false;
    QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        PinItem p;
        p.name = o.value("name").toString();
        p.path = o.value("path").toString();
        p.iconPath = o.value("iconPath").toString();
        m_pins.append(p);
    }
    return true;
}

void AppLauncher::savePins() const
{
    QJsonArray arr;
    for (const auto &p : m_pins) {
        QJsonObject o;
        o.insert("name", p.name);
        o.insert("path", p.path);
        o.insert("iconPath", p.iconPath);
        arr.append(o);
    }
    QJsonDocument doc(arr);
    QSaveFile file(pinFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        if (file.write(doc.toJson(QJsonDocument::Compact)) >= 0) {
            file.commit();
        }
    }
}

QVariantList AppLauncher::pinnedApps() const
{
    QVariantList results;
    for (const auto &p : m_pins) {
        QVariantMap m;
        m["name"] = p.name;
        m["path"] = p.path;
        m["icon"] = p.iconPath;
        results.append(m);
    }
    return results;
}

bool AppLauncher::pinApp(const QString &path, const QString &name, const QString &iconPath)
{
    if (path.isEmpty()) return false;
    for (const auto &p : m_pins) {
        if (p.path == path) return false;
    }
    PinItem p;
    p.name = name;
    p.path = path;
    p.iconPath = iconPath.isEmpty() ? extractIcon(path) : iconPath;
    m_pins.append(p);
    savePins();
    emit pinsChanged();
    return true;
}

bool AppLauncher::unpinApp(const QString &path)
{
    for (int i = 0; i < m_pins.size(); ++i) {
        if (m_pins[i].path == path) {
            m_pins.removeAt(i);
            savePins();
            emit pinsChanged();
            return true;
        }
    }
    return false;
}

bool AppLauncher::isPinned(const QString &path) const
{
    for (const auto &p : m_pins) {
        if (p.path == path) return true;
    }
    return false;
}
