#pragma once

#include <QString>

// 应用图标提取的统一入口，供固定项和结果列表异步加载共用。
// 覆盖普通 exe/lnk、应用执行别名（Store 应用，如 Win11 的画图）和特殊路径。
namespace IconExtractor {

// 提取 32x32 图标并缓存为 PNG，返回缓存文件路径；失败返回空字符串。
// 可在非 GUI 线程调用（不使用 QFileIconProvider / QPixmap）。
QString extract(const QString &path, const QString &cacheDir);

} // namespace IconExtractor
