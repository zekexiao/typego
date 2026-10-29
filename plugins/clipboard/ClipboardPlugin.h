#pragma once

#include <TypeGoPlugin/Plugin.h>
#include <QTimer>
#include <QTranslator>
#include <memory>

#include "ClipboardStore.h"

class QClipboard;

// 剪贴板历史插件：后台监听系统剪贴板，按关键字过滤并粘贴回剪贴板。
class ClipboardPlugin final : public QObject, public TypeGo::PluginApi::Plugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID TYPEGO_PLUGIN_IID FILE "typego-plugin.json")
    Q_INTERFACES(TypeGo::PluginApi::Plugin)
public:
    TypeGo::PluginApi::Error initialize(TypeGo::PluginApi::Host *host) override;
    TypeGo::PluginApi::QueryResult query(const TypeGo::PluginApi::QueryRequest &request) override;
    TypeGo::PluginApi::ExecutionResult execute(const TypeGo::PluginApi::Invocation &invocation) override;
    QWidget *createSettingsView(QWidget *parent) override;
    void activate(const TypeGo::PluginApi::Invocation &invocation) override;
    TypeGo::PluginApi::LeaveDecision prepareToLeave(TypeGo::PluginApi::LeaveReason reason, quint64 token) override;
    void shutdown() override;

private:
    void startMonitoring();
    void stopMonitoring();
    void captureClipboard();
    void applyConfig();
    TypeGo::PluginApi::ResultItem resultItem(const ClipboardItem &item) const;
    bool matches(const ClipboardItem &item, const QString &filter) const;
    int clearHistory();
    void saveHistory();

    TypeGo::PluginApi::Host *m_host = nullptr;
    QClipboard *m_clipboard = nullptr;
    QTranslator m_translator;
    QTimer m_captureTimer;
    std::unique_ptr<ClipboardStore> m_store;
    bool m_captureText = true;
    bool m_captureImages = true;
    bool m_captureFiles = true;
    bool m_skipNextChange = false;
};