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
#include <QProcess>
#include <QScreen>
#include <QSet>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QLocale>

#include "AppLauncher.h"
#include "GlobalHotkey.h"
#include "Logger.h"
#include "MainWindow.h"
#include "Settings.h"
#include "ThemeManager.h"
#include "plugins/AppPlugin.h"
#include "plugins/CalcPlugin.h"
#include "plugins/ConvertPlugin.h"
#include "plugins/DirectoryPlugin.h"
#include "plugins/FilePlugin.h"
#include "plugins/PluginManager.h"
#include "plugins/SettingsPlugin.h"
#include "plugins/ShellPlugin.h"
#include "plugins/TodoPlugin.h"

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
    Logger::instance().init();
    qInstallMessageHandler(Logger::qtMessageHandler);

    SingleApplication app(argc, argv, false, SingleApplication::Mode::User, 1000,
                          QStringLiteral("TypeGo"));
    // 版本号由 CMake 从根目录 VERSION 文件注入，避免多处硬编码不一致。
    app.setApplicationVersion(QStringLiteral(TYPEGO_VERSION));
    app.setQuitOnLastWindowClosed(false);
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
    pluginManager.registerPlugin(new AppPlugin(&launcher, &pluginManager));
    pluginManager.registerPlugin(new CalcPlugin(&pluginManager));
    pluginManager.registerPlugin(new ConvertPlugin(&pluginManager));
    pluginManager.registerPlugin(new DirectoryPlugin(&pluginManager));
    auto *settingsPlugin = new SettingsPlugin(&settings, &pluginManager, &pluginManager);
    pluginManager.registerPlugin(settingsPlugin);
    pluginManager.setSettings(&settings);
    pluginManager.registerPlugin(new FilePlugin(&pluginManager));
    pluginManager.registerPlugin(new ShellPlugin(&pluginManager));
    auto *todoPlugin = new TodoPlugin(&pluginManager);
    pluginManager.registerPlugin(todoPlugin);

    MainWindow window(&launcher, &pluginManager, &settings, nullptr, &themeManager);
    window.setWindowIcon(appIcon);

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
            window.hide();
        } else {
            window.showAndActivate();
        }
    });

    auto reRegister = [&]() {
        hotkey.registerHotkey(settings.globalHotkey());
    };
    QObject::connect(&settings, &Settings::globalHotkeyChanged, &hotkey, reRegister);
    reRegister();

    auto registerPluginHotkeys = [&]() {
        hotkey.clearPluginHotkeys();
        QSet<IPlugin *> registered;
        IPlugin *settingsPluginInstance = pluginManager.plugin(QStringLiteral("set"));
        for (const QString &command : pluginManager.commands()) {
            IPlugin *plugin = pluginManager.plugin(command);
            if (!plugin || plugin == settingsPluginInstance || registered.contains(plugin)) continue;
            registered.insert(plugin);
            if (!settings.isPluginEnabled(plugin->command())) continue;
            hotkey.registerPluginHotkey(plugin->command(), settings.pluginHotkey(plugin->command()));
        }
    };
    QObject::connect(&settings, &Settings::pluginHotkeyChanged, &hotkey, registerPluginHotkeys);
    QObject::connect(&settings, &Settings::pluginEnabledChanged, &hotkey, registerPluginHotkeys);
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
        window.openPluginView(settingsPlugin->command());
    });
    QObject::connect(pinFileAction, &QAction::triggered, &window, [&]() {
        const QString path = QFileDialog::getOpenFileName(&window, QCoreApplication::translate("Main", "Pin File"));
        if (path.isEmpty()) return;
        const QFileInfo info(path);
        launcher.pinApp(path, info.fileName(), QString());
    });
    QObject::connect(restartAction, &QAction::triggered, &app, [&app]() {
        QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList());
        app.quit();
    });
    QObject::connect(quitAction, &QAction::triggered, &app, &SingleApplication::quit);
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, &window, [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason != QSystemTrayIcon::Trigger) return;
        window.isVisible() ? window.hide() : window.showAndActivate();
    });
    trayIcon.show();

    return app.exec();
}
