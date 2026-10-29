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
    bool supportsDetach = false;
    PluginPresentation presentation = PluginPresentation::Embedded;
};

struct PluginDescriptor {
    QString id;
    QString description;
    QList<PluginCommand> commands;
    bool configurable = true;
    bool hasSettings = false;
    // 需要持续后台工作的插件（如剪贴板）在启动时初始化，而不是首次使用时。
    bool background = false;
};
