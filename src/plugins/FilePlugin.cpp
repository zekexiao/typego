#include "FilePlugin.h"
#include <QVariantMap>
#include <QDir>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <limits>
#include <QtConcurrent>

#ifdef Q_OS_WIN
#include <windows.h>
#include "Everything.h"
#endif

FilePlugin::FilePlugin(QObject *parent)
    : IPlugin(parent)
{
    connect(&m_watcher, &QFutureWatcher<QVariantList>::finished, this, [this]() {
        m_searchRunning = false;
        if (m_runningQuery != m_requestedQuery) {
            if (!m_requestedQuery.isEmpty()) startSearch();
            return;
        }
        m_results = m_watcher.result();
        m_resultsReady = true;
        emit queryResultsChanged(m_requestedQuery);
    });
}

FilePlugin::~FilePlugin() = default;

void FilePlugin::cancelQuery()
{
    m_requestedQuery.clear();
    m_resultsReady = false;
    m_results.clear();
}

QVariantList FilePlugin::query(const QString &args)
{
    const QString query = args.trimmed();
    if (query != m_requestedQuery) {
        m_requestedQuery = query;
        m_resultsReady = false;
        m_results.clear();
    }
    if (query.isEmpty()) return searchFiles(query);
    if (m_resultsReady) return m_results;
    if (!m_searchRunning) startSearch();
    QVariantMap item;
    item["title"] = tr("Searching files...");
    item["subtitle"] = query;
    item["action"] = QVariantMap{{"type", "none"}};
    return {item};
}

void FilePlugin::startSearch()
{
    // SDK 使用全局查询状态，始终只运行一个查询，并仅保留最新待查词。
    m_searchRunning = true;
    m_runningQuery = m_requestedQuery;
    m_watcher.setFuture(QtConcurrent::run(&FilePlugin::searchFiles, m_runningQuery));
}

QString FilePlugin::typeLabel(const QFileInfo &fileInfo, bool isFolder)
{
    if (isFolder) return tr("Folder");
    const QString suffix = fileInfo.suffix().trimmed();
    if (!suffix.isEmpty()) {
        return tr("%1 File").arg(suffix.toUpper());
    }
    return tr("File");
}

QVariantList FilePlugin::searchFiles(const QString &args)
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
        action["plugin"] = QStringLiteral("file");
        item["action"] = action;
        results.append(item);
        return results;
    }

    Everything_SetSearchW((LPCWSTR)args.utf16());
    Everything_SetMax(200);
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
        action["plugin"] = QStringLiteral("file");
        item["action"] = action;
        results.append(item);
        Everything_Reset();
        return results;
    }

    DWORD count = Everything_GetNumResults();
    const DWORD totalCount = Everything_GetTotResults();
    const DWORD maxReserve = static_cast<DWORD>((std::numeric_limits<int>::max)());
    const DWORD reserveCount = qMin<DWORD>(count, maxReserve - (totalCount > count ? 1 : 0));
    results.reserve(static_cast<int>(reserveCount + (totalCount > count ? 1 : 0)));
    if (count == 0) {
        QVariantMap item;
        item["title"] = tr("No files found");
        item["subtitle"] = args;
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QStringLiteral("none");
        action["plugin"] = QStringLiteral("file");
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
            const bool isFolder = Everything_IsFolderResult(i);
            const QString kind = typeLabel(fi, isFolder);

            QVariantMap item;
            item["title"] = fi.fileName();
            item["subtitle"] = kind + QStringLiteral(" · ") + fi.absolutePath();
            item["kind"] = kind;
            item["path"] = fullPath;
            item["icon"] = "";

            QVariantMap action;
            action["type"] = isFolder ? QStringLiteral("openFolder") : QStringLiteral("openFile");
            action["data"] = fullPath;
            action["plugin"] = QStringLiteral("file");
            item["action"] = action;
            results.append(item);
        }
    }

    if (totalCount > count) {
        QVariantMap item;
        item["title"] = tr("Showing %1 of %2 results").arg(count).arg(totalCount);
        item["subtitle"] = tr("Use a more specific keyword to narrow the results");
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QStringLiteral("none");
        action["plugin"] = QStringLiteral("file");
        item["action"] = action;
        results.append(item);
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
    action["plugin"] = QStringLiteral("file");
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
    } else if (type == "openFolder") {
        return QDesktopServices::openUrl(QUrl::fromLocalFile(data));
    }
    return false;
}
