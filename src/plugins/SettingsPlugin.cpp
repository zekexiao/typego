#include "SettingsPlugin.h"
#include "../SettingsView.h"

#include <QVariantMap>

SettingsPlugin::SettingsPlugin(Settings *settings, PluginManager *pluginManager, QObject *parent)
    : IPlugin(parent)
    , m_settings(settings)
    , m_pluginManager(pluginManager)
{
}

QVariantList SettingsPlugin::query(const QString &args)
{
    Q_UNUSED(args)
    QVariantMap item;
    item["title"] = tr("Settings");
    item["subtitle"] = tr("Modify global hotkey, window style and other settings");
    item["icon"] = "";
    return QVariantList() << item;
}

bool SettingsPlugin::execute(const QVariantMap &action)
{
    Q_UNUSED(action)
    return true;
}

void SettingsPlugin::activateCommand(const QString &command)
{
    m_activeCommand = command;
    if (m_view) {
        static_cast<SettingsView *>(m_view)->showSection(command);
    }
}

QWidget *SettingsPlugin::viewUrl()
{
    if (!m_view) {
        m_view = new SettingsView(m_settings, m_pluginManager);
        connect(static_cast<SettingsView *>(m_view), &SettingsView::closeRequested, this, &SettingsPlugin::viewCloseRequested);
        connect(m_view, &QObject::destroyed, this, [this]() { m_view = nullptr; });
    }
    static_cast<SettingsView *>(m_view)->reload();
    static_cast<SettingsView *>(m_view)->showSection(m_activeCommand);
    return m_view;
}
