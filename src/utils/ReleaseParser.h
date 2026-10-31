#pragma once

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

// GitHub releases.atom 订阅源的解析规则。
// 不走 api.github.com 的 releases/latest：匿名 API 每 IP 每小时只有 60 次额度，
// 共享出口 IP 的用户会直接 403；atom 订阅源是普通页面接口，不限流。
// 与网络层分开的目的是让版本判定、发布说明的中英文分流与安装包命名约定
// 能在测试里验证，这些规则一旦漂移到打包脚本之外，客户端会静默收不到更新。
namespace ReleaseParser {

enum class Outcome {
    InvalidResponse,   // 不是预期的 Atom 订阅源
    UpToDate,          // 没有比当前版本更新的正式版
    UpdateReady        // 可以提示更新
};

// 单条更新说明。text 是纯文本，html 保留 <code> 等行内格式供界面渲染。
struct NoteItem {
    QString text;
    QString html;
};

// 一个版本的更新说明，中英文分开存放。
struct ReleaseNote {
    QString tag;        // 原始发布标签，如 v1.12
    QString version;    // 去掉 v 前缀的版本号
    QString date;       // 发布日期 YYYY-MM-DD
    QList<NoteItem> chinese;
    QList<NoteItem> english;
};

// release 正文按语言小节分流的结果。plain 是没有语言小节的正文，两种语言都会展示。
struct LocalizedNotes {
    QList<NoteItem> chinese;
    QList<NoteItem> english;
    QList<NoteItem> plain;
};

struct Result {
    Outcome outcome = Outcome::InvalidResponse;
    QString currentVersion;
    QString latestVersion;      // 比当前版本更新的最新版本
    QString installerUrl;       // 按命名约定拼出的安装包下载地址
    QList<ReleaseNote> notes;   // 由新到旧，包含当前版本之后的全部版本
    QString message;            // 失败原因，用于日志与界面提示
};

/// 解析 release 正文（HTML 片段），按“中文”“English”小节分流条目。
LocalizedNotes parseReleaseBody(const QString &html);

/// 解析 GitHub releases.atom 的响应体；currentVersion 为空时视为最旧版本。
/// 安装包下载地址由标签按命名约定推导，约定见 AGENTS.md 发版工作流。
Result parseAtomFeed(const QByteArray &payload, const QString &currentVersion);

} // namespace ReleaseParser

Q_DECLARE_METATYPE(ReleaseParser::Result)
Q_DECLARE_METATYPE(ReleaseParser::ReleaseNote)
