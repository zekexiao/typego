#pragma once

#include <QObject>
#include <QAbstractNativeEventFilter>

// 注册并监听系统级全局快捷键。
class GlobalHotkey : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey();

    bool registerHotkey(int qtModifiers, int qtKey);
    void unregisterHotkey();

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void activated();

private:
    bool m_registered = false;
    int m_hotkeyId = 1;
};
