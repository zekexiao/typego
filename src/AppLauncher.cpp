#include "AppLauncher.h"
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QDebug>
#include <QFileIconProvider>
#include <QPainter>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

AppLauncher::AppLauncher(QObject *parent)
    : QObject(parent)
{
    m_iconCacheDir = QDir::tempPath() + "/neoStart/icons";
    QDir().mkpath(m_iconCacheDir);

    addBuiltinApps();
    scanStartMenu();
}

void AppLauncher::addBuiltinApps()
{
    m_apps.append({ QString::fromUtf8(u8"记事本"), "notepad", QString::fromUtf8(u8"Windows 记事本"), "" });
    m_apps.append({ QString::fromUtf8(u8"计算器"), "calc", QString::fromUtf8(u8"Windows 计算器"), "" });
    m_apps.append({ QString::fromUtf8(u8"画图"), "mspaint", QString::fromUtf8(u8"Windows 画图"), "" });
    m_apps.append({ QString::fromUtf8(u8"命令提示符"), "cmd", QString::fromUtf8(u8"CMD 命令行"), "" });
    m_apps.append({ QString::fromUtf8(u8"资源管理器"), "explorer", QString::fromUtf8(u8"文件资源管理器"), "" });
    m_apps.append({ QString::fromUtf8(u8"任务管理器"), "taskmgr", QString::fromUtf8(u8"Windows 任务管理器"), "" });
    m_apps.append({ QString::fromUtf8(u8"设置"), "ms-settings:", QString::fromUtf8(u8"Windows 设置"), "" });
    m_apps.append({ QString::fromUtf8(u8"浏览器"), "https://www.bing.com", QString::fromUtf8(u8"打开必应搜索"), "" });
}

QString AppLauncher::extractIcon(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) return "";

    QString cacheName = QString::number(qHash(filePath), 16) + ".png";
    QString cachePath = m_iconCacheDir + "/" + cacheName;

    if (QFile::exists(cachePath)) {
        return cachePath;
    }

    QFileIconProvider provider;
    QIcon icon = provider.icon(fi);
    if (icon.isNull()) return "";

    QPixmap pixmap = icon.pixmap(QSize(32, 32));
    if (pixmap.isNull()) return "";

    // 确保是 32x32，如果系统返回的不是正方形，裁剪或缩放
    if (pixmap.width() != 32 || pixmap.height() != 32) {
        pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (pixmap.save(cachePath, "PNG")) {
        return cachePath;
    }
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

    QSet<QString> seen;
    for (const QString &dirPath : dirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;

        QFileInfoList files = dir.entryInfoList(QStringList() << "*.lnk", QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : files) {
            QString name = fi.completeBaseName();
            if (seen.contains(name)) continue;
            seen.insert(name);
            QString icon = extractIcon(fi.absoluteFilePath());
            m_apps.append({ name, fi.absoluteFilePath(), QString::fromUtf8(u8"开始菜单"), icon });
        }

        QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &sdi : subdirs) {
            QDir subDir(sdi.absoluteFilePath());
            QFileInfoList subFiles = subDir.entryInfoList(QStringList() << "*.lnk", QDir::Files | QDir::NoDotAndDotDot);
            for (const QFileInfo &fi : subFiles) {
                QString name = fi.completeBaseName();
                if (seen.contains(name)) continue;
                seen.insert(name);
                QString icon = extractIcon(fi.absoluteFilePath());
                m_apps.append({ name, fi.absoluteFilePath(), QString::fromUtf8(u8"开始菜单") + " / " + sdi.fileName(), icon });
            }
        }
    }
#endif
}

QVariantList AppLauncher::search(const QString &query)
{
    QVariantList results;
    QString q = query.trimmed().toLower();

    for (const auto &app : m_apps) {
        bool match = q.isEmpty();
        if (!match) {
            if (app.name.toLower().contains(q)) match = true;
            else if (app.desc.toLower().contains(q)) match = true;
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
