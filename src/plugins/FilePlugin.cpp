#include "FilePlugin.h"

#include "../utils/ShellOpen.h"

#include <QVariantMap>
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
        if (m_runningQuery.id != m_requestedQuery.id) {
            if (m_requestedQuery.id) startSearch();
            return;
        }
        emit queryFinished(PluginQueryResult{m_runningQuery.id, m_watcher.result()});
    });
}

FilePlugin::~FilePlugin() = default;

void FilePlugin::cancelQuery()
{
    m_requestedQuery = {};
}

PluginQueryResult FilePlugin::query(const PluginQueryRequest &request)
{
    const QString query = request.invocation.arguments.trimmed();
    m_requestedQuery = request;
    if (query.isEmpty()) {
        m_requestedQuery = {};
        return {request.id, searchFiles(query, request.limit)};
    }
    if (!m_searchRunning) startSearch();
    QVariantMap item;
    item["title"] = tr("Searching files...");
    item["subtitle"] = query;
    item["action"] = QVariantMap{{"type", "none"}};
    return {request.id, {item}, true};
}

void FilePlugin::startSearch()
{
    // SDK 使用全局查询状态，始终只运行一个查询，并仅保留最新待查词。
    m_searchRunning = true;
    m_runningQuery = m_requestedQuery;
    m_watcher.setFuture(QtConcurrent::run(&FilePlugin::searchFiles,
        m_runningQuery.invocation.arguments.trimmed(), m_runningQuery.limit));
}

QString FilePlugin::typeLabel(const QString &fileName, bool isFolder)
{
    if (isFolder) return tr("Folder");
    // 从文件名取后缀，避免 QFileInfo 触发文件系统 stat。
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    if (dot > 0 && dot < fileName.size() - 1) {
        return tr("%1 File").arg(fileName.mid(dot + 1).toUpper());
    }
    return tr("File");
}

QVariantList FilePlugin::searchFiles(const QString &args, int limit)
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
    Everything_SetMax(static_cast<DWORD>(limit));
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

    const DWORD count = Everything_GetNumResults();
    const DWORD totalCount = Everything_GetTotResults();
    results.reserve(static_cast<int>(count) + (totalCount > count ? 1 : 0));
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
        const DWORD length = Everything_GetResultFullPathNameW(i, wpath, 1024);
        if (length == 0) continue;
        const QString fullPath = QString::fromWCharArray(wpath, static_cast<int>(length));
        const bool isFolder = Everything_IsFolderResult(i);
        // 纯字符串拆分文件名与所在目录，不为每条结果做文件系统 stat。
        const int sep = fullPath.lastIndexOf(QLatin1Char('\\'));
        const QString name = sep >= 0 ? fullPath.mid(sep + 1) : fullPath;
        const QString dir = sep > 0 ? fullPath.left(sep) : QString();
        const QString kind = typeLabel(name, isFolder);

        QVariantMap item;
        item["title"] = name;
        item["subtitle"] = dir.isEmpty() ? kind : kind + QStringLiteral(" · ") + dir;
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

PluginExecutionResult FilePlugin::execute(const PluginInvocation &invocation)
{
    const QVariantMap action = invocation.action;
    const QString type = action.value("type").toString();
    const QString data = action.value("data").toString();
    if ((type == QLatin1String("openFile") || type == QLatin1String("openFolder")) && ShellOpen::open(data)) {
        return {PluginExecutionResult::Status::Succeeded, PluginExecutionResult::HostAction::Hide};
    }
    return {PluginExecutionResult::Status::Failed, PluginExecutionResult::HostAction::Keep};
}
