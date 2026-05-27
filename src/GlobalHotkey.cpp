#include "GlobalHotkey.h"
#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static int qtModifiersToWin(int qtMods)
{
#ifdef Q_OS_WIN
    int winMods = 0;
    if (qtMods & 0x08000000) winMods |= MOD_ALT;         // Qt::AltModifier
    if (qtMods & 0x04000000) winMods |= MOD_CONTROL;     // Qt::ControlModifier
    if (qtMods & 0x02000000) winMods |= MOD_SHIFT;       // Qt::ShiftModifier
    if (qtMods & 0x10000000) winMods |= MOD_WIN;         // Qt::MetaModifier
    return winMods;
#else
    return qtMods;
#endif
}

static int qtKeyToWin(int qtKey)
{
#ifdef Q_OS_WIN
    if (qtKey >= 0x30 && qtKey <= 0x5A) return qtKey;
    switch (qtKey) {
    case 0x20: return VK_SPACE;
    case 0x70: return VK_F1;
    case 0x71: return VK_F2;
    case 0x72: return VK_F3;
    case 0x73: return VK_F4;
    case 0x74: return VK_F5;
    case 0x75: return VK_F6;
    case 0x76: return VK_F7;
    case 0x77: return VK_F8;
    case 0x78: return VK_F9;
    case 0x79: return VK_F10;
    case 0x7A: return VK_F11;
    case 0x7B: return VK_F12;
    }
    return qtKey;
#else
    return qtKey;
#endif
}

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
}

bool GlobalHotkey::registerHotkey(int qtModifiers, int qtKey)
{
#ifdef Q_OS_WIN
    unregisterHotkey();

    int winMods = qtModifiersToWin(qtModifiers);
    int winKey = qtKeyToWin(qtKey);

    BOOL ok = RegisterHotKey(nullptr, m_hotkeyId, winMods | MOD_NOREPEAT, winKey);
    m_registered = ok;
    if (m_registered) {
        qDebug() << "Global hotkey registered:" << qtModifiers << "+" << qtKey;
    } else {
        qDebug() << "Failed to register global hotkey:" << qtModifiers << "+" << qtKey;
    }
    return m_registered;
#else
    return false;
#endif
}

void GlobalHotkey::unregisterHotkey()
{
#ifdef Q_OS_WIN
    if (m_registered) {
        UnregisterHotKey(nullptr, m_hotkeyId);
        m_registered = false;
    }
#endif
}

bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType)
    Q_UNUSED(result)
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == m_hotkeyId) {
        emit activated();
        return true;
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}
