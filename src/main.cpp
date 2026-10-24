#include <singleapplication.h>
#include <QApplication>
#include <QAction>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkProxyFactory>
#include <QProcess>
#include <QScreen>
#include <QSet>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QTranslator>
#include <QLocale>
#include <QScopeGuard>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "AppLauncher.h"
#include "GlobalHotkey.h"
#include "Logger.h"
#include "MainWindow.h"
#include "Settings.h"
#include "ThemeManager.h"
#include "Updater.h"
#include "utils/Restart.h"
#include "plugins/BuiltinPlugins.h"
#include "plugins/ExternalPluginRegistry.h"
#include "plugins/PluginManager.h"

static QIcon buildAppIcon()
{
    return QIcon(QStringLiteral(":/assets/icon.png"));
}

static void applyConfiguredFont(QApplication &app, const Settings &settings, const QFont &fallbackFont)
{
    const QString family = settings.uiFontFamily();
    if (!family.isEmpty() && QFontDatabase::families().contains(family, Qt::CaseInsensitive)) {
        QFont font(family);
        font.setPixelSize(14);
        app.setFont(font);
        return;
    }

    QFont font = fallbackFont;
    font.setPixelSize(14);
    app.setFont(font);
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--restart-parent") != 0) continue;
        bool valid = false;
        const qint64 pid = i + 1 < argc ? QString::fromLocal8Bit(argv[i + 1]).toLongLong(&valid) : 0;
        if (!valid || !Restart::waitForParent(pid)) {
#ifdef Q_OS_WIN
            // 此时翻译器尚未初始化，按 Windows 界面语言提供启动阶段错误提示。
            const bool chinese = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
            MessageBoxW(nullptr, chinese
                ? L"无法完成重启：旧进程未退出或无法等待。请关闭旧实例后重新打开 TypeGo。"
                : L"Cannot restart: the previous process has not exited or could not be waited for. Close it and reopen TypeGo.",
                L"TypeGo", MB_OK | MB_ICONWARNING);
#endif
            return 1;
        }
        break;
    }
    Logger::instance().init();
    qInstallMessageHandler(Logger::qtMessageHandler);

    SingleApplication app(argc, argv, false, SingleApplication::Mode::User, 1000,
                          QStringLiteral("TypeGo"));
    // 版本号由 CMake 从根目录 VERSION 文件注入，避免多处硬编码不一致。
    app.setApplicationVersion(QStringLiteral(TYPEGO_VERSION));
    app.setQuitOnLastWindowClosed(false);
    // 让更新检查等网络请求跟随系统代理设置（如用户开启了代理），不设置则默认直连。
    QNetworkProxyFactory::setUseSystemConfiguration(true);
#ifdef Q_OS_WIN
    // 安装器通过此互斥量识别运行实例，防止升级时覆盖仍在使用的文件。
    const HANDLE installationMutex = CreateMutexW(nullptr, FALSE, L"TypeGo.Installation");
    const auto releaseInstallationMutex = qScopeGuard([installationMutex]() {
        if (installationMutex) CloseHandle(installationMutex);
    });
#endif
    const bool startInBackground = app.arguments().contains(QStringLiteral("--background"));

    const QIcon appIcon = buildAppIcon();
    app.setWindowIcon(appIcon);
    const QFont defaultAppFont = app.font();

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &lang : uiLanguages) {
        const QString baseName = QStringLiteral("TypeGo_") + QLocale(lang).name();
        if (translator.load(QStringLiteral(":/i18n/") + baseName) ||
            translator.load(baseName, QCoreApplication::applicationDirPath() + QStringLiteral("/i18n"))) {
            app.installTranslator(&translator);
            break;
        }
    }

    Settings settings;
    // 自启项与实际安装路径对齐，原来由 Settings 构造函数负责，现在显式触发。
    settings.reconcileAutoStart();
    applyConfiguredFont(app, settings, defaultAppFont);
    AppLauncher launcher(&settings);
    ThemeManager themeManager;
    GlobalHotkey hotkey;

    themeManager.setTheme(settings.theme());
    themeManager.setFrostedGlass(settings.frostedGlass());
    auto applyGlobalStyle = [&]() {
        app.setStyleSheet(themeManager.styleSheet());
    };
    QObject::connect(&themeManager, &ThemeManager::paletteChanged, &app, applyGlobalStyle);
    QObject::connect(&themeManager, &ThemeManager::frostedGlassChanged, &app, applyGlobalStyle);
    applyGlobalStyle();
    QObject::connect(&settings, &Settings::themeChanged, &themeManager, [&]() {
        themeManager.setTheme(settings.theme());
    });
    QObject::connect(&settings, &Settings::frostedGlassChanged, &themeManager, [&]() {
        themeManager.setFrostedGlass(settings.frostedGlass());
    });
    QObject::connect(&settings, &Settings::uiFontFamilyChanged, &app, [&]() {
        applyConfiguredFont(app, settings, defaultAppFont);
    });
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &themeManager, [&]() {
        themeManager.refresh();
    });

    PluginManager pluginManager;
    Updater updater;
    pluginManager.setSettings(&settings);
    registerBuiltinPlugins(pluginManager, settings, hotkey, launcher, updater);
    // 扫描只读取 DLL 元数据；客户代码在首次使用时才加载。
    const QString localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    ExternalPluginRegistry externalPlugins(pluginManager, {
        localData + QStringLiteral("/plugins"), localData + QStringLiteral("/plugin-data"),
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/plugins"),
        QDir::tempPath() + QStringLiteral("/TypeGo/plugins")});
    externalPlugins.discover();

    MainWindow window(&launcher, &pluginManager, &settings, &updater, &themeManager);
    window.setWindowIcon(appIcon);
    if (!externalPlugins.diagnostics().isEmpty()) {
        QStringList errors;
        for (const auto &diagnostic : externalPlugins.diagnostics())
            errors.append(QStringLiteral("%1\n[%2] %3").arg(diagnostic.path)
                .arg(static_cast<int>(diagnostic.error.code)).arg(diagnostic.error.message));
        QTimer::singleShot(0, &window, [&window, detail = errors.join(QStringLiteral("\n\n"))]() {
            QMessageBox::warning(&window, QCoreApplication::translate("Main", "Plugin unavailable"),
                QCoreApplication::translate("Main", "Some plugins could not be registered. Check the package and SDK compatibility, then restart TypeGo.")
                    + QStringLiteral("\n\n") + detail);
        });
    }

    // 启动数秒后在后台静默检测 GitHub 更新，避免拖慢首屏、与托盘初始化抢占网络。
    // 仅发现新版本时才弹出提示；失败与无更新都不打扰用户。
    QTimer::singleShot(5000, &updater, &Updater::checkForUpdate);

    FloatIcon floatIcon(&settings, &themeManager, &window);
    QPoint floatPos = settings.floatIconPos();
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen && (floatPos.x() < 0 || floatPos.y() < 0)) {
        const QRect geo = screen->availableGeometry();
        floatIcon.move(geo.right() - 60, geo.bottom() - 60);
    } else {
        floatIcon.move(floatPos);
    }

    auto updateFloatVisible = [&]() {
        settings.showFloatIcon() ? floatIcon.show() : floatIcon.hide();
    };
    QObject::connect(&settings, &Settings::showFloatIconChanged, &floatIcon, updateFloatVisible);
    updateFloatVisible();

    QObject::connect(&app, &SingleApplication::instanceStarted, &window, &MainWindow::showAndActivate);
    if (!startInBackground) {
        window.showAndActivate();
    }

    QObject::connect(&hotkey, &GlobalHotkey::activated, &window, [&]() {
        if (window.isVisible()) {
            window.requestHide();
        } else {
            window.showAndActivate();
        }
    });

    auto registerPluginHotkeys = [&]() {
        QHash<QString, QString> pluginShortcuts;
        for (const QString &id : pluginManager.pluginIds()) {
            const PluginDescriptor *descriptor = pluginManager.descriptor(id);
            if (!descriptor->configurable || !settings.isPluginEnabled(id)) continue;
            for (const PluginCommand &command : descriptor->commands) {
                pluginShortcuts.insert(command.keyword, settings.pluginHotkey(command.keyword));
            }
        }
        QString error;
        if (!hotkey.applyShortcuts(settings.globalHotkey(), pluginShortcuts, &error)) {
            // 启动时插件快捷键有冲突，仍尽量保留主窗口入口。
            QString fallbackError;
            hotkey.applyShortcuts(settings.globalHotkey(), {}, &fallbackError);
            QTimer::singleShot(0, &window, [&window, error]() {
                QMessageBox::warning(&window, QCoreApplication::translate("Main", "Shortcut unavailable"),
                                     error + QCoreApplication::translate("Main", "\nOpen Settings from the tray icon to choose another shortcut."));
            });
        }
    };
    QObject::connect(&hotkey, &GlobalHotkey::commandActivated, &window, &MainWindow::activateCommand);
    registerPluginHotkeys();

    QSystemTrayIcon trayIcon;
    trayIcon.setIcon(appIcon);
    trayIcon.setToolTip(QStringLiteral("TypeGo"));

    QMenu trayMenu;
    QAction *showAction = trayMenu.addAction(QCoreApplication::translate("Main", "Show"));
    QAction *settingsAction = trayMenu.addAction(QCoreApplication::translate("Main", "Settings"));
    QAction *pinFileAction = trayMenu.addAction(QCoreApplication::translate("Main", "Pin File..."));
    QAction *restartAction = trayMenu.addAction(QCoreApplication::translate("Main", "Restart"));
    QAction *quitAction = trayMenu.addAction(QCoreApplication::translate("Main", "Quit"));
    trayIcon.setContextMenu(&trayMenu);

    QObject::connect(showAction, &QAction::triggered, &window, &MainWindow::showAndActivate);
    QObject::connect(settingsAction, &QAction::triggered, &window, [&]() {
        window.showAndActivate();
        window.openPluginView(QStringLiteral("set"));
    });
    QObject::connect(pinFileAction, &QAction::triggered, &window, [&]() {
        const QString path = QFileDialog::getOpenFileName(&window, QCoreApplication::translate("Main", "Pin File"));
        if (path.isEmpty()) return;
        const QFileInfo info(path);
        launcher.pinApp(path, info.fileName(), QString());
    });
    auto requestExit = [&](bool restart) {
        pluginManager.requestShutdown([&app, &window, restart](bool accepted) {
            if (!accepted) {
                window.showAndActivate();
                QMessageBox::warning(&window, QCoreApplication::translate("Main", "Cannot exit"),
                    QCoreApplication::translate("Main", "Save your changes and try again."));
                return;
            }
            if (restart && !QProcess::startDetached(QCoreApplication::applicationFilePath(),
                    {QStringLiteral("--restart-parent"), QString::number(QCoreApplication::applicationPid())})) {
                window.showAndActivate();
                QMessageBox::warning(&window, QCoreApplication::translate("Main", "Cannot restart"),
                    QCoreApplication::translate("Main", "The new process could not be started. TypeGo is still running; please try again."));
                return;
            }
            app.quit();
        });
    };
    QObject::connect(restartAction, &QAction::triggered, &app, [&]() {
        requestExit(true);
    });
    QObject::connect(quitAction, &QAction::triggered, &app, [&]() { requestExit(false); });
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, &window, [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason != QSystemTrayIcon::Trigger) return;
        window.isVisible() ? window.requestHide() : window.showAndActivate();
    });
    trayIcon.show();

    return app.exec();
}
