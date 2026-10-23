#include "ShellPlugin.h"

#include "TerminalLauncher.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QVariantMap>
#include <QVBoxLayout>

namespace {
constexpr const char *ShellProgramKey = "plugins/shell/terminalProgram";
constexpr const char *ShellArgumentsKey = "plugins/shell/terminalArguments";

QString defaultTerminalProgram()
{
#ifdef Q_OS_WIN
    return QStringLiteral("cmd.exe");
#else
    return QStringLiteral("/bin/sh");
#endif
}

QString defaultTerminalArgumentsTemplate()
{
#ifdef Q_OS_WIN
    return QStringLiteral("/C {cmd}");
#else
    return QStringLiteral("-c {cmd}");
#endif
}

QString terminalDisplayName(const QString &program, const QString &argumentsTemplate)
{
    return program + QLatin1Char(' ') + argumentsTemplate;
}
}

class ShellView final : public QWidget {
public:
    explicit ShellView(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_title(new QLabel(this))
        , m_status(new QLabel(this))
        , m_output(new QPlainTextEdit(this))
        , m_runButton(new QPushButton(QObject::tr("Run"), this))
        , m_copyButton(new QPushButton(QObject::tr("Copy"), this))
        , m_process(new QProcess(this))
    {
        setObjectName(QStringLiteral("shellView"));
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);

        m_title->setObjectName(QStringLiteral("pageTitle"));
        m_status->setObjectName(QStringLiteral("muted"));
        m_output->setObjectName(QStringLiteral("shellOutput"));
        m_output->setReadOnly(true);
        m_output->setMaximumBlockCount(2000);
        m_output->setLineWrapMode(QPlainTextEdit::NoWrap);

        auto *actions = new QHBoxLayout;
        actions->setSpacing(8);
        m_runButton->setObjectName(QStringLiteral("primary"));
        actions->addWidget(m_runButton);
        actions->addWidget(m_copyButton);
        actions->addStretch();

        layout->addWidget(m_title);
        layout->addWidget(m_status);
        layout->addWidget(m_output, 1);
        layout->addLayout(actions);

        connect(m_runButton, &QPushButton::clicked, this, [this]() { run(); });
        connect(m_copyButton, &QPushButton::clicked, this, [this]() {
            QApplication::clipboard()->setText(m_output->toPlainText());
        });
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
            m_output->appendPlainText(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
        });
        connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
            m_output->appendPlainText(QString::fromLocal8Bit(m_process->readAllStandardError()));
        });
        connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
            m_runButton->setEnabled(true);
            m_status->setText(status == QProcess::NormalExit
                                  ? QObject::tr("Finished with exit code %1").arg(code)
                                  : QObject::tr("Process crashed"));
        });
        connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            m_runButton->setEnabled(m_process->state() == QProcess::NotRunning);
            m_status->setText(QObject::tr("Cannot run command: %1").arg(m_process->errorString()));
        });
    }

    void setTerminal(const QString &program, const QString &argumentsTemplate)
    {
        m_terminalProgram = program.trimmed().isEmpty() ? defaultTerminalProgram() : program.trimmed();
        m_terminalArgumentsTemplate = argumentsTemplate.trimmed().isEmpty()
            ? defaultTerminalArgumentsTemplate()
            : argumentsTemplate.trimmed();
    }

    // 视图销毁前由插件调用，避免后台命令成为孤儿进程。
    void stopProcess()
    {
        if (m_process->state() == QProcess::NotRunning) return;
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    void setCommand(const QString &command)
    {
        if (m_command == command.trimmed()) return;
        m_command = command.trimmed();
        m_title->setText(m_command.isEmpty() ? QObject::tr("Shell") : m_command);
        m_output->clear();
        if (m_command.isEmpty()) {
            m_status->setText(QObject::tr("Enter a command after shell or cmd"));
            m_runButton->setEnabled(false);
            return;
        }
        m_runButton->setEnabled(true);
        run();
    }

private:
    void run()
    {
        if (m_command.isEmpty() || m_process->state() != QProcess::NotRunning) return;
        m_output->clear();
        m_status->setText(QObject::tr("Running with %1").arg(terminalDisplayName(m_terminalProgram, m_terminalArgumentsTemplate)));
        m_runButton->setEnabled(false);
        // cmd.exe 使用 Windows 原始命令行；其他终端将整条命令作为独立参数。
        // 两条路径的拼装规则集中在 TerminalLauncher，便于脱离窗口做单元测试。
        const TerminalLauncher::Invocation invocation =
            TerminalLauncher::build(m_terminalProgram, m_terminalArgumentsTemplate, m_command);
#ifdef Q_OS_WIN
        if (invocation.usesNativeArguments) {
            m_process->setNativeArguments(invocation.nativeArguments);
            m_process->start(invocation.program, QStringList());
            return;
        }
        m_process->setNativeArguments(QString());
#endif
        m_process->start(invocation.program, invocation.arguments);
    }

    QLabel *m_title;
    QLabel *m_status;
    QPlainTextEdit *m_output;
    QPushButton *m_runButton;
    QPushButton *m_copyButton;
    QProcess *m_process;
    QString m_command;
    QString m_terminalProgram = defaultTerminalProgram();
    QString m_terminalArgumentsTemplate = defaultTerminalArgumentsTemplate();
};

ShellPlugin::ShellPlugin(QObject *parent)
    : IPlugin(parent)
{
}

QVariantList ShellPlugin::query(const QString &args)
{
    return query(args, QueryContext::Auto);
}

QVariantList ShellPlugin::query(const QString &args, QueryContext context)
{
    Q_UNUSED(context)
    QVariantMap item;
    const QString commandLine = args.trimmed();
    item["title"] = commandLine.isEmpty() ? tr("Enter a shell command") : tr("Run \"%1\"").arg(commandLine);
    item["subtitle"] = commandLine.isEmpty() ? tr("Example: shell dir") : tr("Press Enter to open the command output view");
    item["icon"] = "";
    QVariantMap action;
    action["type"] = QStringLiteral("none");
    action["plugin"] = command();
    item["action"] = action;
    return QVariantList() << item;
}

PluginExecutionResult ShellPlugin::execute(const PluginInvocation &invocation)
{
    // 与旧适配层语义一致：占位项执行成功即隐藏窗口，真正的打开视图由宿主处理。
    Q_UNUSED(invocation)
    return {PluginExecutionResult::Status::Succeeded, PluginExecutionResult::HostAction::Hide};
}
IPlugin::LaunchMode ShellPlugin::launchMode(const QString &args) const
{
    return args.trimmed().isEmpty() ? LaunchMode::List : LaunchMode::QueryView;
}

void ShellPlugin::activateQuery(const QString &args)
{
    m_query = args;
    if (m_view) {
        m_view->setTerminal(terminalProgram(), terminalArgumentsTemplate());
        m_view->setCommand(m_query);
    }
}

QWidget *ShellPlugin::createView(QWidget *parent)
{
    if (!m_view) {
        m_view = new ShellView(parent);
        connect(m_view, &QObject::destroyed, this, [this]() { m_view = nullptr; });
    }
    m_view->setTerminal(terminalProgram(), terminalArgumentsTemplate());
    m_view->setCommand(m_query);
    return m_view;
}

void ShellPlugin::releaseInactiveResources()
{
    // 离开插件即终止未完成的命令并销毁视图。
    if (!m_view) return;
    m_view->stopProcess();
    m_view->deleteLater();
}

QWidget *ShellPlugin::settingsWidget()
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto *title = new QLabel(tr("Shell command runner"));
    title->setObjectName(QStringLiteral("section"));
    auto *note = new QLabel(tr("Choose the terminal command used to run shell queries. Use {cmd} where the typed command should be inserted."));
    note->setObjectName(QStringLiteral("muted"));
    note->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(note);

    auto *preset = new QComboBox(widget);
    preset->addItem(tr("Command Prompt (cmd.exe /C)"), QStringList{QStringLiteral("cmd.exe"), QStringLiteral("/C {cmd}")});
    preset->addItem(tr("PowerShell"), QStringList{QStringLiteral("powershell.exe"), QStringLiteral("-NoProfile -NonInteractive -Command {cmd}")});
    preset->addItem(tr("Git Bash"), QStringList{QStringLiteral("bash.exe"), QStringLiteral("-lc {cmd}")});
    preset->addItem(tr("Custom"), QStringList{QString(), QString()});

    auto *programEdit = new QLineEdit(terminalProgram(), widget);
    auto *argsEdit = new QLineEdit(terminalArgumentsTemplate(), widget);
    programEdit->setPlaceholderText(tr("cmd.exe"));
    argsEdit->setPlaceholderText(tr("/C {cmd}"));

    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(tr("Preset"), preset);
    form->addRow(tr("Program"), programEdit);
    form->addRow(tr("Arguments"), argsEdit);
    layout->addLayout(form);

    auto save = [this, programEdit, argsEdit]() {
        setTerminalConfig(programEdit->text(), argsEdit->text());
        if (m_view) m_view->setTerminal(terminalProgram(), terminalArgumentsTemplate());
    };
    connect(programEdit, &QLineEdit::editingFinished, widget, save);
    connect(argsEdit, &QLineEdit::editingFinished, widget, save);
    connect(preset, &QComboBox::currentIndexChanged, widget, [programEdit, argsEdit, preset, save](int index) {
        const QStringList data = preset->itemData(index).toStringList();
        if (data.size() != 2 || data.at(0).isEmpty()) return;
        programEdit->setText(data.at(0));
        argsEdit->setText(data.at(1));
        save();
    });

    layout->addStretch();
    return widget;
}

void ShellPlugin::loadTerminalConfig() const
{
    if (m_configLoaded) return;
    QSettings settings(QStringLiteral("TypeGo"), QStringLiteral("TypeGo"));
    m_terminalProgram = settings.value(QString::fromLatin1(ShellProgramKey), defaultTerminalProgram()).toString();
    m_terminalArgumentsTemplate =
        settings.value(QString::fromLatin1(ShellArgumentsKey), defaultTerminalArgumentsTemplate()).toString();
    m_configLoaded = true;
}

QString ShellPlugin::terminalProgram() const
{
    loadTerminalConfig();
    return m_terminalProgram;
}

QString ShellPlugin::terminalArgumentsTemplate() const
{
    loadTerminalConfig();
    return m_terminalArgumentsTemplate;
}

void ShellPlugin::setTerminalConfig(const QString &program, const QString &argumentsTemplate)
{
    loadTerminalConfig();
    m_terminalProgram = program.trimmed().isEmpty() ? defaultTerminalProgram() : program.trimmed();
    m_terminalArgumentsTemplate = argumentsTemplate.trimmed().isEmpty()
        ? defaultTerminalArgumentsTemplate()
        : argumentsTemplate.trimmed();
    QSettings settings(QStringLiteral("TypeGo"), QStringLiteral("TypeGo"));
    settings.setValue(QString::fromLatin1(ShellProgramKey), m_terminalProgram);
    settings.setValue(QString::fromLatin1(ShellArgumentsKey), m_terminalArgumentsTemplate);
}
