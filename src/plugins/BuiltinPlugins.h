#pragma once

class PluginManager;
class Settings;
class GlobalHotkey;
class AppLauncher;
class Updater;

void registerBuiltinPlugins(PluginManager &manager, Settings &settings, GlobalHotkey &hotkeys,
                            AppLauncher &launcher, Updater &updater);
