#pragma once

#include "IPlugin.h"

/**
 * @brief 文件搜索插件（基于 Voidtools Everything SDK）
 *
 * 命令: file
 * 示例: file readme.txt
 *
 * 要求：Everything 服务正在运行，且 Everything64.dll 已部署
 */
class FilePlugin : public IPlugin {
    Q_OBJECT
public:
    explicit FilePlugin(QObject *parent = nullptr);
    ~FilePlugin() override;

    QString command() const override { return QString::fromUtf8(u8"file"); }
    QString description() const override { return tr("File search (Everything)"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;

private:
    QString m_iconCacheDir;
    QString extractFileIcon(const QString &filePath);
};
