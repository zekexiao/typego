#include "ReleaseParser.h"

#include "utils/Version.h"

#include <QXmlStreamReader>

namespace ReleaseParser {

namespace {

// 仓库地址与安装包命名约定与 AGENTS.md 发版工作流一致：
// dist\TypeGo_<版本>_x64.exe 上传到同名 tag 的 Release。
constexpr auto kInstallerUrlTemplate = "https://github.com/zekexiao/typego/releases/download/%1/TypeGo_%2_x64.exe";

QString stripV(const QString &tag)
{
    return tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive) ? tag.mid(1) : tag;
}

} // namespace

Result parseAtomFeed(const QByteArray &payload, const QString &currentVersion)
{
    Result result;

    // 只取第一个 <entry>（即最新 release），收集 <id> 末尾的 tag 与 <content> 正文。
    QXmlStreamReader xml(payload);
    QString entryId;
    QString content;
    bool inEntry = false;
    bool entryDone = false;
    while (!xml.atEnd() && !entryDone) {
        if (xml.readNext() != QXmlStreamReader::StartElement) continue;
        const QStringView name = xml.name();
        if (name == QLatin1String("entry")) {
            inEntry = true;
        } else if (inEntry && name == QLatin1String("id") && entryId.isEmpty()) {
            entryId = xml.readElementText();
        } else if (inEntry && name == QLatin1String("content")) {
            content = xml.readElementText();
            entryDone = true;
        }
    }
    if (xml.hasError()) {
        result.outcome = Outcome::InvalidResponse;
        result.message = QStringLiteral("Invalid GitHub releases feed");
        return result;
    }
    if (entryId.isEmpty()) {
        // 订阅源合法但没有任何 release，等同没有更新。
        result.outcome = Outcome::UpToDate;
        return result;
    }

    // id 形如 tag:github.com,2008:Repository/<id>/<tag>，取最后一段作为标签。
    const QString tag = entryId.section(QLatin1Char('/'), -1).trimmed();
    const QString version = stripV(tag);
    if (version.isEmpty() || !Version::isNewer(tag, currentVersion)) {
        result.outcome = Outcome::UpToDate;
        return result;
    }

    result.version = version;
    result.changelog = content.trimmed();
    result.installerUrl = QString::fromLatin1(kInstallerUrlTemplate).arg(tag, version);
    result.outcome = Outcome::UpdateReady;
    return result;
}

} // namespace ReleaseParser
