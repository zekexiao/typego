#pragma once

#include <QWidget>
#include <QVariantList>
#include <QVariantMap>

class AppLauncher;
class PluginManager;
class Settings;
class ThemeManager;
class Updater;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QHBoxLayout;
class QProgressDialog;

// 管理应用主界面、搜索结果和插件视图。
class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(AppLauncher *launcher,
               PluginManager *pluginManager,
               Settings *settings,
               Updater *updater,
               QWidget *parent = nullptr);

public slots:
    bool openPluginView(const QString &command);
    void activateCommand(const QString &command);
    void showAndActivate();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;

private slots:
    void performSearch();
    void launchCurrent();
    void moveSelection(int delta);

private:
    QString searchUrl(const QString &query) const;
    void setResults(const QVariantList &results);
    QVariantMap currentItemData() const;
    void rebuildPinnedApps();
    void openResults();
    void updateCommandBar();

    AppLauncher *m_launcher;
    PluginManager *m_pluginManager;
    Settings *m_settings;
    Updater *m_updater;

    QLineEdit *m_searchEdit;
    QLabel *m_commandLabel;
    QWidget *m_pinnedPanel;
    QHBoxLayout *m_pinnedLayout;
    QListWidget *m_resultsList;
    QStackedWidget *m_stack;
    QProgressDialog *m_downloadDialog = nullptr;

    QVariantList m_results;
    bool m_commandMode = false;
    QString m_currentCmd;
    QString m_openPluginCmd;
};

// 提供可拖动的悬浮入口图标。
class FloatIcon : public QWidget {
    Q_OBJECT
public:
    FloatIcon(Settings *settings, ThemeManager *themeManager, QWidget *mainWindow, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    Settings *m_settings;
    ThemeManager *m_themeManager;
    QWidget *m_mainWindow;
    QPoint m_dragOffset;
    bool m_dragging = false;
};
