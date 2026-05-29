#pragma once

#include <QObject>
#include <QSettings>

class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(int hotkeyModifiers READ hotkeyModifiers WRITE setHotkeyModifiers NOTIFY hotkeyModifiersChanged)
    Q_PROPERTY(int hotkeyKey READ hotkeyKey WRITE setHotkeyKey NOTIFY hotkeyKeyChanged)
    Q_PROPERTY(QString searchEngine READ searchEngine WRITE setSearchEngine NOTIFY searchEngineChanged)
    Q_PROPERTY(bool pluginAutoEnter READ pluginAutoEnter WRITE setPluginAutoEnter NOTIFY pluginAutoEnterChanged)
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool showFloatIcon READ showFloatIcon WRITE setShowFloatIcon NOTIFY showFloatIconChanged)
    Q_PROPERTY(bool autoHideOnFocusLoss READ autoHideOnFocusLoss WRITE setAutoHideOnFocusLoss NOTIFY autoHideOnFocusLossChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    int hotkeyModifiers() const;
    void setHotkeyModifiers(int mods);

    int hotkeyKey() const;
    void setHotkeyKey(int key);

    QString searchEngine() const;
    void setSearchEngine(const QString &engine);

    Q_INVOKABLE QString hotkeyText() const;

    // 插件启用管理
    Q_INVOKABLE bool isPluginEnabled(const QString &name) const;
    Q_INVOKABLE void setPluginEnabled(const QString &name, bool enabled);

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

signals:
    void hotkeyModifiersChanged();
    void hotkeyKeyChanged();
    void searchEngineChanged();
    void pluginAutoEnterChanged();
    void autoStartChanged();
    void themeChanged();
    void showFloatIconChanged();
    void autoHideOnFocusLossChanged();

private:
    QSettings m_settings;
    int m_modifiers = 0;
    int m_key = 0;
    bool m_autoStart = false;
    void syncAutoStartRegistry(bool enabled);
};
