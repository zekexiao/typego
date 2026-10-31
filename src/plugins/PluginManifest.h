#pragma once

#include "PluginDescriptor.h"
#include <TypeGoPlugin/Plugin.h>
#include <QJsonObject>
#include <QStringList>

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
// 按给定界面语言顺序取插件描述译文，取不到时回退英文原文。
QString localizedPluginDescription(const QJsonObject &metadata, const QStringList &uiLanguages);
bool validPluginParameters(const QVariantMap &parameters);
