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
    item["title"] = QString::fromUtf8(u8"设置");
    item["subtitle"] = QString::fromUtf8(u8"修改全局热键、窗口样式等配置");
    item["icon"] = "";

    QVariantMap action;
    action["type"] = QString::fromUtf8(u8"showCustomView");
    action["data"] = QString::fromUtf8(u8"qrc:/qml/SettingsView.qml");
    action["plugin"] = command();
    item["action"] = action;

    results.append(item);
    return results;
}

bool SettingsPlugin::execute(const QVariantMap &action)
{
    Q_UNUSED(action)
    // 实际打开设置由 QML 处理，此处返回 true 表示已识别
    return true;
}
