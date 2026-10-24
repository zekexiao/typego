#pragma once

#include <QFileInfo>
#include <QList>
#include <atomic>
#include <memory>

namespace DirectoryListing {
using Cancellation = std::shared_ptr<std::atomic_bool>;
struct Result {
    QString path;
    QList<QFileInfo> entries;
    bool valid = false;
    bool truncated = false;
    bool cancelled = false;
};

// 相对路径统一以显式给出的主目录为基准，测试不访问真实用户目录。
Result list(const QString &input, const QString &home, int limit, const Cancellation &cancel = {});
}
