#pragma once

#include "IPlugin.h"

#include <QFutureWatcher>
#include "../utils/DirectoryListing.h"

/**
 * @brief 目录浏览和跳转插件
 *
 * 命令: cd
 * 示例: cd .., cd D:\\, cd Documents
 *
 * 目录列举在工作线程执行，避免大目录/网络盘阻塞输入。
 */
class DirectoryPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit DirectoryPlugin(QObject *parent = nullptr);
    ~DirectoryPlugin() override;

    QString command() const override { return QStringLiteral("cd"); }
    QString description() const override { return tr("Directory browser and navigation"); }
    PluginQueryResult query(const PluginQueryRequest &request) override;
    PluginExecutionResult execute(const PluginInvocation &invocation) override;
    void cancelQuery() override;

private:
    static QVariantList listDirectory(const QString &args, int limit, const DirectoryListing::Cancellation &cancel);
    void startListing();

    QFutureWatcher<QVariantList> m_watcher;
    PluginQueryRequest m_requestedQuery;
    PluginQueryRequest m_runningQuery;
    bool m_listing = false;
    DirectoryListing::Cancellation m_cancel;
};
