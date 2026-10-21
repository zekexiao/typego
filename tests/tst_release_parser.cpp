#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "utils/ReleaseParser.h"

// 更新检查的判定规则决定用户能不能收到新版本，用 JSON fixture 直接验证解析层。
class TstReleaseParser : public QObject
{
    Q_OBJECT

private:
    static QJsonObject asset(const QString &name, const QString &url)
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), name);
        obj.insert(QStringLiteral("browser_download_url"), url);
        return obj;
    }

    static QByteArray payload(const QString &tagName, const QJsonArray &assets,
                              bool draft = false, bool prerelease = false,
                              const QString &body = QStringLiteral("修复了更新提示"))
    {
        QJsonObject obj;
        if (!tagName.isEmpty()) obj.insert(QStringLiteral("tag_name"), tagName);
        obj.insert(QStringLiteral("draft"), draft);
        obj.insert(QStringLiteral("prerelease"), prerelease);
        obj.insert(QStringLiteral("body"), body);
        obj.insert(QStringLiteral("assets"), assets);
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

private slots:
    void newerReleaseWithInstallerIsReady();
    void stripsLeadingVInAnyCase();
    void draftAndPrereleaseAreIgnored();
    void olderOrEqualReleaseIsUpToDate();
    void twoDigitMinorIsNewer();
    void missingInstallerIsAnError();
    void prefersX64AssetOverOtherExe();
    void firstExeWinsWhenNoX64Marked();
    void caseInsensitiveExecutableSuffix();
    void emptyDownloadUrlIsTreatedAsMissing();
    void malformedPayloadIsInvalidResponse();
    void emptyChangelogIsStillReported();
};

void TstReleaseParser::newerReleaseWithInstallerIsReady()
{
    const QJsonArray assets{asset(QStringLiteral("TypeGo_1.3_x64.exe"),
                                  QStringLiteral("https://github.com/x/TypeGo_1.3_x64.exe"))};
    const ReleaseParser::Result result = ReleaseParser::parseLatestRelease(payload(QStringLiteral("v1.3"), assets),
                                                                           QStringLiteral("1.2"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QCOMPARE(result.version, QStringLiteral("1.3"));
    QCOMPARE(result.changelog, QStringLiteral("修复了更新提示"));
    QCOMPARE(result.installerUrl, QStringLiteral("https://github.com/x/TypeGo_1.3_x64.exe"));
    QVERIFY(result.message.isEmpty());
}

void TstReleaseParser::stripsLeadingVInAnyCase()
{
    const QJsonArray assets{asset(QStringLiteral("TypeGo_1.3_x64.exe"), QStringLiteral("https://x/1.3.exe"))};
    QCOMPARE(ReleaseParser::parseLatestRelease(payload(QStringLiteral("V1.3"), assets), QStringLiteral("1.2")).version,
             QStringLiteral("1.3"));
    // 不带 v 的标签同样可用。
    QCOMPARE(ReleaseParser::parseLatestRelease(payload(QStringLiteral("1.3"), assets), QStringLiteral("1.2")).version,
             QStringLiteral("1.3"));
}

void TstReleaseParser::draftAndPrereleaseAreIgnored()
{
    const QJsonArray assets{asset(QStringLiteral("TypeGo_1.3_x64.exe"), QStringLiteral("https://x/1.3.exe"))};
    const ReleaseParser::Result draft = ReleaseParser::parseLatestRelease(
        payload(QStringLiteral("v1.3"), assets, true, false), QStringLiteral("1.2"));
    QCOMPARE(int(draft.outcome), int(ReleaseParser::Outcome::UpToDate));

    const ReleaseParser::Result pre = ReleaseParser::parseLatestRelease(
        payload(QStringLiteral("v1.3"), assets, false, true), QStringLiteral("1.2"));
    QCOMPARE(int(pre.outcome), int(ReleaseParser::Outcome::UpToDate));
}

void TstReleaseParser::olderOrEqualReleaseIsUpToDate()
{
    const QJsonArray assets{asset(QStringLiteral("TypeGo_1.2_x64.exe"), QStringLiteral("https://x/1.2.exe"))};
    QCOMPARE(int(ReleaseParser::parseLatestRelease(payload(QStringLiteral("v1.2"), assets), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::UpToDate));
    QCOMPARE(int(ReleaseParser::parseLatestRelease(payload(QStringLiteral("v1.1"), assets), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::UpToDate));
    // 标签缺失时不能崩，也不能提示更新。
    QCOMPARE(int(ReleaseParser::parseLatestRelease(payload(QString(), assets), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::UpToDate));
}

void TstReleaseParser::twoDigitMinorIsNewer()
{
    const QJsonArray assets{asset(QStringLiteral("TypeGo_1.10_x64.exe"), QStringLiteral("https://x/1.10.exe"))};
    const ReleaseParser::Result result = ReleaseParser::parseLatestRelease(
        payload(QStringLiteral("v1.10"), assets), QStringLiteral("1.9"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QCOMPARE(result.version, QStringLiteral("1.10"));
}

void TstReleaseParser::missingInstallerIsAnError()
{
    // 只发了压缩包没有安装包时，必须报错而不是假装已是最新。
    const QJsonArray onlyZip{asset(QStringLiteral("TypeGo_1.3.zip"), QStringLiteral("https://x/1.3.zip"))};
    const ReleaseParser::Result result = ReleaseParser::parseLatestRelease(
        payload(QStringLiteral("v1.3"), onlyZip), QStringLiteral("1.2"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::NoInstaller));
    QVERIFY(!result.message.isEmpty());
    QVERIFY(result.installerUrl.isEmpty());

    const ReleaseParser::Result noAssets = ReleaseParser::parseLatestRelease(
        payload(QStringLiteral("v1.3"), QJsonArray()), QStringLiteral("1.2"));
    QCOMPARE(int(noAssets.outcome), int(ReleaseParser::Outcome::NoInstaller));
}

void TstReleaseParser::prefersX64AssetOverOtherExe()
{
    const QJsonArray assets{
        asset(QStringLiteral("TypeGo_1.3_arm64.exe"), QStringLiteral("https://x/arm64.exe")),
        asset(QStringLiteral("TypeGo_1.3_x64.exe"), QStringLiteral("https://x/x64.exe")),
        asset(QStringLiteral("TypeGo_1.3_setup.exe"), QStringLiteral("https://x/setup.exe")),
    };
    const ReleaseParser::Result result = ReleaseParser::parseLatestRelease(
        payload(QStringLiteral("v1.3"), assets), QStringLiteral("1.2"));
    QCOMPARE(result.installerUrl, QStringLiteral("https://x/x64.exe"));
}

void TstReleaseParser::firstExeWinsWhenNoX64Marked()
{
    // 没有任何 x64 标记时取第一个 .exe，避免按列表顺序拿到最后一个未知产物。
    const QJsonArray assets{
        asset(QStringLiteral("TypeGo_1.3_online.exe"), QStringLiteral("https://x/online.exe")),
        asset(QStringLiteral("TypeGo_1.3_full.exe"), QStringLiteral("https://x/full.exe")),
    };
    const ReleaseParser::Result result = ReleaseParser::parseLatestRelease(
        payload(QStringLiteral("v1.3"), assets), QStringLiteral("1.2"));
    QCOMPARE(result.installerUrl, QStringLiteral("https://x/online.exe"));
}

void TstReleaseParser::caseInsensitiveExecutableSuffix()
{
    const QJsonArray assets{asset(QStringLiteral("TypeGo_1.3_X64.EXE"), QStringLiteral("https://x/1.3.EXE"))};
    const ReleaseParser::Result result = ReleaseParser::parseLatestRelease(
        payload(QStringLiteral("v1.3"), assets), QStringLiteral("1.2"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QCOMPARE(result.installerUrl, QStringLiteral("https://x/1.3.EXE"));
}

void TstReleaseParser::emptyDownloadUrlIsTreatedAsMissing()
{
    // x64 条目存在但地址为空时不能提示更新，否则用户点了必然下载失败。
    const QJsonArray assets{asset(QStringLiteral("TypeGo_1.3_x64.exe"), QString())};
    const ReleaseParser::Result result = ReleaseParser::parseLatestRelease(
        payload(QStringLiteral("v1.3"), assets), QStringLiteral("1.2"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::NoInstaller));
}

void TstReleaseParser::malformedPayloadIsInvalidResponse()
{
    QCOMPARE(int(ReleaseParser::parseLatestRelease(QByteArrayLiteral("not json"), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::InvalidResponse));
    QCOMPARE(int(ReleaseParser::parseLatestRelease(QByteArrayLiteral("[]"), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::InvalidResponse));
    QCOMPARE(int(ReleaseParser::parseLatestRelease(QByteArray(), QStringLiteral("1.2")).outcome),
             int(ReleaseParser::Outcome::InvalidResponse));
}

void TstReleaseParser::emptyChangelogIsStillReported()
{
    const QJsonArray assets{asset(QStringLiteral("TypeGo_1.3_x64.exe"), QStringLiteral("https://x/1.3.exe"))};
    QJsonObject obj;
    obj.insert(QStringLiteral("tag_name"), QStringLiteral("v1.3"));
    obj.insert(QStringLiteral("assets"), assets);
    const ReleaseParser::Result result = ReleaseParser::parseLatestRelease(
        QJsonDocument(obj).toJson(QJsonDocument::Compact), QStringLiteral("1.2"));
    QCOMPARE(int(result.outcome), int(ReleaseParser::Outcome::UpdateReady));
    QVERIFY(result.changelog.isEmpty());
}

QTEST_GUILESS_MAIN(TstReleaseParser)

#include "tst_release_parser.moc"
