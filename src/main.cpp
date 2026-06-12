#include <singleapplication.h>
#include <QApplication>
#include <QAction>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QScreen>
#include <QSet>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QLocale>

#include "AppLauncher.h"
#include "GlobalHotkey.h"
#include "Logger.h"
#include "MainWindow.h"
#include "Settings.h"
#include "ThemeManager.h"
#include "Updater.h"
#include "plugins/AppPlugin.h"
#include "plugins/CalcPlugin.h"
#include "plugins/DirectoryPlugin.h"
#include "plugins/FilePlugin.h"
#include "plugins/PluginManager.h"
#include "plugins/SettingsPlugin.h"
#include "plugins/TodoPlugin.h"

static QIcon buildAppIcon()
{
    QIcon appIcon;
    QSvgRenderer iconRenderer(QStringLiteral(":/assets/icon.svg"));
    const QList<int> iconSizes = {16, 24, 32, 48, 64, 128, 256};
    for (int size : iconSizes) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        iconRenderer.render(&painter);
        appIcon.addPixmap(pixmap);
    }
    return appIcon;
}

int main(int argc, char *argv[])
{
    Logger::instance().init();
    qInstallMessageHandler(Logger::qtMessageHandler);

    SingleApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    const QIcon appIcon = buildAppIcon();
    app.setWindowIcon(appIcon);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &lang : uiLanguages) {
        const QString baseName = QStringLiteral("qingzhou_") + QLocale(lang).name();
        if (translator.load(QStringLiteral(":/i18n/") + baseName) ||
            translator.load(baseName, QCoreApplication::applicationDirPath() + QStringLiteral("/i18n"))) {
            app.installTranslator(&translator);
            break;
        }
    }

    AppLauncher launcher;
    Settings settings;
    ThemeManager themeManager;
    GlobalHotkey hotkey;
    Updater updater;

    themeManager.setTheme(settings.theme());
    auto applyGlobalStyle = [&]() {
        app.setStyleSheet(themeManager.styleSheet());
    };
    QObject::connect(&themeManager, &ThemeManager::paletteChanged, &app, applyGlobalStyle);
    QObject::connect(&themeManager, &ThemeManager::themeChanged, &app, applyGlobalStyle);
    applyGlobalStyle();
    QObject::connect(&settings, &Settings::themeChanged, &themeManager, [&]() {
        themeManager.setTheme(settings.theme());
    });
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &themeManager, [&]() {
        themeManager.refresh();
    });

    PluginManager pluginManager;
    pluginManager.registerPlugin(new AppPlugin(&launcher, &pluginManager));
    pluginManager.registerPlugin(new CalcPlugin(&pluginManager));
    pluginManager.registerPlugin(new DirectoryPlugin(&pluginManager));
    auto *settingsPlugin = new SettingsPlugin(&settings, &pluginManager, &pluginManager);
    pluginManager.registerPlugin(settingsPlugin);
    pluginManager.setSettings(&settings);
    pluginManager.registerPlugin(new FilePlugin(&pluginManager));
    auto *todoPlugin = new TodoPlugin(&pluginManager);
    pluginManager.registerPlugin(todoPlugin);

    MainWindow window(&launcher, &pluginManager, &settings, &updater);
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
    trayIcon.setToolTip(QStringLiteral("Glide"));

    QMenu trayMenu;
    QAction *showAction = trayMenu.addAction(QCoreApplication::translate("Main", "Show"));
    QAction *settingsAction = trayMenu.addAction(QCoreApplication::translate("Main", "Settings"));
    QAction *restartAction = trayMenu.addAction(QCoreApplication::translate("Main", "Restart"));
    QAction *quitAction = trayMenu.addAction(QCoreApplication::translate("Main", "Quit"));
    trayIcon.setContextMenu(&trayMenu);

    QObject::connect(showAction, &QAction::triggered, &window, &MainWindow::showAndActivate);
    QObject::connect(settingsAction, &QAction::triggered, &window, [&]() {
        window.showAndActivate();
        window.openPluginView(settingsPlugin->command());
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
