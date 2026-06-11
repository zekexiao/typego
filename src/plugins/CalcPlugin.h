#pragma once

#include "IPlugin.h"

// 使用递归下降解析器计算表达式的插件。
class CalcPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit CalcPlugin(QObject *parent = nullptr);

    QString command() const override { return QStringLiteral("calc"); }
    QString description() const override { return tr("Calculator"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;
};

