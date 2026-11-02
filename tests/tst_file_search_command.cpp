#include <QtTest>

#include "utils/FileSearchCommand.h"

// 非 Windows 文件搜索的命令拼装：fd 与 find 两条路径都要把查询当成一个整体参数，
// 子串查询里的通配符特殊字符必须转义，隐藏目录要被剪掉。
class TstFileSearchCommand : public QObject
{
    Q_OBJECT

private slots:
    void detectsGlob();
    void fdUsesFixedStringsForPlainQuery();
    void fdUsesGlobForWildcardQuery();
    void fdClampsMaxResults();
    void findWrapsPlainQueryAsSubstring();
    void findEscapesCharacterClass();
    void findKeepsGlobQuery();
    void parseLineStripsNewlineAndTrailingSlash();
};

void TstFileSearchCommand::detectsGlob()
{
    QVERIFY(FileSearchCommand::isGlob(QStringLiteral("*.txt")));
    QVERIFY(FileSearchCommand::isGlob(QStringLiteral("a?c")));
    QVERIFY(!FileSearchCommand::isGlob(QStringLiteral("report [v2]")));
}

void TstFileSearchCommand::fdUsesFixedStringsForPlainQuery()
{
    const auto cmd = FileSearchCommand::build(QStringLiteral("/usr/bin/fdfind"), QStringLiteral("-my file"),
                                              QStringLiteral("/home/u"), 21);
    QCOMPARE(cmd.program, QStringLiteral("/usr/bin/fdfind"));
    const QStringList expected{QStringLiteral("--absolute-path"), QStringLiteral("--color"), QStringLiteral("never"),
                               QStringLiteral("--max-results"), QStringLiteral("21"), QStringLiteral("--fixed-strings"),
                               QStringLiteral("--"), QStringLiteral("-my file"), QStringLiteral("/home/u")};
    QCOMPARE(cmd.arguments, expected);
}

void TstFileSearchCommand::fdUsesGlobForWildcardQuery()
{
    const auto cmd = FileSearchCommand::build(QStringLiteral("fd"), QStringLiteral("*.md"), QStringLiteral("/h"), 5);
    QVERIFY(cmd.arguments.contains(QStringLiteral("--glob")));
    QVERIFY(!cmd.arguments.contains(QStringLiteral("--fixed-strings")));
    QCOMPARE(cmd.arguments.at(cmd.arguments.size() - 2), QStringLiteral("*.md"));
}

void TstFileSearchCommand::fdClampsMaxResults()
{
    const auto cmd = FileSearchCommand::build(QStringLiteral("fd"), QStringLiteral("a"), QStringLiteral("/h"), 0);
    QCOMPARE(cmd.arguments.at(cmd.arguments.indexOf(QStringLiteral("--max-results")) + 1), QStringLiteral("1"));
}

void TstFileSearchCommand::findWrapsPlainQueryAsSubstring()
{
    const auto cmd = FileSearchCommand::build(QString(), QStringLiteral("my notes"), QStringLiteral("/home/u"), 10);
    QCOMPARE(cmd.program, QStringLiteral("find"));
    const QStringList expected{QStringLiteral("/home/u"), QStringLiteral("-mindepth"), QStringLiteral("1"),
                               QStringLiteral("("), QStringLiteral("-name"), QStringLiteral(".*"),
                               QStringLiteral("-prune"), QStringLiteral(")"), QStringLiteral("-o"),
                               QStringLiteral("-iname"), QStringLiteral("*my notes*"), QStringLiteral("-print")};
    QCOMPARE(cmd.arguments, expected);
}

void TstFileSearchCommand::findEscapesCharacterClass()
{
    const auto cmd = FileSearchCommand::build(QString(), QStringLiteral("a[1]\\b"), QStringLiteral("/h"), 10);
    QCOMPARE(cmd.arguments.at(cmd.arguments.indexOf(QStringLiteral("-iname")) + 1),
             QStringLiteral("*a\\[1\\]\\\\b*"));
}

void TstFileSearchCommand::findKeepsGlobQuery()
{
    const auto cmd = FileSearchCommand::build(QString(), QStringLiteral("*.pdf"), QStringLiteral("/h"), 10);
    QCOMPARE(cmd.arguments.at(cmd.arguments.indexOf(QStringLiteral("-iname")) + 1), QStringLiteral("*.pdf"));
}

void TstFileSearchCommand::parseLineStripsNewlineAndTrailingSlash()
{
    QCOMPARE(FileSearchCommand::parseLine("/home/u/docs/\n"), QStringLiteral("/home/u/docs"));
    QCOMPARE(FileSearchCommand::parseLine("/home/u/a b.txt\r\n"), QStringLiteral("/home/u/a b.txt"));
    QCOMPARE(FileSearchCommand::parseLine("/home/u/\xe4\xb8\xad.txt"), QString::fromUtf8("/home/u/中.txt"));
    QCOMPARE(FileSearchCommand::parseLine("/\n"), QStringLiteral("/"));
    QVERIFY(FileSearchCommand::parseLine("\n").isEmpty());
}

QTEST_GUILESS_MAIN(TstFileSearchCommand)
#include "tst_file_search_command.moc"
