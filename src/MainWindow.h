#pragma once

#include <QFrame>
#include <QIcon>
#include <QPixmap>
#include <QSet>
#include <QStyledItemDelegate>
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
class QListView;
class QPushButton;
class ResultListModel;
class QStackedWidget;
class QHBoxLayout;
class QProgressDialog;
class QResizeEvent;

// 自绘圆形管理入口，确保悬浮背景不受按钮样式影响。
class ManagementIcon final : public QWidget {
    Q_OBJECT
public:
    explicit ManagementIcon(QWidget *parent = nullptr);

signals:
    void clicked();

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QIcon m_icon;
    bool m_hovered = false;
    bool m_pressed = false;
};

// 搜索结果列表的自绘项代理：标题/副标题分层、悬停和选中反馈、长文本省略。
class ResultItemDelegate final : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ResultItemDelegate(ThemeManager *themeManager, QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    ThemeManager *m_themeManager;
};

// 使用独立命令标签和参数输入框实现 token 输入。
class CommandLineEdit final : public QFrame {
    Q_OBJECT
public:
    explicit CommandLineEdit(QWidget *parent = nullptr);

    void setCommands(const QStringList &commands);
    void setPlaceholderText(const QString &text);
    QString text() const;
    void setText(const QString &text);
    QLineEdit *editor() const;

signals:
    void managementRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setToken(const QString &command, const QString &arguments);
    void clearToken(const QString &value);
    void setFocused(bool focused);

    QLabel *m_badge;
    QLineEdit *m_editor;
    ManagementIcon *m_managementIcon;
    QSet<QString> m_commands;
    QString m_command;
    QString m_defaultPlaceholder;
    bool m_updating = false;
};

// 管理应用主界面、搜索结果和插件视图。
class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(AppLauncher *launcher,
               PluginManager *pluginManager,
               Settings *settings,
               Updater *updater,
               ThemeManager *themeManager,
               QWidget *parent = nullptr);

public slots:
    bool openPluginView(const QString &command);
    bool openPluginView(const QString &command, const QString &args);
    void activateCommand(const QString &command);
    void showAndActivate();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void performSearch();
    void launchCurrent();
    void moveSelection(int delta);

private:
    // Esc 分层退出：插件视图回到结果页 -> 清空输入 -> 隐藏窗口。
    void handleEscape();

    QString searchUrl(const QString &query) const;
    void commandParts(QString *command, QString *args) const;
    void setResults(const QVariantList &results);
    QVariantMap currentItemData() const;
    void rebuildPinnedApps();
    void openResults();
    void updateCommandBar();
    void updateResultsLayout();
    void scheduleResultsLayout();
    void applyTaskbarVisibility();
    int resultsHeightForWindow(int windowHeight) const;

    AppLauncher *m_launcher;
    PluginManager *m_pluginManager;
    Settings *m_settings;
    Updater *m_updater;
    ThemeManager *m_themeManager;

    CommandLineEdit *m_searchEdit;
    QWidget *m_titleBar = nullptr;
    QLabel *m_commandLabel;
    QWidget *m_pinnedPanel;
    QHBoxLayout *m_pinnedLayout;
    QListView *m_resultsList;
    ResultListModel *m_resultsModel;
    QStackedWidget *m_stack;
    QWidget *m_pluginPage;
    QStackedWidget *m_pluginStack;
    QPushButton *m_pluginBackButton;
    QWidget *m_titleBackSlot = nullptr;
    QProgressDialog *m_downloadDialog = nullptr;

    QVariantList m_results;
    bool m_commandMode = false;
    QString m_currentCmd;
    QString m_openPluginCmd;
    int m_resultsMaximumWindowHeight = 460;
    int m_resultsBaseHeight = 0;
    bool m_resultsLayoutPending = false;
    bool m_titleBarDragging = false;
    QPoint m_titleBarDragGlobalPos;
    QPoint m_titleBarDragWindowPos;
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
    void updateIconCache();
    QRectF iconRect() const;

    Settings *m_settings;
    ThemeManager *m_themeManager;
    QWidget *m_mainWindow;
    QPixmap m_icon;
    QPixmap m_iconCache;
    qreal m_iconCacheDpr = 0.0;
    QPoint m_dragOffset;
    QPoint m_pressGlobalPos;
    bool m_dragging = false;
};
