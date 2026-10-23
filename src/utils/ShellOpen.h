#pragma once

#include <QString>

// 用系统默认程序打开文件或目录。
// Directory/File 等插件共用这一入口，避免各自维护平台分支。
namespace ShellOpen {
bool open(const QString &path);
}
