#pragma once

#include <QList>
#include <QString>

// 语义化版本号比较，供更新器判断远端 Release 是否比当前版本更新。
// 独立成模块是为了让这段规则可被单元测试直接覆盖，不依赖网络。
namespace Version {

// 解析 "v1.2.3" / "1.2" 形式为整数段；非数字段按 0 处理。
QList<int> parse(const QString &text);

// remote 相对 local 是否为更新版本；缺少的高位段按 0 补齐，相等时返回 false。
bool isNewer(const QString &remote, const QString &local);

} // namespace Version
