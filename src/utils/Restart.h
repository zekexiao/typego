#pragma once
#include <QtGlobal>

namespace Restart {
// 必须在创建单实例应用和日志器之前等待，避免锁冲突及旧实例日志被提前打开。
bool waitForParent(qint64 pid, unsigned long timeoutMs = 30000);
}
