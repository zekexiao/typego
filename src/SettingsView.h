#pragma once

#include <QCheckBox>
#include <QColor>
#include <QKeySequenceEdit>
#include <QList>
#include <QWidget>

class PluginManager;
class Settings;
class QComboBox;
class QListWidget;
class QStackedWidget;
class GlobalHotkey;
class AppLauncher;

class HotkeyEdit final : public QKeySequenceEdit {
    Q_OBJECT
public:
    explicit HotkeyEdit(QWidget *parent = nullptr);

protected:
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

// 设置项使用紧凑开关，颜色仍由全局主题样式注入。
class SettingsSwitch final : public QCheckBox {
    Q_OBJECT
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor)
public:
    explicit SettingsSwitch(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    QColor accentColor() const { return m_accentColor; }
    void setAccentColor(const QColor &color)
    {
        m_accentColor = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_accentColor = QColor(QStringLiteral("#0a84ff"));
};

class SettingsView : public QWidget {
    Q_OBJECT
public:
    SettingsView(Settings *settings, PluginManager *pluginManager, GlobalHotkey *hotkeys,
                 AppLauncher *launcher, QWidget *parent = nullptr);
    void reload();
    void showSection(const QString &command);

signals:
    void closeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Settings *m_settings;
    PluginManager *m_pluginManager;
    GlobalHotkey *m_hotkeys;
    AppLauncher *m_launcher;
    QListWidget *m_navigation;
    QStackedWidget *m_pages;
    HotkeyEdit *m_globalHotkeyEdit;
    QComboBox *m_themeCombo;
    QCheckBox *m_frostedGlassCheck;
    QComboBox *m_engineCombo;
    QCheckBox *m_autoStartCheck;
    QCheckBox *m_floatIconCheck;
    QCheckBox *m_autoHideCheck;
    QCheckBox *m_startHiddenOnBootCheck;
    QCheckBox *m_appDoubleClickCheck;
    QCheckBox *m_showAllAppsCheck;
    QCheckBox *m_showInTaskbarCheck;
    QCheckBox *m_hideUninstallCheck;
    QCheckBox *m_customScanRecursiveCheck;
    QComboBox *m_fontCombo;
    QListWidget *m_customScanDirsList;
    QCheckBox *m_pluginAutoEnterCheck;
    QList<QCheckBox *> m_pluginChecks;
    QList<HotkeyEdit *> m_pluginHotkeyEdits;
};
