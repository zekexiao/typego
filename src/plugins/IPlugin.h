#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QWidget;

// 定义命令插件必须实现的查询、执行和视图接口。
class IPlugin : public QObject {
    Q_OBJECT
public:
    enum class LaunchMode {
        List,
        FullView,
        QueryView
    };

    enum class QueryContext {
        Auto,
        Enter
    };

    explicit IPlugin(QObject *parent = nullptr) : QObject(parent) {}
    ~IPlugin() override = default;

    virtual QString command() const = 0;
    virtual QStringList aliases() const { return {}; }
    virtual QString description() const = 0;
    virtual QVariantList query(const QString &args) = 0;
    virtual QVariantList query(const QString &args, QueryContext context)
    {
        Q_UNUSED(context)
        return query(args);
    }
    virtual bool execute(const QVariantMap &action) = 0;
    virtual void cancelQuery() {}
    virtual LaunchMode launchMode(const QString &args) const
    {
        Q_UNUSED(args)
        return LaunchMode::List;
    }
    virtual bool supportsAutoEnter() const { return false; }
    virtual void activateCommand(const QString &command) { Q_UNUSED(command) }
    virtual void activateQuery(const QString &args) { Q_UNUSED(args) }

    // The widget is created lazily. Once inserted into a layout, Qt owns it.
    virtual QWidget *viewUrl() { return nullptr; }
    virtual bool hasSettingsWidget() const { return false; }
    virtual QWidget *settingsWidget() { return nullptr; }

signals:
    void viewCloseRequested();
    void queryResultsChanged(const QString &args);
};
