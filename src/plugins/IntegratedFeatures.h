#pragma once
#include <QString>
class PluginManager;
class ThemeManager;
struct PluginPaths { QString data; QString cache; QString temporary; };
// themeManager 可为空：不注入时内置功能自绘回退到系统调色板。
void registerIntegratedFeatures(PluginManager &manager, const PluginPaths &paths,
                                ThemeManager *themeManager = nullptr);
