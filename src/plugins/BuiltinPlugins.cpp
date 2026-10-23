#include "BuiltinPlugins.h"
#include "AppPlugin.h"
#include "CalcPlugin.h"
#include "ConvertPlugin.h"
#include "DirectoryPlugin.h"
#include "FilePlugin.h"
#include "PluginManager.h"
#include "SettingsPlugin.h"
#include "ShellPlugin.h"
#include "TodoPlugin.h"
#include <QCoreApplication>

void registerBuiltinPlugins(PluginManager &manager, Settings &settings, GlobalHotkey &hotkeys,
                            AppLauncher &launcher, Updater &updater)
{
    auto add = [&manager](const QString &id, const QString &description, PluginManager::Factory factory,
                          const QStringList &aliases = {}, IPlugin::LaunchMode mode = IPlugin::LaunchMode::List,
                          bool arguments = false, bool autoEnter = false, bool configurable = true,
                          bool hasSettings = false) {
        PluginDescriptor descriptor;
        descriptor.id = id;
        descriptor.description = description;
        descriptor.configurable = configurable;
        descriptor.hasSettings = hasSettings;
        descriptor.commands.append(PluginCommand{id, id, aliases, mode, arguments, autoEnter});
        const bool registered = manager.registerPlugin(std::move(descriptor), std::move(factory));
        Q_ASSERT(registered);
        Q_UNUSED(registered)
    };
    add(QStringLiteral("app"), QCoreApplication::translate("AppPlugin", "Search and launch applications"),
        [&launcher](QObject *parent) { return new AppPlugin(&launcher, parent); });
    add(QStringLiteral("calc"), QCoreApplication::translate("CalcPlugin", "Calculator"),
        [](QObject *parent) { return new CalcPlugin(parent); });
    add(QStringLiteral("convert"), QCoreApplication::translate("ConvertPlugin", "Unit converter"),
        [](QObject *parent) { return new ConvertPlugin(parent); }, {QStringLiteral("conv")},
        IPlugin::LaunchMode::QueryView, true);
    add(QStringLiteral("cd"), QCoreApplication::translate("DirectoryPlugin", "Directory browser and navigation"),
        [](QObject *parent) { return new DirectoryPlugin(parent); });
    add(QStringLiteral("set"), QCoreApplication::translate("SettingsPlugin", "Open Settings"),
        [&settings, &manager, &hotkeys, &launcher, &updater](QObject *parent) {
            return new SettingsPlugin(&settings, &manager, &hotkeys, &launcher, &updater, parent);
        }, {QStringLiteral("user"), QStringLiteral("plugin"), QStringLiteral("scan"), QStringLiteral("about")},
        IPlugin::LaunchMode::FullView, false, true, false);
    add(QStringLiteral("file"), QCoreApplication::translate("FilePlugin", "File search (Everything)"),
        [](QObject *parent) { return new FilePlugin(parent); });
    add(QStringLiteral("shell"), QCoreApplication::translate("ShellPlugin", "Run shell commands"),
        [](QObject *parent) { return new ShellPlugin(parent); }, {QStringLiteral("cmd")},
        IPlugin::LaunchMode::QueryView, true, false, true, true);
    add(QStringLiteral("todo"), QCoreApplication::translate("TodoPlugin", "Task Management"),
        [](QObject *parent) { return new TodoPlugin(parent); }, {}, IPlugin::LaunchMode::FullView, false, true);
}
