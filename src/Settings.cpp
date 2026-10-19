#include "Settings.h"
#include <QGuiApplication>
#include <QKeySequence>

static Qt::Key legacyHotkeyKey(int key)
{
    if (key >= 0x70 && key <= 0x7B) {
        return static_cast<Qt::Key>(Qt::Key_F1 + key - 0x70);
    }
    return static_cast<Qt::Key>(key);
}

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_settings("TypeGo", "TypeGo")
{
    m_autoStart = m_settings.value("autoStart", false).toBool();
    syncAutoStartRegistry(m_autoStart);
}

QString Settings::searchEngine() const
{
    return m_settings.value("searchEngine", "Bing").toString();
}

void Settings::setSearchEngine(const QString &engine)
{
    QString current = searchEngine();
    if (current == engine) return;
    m_settings.setValue("searchEngine", engine);
    emit searchEngineChanged();
}

QString Settings::globalHotkey() const
{
    if (m_settings.contains(QStringLiteral("globalHotkey"))) {
        return m_settings.value(QStringLiteral("globalHotkey")).toString();
    }

    // 兼容旧版拆分保存的修饰键和按键。
    const int modifiers = m_settings.value(QStringLiteral("hotkeyModifiers"),
                                           static_cast<int>(Qt::AltModifier | Qt::ControlModifier)).toInt();
    const int key = m_settings.value(QStringLiteral("hotkeyKey"), static_cast<int>(Qt::Key_Space)).toInt();
    const QKeySequence sequence(QKeyCombination(Qt::KeyboardModifiers(modifiers), legacyHotkeyKey(key)));
    return sequence.toString(QKeySequence::PortableText);
}

void Settings::setGlobalHotkey(const QString &shortcut)
{
    if (globalHotkey() == shortcut) return;
    m_settings.setValue(QStringLiteral("globalHotkey"), shortcut);
    emit globalHotkeyChanged();
}

bool Settings::isPluginEnabled(const QString &name) const
{
    return m_settings.value("plugins/" + name, true).toBool();
}

void Settings::setPluginEnabled(const QString &name, bool enabled)
{
    if (isPluginEnabled(name) == enabled) return;
    m_settings.setValue("plugins/" + name, enabled);
    emit pluginEnabledChanged(name);
}

QString Settings::pluginHotkey(const QString &name) const
{
    return m_settings.value("pluginHotkeys/" + name).toString();
}

void Settings::setPluginHotkey(const QString &name, const QString &shortcut)
{
    if (pluginHotkey(name) == shortcut) return;
    m_settings.setValue("pluginHotkeys/" + name, shortcut);
    emit pluginHotkeyChanged(name);
}

bool Settings::pluginAutoEnter() const
{
    return m_settings.value("pluginAutoEnter", false).toBool();
}

void Settings::setPluginAutoEnter(bool enabled)
{
    if (pluginAutoEnter() == enabled) return;
    m_settings.setValue("pluginAutoEnter", enabled);
    emit pluginAutoEnterChanged();
}

bool Settings::autoStart() const
{
    return m_autoStart;
}

void Settings::setAutoStart(bool enabled)
{
    if (m_autoStart == enabled) return;
    m_autoStart = enabled;
    m_settings.setValue("autoStart", enabled);
    syncAutoStartRegistry(enabled);
    emit autoStartChanged();
}

QString Settings::theme() const
{
    return m_settings.value("theme", QStringLiteral("system")).toString();
}

bool Settings::frostedGlass() const
{
    return m_settings.value("frostedGlass", true).toBool();
}

void Settings::setFrostedGlass(bool enabled)
{
    if (frostedGlass() == enabled) return;
    m_settings.setValue("frostedGlass", enabled);
    emit frostedGlassChanged();
}

void Settings::setTheme(const QString &theme)
{
    QString current = this->theme();
    if (current == theme) return;
    m_settings.setValue("theme", theme);
    emit themeChanged();
}

bool Settings::showFloatIcon() const
{
    return m_settings.value("showFloatIcon", true).toBool();
}

void Settings::setShowFloatIcon(bool enabled)
{
    if (showFloatIcon() == enabled) return;
    m_settings.setValue("showFloatIcon", enabled);
    emit showFloatIconChanged();
}

QPoint Settings::floatIconPos() const
{
    int x = m_settings.value("floatIconX", -1).toInt();
    int y = m_settings.value("floatIconY", -1).toInt();
    return QPoint(x, y);
}

void Settings::setFloatIconPos(const QPoint &pos)
{
    m_settings.setValue("floatIconX", pos.x());
    m_settings.setValue("floatIconY", pos.y());
}

bool Settings::autoHideOnFocusLoss() const
{
    return m_settings.value("autoHideOnFocusLoss", false).toBool();
}

void Settings::setAutoHideOnFocusLoss(bool enabled)
{
    if (autoHideOnFocusLoss() == enabled) return;
    m_settings.setValue("autoHideOnFocusLoss", enabled);
    emit autoHideOnFocusLossChanged();
}

bool Settings::appLaunchRequiresDoubleClick() const
{
    return m_settings.value("appLaunchRequiresDoubleClick", true).toBool();
}

void Settings::setAppLaunchRequiresDoubleClick(bool enabled)
{
    if (appLaunchRequiresDoubleClick() == enabled) return;
    m_settings.setValue("appLaunchRequiresDoubleClick", enabled);
    emit appLaunchRequiresDoubleClickChanged();
}

bool Settings::showAllAppsWhenSearchEmpty() const
{
    return m_settings.value("showAllAppsWhenSearchEmpty", true).toBool();
}

void Settings::setShowAllAppsWhenSearchEmpty(bool enabled)
{
    if (showAllAppsWhenSearchEmpty() == enabled) return;
    m_settings.setValue("showAllAppsWhenSearchEmpty", enabled);
    emit showAllAppsWhenSearchEmptyChanged();
}

bool Settings::showInTaskbar() const
{
    return m_settings.value("showInTaskbar", false).toBool();
}

void Settings::setShowInTaskbar(bool enabled)
{
    if (showInTaskbar() == enabled) return;
    m_settings.setValue("showInTaskbar", enabled);
    emit showInTaskbarChanged();
}

bool Settings::hideUninstallEntries() const
{
    return m_settings.value("hideUninstallEntries", true).toBool();
}

void Settings::setHideUninstallEntries(bool enabled)
{
    if (hideUninstallEntries() == enabled) return;
    m_settings.setValue("hideUninstallEntries", enabled);
    emit hideUninstallEntriesChanged();
}

bool Settings::startHiddenOnBoot() const
{
    return m_settings.value("startHiddenOnBoot", false).toBool();
}

void Settings::setStartHiddenOnBoot(bool enabled)
{
    if (startHiddenOnBoot() == enabled) return;
    m_settings.setValue("startHiddenOnBoot", enabled);
    syncAutoStartRegistry(m_autoStart);
    emit startHiddenOnBootChanged();
}

QStringList Settings::customScanDirs() const
{
    return m_settings.value("customScanDirs").toStringList();
}

void Settings::setCustomScanDirs(const QStringList &dirs)
{
    QStringList normalized;
    for (const QString &dir : dirs) {
        const QString trimmed = dir.trimmed();
        if (!trimmed.isEmpty() && !normalized.contains(trimmed, Qt::CaseInsensitive)) {
            normalized.append(trimmed);
        }
    }
    if (customScanDirs() == normalized) return;
    m_settings.setValue("customScanDirs", normalized);
    emit customScanDirsChanged();
}

bool Settings::customScanDirsRecursive() const
{
    return m_settings.value("customScanDirsRecursive", false).toBool();
}

void Settings::setCustomScanDirsRecursive(bool enabled)
{
    if (customScanDirsRecursive() == enabled) return;
    m_settings.setValue("customScanDirsRecursive", enabled);
    emit customScanDirsRecursiveChanged();
}

QString Settings::uiFontFamily() const
{
    if (m_settings.contains(QStringLiteral("uiFontFamily"))) {
        return m_settings.value(QStringLiteral("uiFontFamily")).toString();
    }
    if (m_settings.value(QStringLiteral("useSystemWenkaiFont"), false).toBool()) {
        return QStringLiteral("LXGW WenKai");
    }
    return QString();
}

void Settings::setUiFontFamily(const QString &family)
{
    const QString normalized = family.trimmed();
    if (uiFontFamily() == normalized) return;
    m_settings.setValue(QStringLiteral("uiFontFamily"), normalized);
    emit uiFontFamilyChanged();
}

void Settings::syncAutoStartRegistry(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enabled) {
        QString path = QCoreApplication::applicationFilePath();
        path.replace('/', '\\');
        QString command = QStringLiteral("\"%1\"").arg(path);
        if (startHiddenOnBoot()) command += QStringLiteral(" --background");
        reg.setValue("TypeGo", command);
    } else {
        reg.remove("TypeGo");
    }
#else
    Q_UNUSED(enabled)
#endif
}
