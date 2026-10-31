#include "utils/UpdateCheckGate.h"

namespace UpdateCheckGate {

bool Gate::request(Mode mode)
{
    if (!m_busy) {
        m_busy = true;
        m_mode = mode;
        return true;
    }
    // 已有检查在进行：不再发起第二次请求，只把口径升级为手动，
    // 让这一次结果同时满足「静默检查」和「手动检查」两个入口的反馈需求。
    if (mode == Mode::Manual) m_mode = Mode::Manual;
    return false;
}

Mode Gate::finish()
{
    const Mode mode = m_mode;
    m_busy = false;
    m_mode = Mode::Automatic;
    return mode;
}

} // namespace UpdateCheckGate
