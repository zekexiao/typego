#include "SettingsView.h"

#include "Settings.h"
#include "plugins/IPlugin.h"
#include "plugins/PluginManager.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStackedWidget>
#include <QWheelEvent>

SettingsView::SettingsView(Settings *settings, PluginManager *pluginManager, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_pluginManager(pluginManager)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(12);
    root->addLayout(columns, 1);

    m_navigation = new QListWidget;
    m_navigation->setObjectName(QStringLiteral("settingsNavigation"));
    m_navigation->setFixedWidth(132);
    m_navigation->addItem(tr("User"));
    m_navigation->addItem(tr("Settings"));
    m_navigation->addItem(tr("Plugins"));
    columns->addWidget(m_navigation);

    m_pages = new QStackedWidget;
    m_pages->setObjectName(QStringLiteral("settingsPages"));
    columns->addWidget(m_pages, 1);

    auto createPage = [this](const QString &title) {
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        auto *body = new QWidget(scroll);
        auto *layout = new QVBoxLayout(body);
        layout->setContentsMargins(14, 10, 14, 14);
        layout->setSpacing(10);
        auto *heading = new QLabel(title);
        heading->setObjectName(QStringLiteral("pageTitle"));
        layout->addWidget(heading);
        scroll->setWidget(body);
        m_pages->addWidget(scroll);
        return layout;
    };
    auto addSection = [](QVBoxLayout *layout, const QString &text) {
        auto *label = new QLabel(text);
        label->setObjectName(QStringLiteral("section"));
        layout->addWidget(label);
    };

    auto *userLayout = createPage(tr("User"));
    auto *profileCard = new QFrame;
    profileCard->setObjectName(QStringLiteral("profileCard"));
    auto *profileLayout = new QHBoxLayout(profileCard);
    auto *avatar = new QLabel(QStringLiteral("G"));
    avatar->setObjectName(QStringLiteral("profileAvatar"));
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(48, 48);
    auto *profileText = new QVBoxLayout;
    auto *profileName = new QLabel(tr("Local user"));
    profileName->setObjectName(QStringLiteral("pluginCommand"));
    auto *profileDescription = new QLabel(tr("Personal application behavior"));
    profileDescription->setObjectName(QStringLiteral("muted"));
    profileText->addWidget(profileName);
    profileText->addWidget(profileDescription);
    profileLayout->addWidget(avatar);
    profileLayout->addLayout(profileText, 1);
    userLayout->addWidget(profileCard);
    addSection(userLayout, tr("Application Behavior"));
    m_autoStartCheck = new QCheckBox(tr("Start on boot"));
    m_floatIconCheck = new QCheckBox(tr("Show float icon"));
    m_autoHideCheck = new QCheckBox(tr("Auto hide on focus loss"));
    m_appDoubleClickCheck = new QCheckBox(tr("Require double-click to launch apps"));
    userLayout->addWidget(m_autoStartCheck);
    userLayout->addWidget(m_floatIconCheck);
    userLayout->addWidget(m_autoHideCheck);
    userLayout->addWidget(m_appDoubleClickCheck);
    userLayout->addStretch();

    auto *settingsLayout = createPage(tr("Settings"));
    addSection(settingsLayout, tr("Global Hotkey"));
    auto *mods = new QHBoxLayout;
    m_altCheck = new QCheckBox(QStringLiteral("Alt"));
    m_ctrlCheck = new QCheckBox(QStringLiteral("Ctrl"));
    m_shiftCheck = new QCheckBox(QStringLiteral("Shift"));
    mods->addWidget(m_altCheck);
    mods->addWidget(m_ctrlCheck);
    mods->addWidget(m_shiftCheck);
    mods->addStretch();
    settingsLayout->addLayout(mods);
    m_keyCombo = new QComboBox;
    m_keyCombo->addItems({"Space","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z","0","1","2","3","4","5","6","7","8","9","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"});
    m_keyCombo->installEventFilter(this);
    settingsLayout->addWidget(m_keyCombo);
    addSection(settingsLayout, tr("Theme"));
    m_themeCombo = new QComboBox;
    m_themeCombo->addItem(tr("System"), QStringLiteral("system"));
    m_themeCombo->addItem(tr("Light"), QStringLiteral("light"));
    m_themeCombo->addItem(tr("Dark"), QStringLiteral("dark"));
    m_themeCombo->installEventFilter(this);
    settingsLayout->addWidget(m_themeCombo);
    addSection(settingsLayout, tr("Search Engine"));
    m_engineCombo = new QComboBox;
    m_engineCombo->addItems({"Bing", "Google", "Baidu", "DuckDuckGo"});
    m_engineCombo->installEventFilter(this);
    settingsLayout->addWidget(m_engineCombo);
    settingsLayout->addStretch();

    auto *pluginLayout = createPage(tr("Plugins"));
    m_pluginAutoEnterCheck = new QCheckBox(tr("Auto enter on empty args"));
    pluginLayout->addWidget(m_pluginAutoEnterCheck);
    auto *pluginActions = new QHBoxLayout;
    auto *enableAllButton = new QPushButton(tr("Enable all"));
    auto *disableAllButton = new QPushButton(tr("Disable all"));
    pluginActions->addWidget(enableAllButton);
    pluginActions->addWidget(disableAllButton);
    pluginActions->addStretch();
    pluginLayout->addLayout(pluginActions);
    QStringList commands = m_pluginManager->commands();
    commands.sort(Qt::CaseInsensitive);
    QSet<IPlugin *> listedPlugins;
    IPlugin *settingsPlugin = m_pluginManager->plugin(QStringLiteral("set"));
    for (const QString &registeredCommand : commands) {
        IPlugin *plugin = m_pluginManager->plugin(registeredCommand);
        if (!plugin || plugin == settingsPlugin || listedPlugins.contains(plugin)) continue;
        listedPlugins.insert(plugin);
        const QString cmd = plugin->command();
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("pluginCard"));
        auto *cardLayout = new QHBoxLayout(card);
        auto *textLayout = new QVBoxLayout;
        auto *commandLabel = new QLabel(cmd);
        commandLabel->setObjectName(QStringLiteral("pluginCommand"));
        auto *descriptionLabel = new QLabel(m_pluginManager->commandDescription(cmd));
        descriptionLabel->setObjectName(QStringLiteral("muted"));
        textLayout->addWidget(commandLabel);
        textLayout->addWidget(descriptionLabel);
        cardLayout->addLayout(textLayout, 1);
        auto *check = new QCheckBox(tr("Enabled"));
        check->setProperty("cmd", cmd);
        m_pluginChecks.append(check);
        cardLayout->addWidget(check);
        pluginLayout->addWidget(card);
    }
    pluginLayout->addStretch();

    connect(enableAllButton, &QPushButton::clicked, this, [this]() {
        for (QCheckBox *check : m_pluginChecks) check->setChecked(true);
    });
    connect(disableAllButton, &QPushButton::clicked, this, [this]() {
        for (QCheckBox *check : m_pluginChecks) check->setChecked(false);
    });
    connect(m_navigation, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    root->addWidget(buttons);
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, [this]() {
        reload();
        emit closeRequested();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        int mods = 0;
        if (m_altCheck->isChecked()) mods |= Qt::AltModifier;
        if (m_ctrlCheck->isChecked()) mods |= Qt::ControlModifier;
        if (m_shiftCheck->isChecked()) mods |= Qt::ShiftModifier;
        m_settings->setHotkeyModifiers(mods);
        m_settings->setHotkeyKey(selectedKey());
        m_settings->setSearchEngine(m_engineCombo->currentText());
        m_settings->setTheme(m_themeCombo->currentData().toString());
        m_settings->setAutoStart(m_autoStartCheck->isChecked());
        m_settings->setShowFloatIcon(m_floatIconCheck->isChecked());
        m_settings->setAutoHideOnFocusLoss(m_autoHideCheck->isChecked());
        m_settings->setAppLaunchRequiresDoubleClick(m_appDoubleClickCheck->isChecked());
        m_settings->setPluginAutoEnter(m_pluginAutoEnterCheck->isChecked());
        for (QCheckBox *check : m_pluginChecks) {
            m_settings->setPluginEnabled(check->property("cmd").toString(), check->isChecked());
        }
    });

    reload();
    showSection(QStringLiteral("set"));
}

void SettingsView::showSection(const QString &command)
{
    const int index = command == QStringLiteral("user") ? 0
        : command == QStringLiteral("plugin") ? 2
        : 1;
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
    const int mods = m_settings->hotkeyModifiers();
    m_altCheck->setChecked(mods & Qt::AltModifier);
    m_ctrlCheck->setChecked(mods & Qt::ControlModifier);
    m_shiftCheck->setChecked(mods & Qt::ShiftModifier);
    m_keyCombo->setCurrentIndex(keyIndex(m_settings->hotkeyKey()));
    m_engineCombo->setCurrentText(m_settings->searchEngine());
    m_autoStartCheck->setChecked(m_settings->autoStart());
    m_floatIconCheck->setChecked(m_settings->showFloatIcon());
    m_autoHideCheck->setChecked(m_settings->autoHideOnFocusLoss());
    m_appDoubleClickCheck->setChecked(m_settings->appLaunchRequiresDoubleClick());
    m_pluginAutoEnterCheck->setChecked(m_settings->pluginAutoEnter());
    const int themeIndex = m_themeCombo->findData(m_settings->theme());
    m_themeCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    for (QCheckBox *check : m_pluginChecks) {
        check->setChecked(m_settings->isPluginEnabled(check->property("cmd").toString()));
    }
}

int SettingsView::selectedKey() const
{
    static const QList<int> keys = {0x20,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B};
    return keys.value(m_keyCombo->currentIndex(), 0x20);
}

int SettingsView::keyIndex(int key) const
{
    static const QList<int> keys = {0x20,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B};
    const int index = keys.indexOf(key);
    return index >= 0 ? index : 0;
}
