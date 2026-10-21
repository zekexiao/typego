#include "TerminalLauncher.h"

#include <QFileInfo>
#include <QProcess>

namespace TerminalLauncher {

namespace {
// 模板里代表待执行命令的占位符。
constexpr const char kPlaceholder[] = "{cmd}";
}

bool isCmdShell(const QString &program)
{
    return QFileInfo(program).completeBaseName().compare(QStringLiteral("cmd"), Qt::CaseInsensitive) == 0;
}

Invocation build(const QString &program, const QString &argumentsTemplate, const QString &command)
{
    Invocation invocation;
    invocation.program = program;

#ifdef Q_OS_WIN
    // cmd.exe 需要 Windows 原始命令行语义，管道和重定向不能被拆成多个参数。
    if (isCmdShell(program)) {
        QString argsText = argumentsTemplate;
        if (argsText.contains(QLatin1String(kPlaceholder))) {
            argsText.replace(QLatin1String(kPlaceholder), command);
        } else {
            argsText += QLatin1Char(' ') + command;
        }
        invocation.nativeArguments = argsText;
        invocation.usesNativeArguments = true;
        return invocation;
    }
#endif

    QStringList arguments = QProcess::splitCommand(argumentsTemplate);
    bool replacedPlaceholder = false;
    for (QString &argument : arguments) {
        if (!argument.contains(QLatin1String(kPlaceholder))) continue;
        argument.replace(QLatin1String(kPlaceholder), command);
        replacedPlaceholder = true;
    }
    // 没有占位符时把命令追加到末尾，保证 -c / -Command 之类的开关仍能接上命令。
    if (!replacedPlaceholder) arguments.append(command);
    invocation.arguments = arguments;
    return invocation;
}

} // namespace TerminalLauncher
