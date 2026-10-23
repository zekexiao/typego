#include "ShellOpen.h"

#include <QDesktopServices>
#include <QUrl>

bool ShellOpen::open(const QString &path)
{
    if (path.isEmpty()) return false;
    // QDesktopServices 在 Windows 上走 ShellExecute 默认动词，等价于手写 "open"。
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
