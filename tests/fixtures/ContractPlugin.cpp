#include <TypeGoPlugin/Plugin.h>
#include <QTimer>

namespace Api = TypeGo::PluginApi;

// 测试真实 DLL 边界的错误与完成时序，不接触界面或用户数据。
class ContractPlugin final : public QObject, public Api::Plugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID TYPEGO_PLUGIN_IID FILE "typego-plugin.json")
    Q_INTERFACES(TypeGo::PluginApi::Plugin)
public:
    Api::Error initialize(Api::Host *host) override { m_host = host; return {}; }
    Api::QueryResult query(const Api::QueryRequest &request) override
    {
        const QString text = request.invocation.arguments;
        Api::QueryResult result{request.id, {{QStringLiteral("item"), QStringLiteral("Item"), {}, {}, text}}};
        if (text == QLatin1String("wrong-id")) ++result.id;
        if (text == QLatin1String("duplicate")) result.items.append(result.items.first());
        if (text == QLatin1String("invalid-parameters"))
            result.items.first().parameters.insert(QStringLiteral("object"), QVariantMap{{QStringLiteral("nested"), true}});
        if (text == QLatin1String("oversized")) result.items.first().title.fill(QLatin1Char('x'), 513);
        if (text == QLatin1String("inline")) {
            m_host->publishQuery(result);
            m_host->publishQuery(result);
            return {request.id, {}, true};
        }
        if (text == QLatin1String("late")) {
            QTimer::singleShot(0, this, [this, result]() { if (m_host) m_host->publishQuery(result); });
            return {request.id, {}, true};
        }
        return result;
    }
    Api::ExecutionResult execute(const Api::Invocation &call) override
    {
        if (call.actionId == QLatin1String("dirty")) m_dirty = true;
        if (call.actionId == QLatin1String("save")) m_dirty = false;
        if (call.actionId == QLatin1String("inline")) {
            m_host->finishExecution(call.operationId + 100, {Api::Status::Succeeded});
            m_host->finishExecution(call.operationId, {Api::Status::Succeeded});
            m_host->finishExecution(call.operationId, {Api::Status::Succeeded});
            return {Api::Status::Pending};
        }
        return {Api::Status::Succeeded};
    }
    Api::LeaveDecision prepareToLeave(Api::LeaveReason, quint64 token) override
    {
        if (m_dirty) return Api::LeaveDecision::Refused;
        m_host->finishLeave(token + 100, false);
        m_host->finishLeave(token, true);
        m_host->finishLeave(token, false);
        return Api::LeaveDecision::Pending;
    }
    void shutdown() override { m_host = nullptr; }
private:
    Api::Host *m_host = nullptr;
    bool m_dirty = false;
};

#include "ContractPlugin.moc"
