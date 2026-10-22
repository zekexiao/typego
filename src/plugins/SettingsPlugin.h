#pragma once

#include "IPlugin.h"

class PluginManager;
class Settings;
class QWidget;
class GlobalHotkey;
class AppLauncher;

// 提供设置入口和设置界面的插件。
class SettingsPlugin : public IPlugin {
    Q_OBJECT
public:
    SettingsPlugin(Settings *settings, PluginManager *pluginManager, GlobalHotkey *hotkeys,
                   AppLauncher *launcher, QObject *parent = nullptr);

    QString command() const override { return QStringLiteral("set"); }
    QStringList aliases() const override
    {
        return {QStringLiteral("user"), QStringLiteral("plugin"), QStringLiteral("scan"), QStringLiteral("about")};
    }
    QString description() const override { return tr("Open Settings"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;
    LaunchMode launchMode(const QString &args) const override;
    bool supportsAutoEnter() const override { return true; }
    void activateCommand(const QString &command) override;
    QWidget *viewUrl() override;

private:
    Settings *m_settings;
    PluginManager *m_pluginManager;
    GlobalHotkey *m_hotkeys;
    AppLauncher *m_launcher;
    QWidget *m_view = nullptr;
    QString m_activeCommand = QStringLiteral("set");
};

