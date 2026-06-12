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
    , m_settings("qingzhou", "qingzhou")
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

void Settings::syncAutoStartRegistry(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enabled) {
        QString path = QCoreApplication::applicationFilePath();
        reg.setValue("qingzhou", path.replace('/', '\\'));
    } else {
        reg.remove("qingzhou");
    }
#else
    Q_UNUSED(enabled)
#endif
}
