#include "DirectoryPlugin.h"
#include <QVariantMap>
#include <QDir>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

DirectoryPlugin::DirectoryPlugin(QObject *parent)
    : IPlugin(parent)
{
}

QString DirectoryPlugin::resolvePath(const QString &args) const
{
    if (args.trimmed().isEmpty()) {
        return QDir::currentPath();
    }
    QString path = args.trimmed();
    // 澶勭悊 ~ 灞曞紑
    if (path.startsWith("~")) {
        path = QDir::homePath() + path.mid(1);
    }
    QDir dir(path);
    if (dir.exists()) {
        return dir.absolutePath();
    }
    // Try resolving the path relative to the current directory.
    QDir currentDir(QDir::currentPath());
    if (currentDir.exists(path)) {
        return currentDir.absoluteFilePath(path);
    }
    return QString();
}

QVariantList DirectoryPlugin::query(const QString &args)
{
    QVariantList results;
    QString path = resolvePath(args);

    if (path.isEmpty()) {
        // 璺緞涓嶅瓨鍦紝杩斿洖褰撳墠鐩綍鍐呭浣滀负 fallback
        path = QDir::currentPath();
    }

    QDir dir(path);
    if (!dir.exists()) {
        QVariantMap item;
        item["title"] = tr("Directory does not exist");
        item["subtitle"] = args;
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QStringLiteral("none");
        action["plugin"] = command();
        item["action"] = action;
        results.append(item);
        return results;
    }

    // Add an action for opening the current directory.
    QVariantMap enterItem;
    enterItem["title"] = tr("Open ") + dir.dirName();
    enterItem["subtitle"] = dir.absolutePath();
    enterItem["icon"] = "";
    QVariantMap enterAction;
    enterAction["type"] = QStringLiteral("openFolder");
    enterAction["data"] = dir.absolutePath();
    enterAction["plugin"] = command();
    enterItem["action"] = enterAction;
    results.append(enterItem);

    // 鍒楀嚭瀛愮洰褰曞拰鏂囦欢
    QFileIconProvider provider;
    QString cacheDir = QDir::tempPath() + "/qingzhou/icons";
    QDir().mkpath(cacheDir);
    
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                               QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &fi : entries) {
        QVariantMap item;
        item["title"] = fi.fileName();
        item["subtitle"] = fi.isDir() ? tr("Folder") : fi.suffix().toUpper();
        
        // 鍥炬爣缂撳瓨锛氬厛妫€鏌ユ槸鍚﹀凡缂撳瓨锛岄伩鍏嶉噸澶嶈皟鐢?provider.icon()
        QString cachePath = cacheDir + "/dir_" + QString::number(qHash(fi.absoluteFilePath()), 16) + ".png";
        if (QFile::exists(cachePath)) {
            item["icon"] = cachePath;
        } else {
            QIcon icon = provider.icon(fi);
            QPixmap pixmap = icon.pixmap(QSize(32, 32));
            if (!pixmap.isNull() && pixmap.save(cachePath, "PNG")) {
                item["icon"] = cachePath;
            } else {
                item["icon"] = "";
            }
        }

        QVariantMap action;
        if (fi.isDir()) {
            action["type"] = QStringLiteral("navigate");
            action["data"] = fi.absoluteFilePath();
        } else {
            action["type"] = QStringLiteral("openFile");
            action["data"] = fi.absoluteFilePath();
        }
        action["plugin"] = command();
        item["action"] = action;
        results.append(item);
    }

    return results;
}

bool DirectoryPlugin::execute(const QVariantMap &action)
{
    QString type = action.value("type").toString();
    QString data = action.value("data").toString();
    if (data.isEmpty()) return false;

    if (type == "openFolder") {
#ifdef Q_OS_WIN
        HINSTANCE result = ShellExecuteW(nullptr, L"open", (LPCWSTR)data.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        return (INT_PTR)result > 32;
#else
        return QDesktopServices::openUrl(QUrl::fromLocalFile(data));
#endif
    } else if (type == "openFile") {
        return QDesktopServices::openUrl(QUrl::fromLocalFile(data));
    } else if (type == "navigate") {
        // Navigation actions open the selected directory.
#ifdef Q_OS_WIN
        HINSTANCE result = ShellExecuteW(nullptr, L"open", (LPCWSTR)data.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        return (INT_PTR)result > 32;
#else
        return QDesktopServices::openUrl(QUrl::fromLocalFile(data));
#endif
    }
    return false;
}

