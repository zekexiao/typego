#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QHash>
#include <QString>

/**
 * @brief 插件接口
 *
 * 所有插件必须实现此接口。
 * 插件通过 command() 返回的命令名被 PluginManager O(1) 索引。
 */
class IPlugin : public QObject {
    Q_OBJECT
public:
    explicit IPlugin(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IPlugin() = default;

    /// 命令名，例如 "calc", "app", "cd"
    virtual QString command() const = 0;

    /// 命令描述，用于提示和补全
    virtual QString description() const = 0;

    /// 接收参数，返回结果列表
    /// 每个结果是 QVariantMap，必须包含：
    ///   - title (QString): 显示标题
    ///   - subtitle (QString): 副标题/描述
    ///   - icon (QString): 图标路径（可选）
    ///   - action (QVariantMap): 动作数据，包含 type 和 data
    virtual QVariantList query(const QString &args) = 0;

    /// 执行某个结果项的动作
    /// action 来自 query 返回结果中的 action 字段
    virtual bool execute(const QVariantMap &action) = 0;

    /// 可选：如果插件提供自定义视图，返回 QML 资源路径（如 "qrc:/qml/TodoView.qml"）
    virtual QString viewUrl() const { return QString(); }

    /// 可选：返回需要注入到 QML 引擎上下文的对象
    /// key 为属性名，value 为 QObject 指针
    virtual QHash<QString, QObject*> contextProperties() const { return {}; }
};
