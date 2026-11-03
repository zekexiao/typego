#include "SettingsView.h"

#include "AppLauncher.h"
#include "PluginSettingsDialog.h"
#include "Settings.h"
#include "ThemeManager.h"
#include "GlobalHotkey.h"
#include "Updater.h"
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
#include <QFontMetrics>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStackedWidget>
#include <QCoreApplication>
#include <QWheelEvent>
#include <QPointer>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
void ensureAltSpaceNativeFilterInstalled();

// 快捷键输入框宽度：全局热键与插件热键共用同一尺寸，两个页面的控件看起来一致，
// 宽度要放得下 "Ctrl+Alt+Shift+Space" 这类较长的组合键。
constexpr int HotkeyEditWidth = 164;
// 插件设置按钮列宽，按钮本身有固定内边距，这里给整列留出同样的宽度。
constexpr int PluginActionColumnWidth = 76;

// 导航图标只在创建页面或切换主题时生成，列表重绘复用 QIcon。
QIcon navigationIcon(int section, const QColor &color)
{
    QPixmap pixmap(40, 40);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    switch (section) {
    case 0:
        for (int y : {5, 10, 15}) painter.drawLine(QPointF(3, y), QPointF(17, y));
        painter.setBrush(color);
        painter.drawEllipse(QPointF(7, 5), 2, 2);
        painter.drawEllipse(QPointF(13, 10), 2, 2);
        painter.drawEllipse(QPointF(9, 15), 2, 2);
        break;
    case 1:
        for (int x : {3, 11})
            for (int y : {3, 11}) painter.drawRoundedRect(QRectF(x, y, 6, 6), 1.5, 1.5);
        break;
    case 2: {
        QPainterPath folder;
        folder.moveTo(3, 6);
        folder.lineTo(3, 4);
        folder.lineTo(8, 4);
        folder.lineTo(10, 6);
        folder.lineTo(17, 6);
        folder.lineTo(17, 16);
        folder.lineTo(3, 16);
        folder.closeSubpath();
        painter.drawPath(folder);
        break;
    }
    case 3:
        painter.drawRoundedRect(QRectF(3, 3, 14, 11), 2, 2);
        painter.drawLine(QPointF(10, 14), QPointF(10, 17));
        painter.drawLine(QPointF(6, 17), QPointF(14, 17));
        break;
    default:
        painter.drawEllipse(QRectF(3, 3, 14, 14));
        painter.drawLine(QPointF(10, 9), QPointF(10, 14));
        painter.drawPoint(QPointF(10, 6));
        break;
    }
    return QIcon(pixmap);
}
}

HotkeyEdit::HotkeyEdit(QWidget *parent)
    : QKeySequenceEdit(parent)
{
    setMaximumSequenceLength(1);
    // QKeySequenceEdit 是普通 QWidget，样式表的背景和边框只在 WA_StyledBackground 下才绘制；
    // 否则输入框看起来像一行普通文字，用户看不出这里可以点击录制。
    setAttribute(Qt::WA_StyledBackground, true);
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
    return QSize(48, 28);
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

    const QRectF track(2.0, (height() - 24.0) / 2.0, 44.0, 24.0);
    const QColor idleBorder(128, 128, 128, 120);
    painter.setBrush(isChecked() ? m_accentColor : QColor(128, 128, 128, 65));
    painter.setPen(QPen(isChecked() || underMouse() ? m_accentColor : idleBorder, 1.0));
    painter.drawRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), 12.0, 12.0);

    QRectF knob(0.0, 0.0, 18.0, 18.0);
    knob.moveCenter(QPointF(isChecked() ? track.right() - 12.0 : track.left() + 12.0,
                            track.center().y()));
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(knob);

    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(m_accentColor, 1.0));
        painter.drawRoundedRect(track.adjusted(-1.5, -1.5, 1.5, 1.5), 13.5, 13.5);
    }
}

SettingsView::SettingsView(Settings *settings, PluginManager *pluginManager, GlobalHotkey *hotkeys,
                           AppLauncher *launcher, Updater *updater, ThemeManager *themeManager,
                           QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_pluginManager(pluginManager)
    , m_hotkeys(hotkeys)
    , m_launcher(launcher)
    , m_updater(updater)
    , m_themeManager(themeManager)
{
    setObjectName(QStringLiteral("settingsView"));

    auto *columns = new QHBoxLayout(this);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(16);

    auto *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("settingsSidebar"));
    sidebar->setFixedWidth(168);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(12, 12, 12, 12);
    sidebarLayout->setSpacing(8);
    auto *brandRow = new QHBoxLayout;
    brandRow->setSpacing(8);
    auto *brandIcon = new QLabel;
    brandIcon->setPixmap(QIcon(QStringLiteral(":/assets/icon.png")).pixmap(28, 28));
    auto *brandLabel = new QLabel(QStringLiteral("TypeGo"));
    brandLabel->setObjectName(QStringLiteral("sidebarBrand"));
    brandRow->addWidget(brandIcon);
    brandRow->addWidget(brandLabel, 1);
    sidebarLayout->addLayout(brandRow);
    sidebarLayout->addSpacing(8);

    m_navigation = new QListWidget;
    m_navigation->setObjectName(QStringLiteral("settingsNavigation"));
    m_navigation->setSpacing(2);
    m_navigation->setIconSize(QSize(20, 20));
    m_navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navigation->addItem(tr("Settings"));
    m_navigation->addItem(tr("Plugins"));
    m_navigation->addItem(tr("Scan Folders"));
    m_navigation->addItem(tr("Interface"));
    m_navigation->addItem(tr("About"));
    sidebarLayout->addWidget(m_navigation, 1);
    auto *sidebarVersion = new QLabel(tr("Version %1").arg(QCoreApplication::applicationVersion()));
    sidebarVersion->setObjectName(QStringLiteral("sidebarCaption"));
    sidebarLayout->addWidget(sidebarVersion);
    columns->addWidget(sidebar);
    const auto refreshNavigation = [this]() {
        const QColor color = m_themeManager ? m_themeManager->textSecondary() : palette().color(QPalette::WindowText);
        for (int i = 0; i < m_navigation->count(); ++i)
            m_navigation->item(i)->setIcon(navigationIcon(i, color));
    };
    refreshNavigation();
    if (m_themeManager)
        connect(m_themeManager, &ThemeManager::paletteChanged, this, refreshNavigation);

    m_pages = new QStackedWidget;
    m_pages->setObjectName(QStringLiteral("settingsPages"));
    // 操作栏只属于右侧内容列，侧栏连续延伸到底部。
    auto *contentLayout = new QVBoxLayout;
    contentLayout->setSpacing(0);
    contentLayout->addWidget(m_pages, 1);
    columns->addLayout(contentLayout, 1);

    auto createPage = [this](const QString &title) {
        auto *scroll = new QScrollArea;
        scroll->setObjectName(QStringLiteral("settingsScroll"));
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *body = new QWidget(scroll);
        body->setObjectName(QStringLiteral("settingsPage"));
        auto *layout = new QVBoxLayout(body);
        // 与操作栏共同保留紧凑间距，避免底部出现重复留白。
        layout->setContentsMargins(0, 2, 8, 8);
        layout->setSpacing(8);
        auto *heading = new QLabel(title);
        heading->setObjectName(QStringLiteral("pageTitle"));
        layout->addWidget(heading);
        layout->addSpacing(2);
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
        groupLayout->setContentsMargins(16, 2, 16, 2);
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
        row->setMinimumHeight(46);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);
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
    m_globalHotkeyEdit->setFixedWidth(HotkeyEditWidth);
    addRow(generalGroup, tr("Global Hotkey"), m_globalHotkeyEdit);
    addDivider(generalGroup);
    m_engineCombo = new QComboBox;
    m_engineCombo->addItems({"Bing", "Google", "Baidu", "DuckDuckGo", "File Search"});
    m_engineCombo->setFixedWidth(168);
    m_engineCombo->installEventFilter(this);
    addRow(generalGroup, tr("Search"), m_engineCombo);
    addSection(settingsLayout, tr("Startup"));
    auto *startupGroup = addGroup(settingsLayout);
    m_autoStartCheck = new SettingsSwitch;
    addRow(startupGroup, tr("Start on boot"), m_autoStartCheck);
    addDivider(startupGroup);
    m_startHiddenOnBootCheck = new SettingsSwitch;
    addRow(startupGroup, tr("Start hidden on boot"), m_startHiddenOnBootCheck);
    addSection(settingsLayout, tr("Privacy"));
    auto *privacyGroup = addGroup(settingsLayout);
    auto *clearUsageButton = new QPushButton(tr("Clear"));
    clearUsageButton->setObjectName(QStringLiteral("danger"));
    addRow(privacyGroup, tr("App usage history used for search ranking"), clearUsageButton);
    connect(clearUsageButton, &QPushButton::clicked, this, [this]() {
        // 清空是破坏性操作且立即生效，不走保存按钮，先弹确认。
        if (QMessageBox::question(this, tr("Clear usage history"),
                tr("Clear the record of how often each app is launched? Search results will no longer be ranked by usage frequency."))
            == QMessageBox::Yes && m_launcher) {
            m_launcher->clearUsageHistory();
        }
    });
    settingsLayout->addStretch();

    auto *pluginLayout = createPage(tr("Plugins"));
    auto *pluginOptionsGroup = addGroup(pluginLayout);
    m_pluginAutoEnterCheck = new SettingsSwitch;
    addRow(pluginOptionsGroup, tr("Auto enter on empty args"), m_pluginAutoEnterCheck);
    pluginLayout->addSpacing(4);

    QStringList ids = m_pluginManager->pluginIds();
    ids.sort(Qt::CaseInsensitive);

    auto *pluginListLayout = addGroup(pluginLayout);
    // 列表头部：左边是分组标题，右边是批量开关，作用对象一目了然，
    // 不再让两个按钮单独悬在列表外面。
    auto *listHeader = new QWidget;
    listHeader->setObjectName(QStringLiteral("pluginListHeader"));
    auto *listHeaderLayout = new QHBoxLayout(listHeader);
    listHeaderLayout->setContentsMargins(0, 6, 0, 6);
    listHeaderLayout->setSpacing(8);
    auto *listTitle = new QLabel(tr("Installed plugins"));
    listTitle->setObjectName(QStringLiteral("pluginListTitle"));
    auto *enableAllButton = new QPushButton(tr("Enable all"));
    auto *disableAllButton = new QPushButton(tr("Disable all"));
    listHeaderLayout->addWidget(listTitle);
    listHeaderLayout->addStretch();
    listHeaderLayout->addWidget(enableAllButton);
    listHeaderLayout->addWidget(disableAllButton);
    pluginListLayout->addWidget(listHeader);
    addDivider(pluginListLayout);

    // 快捷键列的标签宽度按最长的命令关键字统一算一次，
    // 这样所有插件的快捷键输入框都落在同一条竖线上，不会因为插件不同而左右跳动。
    // 只有多命令插件才需要标出命令名，单命令插件不再重复写「快捷键」。
    QFont labelFont = QApplication::font();
    labelFont.setPixelSize(13);
    const QFontMetrics labelMetrics(labelFont);
    int hotkeyLabelWidth = 0;
    for (const QString &cmd : ids) {
        const PluginDescriptor *descriptor = m_pluginManager->descriptor(cmd);
        if (!descriptor || !descriptor->configurable || descriptor->commands.size() <= 1) continue;
        for (const PluginCommand &command : descriptor->commands)
            hotkeyLabelWidth = qMax(hotkeyLabelWidth, labelMetrics.horizontalAdvance(command.keyword));
    }
    hotkeyLabelWidth = qBound(0, hotkeyLabelWidth + 4, 160);
    const int hotkeyColumnWidth = hotkeyLabelWidth + HotkeyEditWidth + (hotkeyLabelWidth > 0 ? 10 : 0);

    bool firstPlugin = true;
    for (const QString &cmd : ids) {
        const PluginDescriptor *descriptor = m_pluginManager->descriptor(cmd);
        if (!descriptor->configurable) continue;
        if (!firstPlugin) addDivider(pluginListLayout);
        firstPlugin = false;
        auto *row = new QWidget;
        row->setObjectName(QStringLiteral("pluginRow"));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 6, 0, 6);
        rowLayout->setSpacing(12);
        // 左侧：插件名加描述。描述可能换行，行变高时名称和描述要继续贴在一起，
        // 否则两者会被布局撑开，中间多出一大段空白。
        auto *textColumn = new QWidget(row);
        auto *textLayout = new QVBoxLayout(textColumn);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);
        auto *commandLabel = new QLabel(cmd, textColumn);
        commandLabel->setObjectName(QStringLiteral("pluginCommand"));
        auto *descriptionLabel = new QLabel(descriptor->description, textColumn);
        descriptionLabel->setObjectName(QStringLiteral("muted"));
        descriptionLabel->setWordWrap(true);
        textLayout->addWidget(commandLabel);
        textLayout->addWidget(descriptionLabel);
        textLayout->addStretch();
        rowLayout->addWidget(textColumn, 1);
        // 设置按钮独占一列并排在快捷键前面，没有设置项的插件也保留同样宽度，
        // 免得这一列忽有忽无，把后面的快捷键输入框推得忽左忽右。
        auto *actionColumn = new QWidget(row);
        actionColumn->setFixedWidth(PluginActionColumnWidth);
        auto *actionLayout = new QHBoxLayout(actionColumn);
        actionLayout->setContentsMargins(0, 0, 0, 0);
        actionLayout->setSpacing(0);
        if (descriptor->hasSettings) {
            auto *settingsButton = new QPushButton(tr("Settings"), actionColumn);
            settingsButton->setObjectName(QStringLiteral("pluginSettingsButton"));
            settingsButton->setProperty("cmd", cmd);
            actionLayout->addWidget(settingsButton, 0, Qt::AlignTop);
            const QString settingsCommand = descriptor->commands.first().keyword;
            connect(settingsButton, &QPushButton::clicked, this, [this, cmd, settingsCommand]() {
                openPluginSettings(cmd, settingsCommand);
            });
        }
        rowLayout->addWidget(actionColumn, 0, Qt::AlignTop);
        // 快捷键列宽度固定，命令名右对齐紧挨输入框，输入框全部落在同一条竖线上。
        auto *hotkeyColumn = new QWidget(row);
        hotkeyColumn->setFixedWidth(hotkeyColumnWidth);
        auto *hotkeyLayout = new QVBoxLayout(hotkeyColumn);
        hotkeyLayout->setContentsMargins(0, 0, 0, 0);
        hotkeyLayout->setSpacing(6);
        HotkeyEdit *rowHotkeyEdit = nullptr;
        for (const PluginCommand &command : descriptor->commands) {
            auto *commandLayout = new QHBoxLayout;
            commandLayout->setContentsMargins(0, 0, 0, 0);
            commandLayout->setSpacing(10);
            // 单命令插件不写标签，只在多命令插件里标出命令名，避免每行重复一遍「快捷键」。
            auto *hotkeyLabel = new QLabel(descriptor->commands.size() == 1 ? QString() : command.keyword,
                                           hotkeyColumn);
            hotkeyLabel->setObjectName(QStringLiteral("muted"));
            hotkeyLabel->setFixedWidth(hotkeyLabelWidth);
            hotkeyLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto *hotkeyEdit = new HotkeyEdit(hotkeyColumn);
            hotkeyEdit->setProperty("cmd", command.keyword);
            hotkeyEdit->setProperty("pluginId", cmd);
            hotkeyEdit->setToolTip(tr("Global plugin hotkey"));
            hotkeyEdit->setFixedWidth(HotkeyEditWidth);
            m_pluginHotkeyEdits.append(hotkeyEdit);
            if (!rowHotkeyEdit) rowHotkeyEdit = hotkeyEdit;
            commandLayout->addWidget(hotkeyLabel);
            commandLayout->addWidget(hotkeyEdit);
            hotkeyLayout->addLayout(commandLayout);
        }
        hotkeyLayout->addStretch();
        rowLayout->addWidget(hotkeyColumn, 0, Qt::AlignTop);
        auto *check = new SettingsSwitch;
        check->setProperty("cmd", cmd);
        check->setAccessibleName(cmd + QStringLiteral(" — ") + tr("Enabled"));
        m_pluginChecks.append(check);
        // 开关比输入框矮，撑到输入框同样的高度后，轨道仍然画在正中，两者视觉对齐。
        if (rowHotkeyEdit) check->setFixedHeight(rowHotkeyEdit->sizeHint().height());
        rowLayout->addWidget(check, 0, Qt::AlignTop);
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
    directoryEditorLayout->setContentsMargins(0, 6, 0, 8);
    directoryEditorLayout->setSpacing(8);
    m_customScanDirsList = new QListWidget;
    m_customScanDirsList->setObjectName(QStringLiteral("scanDirectoryList"));
    m_customScanDirsList->setMinimumHeight(96);
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
    m_themeCombo->setFixedWidth(168);
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
    m_fontCombo->setFixedWidth(168);
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
    aboutCardLayout->setContentsMargins(16, 16, 16, 16);
    aboutCardLayout->setSpacing(12);
    auto *iconLabel = new QLabel;
    iconLabel->setObjectName(QStringLiteral("aboutIcon"));
    iconLabel->setFixedSize(48, 48);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(QIcon(QStringLiteral(":/assets/icon.png")).pixmap(40, 40));
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
    aboutTextLayout->addStretch();
    aboutCardLayout->addLayout(aboutTextLayout, 1);
    aboutLayout->addWidget(aboutCard);
    // 手动检查更新：发现新版本时走 MainWindow 的更新弹窗，这里只反馈「无更新/失败」。
    m_checkUpdateButton = new QPushButton(tr("Check for Updates"));
    m_checkUpdateButton->setCursor(Qt::PointingHandCursor);
    m_checkUpdateButton->setEnabled(m_updater != nullptr);
    aboutLayout->addWidget(m_checkUpdateButton, 0, Qt::AlignLeft);
    if (m_updater) {
        connect(m_checkUpdateButton, &QPushButton::clicked, this, [this]() {
            m_checkUpdateButton->setEnabled(false);
            m_updater->check(Updater::Mode::Manual);
        });
        // 无论检查来自自动还是手动，按钮都要恢复可用；只有手动检查才弹结果提示。
        const auto finishCheck = [this]() {
            m_checkUpdateButton->setEnabled(true);
        };
        connect(m_updater, &Updater::updateAvailable, this, [finishCheck](const ReleaseParser::Result &, UpdateCheckGate::Mode) {
            finishCheck();
        });
        connect(m_updater, &Updater::noUpdateAvailable, this, [this, finishCheck](UpdateCheckGate::Mode mode) {
            finishCheck();
            if (mode != Updater::Mode::Manual) return;
            QMessageBox::information(this, tr("Check for Updates"),
                                     tr("TypeGo %1 is up to date.").arg(QCoreApplication::applicationVersion()));
        });
        connect(m_updater, &Updater::checkError, this, [this, finishCheck](const QString &message, UpdateCheckGate::Mode mode) {
            finishCheck();
            if (mode != Updater::Mode::Manual) return;
            QMessageBox::warning(this, tr("Check for Updates"), tr("Update check failed: %1").arg(message));
        });
    }
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
    footerLayout->setContentsMargins(0, 6, 8, 0);
    footerLayout->setSpacing(0);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    buttons->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("primary"));
    buttons->button(QDialogButtonBox::Save)->setMinimumWidth(84);
    buttons->button(QDialogButtonBox::Cancel)->setMinimumWidth(84);
    footerLayout->addStretch();
    footerLayout->addWidget(buttons);
    contentLayout->addWidget(footer);
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, [this]() {
        reload();
        emit closeRequested();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this, buttons]() {
        QHash<QString, QString> pluginShortcuts;
        for (HotkeyEdit *edit : m_pluginHotkeyEdits) {
            const QString command = edit->property("cmd").toString();
            for (QCheckBox *check : m_pluginChecks) {
                if (check->property("cmd") == edit->property("pluginId") && check->isChecked()) {
                    pluginShortcuts.insert(command, edit->keySequence().toString(QKeySequence::PortableText));
                    break;
                }
            }
        }
        QString error;
        if (!m_hotkeys->applyShortcuts(m_globalHotkeyEdit->keySequence().toString(QKeySequence::PortableText),
                                     pluginShortcuts, &error)) {
            QMessageBox::warning(this, tr("Shortcut unavailable"), error);
            return;
        }
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
        for (HotkeyEdit *edit : m_pluginHotkeyEdits) {
            m_settings->setPluginHotkey(edit->property("cmd").toString(),
                                        edit->keySequence().toString(QKeySequence::PortableText));
        }
        // 保存可能异步完成，等待期间禁止重复提交；拒绝禁用时恢复实际状态与快捷键。
        buttons->setEnabled(false);
        m_pages->setEnabled(false);
        struct Pending { int remaining; bool accepted = true; };
        auto pending = std::make_shared<Pending>(Pending{int(m_pluginChecks.size())});
        for (QCheckBox *check : m_pluginChecks) {
            const QString id = check->property("cmd").toString();
            m_pluginManager->setPluginEnabled(id, check->isChecked(),
                [guard = QPointer<SettingsView>(this), controls = QPointer<QDialogButtonBox>(buttons),
                 pending, id](bool accepted) {
                if (!guard) return;
                pending->accepted = pending->accepted && accepted;
                for (QCheckBox *item : guard->m_pluginChecks) {
                    if (item->property("cmd").toString() == id) item->setChecked(guard->m_settings->isPluginEnabled(id));
                }
                if (--pending->remaining) return;
                QHash<QString, QString> shortcuts;
                for (QKeySequenceEdit *edit : guard->m_pluginHotkeyEdits) {
                    if (guard->m_settings->isPluginEnabled(edit->property("pluginId").toString())) {
                        const QString keyword = edit->property("cmd").toString();
                        shortcuts.insert(keyword, guard->m_settings->pluginHotkey(keyword));
                    }
                }
                QString error;
                if (!guard->m_hotkeys->applyShortcuts(guard->m_settings->globalHotkey(), shortcuts, &error)) {
                    QMessageBox::warning(guard, tr("Shortcut unavailable"), error);
                }
                guard->m_pages->setEnabled(true);
                if (controls) controls->setEnabled(true);
                if (!pending->accepted) QMessageBox::warning(guard, tr("Action failed"),
                    tr("The application could not be closed. Save your changes and try again."));
            });
        }
    });

    reload();
    showSection(QStringLiteral("set"));
}

void SettingsView::openPluginSettings(const QString &cmd, const QString &settingsCommand)
{
    QWidget *settingsWidget = m_pluginManager->settingsWidgetForCommand(settingsCommand);
    if (!settingsWidget) return;
    // 弹窗自带磨砂底和无边框标题栏，与主窗口保持同一套视觉。
    PluginSettingsDialog dialog(tr("%1 Settings").arg(cmd), settingsWidget, m_themeManager, this);
    dialog.adjustSize();
    dialog.exec();
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
    for (HotkeyEdit *edit : m_pluginHotkeyEdits) {
        edit->setKeySequence(QKeySequence::fromString(
            m_settings->pluginHotkey(edit->property("cmd").toString()), QKeySequence::PortableText));
    }
}
