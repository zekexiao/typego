#include "ReleaseParser.h"

#include "utils/Version.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace ReleaseParser {

namespace {

// 在 release assets 中挑选 Windows 安装包，命名约定是 TypeGo_<版本>_x64.exe。
// 优先第一个名字含 x64 的 .exe；没有 x64 标记时取第一个 .exe。
QString selectInstallerUrl(const QJsonArray &assets)
{
    QString firstExe;
    for (const QJsonValue &assetValue : assets) {
        const QJsonObject asset = assetValue.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (!name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) continue;
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
        if (name.contains(QStringLiteral("x64"), Qt::CaseInsensitive)) return url;
        if (firstExe.isEmpty()) firstExe = url;
    }
    return firstExe;
}

} // namespace

Result parseLatestRelease(const QByteArray &payload, const QString &currentVersion)
{
    Result result;

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        result.outcome = Outcome::InvalidResponse;
        result.message = QStringLiteral("Invalid GitHub release response");
        return result;
    }

    const QJsonObject obj = doc.object();
    const QString tagName = obj.value(QStringLiteral("tag_name")).toString();
    // 草稿、预发布以及不比当前版本新的标签都不提示更新，与旧实现一致。
    if (tagName.isEmpty() || obj.value(QStringLiteral("draft")).toBool()
        || obj.value(QStringLiteral("prerelease")).toBool()
        || !Version::isNewer(tagName, currentVersion)) {
        result.outcome = Outcome::UpToDate;
        return result;
    }

    result.version = tagName.startsWith(QLatin1Char('v'), Qt::CaseInsensitive) ? tagName.mid(1) : tagName;
    result.changelog = obj.value(QStringLiteral("body")).toString();
    result.installerUrl = selectInstallerUrl(obj.value(QStringLiteral("assets")).toArray());
    if (result.installerUrl.isEmpty()) {
        result.outcome = Outcome::NoInstaller;
        result.message = QStringLiteral("No Windows installer found in release assets");
        return result;
    }

    result.outcome = Outcome::UpdateReady;
    return result;
}

} // namespace ReleaseParser
