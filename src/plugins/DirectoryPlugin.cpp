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
    // 处理 ~ 展开
    if (path.startsWith("~")) {
        path = QDir::homePath() + path.mid(1);
    }
    QDir dir(path);
    if (dir.exists()) {
        return dir.absolutePath();
    }
    // 尝试相对于当前目录
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
        // 路径不存在，返回当前目录内容作为 fallback
        path = QDir::currentPath();
    }

    QDir dir(path);
    if (!dir.exists()) {
        QVariantMap item;
        item["title"] = tr("Directory does not exist");
        item["subtitle"] = args;
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QString::fromUtf8(u8"none");
        action["plugin"] = command();
        item["action"] = action;
        results.append(item);
        return results;
    }

    // 先添加 "进入此目录" 项
    QVariantMap enterItem;
    enterItem["title"] = tr("Open ") + dir.dirName();
    enterItem["subtitle"] = dir.absolutePath();
    enterItem["icon"] = "";
    QVariantMap enterAction;
    enterAction["type"] = QString::fromUtf8(u8"openFolder");
    enterAction["data"] = dir.absolutePath();
    enterAction["plugin"] = command();
    enterItem["action"] = enterAction;
    results.append(enterItem);

    // 列出子目录和文件
    QFileIconProvider provider;
    QString cacheDir = QDir::tempPath() + "/neoStart/icons";
    QDir().mkpath(cacheDir);
    
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                               QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &fi : entries) {
        QVariantMap item;
        item["title"] = fi.fileName();
        item["subtitle"] = fi.isDir() ? tr("Folder") : fi.suffix().toUpper();
        
        // 图标缓存：先检查是否已缓存，避免重复调用 provider.icon()
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
            action["type"] = QString::fromUtf8(u8"navigate");
            action["data"] = fi.absoluteFilePath();
        } else {
            action["type"] = QString::fromUtf8(u8"openFile");
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
        // navigate 在 query 阶段已经处理了，这里打开文件夹
#ifdef Q_OS_WIN
        HINSTANCE result = ShellExecuteW(nullptr, L"open", (LPCWSTR)data.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        return (INT_PTR)result > 32;
#else
        return QDesktopServices::openUrl(QUrl::fromLocalFile(data));
#endif
    }
    return false;
}
