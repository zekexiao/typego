#pragma once

#include "IPlugin.h"
#include <QFutureWatcher>

// 文件搜索：Windows 走 Everything，其他平台走 fd/find。
class FilePlugin : public IPlugin {
    Q_OBJECT
public:
    explicit FilePlugin(QObject *parent = nullptr);
    ~FilePlugin() override;

    QString command() const override { return QStringLiteral("file"); }
#ifdef Q_OS_WIN
    QString description() const override { return tr("File search (Everything)"); }
#else
    QString description() const override { return tr("File search (fd)"); }
#endif
    PluginQueryResult query(const PluginQueryRequest &request) override;
    PluginExecutionResult execute(const PluginInvocation &invocation) override;
    void cancelQuery() override;

private:
    void startSearch();
    static QVariantList searchFiles(const QString &args, int limit);
    static QString typeLabel(const QString &fileName, bool isFolder);

    QFutureWatcher<QVariantList> m_watcher;
    PluginQueryRequest m_requestedQuery;
    PluginQueryRequest m_runningQuery;
    bool m_searchRunning = false;
};
