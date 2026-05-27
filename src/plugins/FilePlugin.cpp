#include "FilePlugin.h"
#include <QVariantMap>
#include <QDir>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include "Everything.h"
#endif

FilePlugin::FilePlugin(QObject *parent)
    : IPlugin(parent)
{
    m_iconCacheDir = QDir::tempPath() + "/neoStart/icons";
}

FilePlugin::~FilePlugin() = default;

QString FilePlugin::extractFileIcon(const QString &filePath)
{
    QString cacheName = QString::number(qHash(filePath), 16) + ".png";
    QString cachePath = m_iconCacheDir + "/" + cacheName;

    if (QFile::exists(cachePath)) {
        return cachePath;
    }

    QFileIconProvider provider;
    QIcon icon = provider.icon(QFileInfo(filePath));
    if (icon.isNull()) return "";

    QPixmap pixmap = icon.pixmap(QSize(32, 32));
    if (pixmap.isNull()) return "";

    if (pixmap.width() != 32 || pixmap.height() != 32) {
        pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QDir().mkpath(m_iconCacheDir);
    if (pixmap.save(cachePath, "PNG")) {
        return cachePath;
    }
    return "";
}

QVariantList FilePlugin::query(const QString &args)
{
    QVariantList results;

#ifdef Q_OS_WIN
    if (args.trimmed().isEmpty()) {
        QVariantMap item;
        item["title"] = QString::fromUtf8(u8"输入文件名进行搜索");
        item["subtitle"] = QString::fromUtf8(u8"由 Everything 提供极速搜索支持");
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QString::fromUtf8(u8"none");
        action["plugin"] = command();
        item["action"] = action;
        results.append(item);
        return results;
    }

    Everything_SetSearchW((LPCWSTR)args.utf16());
    Everything_SetMax(50);
    Everything_SetRequestFlags(EVERYTHING_REQUEST_FULL_PATH_AND_FILE_NAME);

    if (!Everything_QueryW(TRUE)) {
        DWORD err = Everything_GetLastError();
        QVariantMap item;
        if (err == EVERYTHING_ERROR_IPC) {
            item["title"] = QString::fromUtf8(u8"Everything 服务未运行");
            item["subtitle"] = QString::fromUtf8(u8"请先启动 Everything 再使用文件搜索");
        } else {
            item["title"] = QString::fromUtf8(u8"搜索失败");
            item["subtitle"] = QString::fromUtf8(u8"错误码: ") + QString::number(err);
        }
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QString::fromUtf8(u8"none");
        action["plugin"] = command();
        item["action"] = action;
        results.append(item);
        Everything_Reset();
        return results;
    }

    DWORD count = Everything_GetNumResults();
    if (count == 0) {
        QVariantMap item;
        item["title"] = QString::fromUtf8(u8"未找到文件");
        item["subtitle"] = args;
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QString::fromUtf8(u8"none");
        action["plugin"] = command();
        item["action"] = action;
        results.append(item);
        Everything_Reset();
        return results;
    }

    for (DWORD i = 0; i < count; ++i) {
        wchar_t wpath[1024];
        if (Everything_GetResultFullPathNameW(i, wpath, 1024) > 0) {
            QString fullPath = QString::fromWCharArray(wpath);
            QFileInfo fi(fullPath);

            QVariantMap item;
            item["title"] = fi.fileName();
            item["subtitle"] = fi.absolutePath();
            item["icon"] = extractFileIcon(fullPath);

            QVariantMap action;
            action["type"] = QString::fromUtf8(u8"openFile");
            action["data"] = fullPath;
            action["plugin"] = command();
            item["action"] = action;
            results.append(item);
        }
    }

    Everything_Reset();
#else
    Q_UNUSED(args)
    QVariantMap item;
    item["title"] = QString::fromUtf8(u8"文件搜索仅在 Windows 上可用");
    item["subtitle"] = QString::fromUtf8(u8"Everything SDK 仅支持 Windows 平台");
    item["icon"] = "";
    QVariantMap action;
    action["type"] = QString::fromUtf8(u8"none");
    action["plugin"] = command();
    item["action"] = action;
    results.append(item);
#endif

    return results;
}

bool FilePlugin::execute(const QVariantMap &action)
{
    QString type = action.value("type").toString();
    QString data = action.value("data").toString();
    if (data.isEmpty()) return false;

    if (type == "openFile") {
        return QDesktopServices::openUrl(QUrl::fromLocalFile(data));
    }
    return false;
}
