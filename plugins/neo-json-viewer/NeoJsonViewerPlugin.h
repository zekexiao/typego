#pragma once

#include <TypeGoPlugin/Plugin.h>
#include <QTranslator>
#include <QPointer>
class MainWindow;

class NeoJsonViewerPlugin final : public QObject, public TypeGo::PluginApi::Plugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID TYPEGO_PLUGIN_IID FILE "typego-plugin.json")
    Q_INTERFACES(TypeGo::PluginApi::Plugin)
public:
    TypeGo::PluginApi::Error initialize(TypeGo::PluginApi::Host *host) override;
    TypeGo::PluginApi::QueryResult query(const TypeGo::PluginApi::QueryRequest &request) override;
    TypeGo::PluginApi::ExecutionResult execute(const TypeGo::PluginApi::Invocation &invocation) override;
    QWidget *createView(const TypeGo::PluginApi::Invocation &invocation, QWidget *parent) override;
    TypeGo::PluginApi::LeaveDecision prepareToLeave(TypeGo::PluginApi::LeaveReason reason, quint64 token) override;
    void shutdown() override;
private:
    TypeGo::PluginApi::Host *m_host = nullptr;
    QTranslator m_translator;
    QPointer<MainWindow> m_view;
};
