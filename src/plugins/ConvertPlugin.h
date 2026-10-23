#pragma once

#include "IPlugin.h"

class ConvertView;

class ConvertPlugin final : public IPlugin {
    Q_OBJECT
public:
    explicit ConvertPlugin(QObject *parent = nullptr);
    ~ConvertPlugin() override;

    QString command() const override { return QStringLiteral("convert"); }
    QStringList aliases() const override { return {QStringLiteral("conv")}; }
    QString description() const override { return tr("Unit converter"); }
    QVariantList query(const QString &args) override;
    PluginExecutionResult execute(const PluginInvocation &invocation) override;
    LaunchMode launchMode(const QString &args) const override;
    void activateQuery(const QString &args) override;
    QWidget *createView(QWidget *parent) override;
    void releaseInactiveResources() override;

private:
    ConvertView *m_view = nullptr;
    QString m_query;
};
