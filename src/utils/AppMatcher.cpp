#include "AppMatcher.h"

#include "utils/PinyinConverter.h"

namespace AppMatcher {

namespace {

// 名称按非字母数字切词：标点、空格、下划线都是边界，连续汉字算一个词。
QStringList tokenize(const QString &lowerName)
{
    QStringList tokens;
    QString current;
    for (const QChar &character : lowerName) {
        if (character.isLetterOrNumber()) {
            current.append(character);
            continue;
        }
        if (!current.isEmpty()) {
            tokens.append(current);
            current.clear();
        }
    }
    if (!current.isEmpty()) tokens.append(current);
    return tokens;
}

// 名称是否含汉字。拼音与首字母键只对含汉字的名称有意义：
// 纯英文名的拼音转换结果就是它的小写形式，留着会让「名称包含」被抬成「拼音包含」，
// 还白白多存两份重复字符串。
bool containsHan(const QString &name)
{
    for (const QChar &character : name) {
        const ushort code = character.unicode();
        // 基本区 U+4E00–U+9FFF 与扩展 A U+3400–U+4DBF 覆盖常见应用名。
        if (code >= 0x3400 && code <= 0x9FFF) return true;
    }
    return false;
}

} // namespace

Entry buildEntry(const QString &name, const QString &path, const QString &desc)
{
    Entry entry;
    entry.lowerName = name.toLower();
    entry.lowerDesc = desc.toLower();
    entry.lowerPath = path.toLower();
    entry.tokens = tokenize(entry.lowerName);

    if (containsHan(name)) {
        const PinyinConverter &converter = PinyinConverter::instance();
        entry.pinyin = converter.toPinyin(name);
        entry.acronym = converter.toAcronym(name);
    }
    return entry;
}

Match evaluate(const Entry &entry, const QString &query)
{
    Match match;
    if (query.isEmpty()) {
        // 空查询不过滤、不打分，交给调用方保持索引顺序。
        match.matched = true;
        return match;
    }

    int best = 0;
    auto apply = [&best](bool hit, int score) {
        if (hit && score > best) best = score;
    };

    const QString &name = entry.lowerName;
    apply(name == query, ExactName);
    apply(name.startsWith(query), NamePrefix);
    apply(name.contains(query), NameContains);
    for (const QString &token : entry.tokens) {
        apply(token.startsWith(query), TokenPrefix);
    }

    apply(entry.lowerDesc.startsWith(query), DescPrefix);
    apply(entry.lowerDesc.contains(query), DescContains);

    // 单字符查询不参与拼音与首字母匹配：一个字母能命中上百条，噪声远大于收益。
    if (query.size() >= 2) {
        apply(entry.pinyin.startsWith(query), PinyinPrefix);
        apply(entry.pinyin.contains(query), PinyinContains);
        apply(entry.acronym.startsWith(query), AcronymPrefix);
        apply(entry.acronym.contains(query), AcronymContains);
    }

    match.matched = best > 0;
    match.score = best;
    return match;
}

bool ranksBefore(const Entry &a, int scoreA, const Entry &b, int scoreB)
{
    if (scoreA != scoreB) return scoreA > scoreB;
    // 同分时短名优先，再按名称与路径字典序，保证同一份输入每次结果一致。
    if (a.lowerName.size() != b.lowerName.size()) return a.lowerName.size() < b.lowerName.size();
    if (a.lowerName != b.lowerName) return a.lowerName < b.lowerName;
    return a.lowerPath < b.lowerPath;
}

} // namespace AppMatcher
