#pragma once

#include "PluginTypes.h"
#include <QObject>
#include <QHash>
#include <QPointer>
#include <functional>

class PluginManager;
class QStackedWidget;
class QWidget;

// 集中管理页面和独立窗口的所有权，插件只保存观察引用。
class PluginWindowHost final : public QObject {
    Q_OBJECT
public:
    PluginWindowHost(PluginManager *manager, QStackedWidget *pages, QObject *parent);
    ~PluginWindowHost() override;
    bool open(const PluginInvocation &invocation);
    void closeEmbedded(PluginLeaveReason reason, std::function<void(bool)> completed = {});
    void prepareToHide(std::function<void(bool)> completed);
    QString embeddedCommand() const { return m_embeddedCommand; }

signals:
    void embeddedOpened(const QString &command);
    void embeddedClosed();
    void operationFailed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct View {
        QPointer<QWidget> content;
        QPointer<QWidget> window;
        PluginPresentation presentation = PluginPresentation::Embedded;
        QString command;
        bool closing = false;
    };
    bool showView(const PluginInvocation &invocation);
    void discard(const QString &id);
    void deactivate(const QString &command);

    PluginManager *m_manager;
    QStackedWidget *m_pages;
    QHash<QString, View> m_views;
    QString m_embeddedCommand;
    bool m_navigating = false;
};
