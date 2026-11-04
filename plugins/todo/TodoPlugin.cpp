#include "TodoPlugin.h"
#include "TodoWindow.h"
#include <QCoreApplication>

TypeGo::BuiltinFeature::Error TodoPlugin::initialize(TypeGo::BuiltinFeature::Host *host)
{
    m_host = host;
    const QString translations = QStringLiteral(":/integrated");
    if (m_translator.load(QLocale(), QStringLiteral("Todo"), QStringLiteral("_"), translations))
        QCoreApplication::installTranslator(&m_translator);
    return {};
}

TypeGo::BuiltinFeature::QueryResult TodoPlugin::query(const TypeGo::BuiltinFeature::QueryRequest &request)
{
    using namespace TypeGo::BuiltinFeature;
    ResultItem item;
    item.id = QStringLiteral("open-todo");
    item.title = tr("Todo list");
    item.subtitle = tr("Plan tasks with priority and due time · Press Enter to open");
    return {request.id, {item}};
}

TypeGo::BuiltinFeature::ExecutionResult TodoPlugin::execute(const TypeGo::BuiltinFeature::Invocation &)
{
    return {TypeGo::BuiltinFeature::Status::Succeeded, TypeGo::BuiltinFeature::HostAction::OpenView};
}

QWidget *TodoPlugin::createView(const TypeGo::BuiltinFeature::Invocation &, QWidget *parent)
{
    if (!m_host) return nullptr;
    const QString directory = m_host->directory(TypeGo::BuiltinFeature::Directory::Data);
    auto *window = new TodoWindow(directory, parent);
    window->setWindowFlags(Qt::Widget);
    return window;
}

void TodoPlugin::shutdown()
{
    m_host = nullptr;
    QCoreApplication::removeTranslator(&m_translator);
}
