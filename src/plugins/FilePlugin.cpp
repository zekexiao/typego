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
    m_iconCacheDir = QDir::tempPath() + "/qingzhou/icons";
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
        item["title"] = tr("Enter a filename to search");
        item["subtitle"] = tr("Powered by Everything for fast search");
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QStringLiteral("none");
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
            item["title"] = tr("Everything service is not running");
            item["subtitle"] = tr("Please start Everything before using file search");
        } else {
            item["title"] = tr("Search failed");
            item["subtitle"] = tr("Error code: ") + QString::number(err);
        }
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QStringLiteral("none");
        action["plugin"] = command();
        item["action"] = action;
        results.append(item);
        Everything_Reset();
        return results;
    }

    DWORD count = Everything_GetNumResults();
    if (count == 0) {
        QVariantMap item;
        item["title"] = tr("No files found");
        item["subtitle"] = args;
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QStringLiteral("none");
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
            action["type"] = QStringLiteral("openFile");
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
    item["title"] = tr("File search is only available on Windows");
    item["subtitle"] = tr("Everything SDK only supports Windows");
    item["icon"] = "";
    QVariantMap action;
    action["type"] = QStringLiteral("none");
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

