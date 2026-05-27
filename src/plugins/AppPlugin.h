#pragma once

#include "IPlugin.h"
#include "../AppLauncher.h"

/**
 * @brief 应用搜索插件
 *
 * 命令: app
 * 示例: app vscode
 */
class AppPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit AppPlugin(AppLauncher *launcher, QObject *parent = nullptr);

    QString command() const override { return QString::fromUtf8(u8"app"); }
    QString description() const override { return QString::fromUtf8(u8"搜索并启动应用"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;

private:
    AppLauncher *m_launcher;
};
