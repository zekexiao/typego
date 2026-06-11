#pragma once

#include "IPlugin.h"

class PluginManager;
class Settings;
class ThemeManager;
class QWidget;

class SettingsPlugin : public IPlugin {
    Q_OBJECT
public:
    SettingsPlugin(Settings *settings, PluginManager *pluginManager, ThemeManager *themeManager, QObject *parent = nullptr);

    QString command() const override { return QStringLiteral("set"); }
    QString description() const override { return tr("Open Settings"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;
    QWidget *viewUrl() override;

private:
    Settings *m_settings;
    PluginManager *m_pluginManager;
    ThemeManager *m_themeManager;
    QWidget *m_view = nullptr;
};

