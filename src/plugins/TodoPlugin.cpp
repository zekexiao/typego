#include "TodoPlugin.h"
#include <QVariantMap>

TodoPlugin::TodoPlugin(QObject *parent)
    : IPlugin(parent)
    , m_service(new TaskService(this))
    , m_model(new TaskTreeModel(m_service->repository(), m_service, this))
{
}

QVariantList TodoPlugin::query(const QString &args)
{
    Q_UNUSED(args)
    QVariantMap item;
    item["title"] = tr("Task Management");
    item["subtitle"] = tr("Manage TODO list and subtasks");
    item["icon"] = "";
    return QVariantList() << item;
}

bool TodoPlugin::execute(const QVariantMap &action)
{
    Q_UNUSED(action)
    return true;
}

QWidget *TodoPlugin::viewUrl()
{
    return nullptr;
}
