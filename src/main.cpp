#include <singleapplication.h>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QQuickWindow>
#include <QStyle>
#include <QDebug>
#include <QTranslator>
#include <QLocale>
#include <QIcon>
#include <QPixmap>
#include <QSvgRenderer>
#include <QPainter>
#include <QWKQuick/qwkquickglobal.h>
#include "AppLauncher.h"
#include "GlobalHotkey.h"
#include "Settings.h"
#include "InputHighlighter.h"
#include "Logger.h"
#include "plugins/PluginManager.h"
#include "plugins/AppPlugin.h"
#include "plugins/CalcPlugin.h"
#include "plugins/DirectoryPlugin.h"
#include "plugins/SettingsPlugin.h"
#include "plugins/FilePlugin.h"
#include "plugins/TodoPlugin.h"

int main(int argc, char *argv[])
{
    Logger::instance().init();
    qInstallMessageHandler(Logger::qtMessageHandler);
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    SingleApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    // Load custom icon from SVG
    QIcon appIcon;
    QSvgRenderer iconRenderer(QStringLiteral(":/assets/icon.svg"));
    const QList<int> iconSizes = {16, 24, 32, 48, 64, 128, 256};
    for (int sz : iconSizes) {
        QPixmap pix(sz, sz);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        iconRenderer.render(&painter);
        painter.end();
        appIcon.addPixmap(pix);
    }
    app.setWindowIcon(appIcon);

    // Load translations
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &lang : uiLanguages) {
        const QString baseName = "neoStart_" + QLocale(lang).name();
        if (translator.load(":/i18n/" + baseName) ||
            translator.load(baseName, QCoreApplication::applicationDirPath() + "/i18n")) {
            app.installTranslator(&translator);
            break;
        }
    }

    AppLauncher launcher;
    Settings settings;
    GlobalHotkey hotkey;

    PluginManager pluginManager;
    pluginManager.registerPlugin(new AppPlugin(&launcher, &pluginManager));
    pluginManager.registerPlugin(new CalcPlugin(&pluginManager));
    pluginManager.registerPlugin(new DirectoryPlugin(&pluginManager));
    pluginManager.registerPlugin(new SettingsPlugin(&pluginManager));
    pluginManager.setSettings(&settings);
    pluginManager.registerPlugin(new FilePlugin(&pluginManager));
    TodoPlugin *todoPlugin = new TodoPlugin(&pluginManager);
    pluginManager.registerPlugin(todoPlugin);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("taskTreeModel", todoPlugin->treeModel());
    QWK::registerTypes(&engine);
    engine.rootContext()->setContextProperty("appLauncher", &launcher);
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("pluginManager", &pluginManager);

    InputHighlighter inputHighlighter;
    engine.rootContext()->setContextProperty("inputHighlighter", &inputHighlighter);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Failed to load QML";
        return -1;
    }

    QQuickWindow *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) {
        qWarning() << "Root object is not a window";
        return -1;
    }
    window->setIcon(appIcon);

    // 单例模式：第二个实例启动时，第一个实例收到通知并转到前台
    QObject::connect(&app, &SingleApplication::instanceStarted, window, [window]() {
        window->show();
        window->raise();
        window->requestActivate();
    });

    // 全局热键：激活时显示/隐藏窗口
    QObject::connect(&hotkey, &GlobalHotkey::activated, window, [window]() {
        if (window->isVisible()) {
            window->hide();
        } else {
            window->show();
            window->raise();
            window->requestActivate();
        }
    });

    // 设置变化时重新注册热键
    auto reRegister = [&]() {
        hotkey.registerHotkey(settings.hotkeyModifiers(), settings.hotkeyKey());
    };
    QObject::connect(&settings, &Settings::hotkeyModifiersChanged, &hotkey, reRegister);
    QObject::connect(&settings, &Settings::hotkeyKeyChanged, &hotkey, reRegister);
    reRegister();

    // 系统托盘
    QSystemTrayIcon trayIcon;
    trayIcon.setIcon(appIcon);
    trayIcon.setToolTip("neoStart");

    QMenu trayMenu;
    QAction *showAction = trayMenu.addAction(QCoreApplication::translate("Main", "Show"));
    QAction *settingsAction = trayMenu.addAction(QCoreApplication::translate("Main", "Settings"));
    QAction *quitAction = trayMenu.addAction(QCoreApplication::translate("Main", "Quit"));
    trayIcon.setContextMenu(&trayMenu);

    QObject::connect(showAction, &QAction::triggered, window, [window]() {
        window->show();
        window->raise();
        window->requestActivate();
    });

    QObject::connect(settingsAction, &QAction::triggered, window, [window]() {
        window->show();
        window->raise();
        window->requestActivate();
        QMetaObject::invokeMethod(window, "openSettings");
    });

    QObject::connect(quitAction, &QAction::triggered, &app, &SingleApplication::quit);

    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, window, [window](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            if (window->isVisible()) {
                window->hide();
            } else {
                window->show();
                window->raise();
                window->requestActivate();
            }
        }
    });

    trayIcon.show();

    return app.exec();
}
