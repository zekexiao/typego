#pragma once

#include <QObject>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include "IPlugin.h"

class Settings;
class QWidget;

/**
 * @brief 插件管理器
 *
 * 维护 cmd -> plugin 的映射，提供 O(1) 命令查找。
 * 同时负责统一的结果动作执行分发。
 */
class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject *parent = nullptr);

    /// 设置 Settings 实例（用于插件启用检查）
    void setSettings(Settings *settings);

    /// 注册一个插件
    void registerPlugin(IPlugin *plugin);

    /// QML 可调用：执行命令查询
    /// cmd: 命令名，args: 参数
    /// 返回结果列表，每个元素是 QVariantMap
    Q_INVOKABLE QVariantList queryCommand(const QString &cmd, const QString &args);

    /// QML 可调用：执行某个结果项的动作
    Q_INVOKABLE bool executeAction(const QVariantMap &action);

    /// QML 可调用：获取所有命令名列表
    Q_INVOKABLE QStringList commands() const;

    /// QML 可调用：获取命令描述
    Q_INVOKABLE QString commandDescription(const QString &cmd) const;

    /// QML 可调用：获取命令对应的自定义视图 URL
    QWidget *viewForCommand(const QString &cmd) const;

    /// QML 可调用：命令无参数时是否直接进入
    Q_INVOKABLE bool autoEnterForCommand(const QString &cmd) const;

    /// 获取原始插件实例（用于 C++ 端自动注册上下文属性）
    IPlugin *plugin(const QString &cmd) const;

private:
    QHash<QString, IPlugin *> m_plugins;
    Settings *m_settings = nullptr;
};
