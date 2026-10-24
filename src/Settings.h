#pragma once

#include <QObject>
#include <QPoint>
#include <QSettings>

// 统一读取、保存并通知应用设置变更。
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString globalHotkey READ globalHotkey WRITE setGlobalHotkey NOTIFY globalHotkeyChanged)
    Q_PROPERTY(QString searchEngine READ searchEngine WRITE setSearchEngine NOTIFY searchEngineChanged)
    Q_PROPERTY(bool pluginAutoEnter READ pluginAutoEnter WRITE setPluginAutoEnter NOTIFY pluginAutoEnterChanged)
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool frostedGlass READ frostedGlass WRITE setFrostedGlass NOTIFY frostedGlassChanged)
    Q_PROPERTY(bool showFloatIcon READ showFloatIcon WRITE setShowFloatIcon NOTIFY showFloatIconChanged)
    Q_PROPERTY(bool autoHideOnFocusLoss READ autoHideOnFocusLoss WRITE setAutoHideOnFocusLoss NOTIFY autoHideOnFocusLossChanged)
    Q_PROPERTY(bool appLaunchRequiresDoubleClick READ appLaunchRequiresDoubleClick WRITE setAppLaunchRequiresDoubleClick NOTIFY appLaunchRequiresDoubleClickChanged)
    Q_PROPERTY(bool showAllAppsWhenSearchEmpty READ showAllAppsWhenSearchEmpty WRITE setShowAllAppsWhenSearchEmpty NOTIFY showAllAppsWhenSearchEmptyChanged)
    Q_PROPERTY(bool showInTaskbar READ showInTaskbar WRITE setShowInTaskbar NOTIFY showInTaskbarChanged)
    Q_PROPERTY(bool hideUninstallEntries READ hideUninstallEntries WRITE setHideUninstallEntries NOTIFY hideUninstallEntriesChanged)
    Q_PROPERTY(bool startHiddenOnBoot READ startHiddenOnBoot WRITE setStartHiddenOnBoot NOTIFY startHiddenOnBootChanged)
    Q_PROPERTY(bool customScanDirsRecursive READ customScanDirsRecursive WRITE setCustomScanDirsRecursive NOTIFY customScanDirsRecursiveChanged)
    Q_PROPERTY(QString uiFontFamily READ uiFontFamily WRITE setUiFontFamily NOTIFY uiFontFamilyChanged)

public:
    explicit Settings(QObject *parent = nullptr);
    /// 指定 ini 文件后端的构造，供单元测试和隔离配置使用：不读写注册表。
    explicit Settings(const QString &iniFilePath, QObject *parent = nullptr);

    /// 启动时把开机自启项与实际安装路径对齐。构造函数不再做这件事，
    /// 避免「读一下配置」就产生写 HKCU Run 键的副作用。
    void reconcileAutoStart();

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
    bool frostedGlass() const;
    void setFrostedGlass(bool enabled);

    bool showFloatIcon() const;
    void setShowFloatIcon(bool enabled);
    Q_INVOKABLE QPoint floatIconPos() const;
    bool hasFloatIconPos() const;
    Q_INVOKABLE void setFloatIconPos(const QPoint &pos);

    bool autoHideOnFocusLoss() const;
    void setAutoHideOnFocusLoss(bool enabled);

    bool appLaunchRequiresDoubleClick() const;
    void setAppLaunchRequiresDoubleClick(bool enabled);

    bool showAllAppsWhenSearchEmpty() const;
    void setShowAllAppsWhenSearchEmpty(bool enabled);

    bool showInTaskbar() const;
    void setShowInTaskbar(bool enabled);

    bool hideUninstallEntries() const;
    void setHideUninstallEntries(bool enabled);

    bool startHiddenOnBoot() const;
    void setStartHiddenOnBoot(bool enabled);

    QStringList customScanDirs() const;
    void setCustomScanDirs(const QStringList &dirs);
    bool customScanDirsRecursive() const;
    void setCustomScanDirsRecursive(bool enabled);

    QString uiFontFamily() const;
    void setUiFontFamily(const QString &family);

signals:
    void globalHotkeyChanged();
    void searchEngineChanged();
    void pluginAutoEnterChanged();
    void pluginEnabledChanged(const QString &name);
    void pluginHotkeyChanged(const QString &name);
    void autoStartChanged();
    void themeChanged();
    void frostedGlassChanged();
    void showFloatIconChanged();
    void autoHideOnFocusLossChanged();
    void appLaunchRequiresDoubleClickChanged();
    void showAllAppsWhenSearchEmptyChanged();
    void showInTaskbarChanged();
    void hideUninstallEntriesChanged();
    void startHiddenOnBootChanged();
    void customScanDirsChanged();
    void customScanDirsRecursiveChanged();
    void uiFontFamilyChanged();

private:
    QSettings m_settings;
    bool m_autoStart = false;
    // 仅生产构造（注册表后端）允许同步 Run 键，隔离配置构造关闭，防止测试污染本机。
    bool m_allowRegistrySync = true;
    void syncAutoStartRegistry(bool enabled);
};
