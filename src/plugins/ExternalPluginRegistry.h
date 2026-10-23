#pragma once

#include "PluginManifest.h"
#include <memory>

class PluginManager;

struct PluginPaths {
    QString packages;
    QString data;
    QString cache;
    QString temporary;
};

struct PluginDiagnostic {
    QString path;
    QString pluginId;
    TypeGo::PluginApi::Error error;
};

// 根路径显式注入，测试与客户包发现均不依赖用户配置或注册表。
class ExternalPluginRegistry {
public:
    ExternalPluginRegistry(PluginManager &manager, PluginPaths paths);
    int discover();
    QList<PluginDiagnostic> diagnostics() const { return *m_diagnostics; }
private:
    PluginManager &m_manager;
    PluginPaths m_paths;
    std::shared_ptr<QList<PluginDiagnostic>> m_diagnostics;
};
