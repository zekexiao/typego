#include <QtTest>

#include "plugins/TerminalLauncher.h"

// shell 插件的终端启动规则。cmd.exe 必须拿到原始命令行，其它终端要按参数数组启动，
// 两种路径搞混会让管道、重定向或带空格的命令被拆碎。
class TstTerminalLauncher : public QObject
{
    Q_OBJECT

private slots:
    void cmdWithPlaceholderUsesNativeArguments();
    void cmdWithoutPlaceholderAppendsCommand();
    void cmdInAbsolutePathStillUsesNativeArguments();
    void isCmdShellMatchesOnlyCmdBaseName();
    void otherTerminalsUseArgumentList();
    void commandWithSpacesStaysOneArgument();
    void missingPlaceholderAppendsCommandToTerminalArgs();
    void quotedTemplateStaysOneArgument();
};

void TstTerminalLauncher::cmdWithPlaceholderUsesNativeArguments()
{
    const TerminalLauncher::Invocation inv =
        TerminalLauncher::build(QStringLiteral("cmd.exe"), QStringLiteral("/C {cmd}"), QStringLiteral("dir /w"));
    QVERIFY(inv.usesNativeArguments);
    QCOMPARE(inv.nativeArguments, QStringLiteral("/C dir /w"));
    QCOMPARE(inv.program, QStringLiteral("cmd.exe"));
    QVERIFY(inv.arguments.isEmpty());
}

void TstTerminalLauncher::cmdWithoutPlaceholderAppendsCommand()
{
    // 用户把模板里的 {cmd} 删掉时，命令仍要接在后面，不能变成空跑。
    const TerminalLauncher::Invocation inv =
        TerminalLauncher::build(QStringLiteral("cmd.exe"), QStringLiteral("/C"), QStringLiteral("echo hi"));
    QVERIFY(inv.usesNativeArguments);
    QCOMPARE(inv.nativeArguments, QStringLiteral("/C echo hi"));
}

void TstTerminalLauncher::cmdInAbsolutePathStillUsesNativeArguments()
{
    const TerminalLauncher::Invocation inv = TerminalLauncher::build(
        QStringLiteral("C:/Windows/System32/cmd.exe"), QStringLiteral("/K {cmd}"), QStringLiteral("ping 127.0.0.1"));
    QVERIFY(inv.usesNativeArguments);
    QCOMPARE(inv.nativeArguments, QStringLiteral("/K ping 127.0.0.1"));
}

void TstTerminalLauncher::isCmdShellMatchesOnlyCmdBaseName()
{
    QVERIFY(TerminalLauncher::isCmdShell(QStringLiteral("cmd.exe")));
    QVERIFY(TerminalLauncher::isCmdShell(QStringLiteral("CMD.EXE")));
    QVERIFY(TerminalLauncher::isCmdShell(QStringLiteral("cmd")));
    // 前缀相同的其它程序不能被误判，否则会被塞进 native 命令行。
    QVERIFY(!TerminalLauncher::isCmdShell(QStringLiteral("cmdbat.exe")));
    QVERIFY(!TerminalLauncher::isCmdShell(QStringLiteral("C:/tools/cmdbat.exe")));
    QVERIFY(!TerminalLauncher::isCmdShell(QStringLiteral("powershell.exe")));
    QVERIFY(!TerminalLauncher::isCmdShell(QString()));
}

void TstTerminalLauncher::otherTerminalsUseArgumentList()
{
    const TerminalLauncher::Invocation inv = TerminalLauncher::build(
        QStringLiteral("powershell.exe"), QStringLiteral("-NoProfile -NonInteractive -Command {cmd}"),
        QStringLiteral("Get-Date"));
    QVERIFY(!inv.usesNativeArguments);
    const QStringList expected{QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                               QStringLiteral("-Command"), QStringLiteral("Get-Date")};
    QCOMPARE(inv.arguments, expected);
}

void TstTerminalLauncher::commandWithSpacesStaysOneArgument()
{
    // 管道和空格必须整体作为一个参数交给 -Command，否则后半截会被当成程序参数。
    const TerminalLauncher::Invocation inv = TerminalLauncher::build(
        QStringLiteral("powershell.exe"), QStringLiteral("-Command {cmd}"),
        QStringLiteral("Get-Process | Sort-Object CPU"));
    QCOMPARE(inv.arguments.size(), 2);
    QCOMPARE(inv.arguments.at(0), QStringLiteral("-Command"));
    QCOMPARE(inv.arguments.at(1), QStringLiteral("Get-Process | Sort-Object CPU"));
}

void TstTerminalLauncher::missingPlaceholderAppendsCommandToTerminalArgs()
{
    const TerminalLauncher::Invocation inv = TerminalLauncher::build(
        QStringLiteral("bash.exe"), QStringLiteral("-lc"), QStringLiteral("ls -al"));
    QVERIFY(!inv.usesNativeArguments);
    const QStringList expected{QStringLiteral("-lc"), QStringLiteral("ls -al")};
    QCOMPARE(inv.arguments, expected);
}

void TstTerminalLauncher::quotedTemplateStaysOneArgument()
{
    // splitCommand 语义：模板里被引号包住的参数不因空格拆开。
    const TerminalLauncher::Invocation inv = TerminalLauncher::build(
        QStringLiteral("bash.exe"), QStringLiteral("--init-file \"my profile.sh\" -lc {cmd}"),
        QStringLiteral("echo 1"));
    const QStringList expected{QStringLiteral("--init-file"), QStringLiteral("my profile.sh"),
                               QStringLiteral("-lc"), QStringLiteral("echo 1")};
    QCOMPARE(inv.arguments, expected);
}

QTEST_GUILESS_MAIN(TstTerminalLauncher)

#include "tst_terminal_launcher.moc"
