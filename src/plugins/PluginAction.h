#pragma once

#include <QString>
#include <QVariantMap>

// 结果项 action 字段的分类规则。
// 判定必须和副作用（起进程、开浏览器、写剪贴板）分开，才能在无 GUI 的进程里测试。
namespace PluginAction {

// 内置动作类型；type 字段是插件与界面之间的约定，大小写敏感。
enum class Kind {
    Invalid,   // 缺 type、type 未知，或该动作要求的 data 为空
    Launch,    // 启动程序
    OpenUrl,   // 用系统默认方式打开 URL
    Copy       // 把 data 写入剪贴板
};

struct Request {
    bool valid = false;          // type 非空即为有效请求
    QString type;                // 原始 type 值，透传给插件
    QString pluginCommand;       // 归一化（小写）后的插件命令，可能为空
    QString payload;             // data 原文
    Kind builtin = Kind::Invalid; // 仅按 type 判定的内置动作
};

/// 解析动作意图，不产生任何副作用。
Request parse(const QVariantMap &action);

} // namespace PluginAction
