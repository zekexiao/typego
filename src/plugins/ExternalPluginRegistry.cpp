#include "ExternalPluginRegistry.h"
#include "PluginManager.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QSettings>
#include <QSet>
#include <QThread>
#include <QWidget>
#include <exception>
#include <utility>
#include <algorithm>

namespace Api = TypeGo::PluginApi;

namespace {
void diagnose(const std::shared_ptr<QList<PluginDiagnostic>> &sink, const QString &path,
              const QString &id, const Api::Error &error)
{
    sink->append({path, id, error});
    qWarning().noquote() << "TypeGo plugin" << id << path << static_cast<int>(error.code) << error.message;
}

bool inside(const QString &path, const QString &root)
{
    return !path.isEmpty() && !root.isEmpty()
        && path.startsWith(root + QLatin1Char('/'), Qt::CaseInsensitive);
}

Api::Invocation publicInvocation(const PluginInvocation &call)
{
    return {call.pluginId, call.commandId, call.arguments, static_cast<Api::Source>(call.source),
            call.operationId, call.action.value(QStringLiteral("actionId")).toString(),
            call.action.value(QStringLiteral("parameters")).toMap()};
}

class ExternalPluginAdapter final : public IPlugin, private Api::Host {
public:
    ExternalPluginAdapter(PluginManifest manifest, QString path, PluginPaths paths,
                          std::shared_ptr<QList<PluginDiagnostic>> diagnostics, QObject *parent)
        : IPlugin(parent), m_manifest(std::move(manifest)), m_loader(path), m_paths(std::move(paths)),
          m_diagnostics(std::move(diagnostics))
    {
        // 映射保持到进程结束，避免排队事件或 Qt 类型析构跳进已卸载的代码。
        m_loader.setLoadHints(QLibrary::PreventUnloadHint);
        const auto checked = parsePluginManifest(m_loader.metaData());
        if (checked.error || checked.manifest.metadata != m_manifest.metadata) {
            setError(checked.error ? checked.error : Api::Error{Api::ErrorCode::Conflict, QStringLiteral("Plugin metadata changed; restart TypeGo")});
            return;
        }
        QObject *object = nullptr;
        try { object = m_loader.instance(); }
        catch (...) { setError(exceptionError()); return; }
        if (!object) { setError({Api::ErrorCode::Internal, m_loader.errorString()}); return; }
        m_plugin = qobject_cast<Api::Plugin *>(object);
        if (!m_plugin || object->thread() != thread()) {
            setError({Api::ErrorCode::Incompatible, QStringLiteral("Plugin does not implement the advertised interface on the host thread")});
            m_plugin = nullptr;
            return;
        }
        try {
            if (const auto error = m_plugin->initialize(this)) setError(error);
        } catch (...) { setError(exceptionError()); }
    }

    ~ExternalPluginAdapter() override
    {
        m_stopping = true;
        if (m_plugin) {
            try { m_plugin->shutdown(); } catch (...) { qWarning() << "Plugin shutdown threw an exception:" << command(); }
        }
        // shutdown 必须完成任务收尾；库仍被固定映射，但 Qt 根对象在此销毁。
        m_plugin = nullptr;
        m_loader.unload();
    }

    QString command() const override { return m_manifest.descriptor.commands.first().keyword; }
    QString description() const override { return m_manifest.descriptor.description; }
    PluginQueryResult query(const PluginQueryRequest &request) override
    {
        m_queryId = request.id;
        m_queryLimit = qMin(request.limit, Api::MaxResults);
        m_queryCommand = request.invocation.keyword;
        if (m_error) return errorQuery(request.id, m_error);
        if (request.invocation.arguments.size() > 4096)
            return errorQuery(request.id, {Api::ErrorCode::InvalidArgument, QStringLiteral("Query arguments exceed 4096 characters")});
        try {
            return convertQuery(m_plugin->query({request.id, publicInvocation(request.invocation),
                static_cast<Api::QueryContext>(request.context), m_queryLimit}));
        } catch (...) { return errorQuery(request.id, exceptionError()); }
    }
    void cancelQuery() override
    {
        const quint64 id = std::exchange(m_queryId, 0);
        if (id && m_plugin && !m_error) {
            try { m_plugin->cancelQuery(id); } catch (...) { setError(exceptionError()); }
        }
    }
    PluginExecutionResult execute(const PluginInvocation &call) override
    {
        if (m_error) return failure(m_error);
        const auto invocation = publicInvocation(call);
        if (invocation.arguments.size() > 4096 || invocation.actionId.size() > 128 || !validPluginParameters(invocation.parameters))
            return failure({Api::ErrorCode::InvalidArgument, QStringLiteral("Invalid action parameters")});
        if (m_pending.size() >= Api::MaxPendingExecutions
            || std::find(m_pending.cbegin(), m_pending.cend(), call.commandId) != m_pending.cend())
            return failure({Api::ErrorCode::Busy, QStringLiteral("This plugin command is already running")});
        m_pending.insert(call.operationId, call.commandId);
        try {
            const auto result = convertExecution(m_plugin->execute(invocation));
            if (result.status != PluginExecutionResult::Status::Pending) m_pending.remove(call.operationId);
            return result;
        } catch (...) {
            m_pending.remove(call.operationId);
            return failure(exceptionError());
        }
    }
    void activate(const PluginInvocation &call) override
    {
        m_active = publicInvocation(call);
        if (!m_error) { try { m_plugin->activate(m_active); } catch (...) { setError(exceptionError()); } }
    }
    QWidget *createView(QWidget *parent) override
    {
        if (m_error) return nullptr;
        try { return checkedView(m_plugin->createView(m_active, parent), parent); }
        catch (...) { setError(exceptionError()); return nullptr; }
    }
    QWidget *settingsWidget() override
    {
        if (m_error) return nullptr;
        // 现有设置对话框在接收后接管该临时视图。
        try { return checkedView(m_plugin->createSettingsView(nullptr), nullptr); }
        catch (...) { setError(exceptionError()); return nullptr; }
    }
    bool hasSettingsWidget() const override { return m_manifest.descriptor.hasSettings; }
    void deactivate() override
    {
        if (!m_error) { try { m_plugin->deactivate(); } catch (...) { setError(exceptionError()); } }
    }
    void releaseInactiveResources() override
    {
        if (!m_error) { try { m_plugin->releaseInactiveResources(); } catch (...) { setError(exceptionError()); } }
    }
    PluginLeaveDecision prepareToLeave(PluginLeaveReason reason, quint64 token) override
    {
        if (!m_plugin) return PluginLeaveDecision::Ready;
        m_leaveToken = token;
        try {
            const auto decision = m_plugin->prepareToLeave(static_cast<Api::LeaveReason>(reason), token);
            if (decision != Api::LeaveDecision::Pending) m_leaveToken = 0;
            switch (decision) {
            case Api::LeaveDecision::Ready: return PluginLeaveDecision::Ready;
            case Api::LeaveDecision::Pending: return PluginLeaveDecision::Pending;
            case Api::LeaveDecision::Refused: return PluginLeaveDecision::Refused;
            }
        } catch (...) { setError(exceptionError()); }
        m_leaveToken = 0;
        return PluginLeaveDecision::Refused;
    }

private:
    static Api::Error exceptionError()
    { return {Api::ErrorCode::Internal, QStringLiteral("Plugin raised an exception")}; }
    void setError(const Api::Error &error)
    {
        m_error = error;
        m_error.message.truncate(2048);
        diagnose(m_diagnostics, m_loader.fileName(), m_manifest.descriptor.id, m_error);
    }
    PluginExecutionResult failure(const Api::Error &error) const
    {
        return {PluginExecutionResult::Status::Failed, PluginExecutionResult::HostAction::Keep,
                QStringLiteral("[%1] %2").arg(static_cast<int>(error.code)).arg(error.message.left(2048))};
    }
    PluginQueryResult errorQuery(quint64 id, const Api::Error &error) const
    {
        return {id, {QVariantMap{{QStringLiteral("id"), QStringLiteral("plugin-error")},
            {QStringLiteral("title"), QCoreApplication::translate("ExternalPluginAdapter", "Plugin error")},
            {QStringLiteral("subtitle"), failure(error).error},
            {QStringLiteral("errorCode"), static_cast<int>(error.code)},
            {QStringLiteral("action"), QVariantMap{{QStringLiteral("type"), QStringLiteral("none")}}}}}};
    }
    PluginQueryResult convertQuery(const Api::QueryResult &result) const
    {
        if (result.id != m_queryId)
            return errorQuery(m_queryId, {Api::ErrorCode::InvalidArgument, QStringLiteral("Query returned the wrong request ID")});
        if (result.error) return errorQuery(result.id, result.error);
        if (result.items.size() > m_queryLimit)
            return errorQuery(result.id, {Api::ErrorCode::InvalidArgument, QStringLiteral("Query exceeded the requested result limit")});
        QSet<QString> ids;
        PluginQueryResult converted{result.id, {}, result.pending};
        for (const auto &item : result.items) {
            if (item.id.isEmpty() || item.id.size() > 128 || ids.contains(item.id) || item.title.isEmpty()
                || item.title.size() > 512 || item.subtitle.size() > 2048 || item.icon.size() > 1024
                || item.actionId.size() > 128 || !validPluginParameters(item.parameters))
                return errorQuery(result.id, {Api::ErrorCode::InvalidArgument, QStringLiteral("Invalid query result fields")});
            ids.insert(item.id);
            const QString icon = resolveIcon(item.icon);
            const QVariantMap action = item.actionId.isEmpty()
                ? QVariantMap{{QStringLiteral("type"), QStringLiteral("none")}}
                : QVariantMap{{QStringLiteral("type"), QStringLiteral("external")},
                    {QStringLiteral("plugin"), m_queryCommand}, {QStringLiteral("actionId"), item.actionId},
                    {QStringLiteral("parameters"), item.parameters}};
            converted.items.append(QVariantMap{{QStringLiteral("id"), item.id}, {QStringLiteral("title"), item.title},
                {QStringLiteral("subtitle"), item.subtitle}, {QStringLiteral("icon"), icon}, {QStringLiteral("action"), action}});
        }
        return converted;
    }
    PluginExecutionResult convertExecution(const Api::ExecutionResult &result) const
    {
        if (result.status < Api::Status::Succeeded || result.status > Api::Status::Pending
            || result.hostAction < Api::HostAction::Keep || result.hostAction > Api::HostAction::OpenView)
            return failure({Api::ErrorCode::InvalidArgument, QStringLiteral("Invalid execution status or host action")});
        if (result.error || result.status == Api::Status::Failed)
            return failure(result.error ? result.error : Api::Error{Api::ErrorCode::Internal, QStringLiteral("Plugin operation failed")});
        return {static_cast<PluginExecutionResult::Status>(result.status),
                static_cast<PluginExecutionResult::HostAction>(result.hostAction)};
    }
    // 结果图标只能是包内文件或本插件自己的数据/缓存/临时目录文件，避免任意路径读取。
    QString resolveIcon(const QString &icon) const
    {
        if (icon.isEmpty()) return {};
        const QString id = m_manifest.descriptor.id;
        QStringList roots{QFileInfo(m_loader.fileName()).absolutePath()};
        for (const QString &base : {m_paths.data, m_paths.cache, m_paths.temporary})
            if (!base.isEmpty()) roots.append(QDir(base).filePath(id));
        const QString canonical = QFileInfo(QDir(roots.first()).filePath(icon)).canonicalFilePath();
        if (canonical.isEmpty()) return {};
        for (const QString &root : std::as_const(roots))
            if (inside(canonical, QFileInfo(root).canonicalFilePath())) return canonical;
        return {};
    }
    QWidget *checkedView(QWidget *view, QWidget *parent)
    {
        if (view && (view->parentWidget() != parent || view->thread() != thread())) {
            setError({Api::ErrorCode::InvalidArgument, QStringLiteral("Plugin view must use the supplied parent and GUI thread")});
            return nullptr;
        }
        return view;
    }
    bool callbackAllowed() const
    {
        if (QThread::currentThread() != thread()) {
            qWarning() << "Plugin callback must be posted to the GUI thread";
            return false;
        }
        return !m_stopping;
    }
    QString directory(Api::Directory kind) override
    {
        if (!callbackAllowed()) return {};
        if (kind == Api::Directory::Package) return QFileInfo(m_loader.fileName()).absolutePath();
        QString root;
        switch (kind) {
        case Api::Directory::Data: root = m_paths.data; break;
        case Api::Directory::Cache: root = m_paths.cache; break;
        case Api::Directory::Temporary: root = m_paths.temporary; break;
        default: return {};
        }
        if (root.isEmpty()) return {};
        const QString path = QDir(root).filePath(m_manifest.descriptor.id);
        return QDir().mkpath(path) ? path : QString();
    }
    QSettings *config()
    {
        if (!m_config) {
              const QString path = directory(Api::Directory::Data);
            if (path.isEmpty()) return nullptr;
            m_config = std::make_unique<QSettings>(QDir(path).filePath(QStringLiteral("config.ini")), QSettings::IniFormat);
        }
        return m_config.get();
    }
    QVariant configValue(const QString &key, const QVariant &fallback) override
    {
        if (!callbackAllowed() || key.isEmpty() || key.size() > 128) return fallback;
        auto *settings = config();
        return settings ? settings->value(key, fallback) : fallback;
    }
    Api::Error setConfigValue(const QString &key, const QVariant &value) override
    {
        if (!callbackAllowed() || !validPluginParameters({{key, value}}))
            return {Api::ErrorCode::InvalidArgument, QStringLiteral("Invalid configuration value")};
        auto *settings = config();
        if (!settings) return {Api::ErrorCode::IoError, QStringLiteral("Cannot create plugin data directory")};
        settings->setValue(key, value);
        settings->sync();
        if (settings->status() != QSettings::NoError)
        {
            m_config.reset();
            return {Api::ErrorCode::IoError, QStringLiteral("Cannot save plugin configuration; retry after fixing the data directory")};
        }
        return {};
    }
    void log(Api::LogLevel level, const QString &message) override
    {
        if (!callbackAllowed()) return;
        const auto text = m_manifest.descriptor.id + QStringLiteral(": ") + message.left(2048);
        if (level == Api::LogLevel::Info) qInfo().noquote() << text;
        else qWarning().noquote() << text;
    }
    void publishQuery(const Api::QueryResult &result) override
    {
        if (!callbackAllowed() || !m_queryId || result.id != m_queryId) return;
        emit queryFinished(convertQuery(result));
    }
    void finishExecution(quint64 token, const Api::ExecutionResult &result) override
    {
        if (!callbackAllowed() || !m_pending.contains(token) || result.status == Api::Status::Pending) return;
        m_pending.remove(token);
        emit executionFinished(token, convertExecution(result));
    }
    void finishLeave(quint64 token, bool accepted) override
    {
        if (!callbackAllowed() || !token || token != m_leaveToken) return;
        m_leaveToken = 0;
        emit leaveFinished(token, accepted);
    }
    void closeView() override { if (callbackAllowed()) emit viewCloseRequested(); }

    PluginManifest m_manifest;
    QPluginLoader m_loader;
    PluginPaths m_paths;
    std::shared_ptr<QList<PluginDiagnostic>> m_diagnostics;
    std::unique_ptr<QSettings> m_config;
    Api::Plugin *m_plugin = nullptr;
    Api::Error m_error;
    Api::Invocation m_active;
    QHash<quint64, QString> m_pending;
    quint64 m_queryId = 0;
    quint64 m_leaveToken = 0;
    QString m_queryCommand;
    int m_queryLimit = Api::MaxResults;
    bool m_stopping = false;
};
}

ExternalPluginRegistry::ExternalPluginRegistry(PluginManager &manager, PluginPaths paths)
    : m_manager(manager), m_paths(std::move(paths)), m_diagnostics(std::make_shared<QList<PluginDiagnostic>>()) {}

int ExternalPluginRegistry::discover()
{
    int registered = 0;
    QStringList roots = m_paths.bundledPackages;
    if (!m_paths.packages.isEmpty()) roots.append(m_paths.packages);
    QSet<QString> visitedRoots;
    for (const QString &rootPath : std::as_const(roots)) {
        const QDir root(rootPath);
        const QString canonicalRoot = root.canonicalPath();
        if (canonicalRoot.isEmpty() || visitedRoots.contains(canonicalRoot)) continue;
        visitedRoots.insert(canonicalRoot);
        for (const QFileInfo &package : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QString path = QDir(package.absoluteFilePath()).filePath(QStringLiteral("plugin.dll"));
            auto report = [&](const Api::Error &error, const QString &id = {}) { diagnose(m_diagnostics, path, id, error); };
            if (!inside(QFileInfo(path).canonicalFilePath(), canonicalRoot)
                || !inside(package.canonicalFilePath(), canonicalRoot)) {
                report({Api::ErrorCode::InvalidArgument, QStringLiteral("Package must contain plugin.dll inside its installation directory")});
                continue;
            }
            QPluginLoader metadata(path);
            auto result = parsePluginManifest(metadata.metaData());
            if (result.error) { report(result.error); continue; }
            const QString id = result.manifest.descriptor.id;
            if (id != package.fileName()) {
                report({Api::ErrorCode::InvalidArgument, QStringLiteral("Package directory must match the declared plugin ID")}, id);
                continue;
            }
            const auto manifest = result.manifest;
            const auto paths = m_paths;
            const auto diagnostics = m_diagnostics;
            if (!m_manager.registerPlugin(manifest.descriptor, [manifest, path, paths, diagnostics](QObject *parent) {
                return new ExternalPluginAdapter(manifest, path, paths, diagnostics, parent);
            })) {
                report({Api::ErrorCode::Conflict, QStringLiteral("Plugin identity, command or alias conflicts with an installed plugin")}, id);
                continue;
            }
            ++registered;
        }
    }
    return registered;
}
