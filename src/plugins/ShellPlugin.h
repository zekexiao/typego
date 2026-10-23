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
    PluginExecutionResult execute(const PluginInvocation &invocation) override;
    LaunchMode launchMode(const QString &args) const override;
    void activateQuery(const QString &args) override;
    QWidget *createView(QWidget *parent) override;
    void releaseInactiveResources() override;
    bool hasSettingsWidget() const override { return true; }
    QWidget *settingsWidget() override;

private:
    QString terminalProgram() const;
    QString terminalArgumentsTemplate() const;
    void setTerminalConfig(const QString &program, const QString &argumentsTemplate);
    void loadTerminalConfig() const;

    ShellView *m_view = nullptr;
    QString m_query;
    // QSettings 构造有注册表/文件开销，配置读一次后缓存，setTerminalConfig 时同步更新。
    mutable bool m_configLoaded = false;
    mutable QString m_terminalProgram;
    mutable QString m_terminalArgumentsTemplate;
};
