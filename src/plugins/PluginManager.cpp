#include "PluginManager.h"
#include "../Settings.h"
#include "PluginAction.h"
#include <QDebug>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QGuiApplication>
#include <QClipboard>

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
}

void PluginManager::setSettings(Settings *settings)
{
    m_settings = settings;
}

void PluginManager::registerPlugin(IPlugin *plugin)
{
    if (!plugin) return;
    QStringList commands = plugin->aliases();
    commands.prepend(plugin->command());
    for (const QString &command : commands) {
        const QString cmd = command.toLower().trimmed();
        if (cmd.isEmpty()) continue;
        if (m_plugins.contains(cmd)) {
            qWarning() << "Plugin command conflict:" << cmd;
            continue;
        }
        m_plugins[cmd] = plugin;
        m_commands.append(cmd);
        qDebug() << "Plugin registered:" << cmd << "-" << plugin->description();
    }
}

QVariantList PluginManager::queryCommand(const QString &cmd, const QString &args)
{
    return queryCommand(cmd, args, IPlugin::QueryContext::Auto);
}

QVariantList PluginManager::queryCommand(const QString &cmd, const QString &args, IPlugin::QueryContext context)
{
    QString key = cmd.toLower().trimmed();
    IPlugin *plugin = m_plugins.value(key, nullptr);
    if (!plugin) {
        qDebug() << "Unknown command:" << cmd;
        return QVariantList();
    }
    if (m_settings && !m_settings->isPluginEnabled(plugin->command())) {
        QVariantMap item;
        item["title"] = tr("Plugin disabled");
        item["subtitle"] = key + tr(" plugin has been disabled in settings");
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QStringLiteral("none");
        action["plugin"] = key;
        item["action"] = action;
        return QVariantList() << item;
    }
    return plugin->query(args, context);
}

bool PluginManager::executeAction(const QVariantMap &action)
{
    // 分类交给无副作用的 PluginAction::parse，这里只负责执行，
    // 这样动作约定可以在没有 GUI、没有进程权限的测试进程里验证。
    const PluginAction::Request request = PluginAction::parse(action);
    if (!request.valid) return false;

    // action 指定插件时优先交给对应插件执行。
    if (!request.pluginCommand.isEmpty()) {
        IPlugin *plugin = m_plugins.value(request.pluginCommand, nullptr);
        if (plugin) return plugin->execute(action);
    }

    switch (request.builtin) {
    case PluginAction::Kind::Launch:
        return QProcess::startDetached(request.payload, QStringList());
    case PluginAction::Kind::OpenUrl:
        return QDesktopServices::openUrl(QUrl(request.payload));
    case PluginAction::Kind::Copy: {
        QClipboard *clipboard = QGuiApplication::clipboard();
        if (!clipboard) return false;
        clipboard->setText(request.payload);
        return true;
    }
    case PluginAction::Kind::Invalid:
        break;
    }
    return false;
}

QStringList PluginManager::commands() const
{
    return m_commands;
}

bool PluginManager::hasCommand(const QString &cmd) const
{
    return m_plugins.contains(cmd.toLower().trimmed());
}

QString PluginManager::commandDescription(const QString &cmd) const
{
    IPlugin *plugin = m_plugins.value(cmd.toLower(), nullptr);
    if (plugin) return plugin->description();
    return QString();
}

QWidget *PluginManager::viewForCommand(const QString &cmd) const
{
    return viewForCommand(cmd, QString());
}

QWidget *PluginManager::viewForCommand(const QString &cmd, const QString &args) const
{
    IPlugin *plugin = m_plugins.value(cmd.toLower(), nullptr);
    if (!plugin) return nullptr;
    plugin->activateCommand(cmd.toLower());
    plugin->activateQuery(args);
    return plugin->viewUrl();
}

IPlugin::LaunchMode PluginManager::launchModeForCommand(const QString &cmd, const QString &args) const
{
    IPlugin *plugin = m_plugins.value(cmd.toLower().trimmed(), nullptr);
    if (!plugin) return IPlugin::LaunchMode::List;
    if (m_settings && !m_settings->isPluginEnabled(plugin->command())) return IPlugin::LaunchMode::List;
    return plugin->launchMode(args);
}

QWidget *PluginManager::settingsWidgetForCommand(const QString &cmd) const
{
    IPlugin *plugin = m_plugins.value(cmd.toLower().trimmed(), nullptr);
    return plugin ? plugin->settingsWidget() : nullptr;
}

bool PluginManager::autoEnterForCommand(const QString &cmd) const
{
    IPlugin *plugin = m_plugins.value(cmd.toLower(), nullptr);
    if (!plugin || !plugin->supportsAutoEnter()) return false;
    if (!m_settings || !m_settings->isPluginEnabled(plugin->command())) return false;
    return m_settings->pluginAutoEnter();
}

IPlugin *PluginManager::plugin(const QString &cmd) const
{
    return m_plugins.value(cmd.toLower(), nullptr);
}

