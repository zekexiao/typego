#include "PluginManager.h"
#include "../Settings.h"
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
    QString cmd = plugin->command().toLower();
    if (m_plugins.contains(cmd)) {
        qWarning() << "Plugin command conflict:" << cmd;
        return;
    }
    m_plugins[cmd] = plugin;
    qDebug() << "Plugin registered:" << cmd << "-" << plugin->description();
}

QVariantList PluginManager::queryCommand(const QString &cmd, const QString &args)
{
    QString key = cmd.toLower().trimmed();
    IPlugin *plugin = m_plugins.value(key, nullptr);
    if (!plugin) {
        qDebug() << "Unknown command:" << cmd;
        return QVariantList();
    }
    if (m_settings && !m_settings->isPluginEnabled(key)) {
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
    return plugin->query(args);
}

bool PluginManager::executeAction(const QVariantMap &action)
{
    if (action.isEmpty()) return false;

    QString type = action.value("type").toString();
    if (type.isEmpty()) return false;

    // 濡傛灉 action 涓寚瀹氫簡 plugin锛屼紭鍏堝垎鍙戝埌瀵瑰簲鎻掍欢鎵ц
    QString pluginCmd = action.value("plugin").toString().toLower();
    if (!pluginCmd.isEmpty()) {
        IPlugin *plugin = m_plugins.value(pluginCmd, nullptr);
        if (plugin) {
            return plugin->execute(action);
        }
    }

    // 閫氱敤鍔ㄤ綔绫诲瀷
    if (type == "launch") {
        QString path = action.value("data").toString();
        if (!path.isEmpty()) {
            return QProcess::startDetached(path, QStringList());
        }
    } else if (type == "openUrl") {
        QString url = action.value("data").toString();
        if (!url.isEmpty()) {
            return QDesktopServices::openUrl(QUrl(url));
        }
    } else if (type == "copy") {
        QString text = action.value("data").toString();
        if (!text.isEmpty()) {
            QGuiApplication::clipboard()->setText(text);
            return true;
        }
    }

    return false;
}

QStringList PluginManager::commands() const
{
    return m_plugins.keys();
}

QString PluginManager::commandDescription(const QString &cmd) const
{
    IPlugin *plugin = m_plugins.value(cmd.toLower(), nullptr);
    if (plugin) return plugin->description();
    return QString();
}

QWidget *PluginManager::viewForCommand(const QString &cmd) const
{
    IPlugin *plugin = m_plugins.value(cmd.toLower(), nullptr);
    return plugin ? plugin->viewUrl() : nullptr;
}

bool PluginManager::autoEnterForCommand(const QString &cmd) const
{
    if (m_settings && !m_settings->isPluginEnabled(cmd.toLower())) return false;
    if (m_settings) return m_settings->pluginAutoEnter();
    return false;
}

IPlugin *PluginManager::plugin(const QString &cmd) const
{
    return m_plugins.value(cmd.toLower(), nullptr);
}

