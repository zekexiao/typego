#include "utils/ReleaseParser.h"

#include "utils/Version.h"

#include <QXmlStreamReader>

#include <algorithm>

namespace ReleaseParser {

namespace {

// 仓库地址与安装包命名约定与 AGENTS.md 发版工作流一致：
// dist\TypeGo_<版本>_x64.exe 上传到同名 tag 的 Release。
constexpr auto kInstallerUrlTemplate = "https://github.com/zekexiao/typego/releases/download/%1/TypeGo_%2_x64.exe";

QString stripV(const QString &tag)
{
    return tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive) ? tag.mid(1) : tag;
}

// 还原 HTML 实体：release 正文是 HTML 片段，内联实体要转回字符才能正常显示。
QString decodeEntities(const QString &text)
{
    QString out;
    out.reserve(text.size());
    int i = 0;
    while (i < text.size()) {
        const QChar ch = text.at(i);
        if (ch != QLatin1Char('&')) {
            out.append(ch);
            ++i;
            continue;
        }
        const int end = text.indexOf(QLatin1Char(';'), i + 1);
        if (end < 0 || end - i > 10) {
            // 没有对应的结束分号，当作普通字符处理。
            out.append(ch);
            ++i;
            continue;
        }
        const QString entity = text.mid(i + 1, end - i - 1);
        QChar decoded;
        bool numeric = false;
        uint code = 0;
        if (entity == QLatin1String("lt")) decoded = QLatin1Char('<');
        else if (entity == QLatin1String("gt")) decoded = QLatin1Char('>');
        else if (entity == QLatin1String("amp")) decoded = QLatin1Char('&');
        else if (entity == QLatin1String("quot")) decoded = QLatin1Char('"');
        else if (entity == QLatin1String("apos")) decoded = QLatin1Char('\'');
        else if (entity == QLatin1String("nbsp")) decoded = QLatin1Char(' ');
        else if (entity.size() > 2 && entity.at(0) == QLatin1Char('#')) {
            const bool hex = entity.at(1) == QLatin1Char('x') || entity.at(1) == QLatin1Char('X');
            bool ok = false;
            code = entity.mid(hex ? 2 : 1).toUInt(&ok, hex ? 16 : 10);
            numeric = ok && code > 0 && code <= 0x10FFFF;
        }
        if (!decoded.isNull()) {
            out.append(decoded);
        } else if (numeric) {
            out.append(QChar::fromUcs4(code));
        } else {
            out.append(ch);
            ++i;
            continue;
        }
        i = end + 1;
    }
    return out;
}

// 去掉标签并还原实体，得到用于展示与比较的纯文本。
QString htmlToPlainText(const QString &html)
{
    QString out;
    int i = 0;
    while (i < html.size()) {
        const int lt = html.indexOf(QLatin1Char('<'), i);
        if (lt < 0) {
            out += html.mid(i);
            break;
        }
        out += html.mid(i, lt - i);
        const int gt = html.indexOf(QLatin1Char('>'), lt);
        if (gt < 0) {
            out += html.mid(lt);
            break;
        }
        const QString tag = html.mid(lt + 1, gt - lt - 1).trimmed().toLower();
        if (tag == QLatin1String("br") || tag == QLatin1String("br/") || tag == QLatin1String("br /")) {
            out.append(QLatin1Char(' '));
        }
        i = gt + 1;
    }
    return decodeEntities(out).simplified();
}

// 取标签名，忽略属性与结尾斜杠。
QString tagName(const QString &raw)
{
    QString body = raw.trimmed();
    if (body.startsWith(QLatin1Char('/'))) body.remove(0, 1);
    int end = 0;
    while (end < body.size() && !body.at(end).isSpace()) ++end;
    return body.left(end).toLower();
}

enum class Section {
    Plain,
    Chinese,
    English
};

// 标题语言小节的识别：只认“中文”“English”，其余标题归入无语言正文。
Section sectionForTitle(const QString &title)
{
    const QString trimmed = title.trimmed();
    // 中文标题必须用 QStringLiteral：QLatin1String 不做 UTF-8 解码，比较不会成立。
    if (trimmed == QStringLiteral("中文")
        || trimmed.compare(QStringLiteral("Chinese"), Qt::CaseInsensitive) == 0) {
        return Section::Chinese;
    }
    if (trimmed.compare(QStringLiteral("English"), Qt::CaseInsensitive) == 0) {
        return Section::English;
    }
    return Section::Plain;
}

} // namespace

LocalizedNotes parseReleaseBody(const QString &html)
{
    LocalizedNotes notes;
    Section section = Section::Plain;

    const auto appendItem = [&](const QString &raw) {
        NoteItem item;
        item.text = htmlToPlainText(raw);
        item.html = raw.simplified();
        if (item.text.isEmpty()) return;
        switch (section) {
        case Section::Chinese:
            notes.chinese.append(item);
            break;
        case Section::English:
            notes.english.append(item);
            break;
        case Section::Plain:
            notes.plain.append(item);
            break;
        }
    };

    int pos = 0;
    int headingStart = -1;
    int itemStart = -1;
    int paragraphStart = -1;
    bool inListItem = false;

    while (pos < html.size()) {
        const int lt = html.indexOf(QLatin1Char('<'), pos);
        if (lt < 0) break;
        const int gt = html.indexOf(QLatin1Char('>'), lt);
        if (gt < 0) break;
        const QString raw = html.mid(lt + 1, gt - lt - 1);
        const bool closing = raw.trimmed().startsWith(QLatin1Char('/'));
        const QString name = tagName(raw);
        pos = gt + 1;

        if (name == QLatin1String("li")) {
            if (!closing) {
                itemStart = pos;
                inListItem = true;
            } else if (inListItem && itemStart >= 0) {
                appendItem(html.mid(itemStart, lt - itemStart));
                itemStart = -1;
                inListItem = false;
            }
        } else if (name.size() == 2 && name.at(0) == QLatin1Char('h') && name.at(1).isDigit()) {
            if (!closing) {
                headingStart = pos;
            } else if (headingStart >= 0) {
                section = sectionForTitle(htmlToPlainText(html.mid(headingStart, lt - headingStart)));
                headingStart = -1;
            }
        } else if (name == QLatin1String("p")) {
            if (!closing) {
                paragraphStart = pos;
            } else if (paragraphStart >= 0) {
                // 列表项里的段落已随列表项一起收集，这里只处理独立段落。
                if (!inListItem) appendItem(html.mid(paragraphStart, lt - paragraphStart));
                paragraphStart = -1;
            }
        }
    }
    return notes;
}

Result parseAtomFeed(const QByteArray &payload, const QString &currentVersion)
{
    Result result;
    result.currentVersion = currentVersion;

    struct RawEntry {
        QString id;
        QString updated;
        QString content;
    };
    QList<RawEntry> rawEntries;

    QXmlStreamReader xml(payload);
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement) continue;
        if (xml.name() != QLatin1String("entry")) continue;
        RawEntry entry;
        while (!xml.atEnd()) {
            const QXmlStreamReader::TokenType token = xml.readNext();
            if (token == QXmlStreamReader::EndElement) {
                if (xml.name() == QLatin1String("entry")) break;
                continue;
            }
            if (token != QXmlStreamReader::StartElement) continue;
            const QStringView name = xml.name();
            if (name == QLatin1String("id") && entry.id.isEmpty()) {
                entry.id = xml.readElementText();
            } else if (name == QLatin1String("updated") && entry.updated.isEmpty()) {
                entry.updated = xml.readElementText();
            } else if (name == QLatin1String("content") && entry.content.isEmpty()) {
                entry.content = xml.readElementText();
            }
        }
        rawEntries.append(entry);
    }
    if (xml.hasError()) {
        result.outcome = Outcome::InvalidResponse;
        result.message = QStringLiteral("Invalid GitHub releases feed");
        return result;
    }

    struct Entry {
        QString tag;
        QString version;
        QString date;
        LocalizedNotes body;
    };
    QList<Entry> entries;
    entries.reserve(rawEntries.size());
    for (const RawEntry &raw : rawEntries) {
        // id 形如 tag:github.com,2008:Repository/<id>/<tag>，取最后一段作为标签。
        const QString tag = raw.id.section(QLatin1Char('/'), -1).trimmed();
        const QString version = stripV(tag);
        if (version.isEmpty()) continue;
        Entry entry;
        entry.tag = tag;
        entry.version = version;
        entry.date = raw.updated.left(10);
        entry.body = parseReleaseBody(raw.content);
        entries.append(entry);
    }

    // 以版本号为准排序，避免个别条目发布时间乱序影响“最新版本”的判定。
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return Version::isNewer(a.version, b.version);
    });

    // 收集当前版本之后的全部版本，用户要看到的是整段升级路径而不是只有最新版。
    for (const Entry &entry : entries) {
        if (!Version::isNewer(entry.version, currentVersion)) continue;
        ReleaseNote note;
        note.tag = entry.tag;
        note.version = entry.version;
        note.date = entry.date;
        note.chinese = entry.body.chinese;
        note.english = entry.body.english;
        // 没有语言小节的旧版本只有一份正文，两种语言都展示。
        note.chinese += entry.body.plain;
        note.english += entry.body.plain;
        result.notes.append(note);
    }

    if (result.notes.isEmpty()) {
        result.outcome = Outcome::UpToDate;
        return result;
    }

    const ReleaseNote &latest = result.notes.first();
    result.latestVersion = latest.version;
    result.installerUrl = QString::fromLatin1(kInstallerUrlTemplate).arg(latest.tag, latest.version);
    result.outcome = Outcome::UpdateReady;
    return result;
}

} // namespace ReleaseParser
