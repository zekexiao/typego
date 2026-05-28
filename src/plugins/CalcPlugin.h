#pragma once

#include "IPlugin.h"
#include <QJSEngine>

/**
 * @brief 计算器插件
 *
 * 命令: calc
 * 示例: calc 1+2, calc sin(3.14)
 */
class CalcPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit CalcPlugin(QObject *parent = nullptr);

    QString command() const override { return QString::fromUtf8(u8"calc"); }
    QString description() const override { return tr("Calculator"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;

private:
    QJSEngine m_engine;
};
