#pragma once

#include "IPlugin.h"
#include <QList>

// 元信息不依赖插件实例，设置页和快捷键枚举不会触发业务初始化。
struct PluginCommand {
    QString id;
    QString keyword;
    QStringList aliases;
    IPlugin::LaunchMode launchMode = IPlugin::LaunchMode::List;
    bool requiresArguments = false;
    bool supportsAutoEnter = false;
    bool directActivation = false;
    PluginPresentation presentation = PluginPresentation::Embedded;
};

struct PluginDescriptor {
    QString id;
    QString description;
    QList<PluginCommand> commands;
    bool configurable = true;
    bool hasSettings = false;
};
