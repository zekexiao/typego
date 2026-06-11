#pragma once

#include "IPlugin.h"

/**
 * Calculator plugin backed by a small recursive-descent parser.
 */
class CalcPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit CalcPlugin(QObject *parent = nullptr);

    QString command() const override { return QStringLiteral("calc"); }
    QString description() const override { return tr("Calculator"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;
};

