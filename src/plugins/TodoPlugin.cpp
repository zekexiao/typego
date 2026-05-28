#include "TodoPlugin.h"
#include <QVariantMap>
#include <QStandardPaths>
#include <QDir>

TodoPlugin::TodoPlugin(QObject *parent)
    : IPlugin(parent)
    , m_service(new TaskService(this))
    , m_model(new TaskTreeModel(m_service->repository(), m_service, this))
{
}

QVariantList TodoPlugin::query(const QString &args)
{
    Q_UNUSED(args)
    QVariantList results;

    QVariantMap item;
    item["title"] = tr("Task Management");
    item["subtitle"] = tr("Manage TODO list and subtasks");
    item["icon"] = "";

    QVariantMap action;
    action["type"] = QString::fromUtf8(u8"showCustomView");
    action["data"] = QString::fromUtf8(u8"qrc:/qml/TodoView.qml");
    action["plugin"] = command();
    item["action"] = action;

    results.append(item);
    return results;
}

bool TodoPlugin::execute(const QVariantMap &action)
{
    Q_UNUSED(action)
    return true;
}
