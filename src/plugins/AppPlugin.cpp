#include "AppPlugin.h"
#include <QVariantMap>

AppPlugin::AppPlugin(AppLauncher *launcher, QObject *parent)
    : IPlugin(parent)
    , m_launcher(launcher)
{
}

QVariantList AppPlugin::query(const QString &args)
{
    if (!m_launcher) return QVariantList();

    QVariantList results = m_launcher->search(args);
    // 为每个结果补充 action 字段，标记为 app 插件的动作
    for (int i = 0; i < results.size(); ++i) {
        QVariantMap item = results[i].toMap();
        QVariantMap action;
        action["type"] = QString::fromUtf8(u8"launch");
        action["data"] = item.value("path").toString();
        action["plugin"] = command();
        item["action"] = action;
        results[i] = item;
    }
    return results;
}

bool AppPlugin::execute(const QVariantMap &action)
{
    if (!m_launcher) return false;
    QString path = action.value("data").toString();
    if (path.isEmpty()) return false;
    return m_launcher->launch(path);
}
