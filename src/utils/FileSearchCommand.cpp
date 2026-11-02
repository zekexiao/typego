#include "FileSearchCommand.h"

#include <QFile>

namespace FileSearchCommand {

bool isGlob(const QString &query)
{
    return query.contains(QLatin1Char('*')) || query.contains(QLatin1Char('?'));
}

Command build(const QString &fdProgram, const QString &query, const QString &root, int maxResults)
{
    Command command;
    if (!fdProgram.isEmpty()) {
        // "--" 防止以 "-" 开头的查询被当成选项；fd 默认 smart case 并遵守 .gitignore。
        command.program = fdProgram;
        command.arguments = {QStringLiteral("--absolute-path"), QStringLiteral("--color"), QStringLiteral("never"),
                             QStringLiteral("--max-results"), QString::number(qMax(1, maxResults)),
                             isGlob(query) ? QStringLiteral("--glob") : QStringLiteral("--fixed-strings"),
                             QStringLiteral("--"), query, root};
        return command;
    }

    QString pattern = query;
    if (!isGlob(query)) {
        // 子串匹配时要转义 find 通配符里剩下的特殊字符，否则 "[a]" 会被当成字符集。
        pattern.clear();
        pattern.reserve(query.size() + 4);
        for (const QChar ch : query) {
            if (ch == QLatin1Char('[') || ch == QLatin1Char(']') || ch == QLatin1Char('\\')) pattern += QLatin1Char('\\');
            pattern += ch;
        }
        pattern = QLatin1Char('*') + pattern + QLatin1Char('*');
    }
    // 用 -name 而非 -path 剪掉隐藏目录，避免根目录本身含 "/." 时整棵树被剪掉。
    command.program = QStringLiteral("find");
    command.arguments = {root, QStringLiteral("-mindepth"), QStringLiteral("1"),
                         QStringLiteral("("), QStringLiteral("-name"), QStringLiteral(".*"), QStringLiteral("-prune"),
                         QStringLiteral(")"), QStringLiteral("-o"), QStringLiteral("-iname"), pattern,
                         QStringLiteral("-print")};
    return command;
}

QString parseLine(QByteArray line)
{
    while (line.endsWith('\n') || line.endsWith('\r')) line.chop(1);
    // 新版 fd 会给目录追加分隔符，统一去掉以便按文件名拆分；根目录保持原样。
    while (line.size() > 1 && line.endsWith('/')) line.chop(1);
    return QFile::decodeName(line);
}

} // namespace FileSearchCommand
