#pragma once

#include <TypeGoPlugin/Plugin.h>

#include <QList>
#include <QPointer>
#include <QTranslator>
#include <memory>

#include "ScreenshotSession.h"
#include "ScreenshotStore.h"

class PinWindow;
class QWidget;
class ScrollResultWindow;

// 截图插件：快捷键直接进入区域选择，支持标注、复制、保存、贴图和历史。
class ScreenshotPlugin final : public QObject, public TypeGo::PluginApi::Plugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID TYPEGO_PLUGIN_IID FILE "typego-plugin.json")
    Q_INTERFACES(TypeGo::PluginApi::Plugin)
public:
    TypeGo::PluginApi::Error initialize(TypeGo::PluginApi::Host *host) override;
    TypeGo::PluginApi::QueryResult query(const TypeGo::PluginApi::QueryRequest &request) override;
    TypeGo::PluginApi::ExecutionResult execute(const TypeGo::PluginApi::Invocation &invocation) override;
    QWidget *createView(const TypeGo::PluginApi::Invocation &invocation, QWidget *parent) override;
    QWidget *createSettingsView(QWidget *parent) override;
    void activate(const TypeGo::PluginApi::Invocation &invocation) override;
    void deactivate() override;
    TypeGo::PluginApi::LeaveDecision prepareToLeave(TypeGo::PluginApi::LeaveReason reason, quint64 token) override;
    void shutdown() override;

private:
    void applyConfig();
    void beginCapture();
    void onCompleted(ScreenshotSession::Finish action, const QImage &image, const QRect &selection);
    void onScrollFinished(const QImage &image);
    void onFailed(const QString &message);
    void abandonSession();
    void recordImage(const QImage &image);
    void copyImage(const QImage &image);
    void saveImageAs(const QImage &image);
    void addPin(const QImage &image, const QPoint &position);
    void closePins();
    void showScrollResult(const QImage &image);
    void closeScrollResults();
    QString defaultSaveDir() const;

    TypeGo::PluginApi::Host *m_host = nullptr;
    QTranslator m_translator;
    std::unique_ptr<ScreenshotStore> m_store;
    std::unique_ptr<ScreenshotSession> m_session;
    QPointer<QWidget> m_view;
    QList<QPointer<PinWindow>> m_pins;
    QList<QPointer<ScrollResultWindow>> m_scrollResults;

    QString m_saveDir;
    bool m_magnifier = true;
    int m_magnifierZoom = 8;
    int m_historyLimit = 30;
    quintptr m_previousForeground = 0;
    bool m_pendingStart = false;
    bool m_pendingFullscreen = false;
    bool m_pendingScroll = false;
    bool m_shuttingDown = false;
};