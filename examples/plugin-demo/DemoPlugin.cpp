#include <TypeGoPlugin/Plugin.h>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QSaveFile>
#include <QTextEdit>
#include <QTextDocument>
#include <QTimer>
#include <utility>

namespace Api = TypeGo::PluginApi;

class DemoPlugin final : public QObject, public Api::Plugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID TYPEGO_PLUGIN_IID FILE "typego-plugin.json")
    Q_INTERFACES(TypeGo::PluginApi::Plugin)
public:
    DemoPlugin()
    {
        m_queryTimer.setSingleShot(true);
        connect(&m_queryTimer, &QTimer::timeout, this, [this]() {
            if (m_host) m_host->publishQuery(result(m_queryId, QStringLiteral("Asynchronous result")));
        });
        m_jobTimer.setSingleShot(true);
        connect(&m_jobTimer, &QTimer::timeout, this, [this]() {
            if (!m_host) return;
            m_host->finishExecution(std::exchange(m_jobToken, 0), {Api::Status::Succeeded});
            if (m_leaveToken) m_host->finishLeave(std::exchange(m_leaveToken, 0), true);
        });
    }
    Api::Error initialize(Api::Host *host) override
    {
        m_host = host;
        m_host->log(Api::LogLevel::Info, QStringLiteral("Example initialized on first use"));
        return {};
    }
    Api::QueryResult query(const Api::QueryRequest &request) override
    {
        m_queryId = request.id;
        if (request.invocation.arguments == QLatin1String("async")) {
            m_queryTimer.start(0);
            return {request.id, {}, true};
        }
        return result(request.id, request.invocation.arguments.isEmpty()
            ? QStringLiteral("Hello from the independent SDK") : request.invocation.arguments);
    }
    void cancelQuery(quint64 id) override
    { if (id == m_queryId) { m_queryTimer.stop(); m_queryId = 0; } }
    Api::ExecutionResult execute(const Api::Invocation &call) override
    {
        if (call.commandId == QLatin1String("job")) {
            m_jobToken = call.operationId;
            m_jobTimer.start(20);
            return {Api::Status::Pending};
        }
        if (call.actionId != QLatin1String("remember"))
            return {Api::Status::Failed, Api::HostAction::Keep,
                {Api::ErrorCode::InvalidArgument, QStringLiteral("Select a search result first")}};
        const auto error = m_host->setConfigValue(QStringLiteral("lastText"), call.parameters.value(QStringLiteral("text")));
        return {error ? Api::Status::Failed : Api::Status::Succeeded, Api::HostAction::Keep, error};
    }
    QWidget *createView(const Api::Invocation &, QWidget *parent) override
    {
        auto *editor = new QTextEdit(parent);
        QFile file(notePath());
        if (file.open(QIODevice::ReadOnly)) editor->setPlainText(QString::fromUtf8(file.readAll()));
        editor->document()->setModified(false);
        m_editor = editor;
        return editor;
    }
    Api::LeaveDecision prepareToLeave(Api::LeaveReason, quint64 token) override
    {
        if (m_editor && m_editor->document()->isModified()) {
            const QString path = notePath();
            QSaveFile file(path);
            const QByteArray text = m_editor->toPlainText().toUtf8();
            if (path.isEmpty() || !file.open(QIODevice::WriteOnly)
                || file.write(text) != text.size() || !file.commit()) {
                m_host->log(Api::LogLevel::Error, QStringLiteral("Cannot save editor content; keep the window open and retry"));
                return Api::LeaveDecision::Refused;
            }
            m_editor->document()->setModified(false);
        }
        if (m_jobToken) { m_leaveToken = token; return Api::LeaveDecision::Pending; }
        return Api::LeaveDecision::Ready;
    }
    void shutdown() override
    {
        m_queryTimer.stop(); m_jobTimer.stop();
        m_host = nullptr;
        m_queryId = m_jobToken = m_leaveToken = 0;
    }
private:
    Api::QueryResult result(quint64 id, const QString &text) const
    {
        return {id, {{QStringLiteral("echo"), text.left(512), QStringLiteral("Enter to remember this text in plugin config"),
            {}, QStringLiteral("remember"), {{QStringLiteral("text"), text.left(4096)}}}}};
    }
    QString notePath() const
    {
        const QString root = m_host->directory(Api::Directory::Data);
        return root.isEmpty() ? QString() : QDir(root).filePath(QStringLiteral("example.txt"));
    }
    Api::Host *m_host = nullptr;
    QPointer<QTextEdit> m_editor;
    QTimer m_queryTimer, m_jobTimer;
    quint64 m_queryId = 0, m_jobToken = 0, m_leaveToken = 0;
};

#include "DemoPlugin.moc"
