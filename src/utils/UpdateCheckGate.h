#pragma once

// 更新检查的去重策略。
// 启动后的静默检查与用户手动点击「检查更新」可能撞在一起：两次请求都会各自
// 弹一次更新提示。这里把「同一时刻只允许一个在途检查、手动请求并入在途检查」
// 的规则单独抽出来，既避免界面出现两个弹窗，也便于直接写单元测试。
namespace UpdateCheckGate {

enum class Mode {
    Automatic,   // 启动后的静默检查，失败与无更新都不打扰用户
    Manual       // 用户主动点击「检查更新」，结果必须给用户反馈
};

class Gate {
public:
    // 登记一次检查请求；返回 true 表示需要真正发起网络请求。
    bool request(Mode mode);

    // 结束当前检查，返回结果应归属的口径：期间有手动请求并入时按手动汇报。
    Mode finish();

    bool isBusy() const { return m_busy; }

private:
    bool m_busy = false;
    Mode m_mode = Mode::Automatic;
};

} // namespace UpdateCheckGate
