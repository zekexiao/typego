#include "DirectoryListing.h"
#include <QDir>
#include <QDirIterator>
#include <algorithm>
#include <vector>

namespace DirectoryListing {
Result list(const QString &input, const QString &home, int limit, const Cancellation &cancel)
{
    Result result;
    auto cancelled = [&]() { return cancel && cancel->load(std::memory_order_relaxed); };
    if (cancelled()) { result.cancelled = true; return result; }
    QString path = input.trimmed();
    if (path.isEmpty() || path == QLatin1String("~")) path = home;
    else if (path.startsWith(QLatin1String("~/")) || path.startsWith(QLatin1String("~\\")))
        path = QDir(home).filePath(path.mid(2));
    else if (QDir::isRelativePath(path)) path = QDir(home).filePath(path);
    QDir dir(path);
    result.path = dir.absolutePath();
    if (!dir.exists() || !QFileInfo(result.path).isReadable()) return result;
    result.valid = true;
    limit = qMax(0, limit);

    // 保持目录优先、名称排序；堆只保留前 limit 项，避免全量 QFileInfo 分配和排序。
    // 全局排序仍需遍历目录，但每项之间都可取消旧查询。
    struct Entry { QFileInfo info; QString name; bool directory; };
    auto before = [](const Entry &a, const Entry &b) {
        if (a.directory != b.directory) return a.directory;
        const int order = QString::compare(a.name, b.name, Qt::CaseInsensitive);
        return order ? order < 0 : a.name < b.name;
    };
    std::vector<Entry> best;
    best.reserve(limit);
    QDirIterator it(result.path, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        if (cancelled()) { result.cancelled = true; return result; }
        it.next();
        if (!limit) { result.truncated = true; break; }
        const QFileInfo info = it.fileInfo();
        Entry entry{info, info.fileName(), info.isDir()};
        if (best.size() < static_cast<size_t>(limit)) {
            best.push_back(std::move(entry));
            std::push_heap(best.begin(), best.end(), before);
        } else {
            result.truncated = true;
            if (before(entry, best.front())) {
                std::pop_heap(best.begin(), best.end(), before);
                best.back() = std::move(entry);
                std::push_heap(best.begin(), best.end(), before);
            }
        }
    }
    if (cancelled()) { result.cancelled = true; return result; }
    std::sort_heap(best.begin(), best.end(), before);
    result.entries.reserve(static_cast<qsizetype>(best.size()));
    for (const Entry &entry : best) result.entries.append(entry.info);
    return result;
}
}
