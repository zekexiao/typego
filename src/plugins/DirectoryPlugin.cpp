#include "DirectoryPlugin.h"

#include "../utils/ShellOpen.h"

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>
#include <QtConcurrent>

DirectoryPlugin::DirectoryPlugin(QObject *parent)
    : IPlugin(parent)
{
    // 与 FilePlugin 相同的全局查询状态：始终只跑一个列举，仅保留最新请求。
    connect(&m_watcher, &QFutureWatcher<QVariantList>::finished, this, [this]() {
        m_listing = false;
        if (m_runningQuery.id != m_requestedQuery.id) {
            if (m_requestedQuery.id) startListing();
            return;
        }
        emit queryFinished(PluginQueryResult{m_runningQuery.id, m_watcher.result()});
    });
}

DirectoryPlugin::~DirectoryPlugin()
{
    cancelQuery();
}

void DirectoryPlugin::cancelQuery()
{
    if (m_cancel) m_cancel->store(true, std::memory_order_relaxed);
    m_requestedQuery = {};
}

PluginQueryResult DirectoryPlugin::query(const PluginQueryRequest &request)
{
    if (m_cancel) m_cancel->store(true, std::memory_order_relaxed);
    m_requestedQuery = request;
    if (!m_listing) startListing();
    QVariantMap item;
    item["title"] = tr("Listing directory...");
    item["subtitle"] = request.invocation.arguments.trimmed();
    item["action"] = QVariantMap{{"type", "none"}};
    return {request.id, {item}, true};
}

void DirectoryPlugin::startListing()
{
    m_listing = true;
    m_runningQuery = m_requestedQuery;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    m_watcher.setFuture(QtConcurrent::run(&DirectoryPlugin::listDirectory,
        m_runningQuery.invocation.arguments, m_runningQuery.limit, m_cancel));
}

QVariantList DirectoryPlugin::listDirectory(const QString &args, int limit, const DirectoryListing::Cancellation &cancel)
{
    QVariantList results;
    const auto listing = DirectoryListing::list(args, QDir::homePath(), qMax(0, limit - 1), cancel);
    if (listing.cancelled) return results;
    if (!listing.valid) {
        QVariantMap item;
        item["title"] = tr("Directory does not exist or cannot be read");
        item["subtitle"] = args;
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QStringLiteral("none");
        action["plugin"] = QStringLiteral("cd");
        item["action"] = action;
        results.append(item);
        return results;
    }

    // 添加打开当前目录的操作。
    QDir dir(listing.path);
    QVariantMap enterItem;
    enterItem["title"] = tr("Open %1").arg(dir.dirName());
    enterItem["subtitle"] = listing.truncated
        ? tr("%1 - Showing the first %2 entries").arg(dir.absolutePath()).arg(listing.entries.size())
        : dir.absolutePath();
    enterItem["icon"] = "";
    QVariantMap enterAction;
    enterAction["type"] = QStringLiteral("openFolder");
    enterAction["data"] = dir.absolutePath();
    enterAction["plugin"] = QStringLiteral("cd");
    enterItem["action"] = enterAction;
    results.append(enterItem);

    // 后台仅保留排序后的有限结果，主线程不再访问目录。
    // 图标由结果列表异步加载，避免阻塞输入。
    results.reserve(listing.entries.size() + 1);
    for (const QFileInfo &fi : listing.entries) {
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
        action["plugin"] = QStringLiteral("cd");
        item["action"] = action;
        results.append(item);
    }

    return results;
}

PluginExecutionResult DirectoryPlugin::execute(const PluginInvocation &invocation)
{
    const QVariantMap action = invocation.action;
    const QString type = action.value("type").toString();
    const QString data = action.value("data").toString();
    // 三种动作（打开目录/打开文件/进入目录）统一走系统默认程序。
    if ((type == QLatin1String("openFolder") || type == QLatin1String("openFile") || type == QLatin1String("navigate"))
        && ShellOpen::open(data)) {
        return {PluginExecutionResult::Status::Succeeded, PluginExecutionResult::HostAction::Hide};
    }
    return {PluginExecutionResult::Status::Failed, PluginExecutionResult::HostAction::Keep};
}
