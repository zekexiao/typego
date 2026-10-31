#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QVariantMap>

class QWidget;

namespace TypeGo::BuiltinFeature {

inline constexpr int MaxResults = 200;
inline constexpr int MaxPendingExecutions = 8;
inline constexpr int MaxParameterBytes = 16384;

enum class ErrorCode : int {
    None = 0, InvalidArgument = 1, Incompatible = 2, Conflict = 3,
    Disabled = 4, Cancelled = 5, Busy = 6, Internal = 7, IoError = 8
};

struct Error {
    ErrorCode code = ErrorCode::None;
    QString message;
    explicit operator bool() const { return code != ErrorCode::None; }
};

enum class Source : int { Search, Hotkey, Tray };
enum class QueryContext : int { Auto, Enter };
enum class Status : int { Succeeded, Failed, Pending };
enum class HostAction : int { Keep, Hide, OpenView };
enum class LeaveReason : int { Navigate, Hide, Disable, Shutdown };
enum class LeaveDecision : int { Ready, Refused, Pending };
enum class Directory : int { Data, Cache, Temporary };
enum class LogLevel : int { Info, Warning, Error };

struct Invocation {
    QString pluginId;
    QString commandId;
    QString arguments;
    Source source = Source::Search;
    quint64 operationId = 0;
    QString actionId;
    QVariantMap parameters;
};

struct QueryRequest {
    quint64 id = 0;
    Invocation invocation;
    QueryContext context = QueryContext::Auto;
    int limit = MaxResults;
};

struct ResultItem {
    QString id;
    QString title;
    QString subtitle;
    QString icon;
    QString actionId;
    QVariantMap parameters;
};

struct QueryResult {
    quint64 id = 0;
    QList<ResultItem> items;
    bool pending = false;
    Error error;
};

struct ExecutionResult {
    Status status = Status::Failed;
    HostAction hostAction = HostAction::Keep;
    Error error;
};

// 所有调用与回调均在宿主 GUI 线程；后台任务应向插件 QObject 排队投递。
class Host {
public:
    virtual ~Host() = default;
    virtual QString directory(Directory kind) = 0;
    virtual QVariant configValue(const QString &key, const QVariant &fallback = {}) = 0;
    virtual Error setConfigValue(const QString &key, const QVariant &value) = 0;
    virtual void log(LogLevel level, const QString &message) = 0;
    virtual void publishQuery(const QueryResult &result) = 0;
    virtual void finishExecution(quint64 operationId, const ExecutionResult &result) = 0;
    virtual void finishLeave(quint64 token, bool accepted) = 0;
    virtual void closeView() = 0;
};

// 内置功能与主程序共用的生命周期接口。
class Plugin {
public:
    virtual ~Plugin() = default;
    virtual Error initialize(Host *host) = 0;
    virtual QueryResult query(const QueryRequest &request) { return {request.id}; }
    virtual void cancelQuery(quint64 requestId) { Q_UNUSED(requestId) }
    virtual ExecutionResult execute(const Invocation &invocation)
    {
        Q_UNUSED(invocation)
        return {Status::Failed, HostAction::Keep, {ErrorCode::InvalidArgument, QStringLiteral("Unsupported action")}};
    }
    virtual QWidget *createView(const Invocation &invocation, QWidget *parent)
    { Q_UNUSED(invocation) Q_UNUSED(parent) return nullptr; }
    virtual QWidget *createSettingsView(QWidget *parent) { Q_UNUSED(parent) return nullptr; }
    virtual void activate(const Invocation &invocation) { Q_UNUSED(invocation) }
    virtual void deactivate() {}
    virtual LeaveDecision prepareToLeave(LeaveReason reason, quint64 token)
    { Q_UNUSED(reason) Q_UNUSED(token) return LeaveDecision::Ready; }
    virtual void releaseInactiveResources() {}
    // 返回前停止任务并清除 Host 引用；此后禁止回调。
    virtual void shutdown() = 0;
};

} // namespace TypeGo::BuiltinFeature

