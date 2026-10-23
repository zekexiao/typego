#include "TodoPlugin.h"
#include <QVariantMap>
#include "../todo/TodoView.h"

TodoPlugin::TodoPlugin(QObject *parent)
    : IPlugin(parent)
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

IPlugin::LaunchMode TodoPlugin::launchMode(const QString &args) const
{
    Q_UNUSED(args)
    return LaunchMode::FullView;
}

QWidget *TodoPlugin::viewUrl()
{
    if (!m_view) {
        if (!m_service) m_service = new TaskService(this);
        m_view = new TodoView(m_service);
        connect(m_view, &TodoView::closeRequested, this, &TodoPlugin::viewCloseRequested);
        connect(m_view, &QObject::destroyed, this, [this]() { m_view = nullptr; });
    }
    m_view->reload();
    return m_view;
}

PluginLeaveDecision TodoPlugin::prepareToLeave(PluginLeaveReason reason, quint64 token)
{
    Q_UNUSED(reason)
    Q_UNUSED(token)
    return !m_view || m_view->savePendingChanges() ? PluginLeaveDecision::Ready : PluginLeaveDecision::Refused;
}
