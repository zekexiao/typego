#include "Settings.h"
#include <QGuiApplication>

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_settings("neoStart", "neoStart")
{
    m_modifiers = m_settings.value("hotkeyModifiers", 0x0C000000).toInt(); // Qt::AltModifier | Qt::ControlModifier
    m_key = m_settings.value("hotkeyKey", 0x20).toInt(); // Qt::Key_Space
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

int Settings::hotkeyModifiers() const
{
    return m_modifiers;
}

void Settings::setHotkeyModifiers(int mods)
{
    if (m_modifiers == mods) return;
    m_modifiers = mods;
    m_settings.setValue("hotkeyModifiers", m_modifiers);
    emit hotkeyModifiersChanged();
}

int Settings::hotkeyKey() const
{
    return m_key;
}

void Settings::setHotkeyKey(int key)
{
    if (m_key == key) return;
    m_key = key;
    m_settings.setValue("hotkeyKey", m_key);
    emit hotkeyKeyChanged();
}

QString Settings::hotkeyText() const
{
    QStringList parts;
    if (m_modifiers & 0x08000000) parts << "Alt";      // Qt::AltModifier
    if (m_modifiers & 0x04000000) parts << "Ctrl";     // Qt::ControlModifier
    if (m_modifiers & 0x02000000) parts << "Shift";    // Qt::ShiftModifier
    if (m_modifiers & 0x10000000) parts << "Meta";     // Qt::MetaModifier

    QString keyStr;
    switch (m_key) {
    case 0x20: keyStr = "Space"; break;
    case 0x30: keyStr = "0"; break;
    case 0x31: keyStr = "1"; break;
    case 0x32: keyStr = "2"; break;
    case 0x33: keyStr = "3"; break;
    case 0x34: keyStr = "4"; break;
    case 0x35: keyStr = "5"; break;
    case 0x36: keyStr = "6"; break;
    case 0x37: keyStr = "7"; break;
    case 0x38: keyStr = "8"; break;
    case 0x39: keyStr = "9"; break;
    default:
        if (m_key >= 0x41 && m_key <= 0x5A) {
            keyStr = QChar(m_key);
        } else {
            keyStr = QString("Key_%1").arg(m_key, 0, 16);
        }
    }

    parts << keyStr;
    return parts.join("+");
}

bool Settings::isPluginEnabled(const QString &name) const
{
    return m_settings.value("plugins/" + name, true).toBool();
}

void Settings::setPluginEnabled(const QString &name, bool enabled)
{
    qDebug() << "setPluginEnabled:" << name << enabled;
    m_settings.setValue("plugins/" + name, enabled);
}
