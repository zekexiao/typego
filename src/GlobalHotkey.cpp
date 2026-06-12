#include "GlobalHotkey.h"

#include <QHotkey>
#include <QKeySequence>

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
    , m_mainHotkey(new QHotkey(this))
{
    connect(m_mainHotkey, &QHotkey::activated, this, &GlobalHotkey::activated);
}

bool GlobalHotkey::registerHotkey(const QString &shortcut)
{
    m_mainHotkey->setRegistered(false);
    const QKeySequence sequence(shortcut, QKeySequence::PortableText);
    return !sequence.isEmpty() && m_mainHotkey->setShortcut(sequence, true);
}

void GlobalHotkey::clearPluginHotkeys()
{
    qDeleteAll(m_pluginHotkeys);
    m_pluginHotkeys.clear();
}

bool GlobalHotkey::registerPluginHotkey(const QString &command, const QString &shortcut)
{
    const QKeySequence sequence(shortcut, QKeySequence::PortableText);
    if (command.isEmpty() || sequence.isEmpty()) return false;

    auto *hotkey = new QHotkey(sequence, true, this);
    connect(hotkey, &QHotkey::activated, this, [this, command]() {
        emit commandActivated(command);
    });
    m_pluginHotkeys.insert(command, hotkey);
    return hotkey->isRegistered();
}
