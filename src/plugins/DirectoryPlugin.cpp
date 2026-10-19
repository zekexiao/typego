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
        return QDir::homePath();
    }
    QString path = args.trimmed();
    // 展开用户主目录。
    if (path.startsWith("~")) {
        path = QDir::homePath() + path.mid(1);
    }
    QDir dir(path);
    if (dir.exists()) {
        return dir.absolutePath();
    }
    // 相对路径以用户主目录为基准。
    QDir homeDir(QDir::homePath());
    if (homeDir.exists(path)) {
        return homeDir.absoluteFilePath(path);
    }
    return QString();
}

QVariantList DirectoryPlugin::query(const QString &args)
{
    QVariantList results;
    QString path = resolvePath(args);

    if (path.isEmpty()) {
        // 路径不存在时回退到用户主目录。
        path = QDir::homePath();
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

    // 添加打开当前目录的操作。
    QVariantMap enterItem;
    enterItem["title"] = tr("Open %1").arg(dir.dirName());
    enterItem["subtitle"] = dir.absolutePath();
    enterItem["icon"] = "";
    QVariantMap enterAction;
    enterAction["type"] = QStringLiteral("openFolder");
    enterAction["data"] = dir.absolutePath();
    enterAction["plugin"] = command();
    enterItem["action"] = enterAction;
    results.append(enterItem);

    // 列出子目录和文件。图标由结果列表异步加载，避免阻塞输入。
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                               QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &fi : entries) {
        QVariantMap item;
        item["title"] = fi.fileName();
        item["subtitle"] = fi.isDir() ? tr("Folder") : fi.suffix().toUpper();
        item["path"] = fi.absoluteFilePath();
        item["icon"] = "";

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
        // 导航操作直接打开选中的目录。
#ifdef Q_OS_WIN
        HINSTANCE result = ShellExecuteW(nullptr, L"open", (LPCWSTR)data.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        return (INT_PTR)result > 32;
#else
        return QDesktopServices::openUrl(QUrl::fromLocalFile(data));
#endif
    }
    return false;
}

