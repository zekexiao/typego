#include <QtTest>

#include <algorithm>

#include "utils/AppMatcher.h"

// 应用搜索的匹配与排序规则。打分档位只锁相对顺序，不锁具体数值之外的实现细节。
class TstAppMatcher : public QObject
{
    Q_OBJECT

private:
    struct Fixture {
        QString name;
        QString path;
        QString desc;
    };

    static int scoreOf(const QString &name, const QString &desc, const QString &query)
    {
        const AppMatcher::Entry entry = AppMatcher::buildEntry(name, QStringLiteral("C:/x/l.lnk"), desc);
        const AppMatcher::Match match = AppMatcher::evaluate(entry, query);
        return match.matched ? match.score : 0;
    }

    // 按查询给一组条目排序，返回名称序列，用于验证相对顺序而不是绝对分数。
    static QStringList rank(const QList<Fixture> &fixtures, const QString &query)
    {
        struct Node {
            AppMatcher::Entry entry;
            int score;
            QString name;
        };
        QVector<Node> nodes;
        for (const Fixture &f : fixtures) {
            const AppMatcher::Entry entry = AppMatcher::buildEntry(f.name, f.path, f.desc);
            const AppMatcher::Match match = AppMatcher::evaluate(entry, query);
            if (match.matched) nodes.append(Node{entry, match.score, f.name});
        }
        std::sort(nodes.begin(), nodes.end(), [](const Node &a, const Node &b) {
            return AppMatcher::ranksBefore(a.entry, a.score, b.entry, b.score);
        });
        QStringList names;
        for (const Node &node : nodes) names.append(node.name);
        return names;
    }

private slots:
    void buildEntryDerivesAllSearchKeys();
    void tokensSplitOnNonAlphanumeric();
    void eachTierScores();
    void asciiNamesSkipPinyinKeys();
    void strongestTierWins();
    void emptyQueryMatchesEverythingWithZeroScore();
    void singleCharacterQuerySkipsPinyin();
    void nonMatchingQueryIsNotReturned();
    void rankingPutsStrongerMatchesFirst();
    void tiesBreakOnNameLengthThenLexicalThenPath();
    void comparatorIsStrictWeakOrder();
};

void TstAppMatcher::buildEntryDerivesAllSearchKeys()
{
    const AppMatcher::Entry entry = AppMatcher::buildEntry(
        QStringLiteral("计算器"), QStringLiteral("C:/Start Menu/计算器.lnk"), QStringLiteral("Windows 工具"));
    QCOMPARE(entry.lowerName, QStringLiteral("计算器"));
    QCOMPARE(entry.lowerPath, QStringLiteral("c:/start menu/计算器.lnk"));
    // 拼音键在扫描期算好，搜索期只做字符串比较。
    QCOMPARE(entry.pinyin, QStringLiteral("jisuanqi"));
    QCOMPARE(entry.acronym, QStringLiteral("jsq"));
}

void TstAppMatcher::tokensSplitOnNonAlphanumeric()
{
    const QStringList visual{QStringLiteral("visual"), QStringLiteral("studio"), QStringLiteral("code")};
    const QStringList dbeaver{QStringLiteral("dbeaver"), QStringLiteral("23"), QStringLiteral("0")};
    const QStringList dashed{QStringLiteral("my"), QStringLiteral("app"), QStringLiteral("2")};
    const QStringList han{QStringLiteral("焊接助手")};

    QCOMPARE(AppMatcher::buildEntry(QStringLiteral("Visual Studio Code"), QString(), QString()).tokens, visual);
    QCOMPARE(AppMatcher::buildEntry(QStringLiteral("DBeaver 23.0"), QString(), QString()).tokens, dbeaver);
    QCOMPARE(AppMatcher::buildEntry(QStringLiteral("My_App-2"), QString(), QString()).tokens, dashed);
    // 连续汉字不分词，整个名称是一个 token。
    QCOMPARE(AppMatcher::buildEntry(QStringLiteral("焊接助手"), QString(), QString()).tokens, han);
    QCOMPARE(AppMatcher::buildEntry(QStringLiteral("   "), QString(), QString()).tokens, QStringList{});
}

void TstAppMatcher::eachTierScores()
{
    QCOMPARE(scoreOf(QStringLiteral("cmd"), QString(), QStringLiteral("cmd")), AppMatcher::ExactName);
    QCOMPARE(scoreOf(QStringLiteral("Command Prompt"), QString(), QStringLiteral("com")), AppMatcher::NamePrefix);
    QCOMPARE(scoreOf(QStringLiteral("Microsoft Edge"), QString(), QStringLiteral("edge")), AppMatcher::TokenPrefix);
    QCOMPARE(scoreOf(QStringLiteral("计算器"), QString(), QStringLiteral("jisuan")), AppMatcher::PinyinPrefix);
    QCOMPARE(scoreOf(QStringLiteral("计算器"), QString(), QStringLiteral("jsq")), AppMatcher::AcronymPrefix);
    QCOMPARE(scoreOf(QStringLiteral("计算器"), QString(), QStringLiteral("suan")), AppMatcher::PinyinContains);
    QCOMPARE(scoreOf(QStringLiteral("计算器"), QString(), QStringLiteral("sq")), AppMatcher::AcronymContains);
    QCOMPARE(scoreOf(QStringLiteral("Visual Studio"), QString(), QStringLiteral("studio")), AppMatcher::TokenPrefix);
    QCOMPARE(scoreOf(QStringLiteral("Notepad++"), QString(), QStringLiteral("pad")), AppMatcher::NameContains);
    QCOMPARE(scoreOf(QStringLiteral("Tool"), QStringLiteral("Windows Settings"), QStringLiteral("window")),
             AppMatcher::DescPrefix);
    QCOMPARE(scoreOf(QStringLiteral("Tool"), QStringLiteral("Windows Settings"), QStringLiteral("dows")),
             AppMatcher::DescContains);
}

void TstAppMatcher::asciiNamesSkipPinyinKeys()
{
    // 纯英文名的「拼音」就等于它的小写名，存下来只会让名称包含被抬成拼音包含这一档。
    const AppMatcher::Entry ascii = AppMatcher::buildEntry(
        QStringLiteral("Notepad++"), QStringLiteral("C:/n.lnk"), QString());
    QVERIFY(ascii.pinyin.isEmpty());
    QVERIFY(ascii.acronym.isEmpty());
    QCOMPARE(scoreOf(QStringLiteral("Notepad++"), QString(), QStringLiteral("pad")), AppMatcher::NameContains);

    // 含汉字的名称仍要建键，中英混合也一样。
    const AppMatcher::Entry han = AppMatcher::buildEntry(QStringLiteral("编辑器"), QString(), QString());
    QCOMPARE(han.pinyin, QStringLiteral("bianjiqi"));
    QCOMPARE(han.acronym, QStringLiteral("bjq"));
}

void TstAppMatcher::strongestTierWins()
{
    // 名称精确同时满足前缀、包含、词首，必须取最高档而不是先命中的那档。
    QCOMPARE(scoreOf(QStringLiteral("Code"), QStringLiteral("Code editor"), QStringLiteral("code")),
             AppMatcher::ExactName);
    QCOMPARE(scoreOf(QStringLiteral("Code"), QString(), QStringLiteral("cod")), AppMatcher::NamePrefix);
    // 描述也算命中时不能把名称级压下去。
    QCOMPARE(scoreOf(QStringLiteral("Code"), QStringLiteral("code compiler"), QStringLiteral("code")),
             AppMatcher::ExactName);
}

void TstAppMatcher::emptyQueryMatchesEverythingWithZeroScore()
{
    const AppMatcher::Entry entry = AppMatcher::buildEntry(QStringLiteral("记事本"), QString(), QString());
    const AppMatcher::Match match = AppMatcher::evaluate(entry, QString());
    QVERIFY(match.matched);
    QCOMPARE(match.score, 0);
}

void TstAppMatcher::singleCharacterQuerySkipsPinyin()
{
    // 一个字母能命中上百条拼音/首字母，必须当次不匹配；英文名称仍要能前缀命中。
    QCOMPARE(scoreOf(QStringLiteral("计算器"), QString(), QStringLiteral("j")), 0);
    QCOMPARE(scoreOf(QStringLiteral("记事本"), QString(), QStringLiteral("j")), 0);
    QCOMPARE(scoreOf(QStringLiteral("Java"), QString(), QStringLiteral("j")), AppMatcher::NamePrefix);
    QCOMPARE(scoreOf(QStringLiteral("计算器"), QString(), QStringLiteral("ji")), AppMatcher::PinyinPrefix);
    // 描述是英文时单字符仍能命中描述档位，这条不受拼音开关影响。
    QCOMPARE(scoreOf(QStringLiteral("计算器"), QStringLiteral("Note"), QStringLiteral("n")), AppMatcher::DescPrefix);
}

void TstAppMatcher::nonMatchingQueryIsNotReturned()
{
    const Fixture calc{QStringLiteral("计算器"), QStringLiteral("C:/s/计算器.lnk"), QString()};
    const QList<Fixture> fixtures{calc};
    // 未命中的查询不应出现在排序结果里。
    QVERIFY(rank(fixtures, QStringLiteral("xyz")).isEmpty());
    QCOMPARE(rank(fixtures, QStringLiteral("jsq")).size(), 1);
}

void TstAppMatcher::rankingPutsStrongerMatchesFirst()
{
    const QList<Fixture> fixtures{
        {QStringLiteral("Visual Studio Code"), QStringLiteral("C:/s/vsc.lnk"), QString()},
        {QStringLiteral("Code"), QStringLiteral("C:/s/code.lnk"), QString()},
        {QStringLiteral("Notepad"), QStringLiteral("C:/s/notepad.lnk"), QStringLiteral("code editor")},
    };
    // 名称精确 > 名称前缀 > 词前缀 > 描述命中。
    const QStringList codeOrder{QStringLiteral("Code"), QStringLiteral("Visual Studio Code"),
                                QStringLiteral("Notepad")};
    QCOMPARE(rank(fixtures, QStringLiteral("code")), codeOrder);

    // 拼音前缀压过首字母前缀，首字母前缀压过拼音包含。
    const QList<Fixture> chinese{
        {QStringLiteral("记事本"), QStringLiteral("C:/s/jsb.lnk"), QString()},
        {QStringLiteral("计算器"), QStringLiteral("C:/s/jsq.lnk"), QString()},
    };
    const QStringList jsbOnly{QStringLiteral("记事本")};
    QCOMPARE(rank(chinese, QStringLiteral("jishiben")), jsbOnly);
    QCOMPARE(rank(chinese, QStringLiteral("jsb")), jsbOnly);
    // 单字符查询下，两条首字母都以 j 开头的中文条目都不该出现。
    QVERIFY(rank(chinese, QStringLiteral("j")).isEmpty());
    QCOMPARE(rank(chinese, QStringLiteral("js")).size(), 2);
}

void TstAppMatcher::tiesBreakOnNameLengthThenLexicalThenPath()
{
    const QList<Fixture> samePrefix{
        {QStringLiteral("ab-longer-name"), QStringLiteral("C:/s/a.lnk"), QString()},
        {QStringLiteral("ab"), QStringLiteral("C:/s/z.lnk"), QString()},
    };
    // 同分先按名称长度，短的在前。查询用 "a" 是为了两条都停在名称前缀档，
    // 否则 "ab" 会先拿到精确名称分，测不到决胜逻辑。
    const QStringList byLength{QStringLiteral("ab"), QStringLiteral("ab-longer-name")};
    QCOMPARE(rank(samePrefix, QStringLiteral("a")), byLength);

    const QList<Fixture> sameLength{
        {QStringLiteral("ac"), QStringLiteral("C:/s/a.lnk"), QString()},
        {QStringLiteral("ab"), QStringLiteral("C:/s/z.lnk"), QString()},
    };
    // 长度相同按名称字典序，路径不参与这一级比较。
    const QStringList byLexical{QStringLiteral("ab"), QStringLiteral("ac")};
    QCOMPARE(rank(sameLength, QStringLiteral("a")), byLexical);

    const QList<Fixture> sameName{
        {QStringLiteral("code"), QStringLiteral("C:/z/code.lnk"), QString()},
        {QStringLiteral("code"), QStringLiteral("C:/a/code.lnk"), QString()},
    };
    // 名称完全相同（不同目录的同名快捷方式）时按路径序决胜，保证每次结果一致。
    // 名称序列看不出差别，所以直接验证比较器对两个路径的先后关系。
    QCOMPARE(rank(sameName, QStringLiteral("code")).size(), 2);
    const AppMatcher::Entry lowPath = AppMatcher::buildEntry(QStringLiteral("code"), QStringLiteral("C:/a/c.lnk"), QString());
    const AppMatcher::Entry highPath = AppMatcher::buildEntry(QStringLiteral("code"), QStringLiteral("C:/z/c.lnk"), QString());
    QVERIFY(AppMatcher::ranksBefore(lowPath, AppMatcher::ExactName, highPath, AppMatcher::ExactName));
    QVERIFY(!AppMatcher::ranksBefore(highPath, AppMatcher::ExactName, lowPath, AppMatcher::ExactName));
}

void TstAppMatcher::comparatorIsStrictWeakOrder()
{
    const QList<Fixture> fixtures{
        {QStringLiteral("Code"), QStringLiteral("C:/1.lnk"), QString()},
        {QStringLiteral("code"), QStringLiteral("C:/2.lnk"), QString()},
        {QStringLiteral("Codex"), QStringLiteral("C:/3.lnk"), QString()},
        {QStringLiteral("Visual Studio Code"), QStringLiteral("C:/4.lnk"), QStringLiteral("code")},
    };
    QList<QPair<AppMatcher::Entry, int>> nodes;
    for (const Fixture &f : fixtures) {
        const AppMatcher::Entry entry = AppMatcher::buildEntry(f.name, f.path, f.desc);
        nodes.append(qMakePair(entry, AppMatcher::evaluate(entry, QStringLiteral("code")).score));
    }
    for (const auto &a : nodes) {
        // 自反性：任何条目都不应排在自身之前。
        QVERIFY(!AppMatcher::ranksBefore(a.first, a.second, a.first, a.second));
        for (const auto &b : nodes) {
            const bool before = AppMatcher::ranksBefore(a.first, a.second, b.first, b.second);
            const bool after = AppMatcher::ranksBefore(b.first, b.second, a.first, a.second);
            QVERIFY2(!(before && after), qPrintable(QStringLiteral("%1 与 %2 互相比较失败")
                .arg(a.first.lowerName, b.first.lowerName)));
        }
    }
    // 同一份输入排序两次结果必须一致。
    const QStringList first = rank(fixtures, QStringLiteral("code"));
    const QStringList second = rank(fixtures, QStringLiteral("code"));
    QCOMPARE(first, second);
}

QTEST_GUILESS_MAIN(TstAppMatcher)

#include "tst_app_matcher.moc"
