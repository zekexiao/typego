#pragma once

#include "IPlugin.h"

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

private:
    QString m_iconCacheDir;
    QString extractFileIcon(const QString &filePath);
};

