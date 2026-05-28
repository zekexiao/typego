#include "SettingsPlugin.h"
#include <QVariantMap>

SettingsPlugin::SettingsPlugin(QObject *parent)
    : IPlugin(parent)
{
}

QVariantList SettingsPlugin::query(const QString &args)
{
    Q_UNUSED(args)
    QVariantList results;

    QVariantMap item;
    item["title"] = tr("Settings");
    item["subtitle"] = tr("Modify global hotkey, window style and other settings");
    item["icon"] = "";
    results.append(item);
    return results;
}

bool SettingsPlugin::execute(const QVariantMap &action)
{
    Q_UNUSED(action)
    // 实际打开设置由 QML 处理，此处返回 true 表示已识别
    return true;
}
