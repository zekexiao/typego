#include <QtTest>

#include "utils/ReleaseParser.h"

// 更新检查的判定规则决定用户能不能收到新版本，用 Atom fixture 直接验证解析层。
// 数据源是 releases.atom（不限流的页面接口），不是 api.github.com。
class TstReleaseParser : public QObject
{
    Q_OBJECT

private:
    struct Entry {
        QString tag;
        QString content;
        QString updated = QStringLiteral("2026-10-31T03:02:56Z");
    };

    static QByteArray feed(const QList<Entry> &entries)
    {
        QByteArray xml = QByteArrayLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
            "  <title>Release notes from typego</title>\n");
        for (const Entry &entry : entries) {
            xml += "  <entry>\n"
                   "    <id>tag:github.com,2008:Repository/1425178095/" + entry.tag.toUtf8() + "</id>\n"
                   "    <updated>" + entry.updated.toUtf8() + "</updated>\n"
                   "    <title>TypeGo</title>\n"
                   "    <content type=\"html\">" + entry.content.toHtmlEscaped().toUtf8() + "</content>\n"
                   "  </entry>\n";
        }
        xml += "</feed>\n";
        return xml;
    }

    static QByteArray singleFeed(const QString &tag, const QString &content = QStringLiteral("<ul><li>修复了更新提示</li></ul>"))
    {
        return feed({Entry{tag, content}});
    }

    // 中英文分节的正文，结构与 GitHub 发布页一致。
    static QString bilingualBody()
    {
        return QStringLiteral(
            "<h3>中文</h3>\n<ul>\n<li>新增截图插件</li>\n<li>修复剪贴板历史</li>\n</ul>\n"
            "<h3>English</h3>\n<ul>\n<li>Added the screenshot plugin</li>\n<li>Fixed the clipboard history</li>\n</ul>");
    }

private slots:
    void newerReleaseIsReady();
    void installerUrlFollowsNamingConvention();
    void stripsLeadingVInAnyCase();
    void olderOrEqualReleaseIsUpToDate();
    void twoDigitMinorIsNewer();
    void missingEntryIsUpToDate();
    void malformedPayloadIsInvalidResponse();
    void collectsEveryVersionUpToLatest();
    void splitsChineseAndEnglishSections();
    void itemsWithoutLanguageSectionShowInBothLanguages();
    void decodesEntitiesAndKeepsInlineCode();
    void sortsByVersionNotFeedOrder();
    void ignoresPrereleaseTag();
    void keepsReleaseDate();
};

void TstReleaseParser::newerReleaseIsReady()
{
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(singleFeed(QStringLiteral("v1.3")),
                                                                      QStringLiteral("1.2"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QCOMPARE(result.latestVersion, QStringLiteral("1.3"));
    QCOMPARE(result.currentVersion, QStringLiteral("1.2"));
    QVERIFY(result.message.isEmpty());
}

void TstReleaseParser::installerUrlFollowsNamingConvention()
{
    // 下载地址按发版约定推导，这条字符串和打包脚本的 asset 命名必须一致。
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(singleFeed(QStringLiteral("v1.3")),
                                                                      QStringLiteral("1.2"));
    QCOMPARE(result.installerUrl,
             QStringLiteral("https://github.com/zekexiao/typego/releases/download/v1.3/TypeGo_1.3_x64.exe"));
}

void TstReleaseParser::stripsLeadingVInAnyCase()
{
    QCOMPARE(ReleaseParser::parseAtomFeed(singleFeed(QStringLiteral("V1.3")), QStringLiteral("1.2")).latestVersion,
             QStringLiteral("1.3"));
    // 不带 v 的标签同样可用。
    QCOMPARE(ReleaseParser::parseAtomFeed(singleFeed(QStringLiteral("1.3")), QStringLiteral("1.2")).latestVersion,
             QStringLiteral("1.3"));
}

void TstReleaseParser::olderOrEqualReleaseIsUpToDate()
{
    QCOMPARE(int(ReleaseParser::parseAtomFeed(singleFeed(QStringLiteral("v1.2")), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::UpToDate));
    QCOMPARE(int(ReleaseParser::parseAtomFeed(singleFeed(QStringLiteral("v1.1")), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::UpToDate));
}

void TstReleaseParser::twoDigitMinorIsNewer()
{
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(singleFeed(QStringLiteral("v1.10")),
                                                                      QStringLiteral("1.9"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QCOMPARE(result.latestVersion, QStringLiteral("1.10"));
}

void TstReleaseParser::missingEntryIsUpToDate()
{
    // 没有任何 entry 时不能崩，也不能提示更新。
    QCOMPARE(int(ReleaseParser::parseAtomFeed(feed({}), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::UpToDate));
}

void TstReleaseParser::malformedPayloadIsInvalidResponse()
{
    QCOMPARE(int(ReleaseParser::parseAtomFeed(QByteArrayLiteral("not xml"), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::InvalidResponse));
    QCOMPARE(int(ReleaseParser::parseAtomFeed(QByteArray(), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::InvalidResponse));
}

void TstReleaseParser::collectsEveryVersionUpToLatest()
{
    // 用户提示里要看到当前版本之后每一个版本的内容，而不是只有最新版。
    const QByteArray xml = feed({Entry{QStringLiteral("v1.13"), bilingualBody()},
                                  Entry{QStringLiteral("v1.12"), bilingualBody()},
                                  Entry{QStringLiteral("v1.11"), bilingualBody()},
                                  Entry{QStringLiteral("v1.10"), bilingualBody()}});
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(xml, QStringLiteral("1.11"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QCOMPARE(result.latestVersion, QStringLiteral("1.13"));
    QCOMPARE(result.notes.size(), 2);
    QCOMPARE(result.notes.at(0).version, QStringLiteral("1.13"));
    QCOMPARE(result.notes.at(1).version, QStringLiteral("1.12"));
    // 安装包地址始终指向最新版。
    QCOMPARE(result.installerUrl,
             QStringLiteral("https://github.com/zekexiao/typego/releases/download/v1.13/TypeGo_1.13_x64.exe"));
}

void TstReleaseParser::splitsChineseAndEnglishSections()
{
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(singleFeed(QStringLiteral("v1.13"), bilingualBody()),
                                                                      QStringLiteral("1.12"));
    QCOMPARE(result.notes.size(), 1);
    const ReleaseParser::ReleaseNote &note = result.notes.first();
    QCOMPARE(note.chinese.size(), 2);
    QCOMPARE(note.english.size(), 2);
    QCOMPARE(note.chinese.at(0).text, QStringLiteral("新增截图插件"));
    QCOMPARE(note.english.at(0).text, QStringLiteral("Added the screenshot plugin"));
    // 中英文条目不能串台。
    QCOMPARE(note.chinese.at(1).text, QStringLiteral("修复剪贴板历史"));
    QCOMPARE(note.english.at(1).text, QStringLiteral("Fixed the clipboard history"));
}

void TstReleaseParser::itemsWithoutLanguageSectionShowInBothLanguages()
{
    // 早期版本没有语言小节，正文对两种语言都展示。
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(
        singleFeed(QStringLiteral("v1.3"), QStringLiteral("<ul><li><p>新增检查更新按钮</p></li></ul>")),
        QStringLiteral("1.2"));
    QCOMPARE(result.notes.size(), 1);
    const ReleaseParser::ReleaseNote &note = result.notes.first();
    QCOMPARE(note.chinese.size(), 1);
    QCOMPARE(note.english.size(), 1);
    QCOMPARE(note.chinese.first().text, QStringLiteral("新增检查更新按钮"));
    QCOMPARE(note.english.first().text, QStringLiteral("新增检查更新按钮"));
}

void TstReleaseParser::decodesEntitiesAndKeepsInlineCode()
{
    const QString body = QStringLiteral(
        "<h3>中文</h3><ul><li>使用 <code>shot-full</code> 截图，支持「设置 &gt; 插件」</li></ul>"
        "<h3>English</h3><ul><li>Use <code>shot-full</code> to capture</li></ul>");
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(singleFeed(QStringLiteral("v1.13"), body),
                                                                      QStringLiteral("1.12"));
    const ReleaseParser::ReleaseNote &note = result.notes.first();
    QCOMPARE(note.chinese.first().text, QStringLiteral("使用 shot-full 截图，支持「设置 > 插件」"));
    // 行内格式保留下来，界面里命令名仍然是等宽字体。
    QVERIFY(note.chinese.first().html.contains(QStringLiteral("<code>shot-full</code>")));
}

void TstReleaseParser::sortsByVersionNotFeedOrder()
{
    const QByteArray xml = feed({Entry{QStringLiteral("v1.11"), bilingualBody()},
                                  Entry{QStringLiteral("v1.13"), bilingualBody()},
                                  Entry{QStringLiteral("v1.12"), bilingualBody()}});
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(xml, QStringLiteral("1.10"));
    QCOMPARE(result.latestVersion, QStringLiteral("1.13"));
    QCOMPARE(result.notes.size(), 3);
    QCOMPARE(result.notes.at(0).version, QStringLiteral("1.13"));
    QCOMPARE(result.notes.at(1).version, QStringLiteral("1.12"));
    QCOMPARE(result.notes.at(2).version, QStringLiteral("1.11"));
}

void TstReleaseParser::ignoresPrereleaseTag()
{
    // 预发布标签不是数字版本，不能被当成可用更新推给用户。
    const QByteArray xml = feed({Entry{QStringLiteral("v1.13-beta"), bilingualBody()},
                                  Entry{QStringLiteral("v1.12"), bilingualBody()}});
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(xml, QStringLiteral("1.11"));
    QCOMPARE(result.latestVersion, QStringLiteral("1.12"));
    QCOMPARE(result.notes.size(), 1);
}

void TstReleaseParser::keepsReleaseDate()
{
    QList<Entry> entries{Entry{QStringLiteral("v1.13"), bilingualBody()}};
    entries[0].updated = QStringLiteral("2026-11-01T08:00:00Z");
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(feed(entries), QStringLiteral("1.12"));
    QCOMPARE(result.notes.first().date, QStringLiteral("2026-11-01"));
}

QTEST_GUILESS_MAIN(TstReleaseParser)

#include "tst_release_parser.moc"
