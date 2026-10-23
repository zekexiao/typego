#pragma once

#include "PluginDescriptor.h"
#include <TypeGoPlugin/Plugin.h>
#include <QJsonObject>

struct PluginManifest {
    PluginDescriptor descriptor;
    QString version;
    QString abi;
    QJsonObject metadata;
};

struct PluginManifestResult {
    PluginManifest manifest;
    TypeGo::PluginApi::Error error;
};

// 仅解析元数据，不执行插件代码；可用隔离输入验证兼容策略。
PluginManifestResult parsePluginManifest(const QJsonObject &metadata,
                                        const QString &expectedAbi = TypeGo::PluginApi::nativeAbi());
bool validPluginParameters(const QVariantMap &parameters);
