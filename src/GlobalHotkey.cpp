#include "GlobalHotkey.h"

#include <QHotkey>
#include <QKeySequence>
#include <utility>

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
{
}

bool GlobalHotkey::applyShortcuts(const QString &mainShortcut, const QHash<QString, QString> &pluginShortcuts, QString *error)
{
    QHash<QString, QString> owners;
    QString nextMain;
    QHash<QString, QString> nextPlugins;
    auto validate = [&](const QString &shortcut, const QString &owner, QString *normalized) {
        if (shortcut.trimmed().isEmpty()) return true;
        const QKeySequence sequence(shortcut, QKeySequence::PortableText);
        if (sequence.isEmpty() || sequence.count() != 1 || sequence[0].key() == Qt::Key_unknown) {
            *error = tr("Invalid shortcut for %1.").arg(owner);
            return false;
        }
        *normalized = sequence.toString(QKeySequence::PortableText);
        if (owners.contains(*normalized)) {
            *error = tr("Shortcut %1 is already assigned to %2.").arg(*normalized, owners.value(*normalized));
            return false;
        }
        owners.insert(*normalized, owner);
        return true;
    };
    if (!validate(mainShortcut, tr("Global hotkey"), &nextMain)) return false;
    for (auto it = pluginShortcuts.cbegin(); it != pluginShortcuts.cend(); ++it) {
        QString normalized;
        if (!validate(it.value(), it.key(), &normalized)) return false;
        if (!normalized.isEmpty()) nextPlugins.insert(it.key(), normalized);
    }

    QHash<QString, QHotkey *> nextHotkeys;
    QList<QHotkey *> added;
    for (auto it = owners.cbegin(); it != owners.cend(); ++it) {
        const QString key = it.key();
        QHotkey *hotkey = m_hotkeys.value(key, nullptr);
        if (!hotkey) {
            hotkey = new QHotkey(this);
            if (!hotkey->setShortcut(QKeySequence(key, QKeySequence::PortableText), true)) {
                delete hotkey;
                qDeleteAll(added);
                *error = tr("Cannot register %1 for %2. It may be in use by another application. Previous shortcuts have been kept.")
                             .arg(key, it.value());
                return false;
            }
            added.append(hotkey);
            connect(hotkey, &QHotkey::activated, this, [this, key]() {
                if (key == m_mainShortcut) emit activated();
                else {
                    for (auto it = m_pluginShortcuts.cbegin(); it != m_pluginShortcuts.cend(); ++it) {
                        if (it.value() == key) {
                            emit commandActivated(it.key());
                            break;
                        }
                    }
                }
            });
        }
        nextHotkeys.insert(key, hotkey);
    }
    // 全部新注册成功后才释放旧快捷键，复用注册也允许主快捷键与插件交换。
    for (auto it = m_hotkeys.cbegin(); it != m_hotkeys.cend(); ++it) {
        if (!nextHotkeys.contains(it.key())) delete it.value();
    }
    m_hotkeys = std::move(nextHotkeys);
    m_mainShortcut = nextMain;
    m_pluginShortcuts = std::move(nextPlugins);
    return true;
}
