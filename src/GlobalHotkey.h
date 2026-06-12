#pragma once

#include <QHash>
#include <QObject>

class QHotkey;

// 使用 QHotkey 注册并分发应用及插件的全局快捷键。
class GlobalHotkey : public QObject {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject *parent = nullptr);

    bool registerHotkey(const QString &shortcut);
    void clearPluginHotkeys();
    bool registerPluginHotkey(const QString &command, const QString &shortcut);

signals:
    void activated();
    void commandActivated(const QString &command);

private:
    QHotkey *m_mainHotkey;
    QHash<QString, QHotkey *> m_pluginHotkeys;
};
