#pragma once

#include <QWidget>

class PluginManager;
class Settings;
class QCheckBox;
class QComboBox;
class QListWidget;
class QStackedWidget;

// 提供用户、设置和插件管理的双栏界面。
class SettingsView : public QWidget {
    Q_OBJECT
public:
    SettingsView(Settings *settings, PluginManager *pluginManager, QWidget *parent = nullptr);
    void reload();
    void showSection(const QString &command);

signals:
    void closeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    int selectedKey() const;
    int keyIndex(int key) const;

    Settings *m_settings;
    PluginManager *m_pluginManager;
    QListWidget *m_navigation;
    QStackedWidget *m_pages;
    QCheckBox *m_altCheck;
    QCheckBox *m_ctrlCheck;
    QCheckBox *m_shiftCheck;
    QComboBox *m_keyCombo;
    QComboBox *m_themeCombo;
    QComboBox *m_engineCombo;
    QCheckBox *m_autoStartCheck;
    QCheckBox *m_floatIconCheck;
    QCheckBox *m_autoHideCheck;
    QCheckBox *m_appDoubleClickCheck;
    QCheckBox *m_pluginAutoEnterCheck;
    QList<QCheckBox *> m_pluginChecks;
};
