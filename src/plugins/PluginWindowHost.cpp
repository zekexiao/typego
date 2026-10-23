#include "PluginWindowHost.h"
#include "PluginManager.h"
#include <QCloseEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

PluginWindowHost::PluginWindowHost(PluginManager *manager, QStackedWidget *pages, QObject *parent)
    : QObject(parent), m_manager(manager), m_pages(pages)
{
    connect(manager, &PluginManager::pluginDisabled, this, &PluginWindowHost::discard);
    connect(manager, &PluginManager::pluginCreated, this, [this](const QString &id, IPlugin *plugin) {
        connect(plugin, &IPlugin::viewCloseRequested, this, [this, id]() {
            if (m_manager->pluginIdForCommand(m_embeddedCommand) == id) {
                closeEmbedded(PluginLeaveReason::Navigate);
            } else {
                const auto it = m_views.constFind(id);
                if (it != m_views.cend() && it->window) it->window->close();
            }
        });
    });
}

PluginWindowHost::~PluginWindowHost()
{
    for (const View &view : std::as_const(m_views)) {
        if (view.window) {
            view.window->removeEventFilter(this);
            delete view.window;
        }
    }
}

bool PluginWindowHost::open(const PluginInvocation &call)
{
    const PluginCommand *command = m_manager->commandDescriptor(call.keyword);
    if (!command || !m_manager->isEnabled(call.keyword) || m_navigating) return false;
    if (command->presentation != PluginPresentation::Embedded || m_embeddedCommand.isEmpty()) {
        return showView(call);
    }
    // 离开旧应用成功后才激活新应用，保存失败时保留旧编辑器。
    m_navigating = true;
    QPointer<PluginWindowHost> guard(this);
    m_manager->requestLeave(m_embeddedCommand, PluginLeaveReason::Navigate, [guard, call](bool accepted) {
        if (!guard) return;
        guard->m_navigating = false;
        if (!accepted) { emit guard->operationFailed(); return; }
        guard->deactivate(guard->m_embeddedCommand);
        if (!guard->showView(call)) emit guard->operationFailed();
    });
    return true;
}

bool PluginWindowHost::showView(const PluginInvocation &call)
{
    IPlugin *plugin = m_manager->plugin(call.keyword);
    if (!plugin) return false;
    const auto presentation = m_manager->commandDescriptor(call.keyword)->presentation;
    auto it = m_views.find(call.pluginId);
    if (it != m_views.end() && it->presentation != presentation) {
        // 同一应用的不同呈现方式分别创建界面，避免移动正在编辑的 QWidget。
        if (it->window && it->window->isVisible()) return false;
        discard(call.pluginId);
        it = m_views.end();
    }
    plugin->activate(call);
    if (it == m_views.end() || !it->content) {
        View view;
        view.presentation = presentation;
        if (presentation == PluginPresentation::Embedded) {
            view.content = plugin->createView(m_pages);
            if (!view.content) return false;
            m_pages->addWidget(view.content);
        } else {
            view.window = new QWidget(nullptr, Qt::Window);
            view.window->setWindowTitle(m_manager->descriptor(call.pluginId)->description);
            view.window->resize(960, 640);
            auto *layout = new QVBoxLayout(view.window);
            layout->setContentsMargins(0, 0, 0, 0);
            view.content = plugin->createView(view.window);
            if (!view.content) { delete view.window; return false; }
            layout->addWidget(view.content);
            view.window->installEventFilter(this);
        }
        it = m_views.insert(call.pluginId, view);
    }
    it->command = call.keyword;
    if (presentation == PluginPresentation::Embedded) {
        m_embeddedCommand = call.keyword;
        m_pages->setCurrentWidget(it->content);
        emit embeddedOpened(call.keyword);
    } else {
        if (presentation == PluginPresentation::Overlay) it->window->showFullScreen();
        else it->window->show();
        it->window->raise();
        it->window->activateWindow();
    }
    return true;
}

void PluginWindowHost::deactivate(const QString &command)
{
    if (IPlugin *plugin = m_manager->plugin(command)) {
        plugin->deactivate();
        plugin->releaseInactiveResources();
    }
}

void PluginWindowHost::closeEmbedded(PluginLeaveReason reason, std::function<void(bool)> completed)
{
    if (m_embeddedCommand.isEmpty()) { if (completed) completed(true); return; }
    if (m_navigating) { if (completed) completed(false); return; }
    m_navigating = true;
    QPointer<PluginWindowHost> guard(this);
    m_manager->requestLeave(m_embeddedCommand, reason, [guard, completed = std::move(completed)](bool accepted) {
        if (!guard) return;
        guard->m_navigating = false;
        if (accepted) {
            guard->deactivate(guard->m_embeddedCommand);
            guard->m_embeddedCommand.clear();
            emit guard->embeddedClosed();
        } else emit guard->operationFailed();
        if (completed) completed(accepted);
    });
}

void PluginWindowHost::prepareToHide(std::function<void(bool)> completed)
{
    QPointer<PluginWindowHost> guard(this);
    m_manager->requestLeave(m_embeddedCommand, PluginLeaveReason::Hide,
                            [guard, completed = std::move(completed)](bool accepted) {
        if (!guard) return;
        if (!accepted) emit guard->operationFailed();
        completed(accepted);
    });
}

bool PluginWindowHost::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::Close) return QObject::eventFilter(watched, event);
    for (auto it = m_views.begin(); it != m_views.end(); ++it) {
        if (it->window != watched) continue;
        static_cast<QCloseEvent *>(event)->ignore();
        if (it->closing) return true;
        it->closing = true;
        const QString id = it.key();
        QPointer<PluginWindowHost> guard(this);
        m_manager->requestLeave(it->command, PluginLeaveReason::Navigate, [guard, id](bool accepted) {
            if (!guard) return;
            auto view = guard->m_views.find(id);
            if (view == guard->m_views.end()) return;
            view->closing = false;
            if (accepted) {
                guard->deactivate(view->command);
                if (view->window) view->window->hide();
            } else emit guard->operationFailed();
        });
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void PluginWindowHost::discard(const QString &id)
{
    const View view = m_views.take(id);
    if (m_manager->pluginIdForCommand(m_embeddedCommand) == id) {
        m_embeddedCommand.clear();
        emit embeddedClosed();
    }
    if (view.window) { view.window->removeEventFilter(this); view.window->deleteLater(); }
    else if (view.content) { m_pages->removeWidget(view.content); view.content->deleteLater(); }
}
