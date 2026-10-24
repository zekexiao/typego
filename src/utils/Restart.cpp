#include "Restart.h"
#include <limits>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

bool Restart::waitForParent(qint64 pid, unsigned long timeoutMs)
{
#ifdef Q_OS_WIN
    if (pid <= 0 || pid > (std::numeric_limits<DWORD>::max)() || pid == GetCurrentProcessId()) return false;
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    // 旧进程已退出时无需等待；权限错误不能冒充退出成功。
    if (!process) return GetLastError() == ERROR_INVALID_PARAMETER;
    const DWORD result = WaitForSingleObject(process, timeoutMs);
    CloseHandle(process);
    return result == WAIT_OBJECT_0;
#else
    Q_UNUSED(pid)
    Q_UNUSED(timeoutMs)
    return false;
#endif
}
