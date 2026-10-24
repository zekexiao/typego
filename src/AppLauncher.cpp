#include "AppLauncher.h"
#include "Settings.h"
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QDebug>
#include <QFileIconProvider>
#include <QPainter>
#include <QStandardPaths>
#include <utility>
#include <QTimer>
#include <QVector>
#include <QtConcurrent>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

AppLauncher::AppLauncher(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_pins(pinFilePath())
    , m_settings(settings)
    , m_usage(usageFilePath())
{
    m_iconCacheDir = QDir::tempPath() + "/TypeGo/icons";
    QDir().mkpath(m_iconCacheDir);

    addBuiltinApps();
    connect(&m_scanWatcher, &QFutureWatcher<QList<AppItem>>::finished, this, [this]() {
        m_scanning = false;
        if (m_scanPending) {
            m_scanPending = false;
            rebuildApps();
            return;
        }
        m_apps.clear();
        addBuiltinApps();
        m_apps.append(m_scanWatcher.future().takeResult());
        m_scanReady = true;
        emit appsChanged();
    });
    QTimer::singleShot(0, this, &AppLauncher::rebuildApps);
    if (m_settings) {
        connect(m_settings, &Settings::hideUninstallEntriesChanged, this, &AppLauncher::rebuildApps);
        connect(m_settings, &Settings::customScanDirsChanged, this, &AppLauncher::rebuildApps);
        connect(m_settings, &Settings::customScanDirsRecursiveChanged, this, &AppLauncher::rebuildApps);
    }
    // 仅首次运行时创建默认固定项，已保存的空列表必须保持为空。
    if (m_pins.load() && m_pins.missing()) {
        pinApp(QStringLiteral("calc"), tr("Calculator"), QString());
        pinApp(QStringLiteral("taskmgr"), tr("Task Manager"), QString());
    }
    if (!m_pins.error().isEmpty()) {
        // 窗口在启动器之后创建，延后提示才能接上界面的错误反馈。
        QTimer::singleShot(0, this, [this]() {
            emit pinStorageError(tr("Pinned apps could not be loaded or saved. The original file has been kept. Check pins.json and folder permissions; restart TypeGo after repairing the file.\n%1").arg(m_pins.error()));
        });
    }
}

static AppItem makeAppItem(const QString &name, const QString &path, const QString &desc, const QString &iconPath = "")
{
    AppItem item;
    item.name = name;
    item.path = path;
    item.desc = desc;
    item.iconPath = iconPath;
    item.match = AppMatcher::buildEntry(name, path, desc);
    return item;
}

void AppLauncher::addBuiltinApps()
{
    m_apps.append(makeAppItem(tr("Notepad"), "notepad", tr("Windows Notepad")));
    m_apps.append(makeAppItem(tr("Calculator"), "calc", tr("Windows Calculator")));
    m_apps.append(makeAppItem(tr("Paint"), "mspaint", tr("Windows Paint")));
    m_apps.append(makeAppItem(tr("Command Prompt"), "cmd", tr("CMD Command Line")));
    m_apps.append(makeAppItem(tr("File Explorer"), "explorer", tr("File Explorer")));
    m_apps.append(makeAppItem(tr("Task Manager"), "taskmgr", tr("Windows Task Manager")));
    m_apps.append(makeAppItem(tr("Settings"), "ms-settings:", tr("Windows Settings")));
    m_apps.append(makeAppItem(tr("Browser"), "https://www.bing.com", tr("Open Bing Search")));
}

void AppLauncher::rebuildApps()
{
    if (m_scanning) {
        m_scanPending = true;
        return;
    }
    m_scanning = true;
    // 在线程启动前读取设置，后台仅操作独立数据，不提取图标或访问界面对象。
    m_scanWatcher.setFuture(QtConcurrent::run(&AppLauncher::scanApps,
        m_settings ? m_settings->customScanDirs() : QStringList(),
        m_settings && m_settings->customScanDirsRecursive(),
        m_settings && m_settings->hideUninstallEntries()));
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

QList<AppItem> AppLauncher::scanApps(const QStringList &customDirs, bool recursive, bool hideUninstall)
{
    QList<AppItem> apps;
    QSet<QString> paths;
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
        scanDirectory(dirPath, tr("Start Menu"), true, 0, 8, hideUninstall, apps, paths);
    }
#endif
    for (const QString &dirPath : customDirs) {
        scanDirectory(dirPath, tr("Custom Directory"), recursive, 0, 3, hideUninstall, apps, paths);
    }
    return apps;
}

void AppLauncher::scanDirectory(const QString &dirPath, const QString &desc, bool recursive, int depth, int maxDepth,
                                bool hideUninstall, QList<AppItem> &apps, QSet<QString> &paths)
{
    QDir dir(dirPath);
    if (!dir.exists()) return;

    const QFileInfoList entries = dir.entryInfoList(QStringList() << "*.lnk" << "*.exe",
                                                    QDir::Files | QDir::NoDotAndDotDot,
                                                    QDir::Name);
    for (const QFileInfo &fi : entries) {
        const QString name = fi.completeBaseName();
        const QString filePath = fi.absoluteFilePath();
        if (hideUninstall && shouldHideEntry(name, filePath)) continue;

        const QString pathKey = filePath.toLower();
        if (paths.contains(pathKey)) continue;
        paths.insert(pathKey);

        apps.append(makeAppItem(name, filePath, desc));
    }

    if (!recursive || depth >= maxDepth) return;
    const QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &subdir : subdirs) {
        scanDirectory(subdir.absoluteFilePath(),
                      desc + QStringLiteral(" / ") + subdir.fileName(),
                      true,
                      depth + 1,
                      maxDepth, hideUninstall, apps, paths);
    }
}

bool AppLauncher::shouldHideEntry(const QString &name, const QString &path)
{
    const QString text = (name + QStringLiteral(" ") + path).toLower();
    return text.contains(QStringLiteral("uninstall"))
        || text.contains(QStringLiteral("unins"))
        || text.contains(QStringLiteral("uninstaller"))
        || text.contains(QStringLiteral("remove "))
        || text.contains(QStringLiteral("卸载"));
}

QVariantList AppLauncher::search(const QString &query)
{
    const QString q = query.trimmed().toLower();

    // 命中集合带着排序键一起排，键与条目必须同生命周期，不能拆成两个并行数组。
    // 比较器会被调用 O(n log n) 次，频次在填充时一次查好，不在比较器里查哈希表。
    struct Hit {
        const AppItem *item;
        AppMatcher::RankKey key;
    };
    QVector<Hit> hits;
    hits.reserve(m_apps.size());

    if (q.isEmpty()) {
        // 空输入按使用频次稳定排序：常用在前，未使用过的条目保持索引原序
        //（内置项在前，其后按扫描目录顺序），不引入名称字典序。
        for (const AppItem &app : m_apps) {
            const AppUsageStore::Usage usage = m_usage.find(app.match.lowerPath);
            hits.append(Hit{&app, {0, usage.count, usage.lastUsed}});
        }
        std::stable_sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b) {
            return AppMatcher::usageRanksBefore(a.key, b.key);
        });
    } else {
        for (const AppItem &app : m_apps) {
            const AppMatcher::Match match = AppMatcher::evaluate(app.match, q);
            if (!match.matched) continue;
            const AppUsageStore::Usage usage = m_usage.find(app.match.lowerPath);
            hits.append(Hit{&app, {match.score, usage.count, usage.lastUsed}});
        }
        std::sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b) {
            return AppMatcher::ranksBefore(a.item->match, a.key, b.item->match, b.key);
        });
    }

    QVariantList results;
    results.reserve(hits.size());
    for (const Hit &hit : hits) {
        QVariantMap m;
        m[QStringLiteral("name")] = hit.item->name;
        m[QStringLiteral("path")] = hit.item->path;
        m[QStringLiteral("desc")] = hit.item->desc;
        m[QStringLiteral("icon")] = hit.item->iconPath;
        results.append(m);
    }
    return results;
}

bool AppLauncher::launch(const QString &path)
{
    if (path.isEmpty()) return false;

    qDebug() << "Launching:" << path;

    // 各启动分支收敛成一个 ok，函数末尾统一记频次，避免每个分支重复调用。
    bool ok = false;

    if (path.startsWith("http://") || path.startsWith("https://")
        || path.startsWith("ms-settings:")) {
        ok = QDesktopServices::openUrl(QUrl(path));
    } else {
        const QFileInfo fi(path);
        if (fi.isDir()) {
            ok = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        } else if (fi.suffix().toLower() == "lnk") {
#ifdef Q_OS_WIN
            HINSTANCE result = ShellExecuteW(nullptr, L"open", (LPCWSTR)path.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
            ok = (INT_PTR)result > 32;
#else
            ok = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
        } else if (fi.exists() && fi.suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) != 0) {
            ok = QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
        } else {
            // startDetached 只保证进程已排队，不保证应用最终启动成功；沿用现有语义，不额外探测。
            ok = QProcess::startDetached(path, QStringList());
        }
    }

    // 只有系统接受了启动请求才计数；记录只改内存，2 秒防抖落盘，不阻塞启动。
    if (ok) m_usage.record(path);
    return ok;
}

void AppLauncher::clearUsageHistory()
{
    m_usage.clear();
    // 复用 appsChanged 通知界面重查，当前结果列表立即失去频次加成。
    emit appsChanged();
}

QString AppLauncher::usageFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/app_usage.json";
}

QString AppLauncher::pinFilePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/pins.json";
}

QVariantList AppLauncher::pinnedApps() const
{
    QVariantList results;
    for (const auto &p : m_pins.items()) {
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
    for (const auto &p : m_pins.items()) {
        if (p.path == path) return false;
    }
    PinItem p;
    p.name = name;
    p.path = path;
    p.iconPath = iconPath.isEmpty() ? extractIcon(path) : iconPath;
    QList<PinItem> items = m_pins.items();
    items.append(p);
    if (!m_pins.save(items)) {
        emit pinStorageError(tr("Cannot save pinned apps. Your previous pins have been kept. Check the data file and folder permissions, then try again.\n%1").arg(m_pins.error()));
        return false;
    }
    emit pinsChanged();
    return true;
}

bool AppLauncher::unpinApp(const QString &path)
{
    for (int i = 0; i < m_pins.items().size(); ++i) {
        if (m_pins.items().at(i).path == path) {
            QList<PinItem> items = m_pins.items();
            items.removeAt(i);
            if (!m_pins.save(items)) {
                emit pinStorageError(tr("Cannot save pinned apps. Your previous pins have been kept. Check the data file and folder permissions, then try again.\n%1").arg(m_pins.error()));
                return false;
            }
            emit pinsChanged();
            return true;
        }
    }
    return false;
}

bool AppLauncher::isPinned(const QString &path) const
{
    for (const auto &p : m_pins.items()) {
        if (p.path == path) return true;
    }
    return false;
}
