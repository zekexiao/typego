#pragma once

#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <QVariantList>

enum class PluginPresentation { Embedded, Window, Overlay };
enum class PluginLeaveReason { Navigate, Hide, Disable, Shutdown };
enum class PluginLeaveDecision { Ready, Refused, Pending };
enum class PluginQueryContext { Auto, Enter };

struct PluginInvocation {
    enum class Source { Search, Hotkey, Tray };
    QString pluginId;
    QString commandId;
    QString keyword;
    QString arguments;
    Source source = Source::Search;
    QVariantMap action;
    quint64 operationId = 0;
};

struct PluginExecutionResult {
    enum class Status { Succeeded, Failed, Pending };
    enum class HostAction { Keep, Hide, OpenView };
    Status status = Status::Failed;
    HostAction hostAction = HostAction::Keep;
    QString error;
    quint64 operationId = 0;
};
Q_DECLARE_METATYPE(PluginInvocation)
Q_DECLARE_METATYPE(PluginExecutionResult)

struct PluginQueryRequest {
    quint64 id = 0;
    PluginInvocation invocation;
    PluginQueryContext context = PluginQueryContext::Auto;
    int limit = 200;
};

struct PluginQueryResult {
    quint64 id = 0;
    QVariantList items;
    bool pending = false;
};
Q_DECLARE_METATYPE(PluginQueryResult)
