#pragma once

#include <QByteArray>
#include <QString>

// GitHub releases.atom 订阅源的解析规则。
// 不走 api.github.com 的 releases/latest：匿名 API 每 IP 每小时只有 60 次额度，
// 共享出口 IP 的用户会直接 403；atom 订阅源是普通页面接口，不限流。
// 与网络层分开的目的是让版本判定与安装包命名约定（TypeGo_x.y_x64.exe）能在测试里验证，
// 这条规则一旦漂移到打包脚本之外，客户端会静默收不到更新。
namespace ReleaseParser {

enum class Outcome {
    InvalidResponse,   // 不是预期的 Atom 订阅源
    UpToDate,          // 没有比当前版本更新的正式版
    UpdateReady        // 可以提示更新
};

struct Result {
    Outcome outcome = Outcome::InvalidResponse;
    QString version;        // 去掉 v 前缀后的版本号
    QString changelog;      // release 正文（HTML 片段）
    QString installerUrl;   // 按命名约定拼出的安装包下载地址
    QString message;        // 失败原因，用于日志与界面提示
};

/// 解析 GitHub releases.atom 的响应体；currentVersion 为空时视为最旧版本。
/// 安装包下载地址由标签按命名约定推导，约定见 AGENTS.md 发版工作流。
Result parseAtomFeed(const QByteArray &payload, const QString &currentVersion);

} // namespace ReleaseParser
