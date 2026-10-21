#pragma once

#include <QString>
#include <QStringList>

// 应用搜索的匹配键构造与打分规则。
// 纯逻辑、只依赖名称/路径/描述字符串，因此放在 typego_core 里可以脱离索引和界面测试。
namespace AppMatcher {

// 打分档位，数值只代表相对顺序：精确 > 名称前缀 > 词前缀 > 拼音前缀 > 首字母前缀
// > 拼音包含 > 首字母包含 > 名称包含 > 描述。
enum Score : int {
    ExactName = 1000,     // 名称与查询完全一致
    NamePrefix = 700,     // 名称以查询开头
    TokenPrefix = 600,    // 名称中任一词以查询开头
    PinyinPrefix = 550,   // 全拼以查询开头
    AcronymPrefix = 500,  // 首字母以查询开头
    PinyinContains = 450, // 全拼中间包含查询
    AcronymContains = 380,// 首字母中间包含查询
    NameContains = 300,   // 名称中间包含查询
    DescPrefix = 200,     // 描述以查询开头
    DescContains = 120    // 描述中间包含查询
};

// 一个应用条目的匹配键，扫描期一次性构造，搜索期只读。
struct Entry {
    QString lowerName;
    QString lowerDesc;
    QString pinyin;    // 仅含汉字的名称才建；非汉字字符按小写原样参与
    QString acronym;   // 仅含汉字的名称才建，取拼音首字母
    QStringList tokens; // 名称按非字母数字切分后的小写词
    QString lowerPath; // 不参与匹配，仅用于同分时保证排序稳定
};

struct Match {
    bool matched = false;
    int score = 0;
};

/// 由原始名称、路径、描述构造匹配键。
Entry buildEntry(const QString &name, const QString &path, const QString &desc);

/// 计算单个条目对查询的匹配强度；query 必须已去空白并转小写。
/// 空查询视为匹配且分数为 0，调用方应在空查询时跳过排序以保持索引顺序。
Match evaluate(const Entry &entry, const QString &query);

/// 同分决胜：分数高者先，其次名称短者先，再按名称与路径字典序。
/// 必须构成严格弱序，否则排序结果会随输入抖动。
bool ranksBefore(const Entry &a, int scoreA, const Entry &b, int scoreB);

} // namespace AppMatcher
