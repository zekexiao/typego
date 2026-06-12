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
#include <QKeySequenceEdit>
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
    m_globalHotkeyEdit = new QKeySequenceEdit;
    m_globalHotkeyEdit->setMaximumSequenceLength(1);
    m_globalHotkeyEdit->setClearButtonEnabled(true);
    settingsLayout->addWidget(m_globalHotkeyEdit);
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
        auto *hotkeyEdit = new QKeySequenceEdit;
        hotkeyEdit->setProperty("cmd", cmd);
        hotkeyEdit->setMaximumSequenceLength(1);
        hotkeyEdit->setClearButtonEnabled(true);
        hotkeyEdit->setToolTip(tr("Global plugin hotkey"));
        hotkeyEdit->setFixedWidth(150);
        m_pluginHotkeyEdits.append(hotkeyEdit);
        auto *hotkeyLabel = new QLabel(tr("Hotkey"));
        hotkeyLabel->setObjectName(QStringLiteral("muted"));
        cardLayout->addWidget(hotkeyLabel);
        cardLayout->addWidget(hotkeyEdit);
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
        m_settings->setGlobalHotkey(m_globalHotkeyEdit->keySequence().toString(QKeySequence::PortableText));
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
    m_globalHotkeyEdit->setKeySequence(
        QKeySequence::fromString(m_settings->globalHotkey(), QKeySequence::PortableText));
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
    for (QKeySequenceEdit *edit : m_pluginHotkeyEdits) {
        edit->setKeySequence(QKeySequence::fromString(
            m_settings->pluginHotkey(edit->property("cmd").toString()), QKeySequence::PortableText));
    }
}
