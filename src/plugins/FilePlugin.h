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
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;
    void cancelQuery() override;

private:
    void startSearch();
    static QVariantList searchFiles(const QString &args);
    static QString typeLabel(const QFileInfo &fileInfo, bool isFolder);

    QFutureWatcher<QVariantList> m_watcher;
    QString m_requestedQuery;
    QString m_runningQuery;
    QVariantList m_results;
    bool m_searchRunning = false;
    bool m_resultsReady = false;
};
