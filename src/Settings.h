#pragma once

#include <QObject>
#include <QSettings>

// 统一读取、保存并通知应用设置变更。
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString globalHotkey READ globalHotkey WRITE setGlobalHotkey NOTIFY globalHotkeyChanged)
    Q_PROPERTY(QString searchEngine READ searchEngine WRITE setSearchEngine NOTIFY searchEngineChanged)
    Q_PROPERTY(bool pluginAutoEnter READ pluginAutoEnter WRITE setPluginAutoEnter NOTIFY pluginAutoEnterChanged)
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool showFloatIcon READ showFloatIcon WRITE setShowFloatIcon NOTIFY showFloatIconChanged)
    Q_PROPERTY(bool autoHideOnFocusLoss READ autoHideOnFocusLoss WRITE setAutoHideOnFocusLoss NOTIFY autoHideOnFocusLossChanged)
    Q_PROPERTY(bool appLaunchRequiresDoubleClick READ appLaunchRequiresDoubleClick WRITE setAppLaunchRequiresDoubleClick NOTIFY appLaunchRequiresDoubleClickChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    QString globalHotkey() const;
    void setGlobalHotkey(const QString &shortcut);

    QString searchEngine() const;
    void setSearchEngine(const QString &engine);

    // 插件启用管理
    Q_INVOKABLE bool isPluginEnabled(const QString &name) const;
    Q_INVOKABLE void setPluginEnabled(const QString &name, bool enabled);
    QString pluginHotkey(const QString &name) const;
    void setPluginHotkey(const QString &name, const QString &shortcut);

    // 插件全局空参数自动进入开关
    Q_INVOKABLE bool pluginAutoEnter() const;
    Q_INVOKABLE void setPluginAutoEnter(bool enabled);

    // 开机自启动
    bool autoStart() const;
    void setAutoStart(bool enabled);

    QString theme() const;
    void setTheme(const QString &theme);

    bool showFloatIcon() const;
    void setShowFloatIcon(bool enabled);
    Q_INVOKABLE QPoint floatIconPos() const;
    Q_INVOKABLE void setFloatIconPos(const QPoint &pos);

    bool autoHideOnFocusLoss() const;
    void setAutoHideOnFocusLoss(bool enabled);

    bool appLaunchRequiresDoubleClick() const;
    void setAppLaunchRequiresDoubleClick(bool enabled);

signals:
    void globalHotkeyChanged();
    void searchEngineChanged();
    void pluginAutoEnterChanged();
    void pluginEnabledChanged(const QString &name);
    void pluginHotkeyChanged(const QString &name);
    void autoStartChanged();
    void themeChanged();
    void showFloatIconChanged();
    void autoHideOnFocusLossChanged();
    void appLaunchRequiresDoubleClickChanged();

private:
    QSettings m_settings;
    bool m_autoStart = false;
    void syncAutoStartRegistry(bool enabled);
};
