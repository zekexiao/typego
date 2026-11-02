#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// 非 Windows 平台的文件搜索：优先 fd（Debian/Ubuntu 上叫 fdfind），没有时退回 POSIX find。
// 命令拼装与输出解析抽成纯函数，测试不依赖外部程序。
namespace FileSearchCommand {

struct Command {
    QString program;
    QStringList arguments;
};

/// 查询里含 * 或 ? 时按通配符匹配文件名，否则按子串匹配。
bool isGlob(const QString &query);

/// fdProgram 为空表示没有 fd，改用 find；两者都跳过隐藏目录。
/// maxResults 只约束 fd，find 由调用方读够条数后结束进程。
Command build(const QString &fdProgram, const QString &query, const QString &root, int maxResults);

/// 解析一行输出为路径：去掉换行和目录尾部的分隔符，空行返回空串。
QString parseLine(QByteArray line);

} // namespace FileSearchCommand
