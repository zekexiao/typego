#pragma once

#include "IPlugin.h"

/**
 * @brief 目录跳转插件
 *
 * 命令: cd
 * 示例: cd .., cd D:\\, cd Documents
 */
class DirectoryPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit DirectoryPlugin(QObject *parent = nullptr);

    QString command() const override { return QString::fromUtf8(u8"cd"); }
    QString description() const override { return tr("Directory browser and navigation"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;

private:
    QString resolvePath(const QString &args) const;
};
