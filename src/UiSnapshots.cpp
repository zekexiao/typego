#include "UiSnapshots.h"
#include "AppLauncher.h"
#include "GlobalHotkey.h"
#include "MainWindow.h"
#include "PluginSettingsDialog.h"
#include "Settings.h"
#include "SettingsView.h"
#include "ThemeManager.h"
#include "UpdateDialog.h"
#include "Updater.h"
#include "plugins/BuiltinPlugins.h"
#include "plugins/IntegratedFeatures.h"
#include "plugins/PluginManager.h"
#include "plugins/ShellPlugin.h"
#include "../plugins/clipboard/ClipboardSettingsWidget.h"
#include "../plugins/screenshot/ScreenshotSettingsWidget.h"
#include "../plugins/screenshot/CaptureToolbar.h"
#include "../plugins/screenshot/PinEditor.h"
#include "../plugins/screenshot/ScrollResultWindow.h"
#include "../plugins/screenshot/ScrollCaptureSession.h"
#include "../plugins/screenshot/ScreenshotSession.h"
#include "../plugins/screenshot/ScreenOverlay.h"
#include "../plugins/screenshot/PinWindow.h"
#include "../plugins/neo-json-viewer/MainWindow.h"
#include "../plugins/neo-json-viewer/AboutDialog.h"
#include "../plugins/neo-json-viewer/AddItemDialog.h"

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <memory>
#include <vector>

namespace {
// 等待真实布局、延迟查询和图标绘制完成，禁止用固定长休眠阻塞界面线程。
void settle()
{
    QEventLoop loop;
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
}

// 唯一的界面夹具入口；只注入结果数据，仍使用生产窗口、模型和绘制代理。
class UiSnapshotRunner {
public:
    explicit UiSnapshotRunner(QString directory, QStringList groups)
        : m_directory(std::move(directory)), m_groups(std::move(groups)) {}

    // 分组开关：包含 all 或该分组名时才生成，避免每次交付无关截图。
    bool wants(const QString &group) const
    {
        return m_groups.contains(QStringLiteral("all")) || m_groups.contains(group);
    }

    bool capture(QWidget &widget, const QString &id, const QSize &size = {})
    {
        widget.setProperty("skipWindowMaterial", true);
        widget.setProperty("frostedWindow", false);
        widget.style()->unpolish(&widget);
        widget.style()->polish(&widget);
        if (size.isValid()) widget.resize(size);
        widget.show();
        settle();
        const QPixmap pixmap = widget.grab();
        const QString name = m_theme + QLatin1Char('-') + id + QStringLiteral(".png");
        if (pixmap.isNull() || !pixmap.save(QDir(m_directory).filePath(name))) {
            m_error = QStringLiteral("Cannot save screenshot: ") + name;
            return false;
        }
        m_images.append(QJsonObject{{"id", id}, {"theme", m_theme}, {"file", name},
            {"width", pixmap.width()}, {"height", pixmap.height()}});
        return true;
    }

    bool run(const QString &temporary)
    {
        for (const QString &theme : {QStringLiteral("light"), QStringLiteral("dark")}) {
            m_theme = theme;
            const QString data = temporary + QLatin1Char('/') + theme;
            QDir().mkpath(data);
            Settings settings(data + QStringLiteral("/settings.ini"));
            settings.setTheme(theme);
            settings.setFrostedGlass(false);
            settings.setAutoHideOnFocusLoss(false);
            settings.setShowAllAppsWhenSearchEmpty(true);
            ThemeManager palette;
            palette.setFrostedGlass(false);
            palette.setTheme(theme);
            qApp->setStyleSheet(palette.styleSheet());
            QList<AppItem> apps;
            for (const auto &name : {QStringLiteral("Visual Studio Code"), QStringLiteral("Terminal"),
                                     QStringLiteral("Notepad"), QStringLiteral("File Explorer")}) {
                AppItem item{name, QStringLiteral("C:/Example/") + name + QStringLiteral(".exe"),
                             QStringLiteral("Example application"), QStringLiteral(":/assets/icon.png"), {}};
                item.match = AppMatcher::buildEntry(item.name, item.path, item.desc);
                apps.append(std::move(item));
            }
            AppLauncher launcher(&settings, data, apps);
            launcher.pinApp(apps.first().path, apps.first().name, apps.first().iconPath);
            GlobalHotkey hotkeys;
            Updater updater;
            PluginManager plugins;
            plugins.setSettings(&settings);
            registerBuiltinPlugins(plugins, settings, hotkeys, launcher, updater, palette);
            // 仅注册描述，不启动后台服务；剪贴板历史使用下面的静态结果夹具。
            registerIntegratedFeatures(plugins, {data, data + QStringLiteral("/cache"), data + QStringLiteral("/temp")});
            if (wants(QStringLiteral("main")))
            {
                MainWindow window(&launcher, &plugins, &settings, &updater, &palette);
                window.setProperty("skipWindowMaterial", true);
                if (!capture(window, QStringLiteral("launcher"))) return false;
                window.m_searchEdit->setText(QStringLiteral("calc 128 * 1.08"));
                window.performSearch();
                if (!capture(window, QStringLiteral("calculator"))) return false;
                window.m_searchEdit->setText(QStringLiteral("zz_no_matching_application_zz"));
                window.performSearch();
                if (!capture(window, QStringLiteral("search-empty"))) return false;
                // 屏蔽编辑信号，防止展示示例历史时实例化剪贴板监控。
                {
                    QSignalBlocker block(window.m_searchEdit->editor());
                    window.m_searchEdit->setText(QStringLiteral("clip "));
                }
                window.m_searchDebounce->stop();
                QVariantList history;
                for (const auto &text : {QStringLiteral("Design review notes"),
                    QStringLiteral("https://heroui.com"), QStringLiteral("npm run build")})
                    history.append(QVariantMap{{"title", text}, {"subtitle", QStringLiteral("Example clipboard item")},
                        {"icon", QStringLiteral(":/assets/icon.png")},
                        {"action", QVariantMap{{"type", QStringLiteral("none")}}}});
                window.setResults(history);
                window.m_commandMode = true;
                window.m_currentCmd = QStringLiteral("clip");
                window.showResults();
                if (!capture(window, QStringLiteral("clipboard-history"))) return false;
                {
                    QSignalBlocker block(window.m_searchEdit->editor());
                    window.m_searchEdit->setText(QStringLiteral("shot-history "));
                }
                for (int index = 0; index < history.size(); ++index) {
                    auto entry = history.at(index).toMap();
                    entry[QStringLiteral("title")] = QStringLiteral("Screenshot %1.png").arg(index + 1);
                    entry[QStringLiteral("subtitle")] = QStringLiteral("960 x 560 / Example screenshot");
                    history[index] = entry;
                }
                window.setResults(history);
                window.m_currentCmd = QStringLiteral("shot-history");
                window.showResults();
                if (!capture(window, QStringLiteral("screenshot-history"))) return false;
                const QString directory = data + QStringLiteral("/Example");
                QDir().mkpath(directory + QStringLiteral("/Documents"));
                if (!writeFile(directory + QStringLiteral("/README.txt"), QByteArrayLiteral("TypeGo fixture"))) return false;
                window.m_searchEdit->setText(QStringLiteral("cd ") + directory);
                window.performSearch();
                settle();
                if (!capture(window, QStringLiteral("directory"))) return false;
                FloatIcon icon(&settings, &palette, &window);
                if (!capture(icon, QStringLiteral("floating-icon"))) return false;
            }
            if (wants(QStringLiteral("tools")))
            {
                MainWindow window(&launcher, &plugins, &settings, &updater, &palette);
                window.setProperty("skipWindowMaterial", true);
                if (!window.openPluginView(QStringLiteral("convert"), QStringLiteral("10 km"))) {
                    m_error = QStringLiteral("Cannot open converter view");
                    return false;
                }
                if (!capture(window, QStringLiteral("converter"), {960, 560})) return false;
            }
            if (wants(QStringLiteral("settings")))
            {
                MainWindow window(&launcher, &plugins, &settings, &updater, &palette);
                window.setProperty("skipWindowMaterial", true);
                const QStringList sections{QStringLiteral("set"), QStringLiteral("plugin"),
                    QStringLiteral("scan"), QStringLiteral("user"), QStringLiteral("about")};
                for (const auto &section : sections) {
                    if (!window.openPluginView(section)) {
                        m_error = QStringLiteral("Cannot open settings section: ") + section;
                        return false;
                    }
                    window.resize(960, 560);
                    window.show();
                    settle();
                    auto *view = window.findChild<SettingsView *>();
                    if (!view) { m_error = QStringLiteral("Settings view missing"); return false; }
                    QScrollArea *active = nullptr;
                    for (auto *scroll : view->findChildren<QScrollArea *>())
                        if (scroll->isVisible()) { active = scroll; break; }
                    if (!active) { m_error = QStringLiteral("Settings scroll area missing"); return false; }
                    auto *bar = active->verticalScrollBar();
                    bar->setValue(0);
                    settle();
                    int part = 1;
                    for (;;) {
                        if (!capture(window, QStringLiteral("settings-%1-%2").arg(section).arg(part++))) return false;
                        if (bar->value() >= bar->maximum()) break;
                        const int next = qMin(bar->maximum(), bar->value() + qMax(1, active->viewport()->height() - 48));
                        bar->setValue(next);
                        settle();
                    }
                }
            }
            if (wants(QStringLiteral("tools")))
            {
                ClipboardSettingsWidget::Values values;
                values.count = 12;
                PluginSettingsDialog dialog(plugins.descriptor(QStringLiteral("clipboard"))->description,
                    new ClipboardSettingsWidget(values, nullptr, [](const auto &) {}, [] { return 0; }), &palette);
                if (!capture(dialog, QStringLiteral("clipboard-settings"), {560, 450})) return false;
            }
            if (wants(QStringLiteral("tools")))
            {
                ScreenshotSettingsWidget::Values values;
                values.saveDir = QStringLiteral("C:/Example/Screenshots");
                values.historyCount = 8;
                PluginSettingsDialog dialog(plugins.descriptor(QStringLiteral("screenshot"))->description,
                    new ScreenshotSettingsWidget(values, nullptr, [](const auto &) {}, [] { return 0; }), &palette);
                if (!capture(dialog, QStringLiteral("screenshot-settings"), {620, 490})) return false;
            }
            if (wants(QStringLiteral("tools")))
            {
                ShellPlugin shell;
                PluginSettingsDialog dialog(shell.description(), shell.settingsWidget(), &palette);
                if (!capture(dialog, QStringLiteral("shell-settings"), {620, 390})) return false;
                std::unique_ptr<QWidget> view(shell.createView(nullptr));
                if (!capture(*view, QStringLiteral("shell"), {960, 480})) return false;
            }
            if (wants(QStringLiteral("json")))
            {
                JsonViewerWindow json(nullptr, data + QStringLiteral("/json.ini"));
                if (!capture(json, QStringLiteral("json-empty"), {960, 560})) return false;
                auto *tree = json.findChild<QTreeView *>(QStringLiteral("jsonTreeView"));
                auto *model = tree ? qobject_cast<JsonTreeModel *>(tree->model()) : nullptr;
                if (!model || !model->loadFromString(QStringLiteral(R"({"name":"TypeGo","version":"1.12","enabled":true,"tools":["Search","Clipboard","Screenshot"],"appearance":{"theme":"system","radius":16},"optional":null})"))) {
                    m_error = QStringLiteral("Cannot populate JSON view"); return false;
                }
                tree->expandAll();
                tree->setCurrentIndex(model->index(0, 0));
                if (!capture(json, QStringLiteral("json-tree"))) return false;
            }
            if (wants(QStringLiteral("json")))
            {
                SearchDialog dialog;
                dialog.setOptions({QStringLiteral("name"), false, true, true, false, true});
                if (!capture(dialog, QStringLiteral("json-search"), {520, 360})) return false;
            }
            if (wants(QStringLiteral("json")))
            {
                SettingsDialog dialog(120, true);
                if (!capture(dialog, QStringLiteral("json-settings"), {520, 250})) return false;
            }
            if (wants(QStringLiteral("json")))
            {
                AddItemDialog dialog(false);
                auto *combo = dialog.findChild<QComboBox *>();
                if (!combo) { m_error = QStringLiteral("JSON type selector missing"); return false; }
                for (int index = 0; index < combo->count(); ++index) {
                    combo->setCurrentIndex(index);
                    if (!capture(dialog, QStringLiteral("json-add-%1").arg(index), {500, 300})) return false;
                }
                AddItemDialog array(true);
                if (!capture(array, QStringLiteral("json-add-array"), {500, 300})) return false;
            }
            if (wants(QStringLiteral("json")))
            {
                AboutDialog dialog;
                if (!capture(dialog, QStringLiteral("json-about"))) return false;
            }
            if (wants(QStringLiteral("update")))
            {
                ReleaseParser::Result result;
                result.outcome = ReleaseParser::Outcome::UpdateReady;
                result.currentVersion = QStringLiteral("1.11");
                result.latestVersion = QStringLiteral("1.12");
                ReleaseParser::ReleaseNote note;
                note.version = result.latestVersion;
                note.date = QStringLiteral("2026-10-31");
                note.chinese.append({QString::fromUtf8("更新界面与工具体验"), QString::fromUtf8("更新界面与工具体验")});
                note.english.append({QStringLiteral("Updated interface and tools"), QStringLiteral("Updated interface and tools")});
                result.notes.append(note);
                UpdateDialog dialog(result);
                if (!capture(dialog, QStringLiteral("update"), {700, 460})) return false;
            }
            if (wants(QStringLiteral("capture")))
            {
                CaptureToolbar toolbar;
                toolbar.setUndoRedo(true, false);
                if (!capture(toolbar, QStringLiteral("capture-toolbar"))) return false;
                CaptureToolbar pin(nullptr, CaptureToolbar::Mode::Pin);
                if (!capture(pin, QStringLiteral("pin-toolbar"))) return false;
            }
            if (wants(QStringLiteral("capture")))
            {
                QImage image(720, 420, QImage::Format_RGB32);
                image.fill(palette.surface());
                QPainter painter(&image);
                painter.setPen(palette.text());
                painter.setFont(qApp->font());
                painter.drawText(QRect(40, 40, 640, 100), QStringLiteral("TypeGo / Screenshot example"));
                painter.setPen(Qt::NoPen);
                painter.setBrush(palette.accentFill());
                painter.drawRoundedRect(QRect(40, 140, 640, 220), 16, 16);
                painter.end();
                PinEditor editor(image, {});
                if (!capture(editor, QStringLiteral("pin-editor"))) return false;
                PinWindow pinned(image, {}, QPoint(0, 0));
                if (!capture(pinned, QStringLiteral("pinned-image"))) return false;
                ScrollResultWindow result(image, QPoint(0, 0));
                if (!capture(result, QStringLiteral("scroll-result"), {960, 560})) return false;
                // 只注入合成桌面，不调用 start()，不截取桌面或改动真实窗口焦点。
                ScreenshotSession session(0, {});
                session.m_virtualImage = image;
                session.m_virtualLogical = image.rect();
                session.m_virtualPhysical = image.rect();
                ScreenInfo screen;
                screen.logical = image.rect();
                screen.physical = image.rect();
                screen.source = image.rect();
                session.m_screens.append(screen);
                session.m_selection = QRect(60, 80, 520, 270);
                session.m_cursor = QPointF(570, 340);
                session.m_hasCursor = true;
                ScreenOverlay overlay(&session, screen);
                if (!capture(overlay, QStringLiteral("capture-selection"))) return false;
                overlay.beginTextInput(QPointF(110, 180));
                if (!capture(overlay, QStringLiteral("capture-text"))) return false;
                overlay.cancelTextInput();
                ScrollCaptureSession scroll(session.m_selection, screen);
                scroll.createPanel();
                if (!capture(*scroll.m_panel, QStringLiteral("scroll-panel"))) return false;
            }
        }
        return finish();
    }

    QString error() const { return m_error; }

private:
    bool finish()
    {
        const QJsonObject manifest{{"schema", 2}, {"version", QCoreApplication::applicationVersion()},
            {"groups", QJsonArray::fromStringList(m_groups)},
            {"createdUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
            {"renderer", QStringLiteral("Qt Widgets / offscreen / zh_CN / 1x")},
            {"limitations", QJsonArray{QStringLiteral("OS blur, native file pickers, real desktop capture and interactive behavior require on-device verification.")}},
            {"images", m_images}};
        QByteArray html = QByteArrayLiteral(R"(<!doctype html><html lang='zh-CN'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>TypeGo UI</title><style>body{font:16px system-ui;margin:0 auto;padding:16px;max-width:1040px;background:#f4f4f5;color:#18181b}img{max-width:100%;height:auto;display:block;border-radius:12px}figure{margin:16px 0 32px}figcaption{margin:8px 0}summary{cursor:pointer;font-weight:600;padding:16px 0}a{color:inherit}header{position:sticky;top:0;background:inherit;padding:8px 0}nav{display:flex;gap:24px}@media(prefers-color-scheme:dark){body{background:#18181b;color:#f4f4f5}}</style><header><h1>TypeGo UI</h1><nav><a href='#light'>Light</a><a href='#dark'>Dark</a></nav></header>)");
        for (const QString &theme : {QStringLiteral("light"), QStringLiteral("dark")}) {
            html += "<details open id='" + theme.toUtf8() + "'><summary>" + theme.toUtf8() + "</summary>";
            for (const auto &value : m_images) {
                const auto entry = value.toObject();
                if (entry.value("theme").toString() != theme) continue;
                QFile file(QDir(m_directory).filePath(entry.value("file").toString()));
                if (!file.open(QIODevice::ReadOnly)) { m_error = file.errorString(); return false; }
                html += "<figure><figcaption>" + entry.value("id").toString().toHtmlEscaped().toUtf8()
                    + "</figcaption><img loading='lazy' alt='" + entry.value("id").toString().toHtmlEscaped().toUtf8()
                    + "' src='data:image/png;base64," + file.readAll().toBase64() + "'></figure>";
            }
            html += "</details>";
        }
        html += "</html>";
        if (!writeFile(QDir(m_directory).filePath(QStringLiteral("manifest.json")), QJsonDocument(manifest).toJson())
            || !writeFile(QDir(m_directory).filePath(QStringLiteral("index.html")), html)) {
            m_error = QStringLiteral("Cannot save screenshot manifest or gallery"); return false;
        }
        return true;
    }

    QString m_directory;
    QStringList m_groups;
    QString m_theme;
    QString m_error;
    QJsonArray m_images;
};

int runUiSnapshots(int argc, char *argv[])
{
    // 进程内重定向所有默认 QSettings，兼容仍使用默认后端的工具设置。
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temporary.path());
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_SCALE_FACTOR", "1");
    QLocale::setDefault(QLocale(QStringLiteral("zh_CN")));
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TypeGo-UI-Snapshots"));
    app.setApplicationVersion(QStringLiteral(TYPEGO_VERSION));
    app.setQuitOnLastWindowClosed(false);
    // offscreen 平台不枚举 Windows 系统字体，必须显式加载，防止截出全是方框的文字。
    const QString fonts = qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")) + QStringLiteral("/Fonts/");
    const int fontId = QFontDatabase::addApplicationFont(fonts + QStringLiteral("msyh.ttc"));
    if (fontId < 0) return 7;
    QFontDatabase::addApplicationFont(fonts + QStringLiteral("msyhbd.ttc"));
    QFontDatabase::addApplicationFont(fonts + QStringLiteral("consola.ttf"));
    QFont font(QFontDatabase::applicationFontFamilies(fontId).first());
    font.setPixelSize(14);
    const QFontMetrics metrics(font);
    if (!metrics.inFontUcs4(0x6c49) || !metrics.inFontUcs4('A')) return 7;
    app.setFont(font);
    std::vector<std::unique_ptr<QTranslator>> translators;
    for (const auto &path : {QStringLiteral(":/i18n/qtbase_zh_CN.qm"), QStringLiteral(":/i18n/TypeGo_zh_CN.qm"),
        QStringLiteral(":/integrated/Clipboard_zh_CN.qm"), QStringLiteral(":/integrated/Screenshot_zh_CN.qm"),
        QStringLiteral(":/integrated/NeoJsonViewer_zh_CN.qm")}) {
        auto translator = std::make_unique<QTranslator>();
        if (!translator->load(path)) return 3;
        app.installTranslator(translator.get());
        translators.push_back(std::move(translator));
    }
    const auto args = app.arguments();
    const int flag = args.indexOf(QStringLiteral("--ui-snapshots"));
    const QString root = QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../ui-snapshots")));
    // 页面分组：--pages=core / --pages=all / --pages=main,settings,json，缺省只看主界面输入态与设置页。
    QStringList groups;
    for (const auto &arg : args) {
        if (!arg.startsWith(QStringLiteral("--pages="))) continue;
        groups = arg.section(QLatin1Char('='), 1).toLower().split(QLatin1Char(','), Qt::SkipEmptyParts);
    }
    if (groups.isEmpty()) groups << QStringLiteral("main") << QStringLiteral("settings");
    // core 是 main+settings 的别名，展平后去重。
    if (groups.removeAll(QStringLiteral("core")) > 0) {
        groups << QStringLiteral("main") << QStringLiteral("settings");
    }
    groups.removeDuplicates();
    // 输出目录是 --ui-snapshots 之后第一个非选项参数，因此选项可以写在前面。
    QString next;
    for (int i = flag + 1; i < args.size(); ++i) {
        if (!args.at(i).startsWith(QStringLiteral("--"))) { next = args.at(i); break; }
    }
    const QString output = next.isEmpty()
        ? QDir(root).filePath(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")))
        : QDir(next).absolutePath();
    // 拒绝覆盖旧批次，避免把过期截图误认为本次成果。
    if (QDir(output).exists() || !QDir().mkpath(output)) return 4;
    // 夹具目录用固定路径：随机临时目录会被写进目录浏览截图，让跨批次对比永远报“有变化”。
    const QString fixture = QDir(root).filePath(QStringLiteral("_fixture"));
    QDir(fixture).removeRecursively();
    if (!QDir().mkpath(fixture)) return 2;
    UiSnapshotRunner runner(output, groups);
    if (!runner.run(fixture)) {
        writeFile(QDir(output).filePath(QStringLiteral("FAILED.txt")), runner.error().toUtf8());
        return 5;
    }
    QDir().mkpath(root);
    return writeFile(QDir(root).filePath(QStringLiteral("LATEST.txt")), output.toUtf8()) ? 0 : 6;
}
