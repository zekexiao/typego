#include <QtTest>

#include <QKeySequence>
#include <QTemporaryDir>

#include "Settings.h"

// Settings 决定快捷键、扫描目录、字体等一整套行为，默认值变化等同于用户升级后
// 设置「被改了」。这里用 ini 后端做隔离，注册表路径不会被触碰。
class TstSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultsMatchCurrentBehavior();
    void globalHotkeyFallsBackToLegacySplitKeys();
    void explicitHotkeyOverridesLegacyKeys();
    void scanDirsAreTrimmedAndDeduplicated();
    void uiFontFamilyKeepsLegacyWenkaiFallback();
    void signalsOnlyFireOnRealChanges();
    void valuesSurviveNewInstance();
    void reconcileAutoStartIsInertForIniBackend();

private:
    QString iniPath(const QString &name) const { return m_dir.filePath(name + QStringLiteral(".ini")); }

    QTemporaryDir m_dir;
};

void TstSettings::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void TstSettings::defaultsMatchCurrentBehavior()
{
    Settings s(iniPath(QStringLiteral("defaults")));
    QCOMPARE(s.searchEngine(), QStringLiteral("Bing"));
    QCOMPARE(s.theme(), QStringLiteral("system"));
    QCOMPARE(s.globalHotkey(),
             QKeySequence(QKeyCombination(Qt::AltModifier | Qt::ControlModifier, Qt::Key_Space))
                 .toString(QKeySequence::PortableText));
    QVERIFY(s.frostedGlass());
    QVERIFY(s.showFloatIcon());
    QVERIFY(s.appLaunchRequiresDoubleClick());
    QVERIFY(s.showAllAppsWhenSearchEmpty());
    QVERIFY(s.hideUninstallEntries());
    QVERIFY(s.isPluginEnabled(QStringLiteral("calc")));
    QVERIFY(!s.autoStart());
    QVERIFY(!s.pluginAutoEnter());
    QVERIFY(!s.autoHideOnFocusLoss());
    QVERIFY(!s.showInTaskbar());
    QVERIFY(!s.startHiddenOnBoot());
    QVERIFY(!s.customScanDirsRecursive());
    QVERIFY(s.customScanDirs().isEmpty());
    QVERIFY(s.uiFontFamily().isEmpty());
    QVERIFY(s.pluginHotkey(QStringLiteral("calc")).isEmpty());
    // 浮动图标默认位置是 -1,-1，由界面层解释为「未拖动过」。
    QCOMPARE(s.floatIconPos(), QPoint(-1, -1));
}

void TstSettings::globalHotkeyFallsBackToLegacySplitKeys()
{
    // 先落旧键再构造 Settings，否则新实例读不到别人写进去的值。
    {
        QSettings raw(iniPath(QStringLiteral("legacyHotkey")), QSettings::IniFormat);
        raw.setValue(QStringLiteral("hotkeyModifiers"), static_cast<int>(Qt::ShiftModifier));
        // 旧版把 Windows 虚拟键码原样存进来，F1 是 0x70。
        raw.setValue(QStringLiteral("hotkeyKey"), 0x70);
    }

    Settings s(iniPath(QStringLiteral("legacyHotkey")));
    const QString hotkey = s.globalHotkey();
    QVERIFY2(hotkey.contains(QStringLiteral("F1")), qPrintable(hotkey));
    QVERIFY2(hotkey.contains(QStringLiteral("Shift")), qPrintable(hotkey));
    QVERIFY(!hotkey.contains(QStringLiteral("Ctrl")));
    QVERIFY(!hotkey.contains(QStringLiteral("Alt")));
}

void TstSettings::explicitHotkeyOverridesLegacyKeys()
{
    {
        QSettings raw(iniPath(QStringLiteral("explicitHotkey")), QSettings::IniFormat);
        raw.setValue(QStringLiteral("hotkeyModifiers"), static_cast<int>(Qt::ShiftModifier));
        raw.setValue(QStringLiteral("hotkeyKey"), 0x70);
    }
    Settings s(iniPath(QStringLiteral("explicitHotkey")));
    s.setGlobalHotkey(QStringLiteral("Ctrl+Alt+D"));
    QCOMPARE(s.globalHotkey(), QStringLiteral("Ctrl+Alt+D"));
    // 重复写入同一个快捷键不应广播变更。
    QSignalSpy changed(&s, &Settings::globalHotkeyChanged);
    s.setGlobalHotkey(QStringLiteral("Ctrl+Alt+D"));
    QCOMPARE(changed.count(), 0);
}

void TstSettings::scanDirsAreTrimmedAndDeduplicated()
{
    Settings s(iniPath(QStringLiteral("scanDirs")));
    s.setCustomScanDirs({QStringLiteral("  C:/Tools  "), QStringLiteral("C:/tools"),
                         QStringLiteral("D:/Apps"), QStringLiteral(" "), QString()});
    const QStringList expected{QStringLiteral("C:/Tools"), QStringLiteral("D:/Apps")};
    QCOMPARE(s.customScanDirs(), expected);

    // 顺序稳定，重复写入归一化后的同一份列表时不广播。
    QSignalSpy changed(&s, &Settings::customScanDirsChanged);
    s.setCustomScanDirs(expected);
    QCOMPARE(changed.count(), 0);
    s.setCustomScanDirs(QStringList{QStringLiteral("C:/Tools")});
    QCOMPARE(changed.count(), 1);
}

void TstSettings::uiFontFamilyKeepsLegacyWenkaiFallback()
{
    {
        QSettings raw(iniPath(QStringLiteral("fontLegacy")), QSettings::IniFormat);
        raw.setValue(QStringLiteral("useSystemWenkaiFont"), true);
    }
    Settings s(iniPath(QStringLiteral("fontLegacy")));
    QCOMPARE(s.uiFontFamily(), QStringLiteral("LXGW WenKai"));

    s.setUiFontFamily(QStringLiteral("  Noto Sans SC  "));
    QCOMPARE(s.uiFontFamily(), QStringLiteral("Noto Sans SC"));

    // 显式写空字符串代表「用默认字体」，此时旧开关必须失效。
    s.setUiFontFamily(QString());
    QVERIFY(s.uiFontFamily().isEmpty());
}

void TstSettings::signalsOnlyFireOnRealChanges()
{
    Settings s(iniPath(QStringLiteral("signals")));
    QSignalSpy engineChanged(&s, &Settings::searchEngineChanged);
    QSignalSpy pluginEnabledChanged(&s, &Settings::pluginEnabledChanged);

    s.setSearchEngine(QStringLiteral("Bing"));   // 与默认值相同，不算变更
    QCOMPARE(engineChanged.count(), 0);
    s.setSearchEngine(QStringLiteral("Google"));
    QCOMPARE(engineChanged.count(), 1);
    s.setSearchEngine(QStringLiteral("Google"));
    QCOMPARE(engineChanged.count(), 1);

    s.setPluginEnabled(QStringLiteral("calc"), true);   // 默认就是启用
    QCOMPARE(pluginEnabledChanged.count(), 0);
    s.setPluginEnabled(QStringLiteral("calc"), false);
    QCOMPARE(pluginEnabledChanged.count(), 1);
    QCOMPARE(pluginEnabledChanged.at(0).first().toString(), QStringLiteral("calc"));
    QVERIFY(!s.isPluginEnabled(QStringLiteral("calc")));
    // 插件启用状态按命令名分键，互不影响。
    QVERIFY(s.isPluginEnabled(QStringLiteral("convert")));
}

void TstSettings::valuesSurviveNewInstance()
{
    const QString path = iniPath(QStringLiteral("reopen"));
    {
        Settings writer(path);
        writer.setTheme(QStringLiteral("dark"));
        writer.setCustomScanDirs({QStringLiteral("E:/Bin")});
        writer.setPluginHotkey(QStringLiteral("todo"), QStringLiteral("Ctrl+T"));
        writer.setFloatIconPos(QPoint(120, 240));
    }
    Settings reader(path);
    const QStringList expectedDirs{QStringLiteral("E:/Bin")};
    QCOMPARE(reader.theme(), QStringLiteral("dark"));
    QCOMPARE(reader.customScanDirs(), expectedDirs);
    QCOMPARE(reader.pluginHotkey(QStringLiteral("todo")), QStringLiteral("Ctrl+T"));
    QCOMPARE(reader.floatIconPos(), QPoint(120, 240));
}

void TstSettings::reconcileAutoStartIsInertForIniBackend()
{
    // ini 后端禁止写注册表，因此这一步在测试进程里是安全的空操作。
    Settings s(iniPath(QStringLiteral("autoStart")));
    s.setAutoStart(true);
    QVERIFY(s.autoStart());
    s.reconcileAutoStart();
    s.setStartHiddenOnBoot(true);
    QVERIFY(s.startHiddenOnBoot());

    QSignalSpy changed(&s, &Settings::autoStartChanged);
    s.setAutoStart(true);   // 同值不广播
    QCOMPARE(changed.count(), 0);
}

QTEST_GUILESS_MAIN(TstSettings)

#include "tst_settings.moc"
