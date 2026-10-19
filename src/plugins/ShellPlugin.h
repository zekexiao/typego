#pragma once

#include "IPlugin.h"

class ShellView;

class ShellPlugin final : public IPlugin {
    Q_OBJECT
public:
    explicit ShellPlugin(QObject *parent = nullptr);

    QString command() const override { return QStringLiteral("shell"); }
    QStringList aliases() const override { return {QStringLiteral("cmd")}; }
    QString description() const override { return tr("Run shell commands"); }
    QVariantList query(const QString &args) override;
    QVariantList query(const QString &args, QueryContext context) override;
    bool execute(const QVariantMap &action) override;
    LaunchMode launchMode(const QString &args) const override;
    void activateQuery(const QString &args) override;
    QWidget *viewUrl() override;
    bool hasSettingsWidget() const override { return true; }
    QWidget *settingsWidget() override;

private:
    QString terminalProgram() const;
    QString terminalArgumentsTemplate() const;
    void setTerminalConfig(const QString &program, const QString &argumentsTemplate);

    ShellView *m_view = nullptr;
    QString m_query;
};
