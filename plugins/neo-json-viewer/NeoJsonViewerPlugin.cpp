#include "NeoJsonViewerPlugin.h"
#include "MainWindow.h"
#include <QCoreApplication>
#include <QCloseEvent>
#include <QDir>
#include <QLocale>

namespace {
class PluginViewerWindow final : public MainWindow {
public:
    PluginViewerWindow(TypeGo::PluginApi::Host *host, QWidget *parent)
        : MainWindow(parent, QDir(host->directory(TypeGo::PluginApi::Directory::Data)).filePath(QStringLiteral("settings.ini"))), m_host(host) {}

protected:
    void closeEvent(QCloseEvent *event) override
    {
        event->ignore();
        m_host->closeView();
    }

private:
    TypeGo::PluginApi::Host *m_host;
};
}

TypeGo::PluginApi::Error NeoJsonViewerPlugin::initialize(TypeGo::PluginApi::Host *host)
{
    m_host = host;
    const QString translations = QDir(host->directory(TypeGo::PluginApi::Directory::Package))
        .filePath(QStringLiteral("translations"));
    if (m_translator.load(QLocale(), QStringLiteral("NeoJsonViewer"), QStringLiteral("_"), translations))
        QCoreApplication::installTranslator(&m_translator);
    return {};
}

TypeGo::PluginApi::QueryResult NeoJsonViewerPlugin::query(const TypeGo::PluginApi::QueryRequest &request)
{
    using namespace TypeGo::PluginApi;
    return {request.id, {{QStringLiteral("open-viewer"), tr("JSON View"),
        tr("Browse JSON and MessagePack files · Press Enter to open"), {}, {}, {}}}};
}

TypeGo::PluginApi::ExecutionResult NeoJsonViewerPlugin::execute(const TypeGo::PluginApi::Invocation &)
{
    return {TypeGo::PluginApi::Status::Succeeded, TypeGo::PluginApi::HostAction::OpenView};
}

QWidget *NeoJsonViewerPlugin::createView(const TypeGo::PluginApi::Invocation &, QWidget *parent)
{
    if (!m_host) return nullptr;
    auto *window = new PluginViewerWindow(m_host, parent);
    m_view = window;
    window->setWindowFlags(Qt::Widget);
    return window;
}

TypeGo::PluginApi::LeaveDecision NeoJsonViewerPlugin::prepareToLeave(TypeGo::PluginApi::LeaveReason reason, quint64 token)
{
    Q_UNUSED(token)
    using namespace TypeGo::PluginApi;
    if (reason == LeaveReason::Hide) return LeaveDecision::Ready;
    return !m_view || m_view->confirmLeave() ? LeaveDecision::Ready : LeaveDecision::Refused;
}

void NeoJsonViewerPlugin::shutdown()
{
    m_host = nullptr;
    QCoreApplication::removeTranslator(&m_translator);
}
