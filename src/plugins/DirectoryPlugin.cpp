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

void DirectoryPlugin::cancelQuery()
{
    m_requestedQuery = {};
}

PluginQueryResult DirectoryPlugin::query(const PluginQueryRequest &request)
{
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
    m_watcher.setFuture(QtConcurrent::run(&DirectoryPlugin::listDirectory,
        m_runningQuery.invocation.arguments, m_runningQuery.limit));
}

QString DirectoryPlugin::resolvePath(const QString &args)
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

QVariantList DirectoryPlugin::listDirectory(const QString &args, int limit)
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
        action["plugin"] = QStringLiteral("cd");
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
    enterAction["plugin"] = QStringLiteral("cd");
    enterItem["action"] = enterAction;
    results.append(enterItem);

    // 列出子目录和文件；超出 limit 提前结束，避免巨型目录的无效遍历。
    // 图标由结果列表异步加载，避免阻塞输入。
    const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                    QDir::DirsFirst | QDir::Name);
    results.reserve(qMin(entries.size() + 1, limit));
    for (const QFileInfo &fi : entries) {
        if (results.size() >= limit) break;
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
