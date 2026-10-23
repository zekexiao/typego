#pragma once

#include "IPlugin.h"
#include <QFutureWatcher>

class QFileInfo;

// everything 文件搜索
class FilePlugin : public IPlugin {
    Q_OBJECT
public:
    explicit FilePlugin(QObject *parent = nullptr);
    ~FilePlugin() override;

    QString command() const override { return QStringLiteral("file"); }
    QString description() const override { return tr("File search (Everything)"); }
    PluginQueryResult query(const PluginQueryRequest &request) override;
    bool execute(const QVariantMap &action) override;
    void cancelQuery() override;

private:
    void startSearch();
    static QVariantList searchFiles(const QString &args, int limit);
    static QString typeLabel(const QFileInfo &fileInfo, bool isFolder);

    QFutureWatcher<QVariantList> m_watcher;
    PluginQueryRequest m_requestedQuery;
    PluginQueryRequest m_runningQuery;
    bool m_searchRunning = false;
};
