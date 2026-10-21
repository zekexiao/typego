#pragma once

#include <QByteArray>
#include <QString>

// GitHub latest release 响应的解析规则。
// 与网络层分开的目的是让发版命名约定（TypeGo_x.y_x64.exe）能在测试里验证，
// 这条规则一旦漂移到打包脚本之外，客户端会静默收不到更新。
namespace ReleaseParser {

enum class Outcome {
    InvalidResponse,   // 不是预期的 JSON 对象
    UpToDate,          // 没有比当前版本更新的正式版
    NoInstaller,       // 有新版本，但 assets 里找不到 Windows 安装包
    UpdateReady        // 可以提示更新
};

struct Result {
    Outcome outcome = Outcome::InvalidResponse;
    QString version;        // 去掉 v 前缀后的版本号
    QString changelog;      // release 正文
    QString installerUrl;   // browser_download_url
    QString message;        // 失败原因，用于日志与界面提示
};

/// 解析 GitHub releases/latest 的响应体；currentVersion 为空时视为最旧版本。
Result parseLatestRelease(const QByteArray &payload, const QString &currentVersion);

} // namespace ReleaseParser
