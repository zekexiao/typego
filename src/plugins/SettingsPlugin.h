#pragma once

#include "IPlugin.h"

/**
 * @brief 设置插件
 *
 * 命令: set
 * 示例: set
 * 回车后打开设置弹窗
 */
class SettingsPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit SettingsPlugin(QObject *parent = nullptr);

    QString command() const override { return QString::fromUtf8(u8"set"); }
    QString description() const override { return QString::fromUtf8(u8"打开设置"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;
};
