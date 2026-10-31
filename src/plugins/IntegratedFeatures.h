#pragma once
#include <QString>
class PluginManager;
struct PluginPaths { QString data; QString cache; QString temporary; };
void registerIntegratedFeatures(PluginManager &manager, const PluginPaths &paths);
