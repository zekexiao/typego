#include "NeoJsonViewerPlugin.h"
#include "MainWindow.h"
#include <QCoreApplication>
#include <QCloseEvent>
#include <QDir>
#include <QLocale>

namespace {
class PluginViewerWindow final : public JsonViewerWindow {
public:
    PluginViewerWindow(TypeGo::BuiltinFeature::Host *host, QWidget *parent)
        : JsonViewerWindow(parent, QDir(host->directory(TypeGo::BuiltinFeature::Directory::Data)).filePath(QStringLiteral("settings.ini"))), m_host(host) {}

protected:
    void closeEvent(QCloseEvent *event) override
    {
        event->ignore();
        m_host->closeView();
    }

private:
    TypeGo::BuiltinFeature::Host *m_host;
};
}

TypeGo::BuiltinFeature::Error NeoJsonViewerPlugin::initialize(TypeGo::BuiltinFeature::Host *host)
{
    m_host = host;
    const QString translations = QStringLiteral(":/integrated");
    if (m_translator.load(QLocale(), QStringLiteral("NeoJsonViewer"), QStringLiteral("_"), translations))
        QCoreApplication::installTranslator(&m_translator);
    return {};
}

TypeGo::BuiltinFeature::QueryResult NeoJsonViewerPlugin::query(const TypeGo::BuiltinFeature::QueryRequest &request)
{
    using namespace TypeGo::BuiltinFeature;
    return {request.id, {{QStringLiteral("open-viewer"), tr("JSON View"),
        tr("Browse JSON and MessagePack files · Press Enter to open"), {}, {}, {}}}};
}

TypeGo::BuiltinFeature::ExecutionResult NeoJsonViewerPlugin::execute(const TypeGo::BuiltinFeature::Invocation &)
{
    return {TypeGo::BuiltinFeature::Status::Succeeded, TypeGo::BuiltinFeature::HostAction::OpenView};
}

QWidget *NeoJsonViewerPlugin::createView(const TypeGo::BuiltinFeature::Invocation &, QWidget *parent)
{
    if (!m_host) return nullptr;
    auto *window = new PluginViewerWindow(m_host, parent);
    m_view = window;
    window->setWindowFlags(Qt::Widget);
    return window;
}

TypeGo::BuiltinFeature::LeaveDecision NeoJsonViewerPlugin::prepareToLeave(TypeGo::BuiltinFeature::LeaveReason reason, quint64 token)
{
    Q_UNUSED(token)
    using namespace TypeGo::BuiltinFeature;
    if (reason == LeaveReason::Hide) return LeaveDecision::Ready;
    return !m_view || m_view->confirmLeave() ? LeaveDecision::Ready : LeaveDecision::Refused;
}

void NeoJsonViewerPlugin::shutdown()
{
    m_host = nullptr;
    QCoreApplication::removeTranslator(&m_translator);
}
