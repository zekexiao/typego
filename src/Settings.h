#pragma once

#include <QObject>
#include <QSettings>

class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(int hotkeyModifiers READ hotkeyModifiers WRITE setHotkeyModifiers NOTIFY hotkeyModifiersChanged)
    Q_PROPERTY(int hotkeyKey READ hotkeyKey WRITE setHotkeyKey NOTIFY hotkeyKeyChanged)
    Q_PROPERTY(QString searchEngine READ searchEngine WRITE setSearchEngine NOTIFY searchEngineChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    int hotkeyModifiers() const;
    void setHotkeyModifiers(int mods);

    int hotkeyKey() const;
    void setHotkeyKey(int key);

    QString searchEngine() const;
    void setSearchEngine(const QString &engine);

    Q_INVOKABLE QString hotkeyText() const;

    // 插件启用管理
    Q_INVOKABLE bool isPluginEnabled(const QString &name) const;
    Q_INVOKABLE void setPluginEnabled(const QString &name, bool enabled);

signals:
    void hotkeyModifiersChanged();
    void hotkeyKeyChanged();
    void searchEngineChanged();

private:
    QSettings m_settings;
    int m_modifiers = 0;
    int m_key = 0;
};
