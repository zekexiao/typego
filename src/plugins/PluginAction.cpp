#include "PluginAction.h"

namespace PluginAction {

namespace {

// type 到内置动作的映射；未知类型返回 Invalid，交给插件或不执行。
Kind kindFromType(const QString &type)
{
    if (type == QLatin1String("launch")) return Kind::Launch;
    if (type == QLatin1String("openUrl")) return Kind::OpenUrl;
    if (type == QLatin1String("copy")) return Kind::Copy;
    return Kind::Invalid;
}

} // namespace

Request parse(const QVariantMap &action)
{
    Request request;
    request.type = action.value(QStringLiteral("type")).toString();
    if (request.type.isEmpty()) return request;   // valid 保持 false

    request.valid = true;
    request.pluginCommand = action.value(QStringLiteral("plugin")).toString().trimmed().toLower();
    request.payload = action.value(QStringLiteral("data")).toString();
    request.builtin = kindFromType(request.type);
    // 内置动作没有 data 就没有意义，降级为 Invalid。
    if (request.builtin != Kind::Invalid && request.payload.isEmpty()) {
        request.builtin = Kind::Invalid;
    }
    return request;
}

} // namespace PluginAction
