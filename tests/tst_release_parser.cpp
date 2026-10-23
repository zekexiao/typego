#include <QtTest>

#include "utils/ReleaseParser.h"

// 更新检查的判定规则决定用户能不能收到新版本，用 Atom fixture 直接验证解析层。
// 数据源是 releases.atom（不限流的页面接口），不是 api.github.com。
class TstReleaseParser : public QObject
{
    Q_OBJECT

private:
    static QByteArray feed(const QString &tag, const QString &content = QStringLiteral("<ul><li>修复了更新提示</li></ul>"))
    {
        QByteArray xml = QByteArrayLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
            "  <title>Release notes from typego</title>\n");
        if (!tag.isEmpty()) {
            xml += "  <entry>\n"
                   "    <id>tag:github.com,2008:Repository/1425178095/" + tag.toUtf8() + "</id>\n"
                   "    <title>TypeGo</title>\n"
                   "    <content type=\"html\">" + content.toHtmlEscaped().toUtf8() + "</content>\n"
                   "  </entry>\n";
        }
        xml += "</feed>\n";
        return xml;
    }

private slots:
    void newerReleaseIsReady();
    void installerUrlFollowsNamingConvention();
    void stripsLeadingVInAnyCase();
    void olderOrEqualReleaseIsUpToDate();
    void twoDigitMinorIsNewer();
    void missingEntryIsUpToDate();
    void malformedPayloadIsInvalidResponse();
    void emptyChangelogIsStillReported();
    void onlyFirstEntryMatters();
};

void TstReleaseParser::newerReleaseIsReady()
{
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(feed(QStringLiteral("v1.3")),
                                                                      QStringLiteral("1.2"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QCOMPARE(result.version, QStringLiteral("1.3"));
    QCOMPARE(result.changelog, QStringLiteral("<ul><li>修复了更新提示</li></ul>"));
    QVERIFY(result.message.isEmpty());
}

void TstReleaseParser::installerUrlFollowsNamingConvention()
{
    // 下载地址按发版约定推导，这条字符串和 tools/release-github.ps1 的 asset 命名必须一致。
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(feed(QStringLiteral("v1.3")),
                                                                      QStringLiteral("1.2"));
    QCOMPARE(result.installerUrl,
             QStringLiteral("https://github.com/zekexiao/typego/releases/download/v1.3/TypeGo_1.3_x64.exe"));
}

void TstReleaseParser::stripsLeadingVInAnyCase()
{
    QCOMPARE(ReleaseParser::parseAtomFeed(feed(QStringLiteral("V1.3")), QStringLiteral("1.2")).version,
             QStringLiteral("1.3"));
    // 不带 v 的标签同样可用。
    QCOMPARE(ReleaseParser::parseAtomFeed(feed(QStringLiteral("1.3")), QStringLiteral("1.2")).version,
             QStringLiteral("1.3"));
}

void TstReleaseParser::olderOrEqualReleaseIsUpToDate()
{
    QCOMPARE(int(ReleaseParser::parseAtomFeed(feed(QStringLiteral("v1.2")), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::UpToDate));
    QCOMPARE(int(ReleaseParser::parseAtomFeed(feed(QStringLiteral("v1.1")), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::UpToDate));
}

void TstReleaseParser::twoDigitMinorIsNewer()
{
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(feed(QStringLiteral("v1.10")),
                                                                      QStringLiteral("1.9"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QCOMPARE(result.version, QStringLiteral("1.10"));
}

void TstReleaseParser::missingEntryIsUpToDate()
{
    // 没有任何 entry 时不能崩，也不能提示更新。
    QCOMPARE(int(ReleaseParser::parseAtomFeed(feed(QString()), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::UpToDate));
}

void TstReleaseParser::malformedPayloadIsInvalidResponse()
{
    QCOMPARE(int(ReleaseParser::parseAtomFeed(QByteArrayLiteral("not xml"), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::InvalidResponse));
    QCOMPARE(int(ReleaseParser::parseAtomFeed(QByteArray(), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::InvalidResponse));
}

void TstReleaseParser::emptyChangelogIsStillReported()
{
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(feed(QStringLiteral("v1.3"), QString()),
                                                                      QStringLiteral("1.2"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QVERIFY(result.changelog.isEmpty());
}

void TstReleaseParser::onlyFirstEntryMatters()
{
    // atom 第一个 entry 才是最新版；后面的旧版本不能干扰判定。
    QByteArray xml = QByteArrayLiteral(
        "<?xml version=\"1.0\"?>\n<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
        "  <entry><id>tag:github.com,2008:R/1/v1.3</id><content type=\"html\">n1</content></entry>\n"
        "  <entry><id>tag:github.com,2008:R/1/v9.9</id><content type=\"html\">n2</content></entry>\n"
        "</feed>\n");
    const ReleaseParser::Result result = ReleaseParser::parseAtomFeed(xml, QStringLiteral("1.2"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QCOMPARE(result.version, QStringLiteral("1.3"));
}

QTEST_GUILESS_MAIN(TstReleaseParser)

#include "tst_release_parser.moc"
