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

// 排序键：score 是文本匹配档位，count/lastUsed 来自使用频次记录。
// 比较是字典序：档位优先，同档内频次权重无限大、跨档权重为零，
// 保证高频应用只能在同档内靠前，永远不能越过文本相关性档位。
struct RankKey {
    int score = 0;
    quint32 count = 0;     // 成功启动次数
    qint64 lastUsed = 0;   // 最近使用时间（UTC 毫秒）
};

/// 由原始名称、路径、描述构造匹配键。
Entry buildEntry(const QString &name, const QString &path, const QString &desc);

/// 计算单个条目对查询的匹配强度；query 必须已去空白并转小写。
/// 空查询视为匹配且分数为 0，调用方应在空查询时跳过排序以保持索引顺序。
Match evaluate(const Entry &entry, const QString &query);

/// 字典序比较：档位降序 → 次数降序 → 最近使用降序 → 名称长度升序 → 名称升序 → 路径升序。
/// 必须构成严格弱序，否则排序结果会随输入抖动。
bool ranksBefore(const Entry &a, const RankKey &ka, const Entry &b, const RankKey &kb);

/// 旧接口：等价于 RankKey{score, 0, 0}，即完全不看使用频次的纯文本排序。
bool ranksBefore(const Entry &a, int scoreA, const Entry &b, int scoreB);

/// 空查询的使用频次比较：次数降序 → 最近使用降序。
/// 与 std::stable_sort 配合时，未使用过的条目保持索引原序。
bool usageRanksBefore(const RankKey &ka, const RankKey &kb);

} // namespace AppMatcher
