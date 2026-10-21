#pragma once

#include <QString>
#include <QStringList>

// shell 插件启动终端的命令拼装规则。
// cmd.exe 与其它终端走的是两条不同路径，规则一旦改错会把命令拆坏，
// 因此把判定与拼装从 QProcess/窗口里抽出来，供测试直接验证。
namespace TerminalLauncher {

struct Invocation {
    QString program;
    QStringList arguments;         // 常规路径：按参数数组启动
    QString nativeArguments;       // cmd.exe 路径：Windows 原始命令行
    bool usesNativeArguments = false;
};

/// 终端程序是否为 Windows 的 cmd.exe（含绝对路径与任意大小写）。
bool isCmdShell(const QString &program);

/// 按模板拼装一次启动参数；command 需为已去空白的命令行。
/// 模板里没有占位符时，cmd 走尾部追加，其它终端把命令作为最后一个参数。
Invocation build(const QString &program, const QString &argumentsTemplate, const QString &command);

} // namespace TerminalLauncher
