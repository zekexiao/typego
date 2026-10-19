#pragma once

#include <QHash>
#include <QObject>

class QHotkey;

// 使用 QHotkey 注册并分发应用及插件的全局快捷键。
class GlobalHotkey : public QObject {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject *parent = nullptr);

    bool applyShortcuts(const QString &mainShortcut, const QHash<QString, QString> &pluginShortcuts, QString *error);

signals:
    void activated();
    void commandActivated(const QString &command);

private:
    QHash<QString, QHotkey *> m_hotkeys;
    QString m_mainShortcut;
    QHash<QString, QString> m_pluginShortcuts;
};
