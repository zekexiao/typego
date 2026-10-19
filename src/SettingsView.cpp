#include "SettingsView.h"

#include "Settings.h"
#include "plugins/IPlugin.h"
#include "plugins/PluginManager.h"

#include <QBoxLayout>
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QFileDialog>
#include <QFontDatabase>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStackedWidget>
#include <QCoreApplication>
#include <QWheelEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
void ensureAltSpaceNativeFilterInstalled();
}

HotkeyEdit::HotkeyEdit(QWidget *parent)
    : QKeySequenceEdit(parent)
{
    setMaximumSequenceLength(1);
    setClearButtonEnabled(true);
    ensureAltSpaceNativeFilterInstalled();
}

bool HotkeyEdit::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Space && (keyEvent->modifiers() & Qt::AltModifier)) {
            event->accept();
            return true;
        }
    }
    return QKeySequenceEdit::event(event);
}

void HotkeyEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && (event->modifiers() & Qt::AltModifier)) {
        setKeySequence(QKeySequence(QKeyCombination(Qt::AltModifier, Qt::Key_Space)));
        event->accept();
        return;
    }
    QKeySequenceEdit::keyPressEvent(event);
}

namespace {

HotkeyEdit *focusedHotkeyEdit()
{
    QWidget *widget = QApplication::focusWidget();
    while (widget) {
        if (auto *edit = qobject_cast<HotkeyEdit *>(widget)) return edit;
        widget = widget->parentWidget();
    }
    return nullptr;
}

#ifdef Q_OS_WIN
class AltSpaceNativeFilter final : public QAbstractNativeEventFilter {
public:
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override
    {
        MSG *msg = static_cast<MSG *>(message);
        HotkeyEdit *edit = focusedHotkeyEdit();
        if (!edit) return false;

        if ((msg->message == WM_SYSKEYDOWN || msg->message == WM_KEYDOWN)
            && msg->wParam == VK_SPACE
            && (GetKeyState(VK_MENU) & 0x8000)) {
            edit->setKeySequence(QKeySequence(QKeyCombination(Qt::AltModifier, Qt::Key_Space)));
            if (result) *result = 0;
            return true;
        }

        if (msg->message == WM_SYSCHAR && msg->wParam == VK_SPACE) {
            if (result) *result = 0;
            return true;
        }

        if (msg->message == WM_SYSCOMMAND && (msg->wParam & 0xfff0) == SC_KEYMENU) {
            if (result) *result = 0;
            return true;
        }

        return false;
    }
};
#endif

void ensureAltSpaceNativeFilterInstalled()
{
#ifdef Q_OS_WIN
    static AltSpaceNativeFilter filter;
    static bool installed = false;
    if (!installed) {
        qApp->installNativeEventFilter(&filter);
        installed = true;
    }
#endif
}

} // namespace

SettingsSwitch::SettingsSwitch(QWidget *parent)
    : QCheckBox(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize SettingsSwitch::sizeHint() const
{
    return QSize(40, 24);
}

QSize SettingsSwitch::minimumSizeHint() const
{
    return sizeHint();
}

void SettingsSwitch::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled()) painter.setOpacity(0.45);

    const QRectF track(2.0, (height() - 20.0) / 2.0, 36.0, 20.0);
    const QColor idleBorder(128, 128, 128, 120);
    painter.setBrush(isChecked() ? m_accentColor : QColor(128, 128, 128, 65));
    painter.setPen(QPen(isChecked() || underMouse() ? m_accentColor : idleBorder, 1.0));
    painter.drawRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), 10.0, 10.0);

    QRectF knob(0.0, 0.0, 16.0, 16.0);
    knob.moveCenter(QPointF(isChecked() ? track.right() - 10.0 : track.left() + 10.0,
                            track.center().y()));
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(knob);

    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(m_accentColor, 1.0));
        painter.drawRoundedRect(track.adjusted(-1.5, -1.5, 1.5, 1.5), 11.5, 11.5);
    }
}

SettingsView::SettingsView(Settings *settings, PluginManager *pluginManager, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_pluginManager(pluginManager)
{
    setObjectName(QStringLiteral("settingsView"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(20);
    root->addLayout(columns, 1);

    m_navigation = new QListWidget;
    m_navigation->setObjectName(QStringLiteral("settingsNavigation"));
    m_navigation->setFixedWidth(148);
    m_navigation->setSpacing(2);
    m_navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navigation->addItem(tr("Settings"));
    m_navigation->addItem(tr("Plugins"));
    m_navigation->addItem(tr("Scan Folders"));
    m_navigation->addItem(tr("Interface"));
    m_navigation->addItem(tr("About"));
    columns->addWidget(m_navigation);

    m_pages = new QStackedWidget;
    m_pages->setObjectName(QStringLiteral("settingsPages"));
    columns->addWidget(m_pages, 1);

    auto createPage = [this](const QString &title) {
        auto *scroll = new QScrollArea;
        scroll->setObjectName(QStringLiteral("settingsScroll"));
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *body = new QWidget(scroll);
        body->setObjectName(QStringLiteral("settingsPage"));
        auto *layout = new QVBoxLayout(body);
        layout->setContentsMargins(0, 2, 8, 0);
        layout->setSpacing(10);
        auto *heading = new QLabel(title);
        heading->setObjectName(QStringLiteral("pageTitle"));
        layout->addWidget(heading);
        layout->addSpacing(4);
        scroll->setWidget(body);
        m_pages->addWidget(scroll);
        return layout;
    };
    auto addSection = [](QVBoxLayout *layout, const QString &text) {
        auto *label = new QLabel(text);
        label->setObjectName(QStringLiteral("section"));
        layout->addWidget(label);
    };
    auto addGroup = [](QVBoxLayout *layout) {
        auto *group = new QFrame;
        group->setObjectName(QStringLiteral("settingsGroup"));
        auto *groupLayout = new QVBoxLayout(group);
        groupLayout->setContentsMargins(16, 0, 16, 0);
        groupLayout->setSpacing(0);
        layout->addWidget(group);
        return groupLayout;
    };
    auto addDivider = [](QVBoxLayout *layout) {
        auto *divider = new QFrame;
        divider->setObjectName(QStringLiteral("settingsDivider"));
        divider->setFixedHeight(1);
        divider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(divider);
    };
    auto addRow = [](QVBoxLayout *layout, const QString &text, QWidget *control) {
        auto *row = new QWidget;
        row->setObjectName(QStringLiteral("settingsRow"));
        row->setMinimumHeight(50);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(16);
        auto *label = new QLabel(text);
        label->setObjectName(QStringLiteral("settingLabel"));
        label->setWordWrap(true);
        label->setBuddy(control);
        rowLayout->addWidget(label, 1);
        control->setAccessibleName(text);
        rowLayout->addWidget(control, 0, Qt::AlignVCenter);
        layout->addWidget(row);
        return rowLayout;
    };

    auto *settingsLayout = createPage(tr("Settings"));
    auto *generalGroup = addGroup(settingsLayout);
    m_globalHotkeyEdit = new HotkeyEdit;
    m_globalHotkeyEdit->setFixedWidth(164);
    addRow(generalGroup, tr("Global Hotkey"), m_globalHotkeyEdit);
    addDivider(generalGroup);
    m_engineCombo = new QComboBox;
    m_engineCombo->addItems({"Bing", "Google", "Baidu", "DuckDuckGo", "File Search"});
    m_engineCombo->setFixedWidth(184);
    m_engineCombo->installEventFilter(this);
    addRow(generalGroup, tr("Search"), m_engineCombo);
    addSection(settingsLayout, tr("Startup"));
    auto *startupGroup = addGroup(settingsLayout);
    m_autoStartCheck = new SettingsSwitch;
    addRow(startupGroup, tr("Start on boot"), m_autoStartCheck);
    addDivider(startupGroup);
    m_startHiddenOnBootCheck = new SettingsSwitch;
    addRow(startupGroup, tr("Start hidden on boot"), m_startHiddenOnBootCheck);
    settingsLayout->addStretch();

    auto *pluginLayout = createPage(tr("Plugins"));
    auto *pluginOptionsGroup = addGroup(pluginLayout);
    m_pluginAutoEnterCheck = new SettingsSwitch;
    addRow(pluginOptionsGroup, tr("Auto enter on empty args"), m_pluginAutoEnterCheck);
    auto *pluginActions = new QHBoxLayout;
    pluginActions->setContentsMargins(0, 2, 0, 2);
    pluginActions->setSpacing(8);
    auto *enableAllButton = new QPushButton(tr("Enable all"));
    auto *disableAllButton = new QPushButton(tr("Disable all"));
    pluginActions->addStretch();
    pluginActions->addWidget(enableAllButton);
    pluginActions->addWidget(disableAllButton);
    pluginLayout->addLayout(pluginActions);
    auto *pluginListLayout = addGroup(pluginLayout);
    QStringList commands = m_pluginManager->commands();
    commands.sort(Qt::CaseInsensitive);
    QSet<IPlugin *> listedPlugins;
    IPlugin *settingsPlugin = m_pluginManager->plugin(QStringLiteral("set"));
    for (const QString &registeredCommand : commands) {
        IPlugin *plugin = m_pluginManager->plugin(registeredCommand);
        if (!plugin || plugin == settingsPlugin || listedPlugins.contains(plugin)) continue;
        listedPlugins.insert(plugin);
        const QString cmd = plugin->command();
        if (listedPlugins.size() > 1) addDivider(pluginListLayout);
        auto *row = new QWidget;
        row->setObjectName(QStringLiteral("pluginRow"));
        row->setMinimumHeight(68);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 8, 0, 8);
        rowLayout->setSpacing(12);
        auto *textLayout = new QVBoxLayout;
        textLayout->setSpacing(2);
        auto *commandLabel = new QLabel(cmd);
        commandLabel->setObjectName(QStringLiteral("pluginCommand"));
        auto *descriptionLabel = new QLabel(m_pluginManager->commandDescription(cmd));
        descriptionLabel->setObjectName(QStringLiteral("muted"));
        descriptionLabel->setWordWrap(true);
        textLayout->addWidget(commandLabel);
        textLayout->addWidget(descriptionLabel);
        rowLayout->addLayout(textLayout, 1);
        auto *check = new SettingsSwitch;
        check->setProperty("cmd", cmd);
        check->setAccessibleName(cmd + QStringLiteral(" — ") + tr("Enabled"));
        m_pluginChecks.append(check);
        auto *hotkeyGroup = new QHBoxLayout;
        hotkeyGroup->setSpacing(6);
        auto *hotkeyEdit = new HotkeyEdit;
        hotkeyEdit->setProperty("cmd", cmd);
        hotkeyEdit->setToolTip(tr("Global plugin hotkey"));
        hotkeyEdit->setFixedWidth(136);
        m_pluginHotkeyEdits.append(hotkeyEdit);
        auto *hotkeyLabel = new QLabel(tr("Hotkey"));
        hotkeyLabel->setObjectName(QStringLiteral("muted"));
        hotkeyGroup->addWidget(hotkeyLabel);
        hotkeyGroup->addWidget(hotkeyEdit);
        rowLayout->addLayout(hotkeyGroup);
        if (plugin->hasSettingsWidget()) {
            auto *settingsButton = new QPushButton(tr("Settings"));
            settingsButton->setObjectName(QStringLiteral("pluginSettingsButton"));
            settingsButton->setProperty("cmd", cmd);
            rowLayout->addWidget(settingsButton);
            connect(settingsButton, &QPushButton::clicked, this, [this, plugin, cmd]() {
                auto *dialog = new QDialog(this);
                dialog->setWindowTitle(tr("%1 Settings").arg(cmd));
                auto *dialogLayout = new QVBoxLayout(dialog);
                dialogLayout->setContentsMargins(16, 16, 16, 16);
                dialogLayout->setSpacing(12);

                if (QWidget *settingsWidget = plugin->settingsWidget()) {
                    dialogLayout->addWidget(settingsWidget);
                }
                auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
                buttons->button(QDialogButtonBox::Close)->setText(tr("Close"));
                connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
                dialogLayout->addWidget(buttons);
                dialog->resize(460, 280);
                dialog->exec();
            });
        }
        rowLayout->addWidget(check, 0, Qt::AlignVCenter);
        pluginListLayout->addWidget(row);
    }
    pluginLayout->addStretch();

    auto *scanLayout = createPage(tr("Scan Folders"));
    addSection(scanLayout, tr("Start Menu"));
    auto *startMenuGroup = addGroup(scanLayout);
    m_hideUninstallCheck = new SettingsSwitch;
    addRow(startMenuGroup, tr("Hide uninstall entries"), m_hideUninstallCheck);
    addSection(scanLayout, tr("Custom Directories"));
    auto *customDirectoryGroup = addGroup(scanLayout);
    m_customScanRecursiveCheck = new SettingsSwitch;
    addRow(customDirectoryGroup, tr("Search subdirectories up to 3 levels"), m_customScanRecursiveCheck);
    addDivider(customDirectoryGroup);
    auto *directoryEditor = new QWidget;
    directoryEditor->setObjectName(QStringLiteral("scanDirectoryEditor"));
    auto *directoryEditorLayout = new QVBoxLayout(directoryEditor);
    directoryEditorLayout->setContentsMargins(0, 10, 0, 12);
    directoryEditorLayout->setSpacing(8);
    m_customScanDirsList = new QListWidget;
    m_customScanDirsList->setObjectName(QStringLiteral("scanDirectoryList"));
    m_customScanDirsList->setMinimumHeight(112);
    directoryEditorLayout->addWidget(m_customScanDirsList);
    auto *scanDirActions = new QHBoxLayout;
    scanDirActions->setSpacing(8);
    auto *addScanDirButton = new QPushButton(tr("Add Directory"));
    auto *removeScanDirButton = new QPushButton(tr("Remove"));
    removeScanDirButton->setEnabled(false);
    scanDirActions->addWidget(addScanDirButton);
    scanDirActions->addWidget(removeScanDirButton);
    scanDirActions->addStretch();
    directoryEditorLayout->addLayout(scanDirActions);
    customDirectoryGroup->addWidget(directoryEditor);
    scanLayout->addStretch();
    connect(m_customScanDirsList, &QListWidget::currentRowChanged, removeScanDirButton, [removeScanDirButton](int row) {
        removeScanDirButton->setEnabled(row >= 0);
    });
    connect(addScanDirButton, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Add Scan Directory"));
        if (dir.isEmpty()) return;
        for (int i = 0; i < m_customScanDirsList->count(); ++i) {
            if (m_customScanDirsList->item(i)->text().compare(dir, Qt::CaseInsensitive) == 0) return;
        }
        m_customScanDirsList->addItem(dir);
    });
    connect(removeScanDirButton, &QPushButton::clicked, this, [this]() {
        delete m_customScanDirsList->takeItem(m_customScanDirsList->currentRow());
    });

    auto *interfaceLayout = createPage(tr("Interface"));
    auto *appearanceGroup = addGroup(interfaceLayout);
    m_themeCombo = new QComboBox;
    m_themeCombo->addItem(tr("System"), QStringLiteral("system"));
    m_themeCombo->addItem(tr("Light"), QStringLiteral("light"));
    m_themeCombo->addItem(tr("Dark"), QStringLiteral("dark"));
    m_themeCombo->setFixedWidth(184);
    m_themeCombo->installEventFilter(this);
    addRow(appearanceGroup, tr("Theme"), m_themeCombo);
    addDivider(appearanceGroup);
    m_frostedGlassCheck = new SettingsSwitch;
    addRow(appearanceGroup, tr("Enable frosted glass"), m_frostedGlassCheck);
    addDivider(appearanceGroup);
    m_fontCombo = new QComboBox;
    m_fontCombo->addItem(tr("System default"), QString());
    const QStringList fontFamilies = QFontDatabase::families();
    for (const QString &family : fontFamilies) {
        m_fontCombo->addItem(family, family);
    }
    m_fontCombo->setFixedWidth(184);
    m_fontCombo->installEventFilter(this);
    addRow(appearanceGroup, tr("Font"), m_fontCombo);
    addSection(interfaceLayout, tr("Window"));
    auto *windowGroup = addGroup(interfaceLayout);
    m_floatIconCheck = new SettingsSwitch;
    addRow(windowGroup, tr("Show float icon"), m_floatIconCheck);
    addDivider(windowGroup);
    m_autoHideCheck = new SettingsSwitch;
    addRow(windowGroup, tr("Auto hide on focus loss"), m_autoHideCheck);
    addDivider(windowGroup);
    m_showInTaskbarCheck = new SettingsSwitch;
    addRow(windowGroup, tr("Show main window in taskbar"), m_showInTaskbarCheck);
    addSection(interfaceLayout, tr("Results"));
    auto *resultsGroup = addGroup(interfaceLayout);
    m_appDoubleClickCheck = new SettingsSwitch;
    addRow(resultsGroup, tr("Require double-click to launch apps"), m_appDoubleClickCheck);
    addDivider(resultsGroup);
    m_showAllAppsCheck = new SettingsSwitch;
    addRow(resultsGroup, tr("Show all apps when search is empty"), m_showAllAppsCheck);
    interfaceLayout->addStretch();

    auto *aboutLayout = createPage(tr("About"));
    auto *aboutCard = new QFrame;
    aboutCard->setObjectName(QStringLiteral("aboutCard"));
    auto *aboutCardLayout = new QHBoxLayout(aboutCard);
    aboutCardLayout->setContentsMargins(18, 18, 18, 18);
    aboutCardLayout->setSpacing(16);
    auto *iconLabel = new QLabel;
    iconLabel->setObjectName(QStringLiteral("aboutIcon"));
    iconLabel->setFixedSize(56, 56);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(QIcon(QStringLiteral(":/assets/icon.png")).pixmap(48, 48));
    aboutCardLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    auto *aboutTextLayout = new QVBoxLayout;
    aboutTextLayout->setSpacing(3);
    auto *nameLabel = new QLabel(QStringLiteral("TypeGo"));
    nameLabel->setObjectName(QStringLiteral("aboutName"));
    aboutTextLayout->addWidget(nameLabel);
    // 版本由 CMake 从 VERSION 文件注入，这兜底值会和安装包版本不一致，故不保留。
    auto *versionLabel = new QLabel(tr("Version %1").arg(QCoreApplication::applicationVersion()));
    versionLabel->setObjectName(QStringLiteral("muted"));
    aboutTextLayout->addWidget(versionLabel);
    auto *descLabel = new QLabel(tr("Windows application launcher and command plugin host."));
    descLabel->setObjectName(QStringLiteral("aboutDescription"));
    descLabel->setWordWrap(true);
    aboutTextLayout->addWidget(descLabel);
    aboutTextLayout->addStretch();
    aboutCardLayout->addLayout(aboutTextLayout, 1);
    aboutLayout->addWidget(aboutCard);
    aboutLayout->addStretch();

    connect(enableAllButton, &QPushButton::clicked, this, [this]() {
        for (QCheckBox *check : m_pluginChecks) check->setChecked(true);
    });
    connect(disableAllButton, &QPushButton::clicked, this, [this]() {
        for (QCheckBox *check : m_pluginChecks) check->setChecked(false);
    });
    connect(m_navigation, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);

    auto *footer = new QFrame;
    footer->setObjectName(QStringLiteral("settingsFooter"));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(0, 12, 0, 0);
    footerLayout->setSpacing(0);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    buttons->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    buttons->button(QDialogButtonBox::Save)->setMinimumWidth(84);
    buttons->button(QDialogButtonBox::Cancel)->setMinimumWidth(84);
    footerLayout->addStretch();
    footerLayout->addWidget(buttons);
    root->addWidget(footer);
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, [this]() {
        reload();
        emit closeRequested();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        m_settings->setGlobalHotkey(m_globalHotkeyEdit->keySequence().toString(QKeySequence::PortableText));
        m_settings->setSearchEngine(m_engineCombo->currentText());
        m_settings->setTheme(m_themeCombo->currentData().toString());
        m_settings->setFrostedGlass(m_frostedGlassCheck->isChecked());
        m_settings->setAutoStart(m_autoStartCheck->isChecked());
        m_settings->setStartHiddenOnBoot(m_startHiddenOnBootCheck->isChecked());
        m_settings->setShowFloatIcon(m_floatIconCheck->isChecked());
        m_settings->setAutoHideOnFocusLoss(m_autoHideCheck->isChecked());
        m_settings->setAppLaunchRequiresDoubleClick(m_appDoubleClickCheck->isChecked());
        m_settings->setShowAllAppsWhenSearchEmpty(m_showAllAppsCheck->isChecked());
        m_settings->setShowInTaskbar(m_showInTaskbarCheck->isChecked());
        m_settings->setHideUninstallEntries(m_hideUninstallCheck->isChecked());
        m_settings->setCustomScanDirsRecursive(m_customScanRecursiveCheck->isChecked());
        m_settings->setUiFontFamily(m_fontCombo->currentData().toString());
        QStringList scanDirs;
        for (int i = 0; i < m_customScanDirsList->count(); ++i) {
            scanDirs.append(m_customScanDirsList->item(i)->text());
        }
        m_settings->setCustomScanDirs(scanDirs);
        m_settings->setPluginAutoEnter(m_pluginAutoEnterCheck->isChecked());
        for (QCheckBox *check : m_pluginChecks) {
            m_settings->setPluginEnabled(check->property("cmd").toString(), check->isChecked());
        }
        for (QKeySequenceEdit *edit : m_pluginHotkeyEdits) {
            m_settings->setPluginHotkey(edit->property("cmd").toString(),
                                        edit->keySequence().toString(QKeySequence::PortableText));
        }
    });

    reload();
    showSection(QStringLiteral("set"));
}

void SettingsView::showSection(const QString &command)
{
    const QString normalized = command.toLower();
    const int index = normalized == QStringLiteral("plugin") ? 1
        : normalized == QStringLiteral("scan") ? 2
        : normalized == QStringLiteral("user") || normalized == QStringLiteral("interface") ? 3
        : normalized == QStringLiteral("about") ? 4
        : 0;
    m_navigation->setCurrentRow(index);
}

bool SettingsView::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Wheel && qobject_cast<QComboBox *>(watched)) {
        QObject *ancestor = watched->parent();
        while (ancestor && !qobject_cast<QScrollArea *>(ancestor)) ancestor = ancestor->parent();
        if (auto *scroll = qobject_cast<QScrollArea *>(ancestor)) {
            auto *wheelEvent = static_cast<QWheelEvent *>(event);
            const int delta = wheelEvent->pixelDelta().isNull() ? wheelEvent->angleDelta().y() : wheelEvent->pixelDelta().y();
            QScrollBar *bar = scroll->verticalScrollBar();
            bar->setValue(bar->value() - delta);
        }
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void SettingsView::reload()
{
    m_globalHotkeyEdit->setKeySequence(
        QKeySequence::fromString(m_settings->globalHotkey(), QKeySequence::PortableText));
    m_engineCombo->setCurrentText(m_settings->searchEngine());
    m_autoStartCheck->setChecked(m_settings->autoStart());
    m_startHiddenOnBootCheck->setChecked(m_settings->startHiddenOnBoot());
    m_floatIconCheck->setChecked(m_settings->showFloatIcon());
    m_autoHideCheck->setChecked(m_settings->autoHideOnFocusLoss());
    m_appDoubleClickCheck->setChecked(m_settings->appLaunchRequiresDoubleClick());
    m_showAllAppsCheck->setChecked(m_settings->showAllAppsWhenSearchEmpty());
    m_showInTaskbarCheck->setChecked(m_settings->showInTaskbar());
    m_hideUninstallCheck->setChecked(m_settings->hideUninstallEntries());
    m_customScanRecursiveCheck->setChecked(m_settings->customScanDirsRecursive());
    const QString fontFamily = m_settings->uiFontFamily();
    const int fontIndex = m_fontCombo->findData(fontFamily);
    m_fontCombo->setCurrentIndex(fontIndex >= 0 ? fontIndex : 0);
    m_customScanDirsList->clear();
    m_customScanDirsList->addItems(m_settings->customScanDirs());
    m_pluginAutoEnterCheck->setChecked(m_settings->pluginAutoEnter());
    const int themeIndex = m_themeCombo->findData(m_settings->theme());
    m_frostedGlassCheck->setChecked(m_settings->frostedGlass());
    m_themeCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    for (QCheckBox *check : m_pluginChecks) {
        check->setChecked(m_settings->isPluginEnabled(check->property("cmd").toString()));
    }
    for (QKeySequenceEdit *edit : m_pluginHotkeyEdits) {
        edit->setKeySequence(QKeySequence::fromString(
            m_settings->pluginHotkey(edit->property("cmd").toString()), QKeySequence::PortableText));
    }
}
