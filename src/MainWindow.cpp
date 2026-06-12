#include "MainWindow.h"

#include "AppLauncher.h"
#include "Settings.h"
#include "ThemeManager.h"
#include "Updater.h"
#include "plugins/PluginManager.h"

#include <QWKWidgets/widgetwindowagent.h>

#include <QBoxLayout>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QTreeView>
#include <QUrl>

static QString titleOf(const QVariantMap &item)
{
    return item.value(QStringLiteral("title"), item.value(QStringLiteral("name"))).toString();
}

static QString subtitleOf(const QVariantMap &item)
{
    return item.value(QStringLiteral("subtitle"), item.value(QStringLiteral("desc"))).toString();
}

MainWindow::MainWindow(AppLauncher *launcher, PluginManager *pluginManager, Settings *settings, Updater *updater, QWidget *parent)
    : QWidget(parent)
    , m_launcher(launcher)
    , m_pluginManager(pluginManager)
    , m_settings(settings)
    , m_updater(updater)
{
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setWindowTitle(QStringLiteral("Glide"));
    resize(760, 460);
    setMinimumWidth(720);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *windowAgent = new QWK::WidgetWindowAgent(this);
    windowAgent->setup(this);

    auto *titleBar = new QWidget(this);
    titleBar->setObjectName(QStringLiteral("windowTitleBar"));
    titleBar->setFixedHeight(36);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(14, 0, 6, 0);
    auto *titleLabel = new QLabel(QStringLiteral("Glide"), titleBar);
    titleLabel->setObjectName(QStringLiteral("windowTitle"));
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    auto *closeButton = new QPushButton(QStringLiteral("×"), titleBar);
    closeButton->setObjectName(QStringLiteral("windowCloseButton"));
    closeButton->setFixedSize(32, 28);
    titleLayout->addWidget(closeButton);
    windowAgent->setTitleBar(titleBar);
    windowAgent->setSystemButton(QWK::WindowAgentBase::Close, closeButton);
    windowAgent->setHitTestVisible(closeButton, true);
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    layout->addWidget(titleBar);

    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(20, 8, 20, 20);
    contentLayout->setSpacing(12);
    layout->addWidget(content, 1);

    auto *searchRow = new QHBoxLayout;
    searchRow->setSpacing(8);
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("Search apps or type cmd space args..."));
    searchRow->addWidget(m_searchEdit, 1);
    auto *managementButton = new QPushButton;
    managementButton->setObjectName(QStringLiteral("managementButton"));
    managementButton->setIcon(QIcon(QStringLiteral(":/assets/icon.svg")));
    managementButton->setIconSize(QSize(24, 24));
    managementButton->setFixedSize(38, 38);
    managementButton->setToolTip(tr("Open user management"));
    connect(managementButton, &QPushButton::clicked, this, [this]() {
        openPluginView(QStringLiteral("user"));
    });
    searchRow->addWidget(managementButton);
    contentLayout->addLayout(searchRow);

    m_commandLabel = new QLabel;
    m_commandLabel->setObjectName(QStringLiteral("muted"));
    m_commandLabel->hide();
    contentLayout->addWidget(m_commandLabel);

    m_pinnedPanel = new QWidget;
    m_pinnedLayout = new QHBoxLayout(m_pinnedPanel);
    m_pinnedLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->addWidget(m_pinnedPanel);

    m_stack = new QStackedWidget;
    m_resultsList = new QListWidget;
    m_resultsList->setIconSize(QSize(40, 40));
    m_stack->addWidget(m_resultsList);
    contentLayout->addWidget(m_stack, 1);

    auto *debounce = new QTimer(this);
    debounce->setInterval(120);
    debounce->setSingleShot(true);
    connect(m_searchEdit, &QLineEdit::textChanged, debounce, qOverload<>(&QTimer::start));
    connect(debounce, &QTimer::timeout, this, &MainWindow::performSearch);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &MainWindow::launchCurrent);
    connect(m_resultsList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (m_settings->appLaunchRequiresDoubleClick() || m_commandMode) return;
        if (item->data(Qt::UserRole).toMap().value(QStringLiteral("path")).toString().isEmpty()) return;
        launchCurrent();
    });
    connect(m_resultsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        if (m_settings->appLaunchRequiresDoubleClick() || m_commandMode) launchCurrent();
    });
    connect(m_resultsList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *listItem = m_resultsList->itemAt(pos);
        if (!listItem) return;
        const QVariantMap item = listItem->data(Qt::UserRole).toMap();
        const QString path = item.value(QStringLiteral("path")).toString();
        if (path.isEmpty()) return;
        QMenu menu(this);
        QAction *pinAction = menu.addAction(m_launcher->isPinned(path) ? tr("Unpin from Start") : tr("Pin to Start"));
        if (menu.exec(m_resultsList->viewport()->mapToGlobal(pos)) == pinAction) {
            if (m_launcher->isPinned(path)) {
                m_launcher->unpinApp(path);
            } else {
                m_launcher->pinApp(path, titleOf(item), item.value(QStringLiteral("icon")).toString());
            }
        }
    });
    m_resultsList->setContextMenuPolicy(Qt::CustomContextMenu);

    new QShortcut(QKeySequence(Qt::Key_Down), this, [this]() { moveSelection(1); });
    new QShortcut(QKeySequence(Qt::Key_Up), this, [this]() { moveSelection(-1); });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+N")), this, [this]() { moveSelection(1); });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+P")), this, [this]() { moveSelection(-1); });
    new QShortcut(QKeySequence(Qt::Key_Escape), this, [this]() { hide(); });

    connect(m_launcher, &AppLauncher::pinsChanged, this, &MainWindow::rebuildPinnedApps);
    connect(m_updater, &Updater::updateAvailable, this, [this](const QString &version, const QString &changelog, const QString &url, bool) {
        if (QMessageBox::question(this, tr("Update available"), tr("New version %1 is available.\n\n%2\n\nUpdate now?").arg(version, changelog)) == QMessageBox::Yes) {
            m_updater->downloadAndInstall(url);
        }
    });
    connect(m_updater, &Updater::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (!m_downloadDialog) {
            m_downloadDialog = new QProgressDialog(tr("Downloading update..."), QString(), 0, 100, this);
            m_downloadDialog->setWindowModality(Qt::WindowModal);
            m_downloadDialog->setCancelButton(nullptr);
        }
        if (total > 0) m_downloadDialog->setValue(static_cast<int>((received * 100) / total));
        m_downloadDialog->show();
    });
    connect(m_updater, &Updater::downloadFinished, this, [this]() {
        if (m_downloadDialog) m_downloadDialog->close();
    });
    connect(m_updater, &Updater::downloadError, this, [this](const QString &message) {
        if (m_downloadDialog) m_downloadDialog->close();
        QMessageBox::warning(this, tr("Update failed"), message);
    });

    rebuildPinnedApps();
    performSearch();
    QTimer::singleShot(3000, m_updater, &Updater::checkForUpdate);
}

void MainWindow::showAndActivate()
{
    show();
    raise();
    activateWindow();
    m_searchEdit->setFocus();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate && m_settings->autoHideOnFocusLoss()) {
        QTimer::singleShot(50, this, [this]() {
            if (!isActiveWindow() && m_settings->autoHideOnFocusLoss()) hide();
        });
    }
    return QWidget::event(event);
}

void MainWindow::performSearch()
{
    const bool viewingPlugin = m_stack->currentWidget() != m_resultsList;

    const QString text = m_searchEdit->text();
    const QString trimmed = text.trimmed();
    QVariantList results;
    m_commandMode = false;
    m_currentCmd.clear();

    const int firstSpace = text.indexOf(' ');
    if (firstSpace > 0) {
        m_commandMode = true;
        m_currentCmd = text.left(firstSpace).trimmed();
        const QString args = text.mid(firstSpace + 1);
        if (!viewingPlugin && args.trimmed().isEmpty() && m_pluginManager->autoEnterForCommand(m_currentCmd)) {
            launchCurrent();
            return;
        }
        results = m_pluginManager->queryCommand(m_currentCmd, args);
    } else if (!trimmed.isEmpty() && m_pluginManager->commands().contains(trimmed)) {
        m_commandMode = true;
        m_currentCmd = trimmed;
        results = m_pluginManager->queryCommand(m_currentCmd, QString());
    } else {
        results = m_launcher->search(text);
    }

    if (results.isEmpty() && !trimmed.isEmpty()) {
        QVariantMap searchItem;
        searchItem["title"] = tr("Search \"%1\"").arg(trimmed);
        searchItem["subtitle"] = tr("Search with %1").arg(m_settings->searchEngine());
        QVariantMap action;
        action["type"] = QStringLiteral("openUrl");
        action["data"] = searchUrl(trimmed);
        searchItem["action"] = action;
        results << searchItem;
    }

    setResults(results);
    updateCommandBar();
    const bool keepPluginOpen = viewingPlugin && m_commandMode && m_currentCmd == m_openPluginCmd;
    if (!keepPluginOpen) {
        openResults();
    }
    m_pinnedPanel->setVisible(text.isEmpty() && !m_commandMode && !keepPluginOpen);
}

void MainWindow::launchCurrent()
{
    if (m_commandMode && openPluginView(m_currentCmd)) return;

    const QVariantMap item = currentItemData();
    if (item.isEmpty()) return;
    const QVariantMap action = item.value(QStringLiteral("action")).toMap();
    if (!action.isEmpty() && action.value(QStringLiteral("type")).toString() != QStringLiteral("none")) {
        m_pluginManager->executeAction(action);
        hide();
        return;
    }
    const QString path = item.value(QStringLiteral("path")).toString();
    if (!path.isEmpty()) {
        m_launcher->launch(path);
        hide();
    }
}

void MainWindow::moveSelection(int delta)
{
    if (m_stack->currentWidget() != m_resultsList || m_resultsList->count() == 0) return;
    int row = m_resultsList->currentRow();
    row = qBound(0, row + delta, m_resultsList->count() - 1);
    m_resultsList->setCurrentRow(row);
}

QString MainWindow::searchUrl(const QString &query) const
{
    const QString encoded = QUrl::toPercentEncoding(query);
    const QString engine = m_settings->searchEngine();
    if (engine == QStringLiteral("Google")) return QStringLiteral("https://www.google.com/search?q=%1").arg(encoded);
    if (engine == QStringLiteral("Baidu")) return QStringLiteral("https://www.baidu.com/s?wd=%1").arg(encoded);
    if (engine == QStringLiteral("DuckDuckGo")) return QStringLiteral("https://duckduckgo.com/?q=%1").arg(encoded);
    return QStringLiteral("https://www.bing.com/search?q=%1").arg(encoded);
}

void MainWindow::setResults(const QVariantList &results)
{
    m_results = results;
    m_resultsList->clear();
    for (const QVariant &value : m_results) {
        const QVariantMap item = value.toMap();
        auto *listItem = new QListWidgetItem(titleOf(item) + QStringLiteral("\n") + subtitleOf(item));
        const QString iconPath = item.value(QStringLiteral("icon")).toString();
        if (!iconPath.isEmpty()) listItem->setIcon(QIcon(iconPath));
        listItem->setData(Qt::UserRole, item);
        m_resultsList->addItem(listItem);
    }
    if (m_resultsList->count() > 0) m_resultsList->setCurrentRow(0);
}

QVariantMap MainWindow::currentItemData() const
{
    QListWidgetItem *item = m_resultsList->currentItem();
    return item ? item->data(Qt::UserRole).toMap() : QVariantMap();
}

void MainWindow::rebuildPinnedApps()
{
    while (QLayoutItem *item = m_pinnedLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    auto *title = new QLabel(tr("Pinned"));
    title->setObjectName(QStringLiteral("muted"));
    m_pinnedLayout->addWidget(title);
    for (const QVariant &value : m_launcher->pinnedApps()) {
        const QVariantMap pin = value.toMap();
        auto *button = new QPushButton(pin.value(QStringLiteral("name")).toString());
        button->setObjectName(QStringLiteral("pinnedAppButton"));
        const QString icon = pin.value(QStringLiteral("icon")).toString();
        if (!icon.isEmpty()) button->setIcon(QIcon(icon));
        button->setIconSize(QSize(24, 24));
        const QString path = pin.value(QStringLiteral("path")).toString();
        connect(button, &QPushButton::clicked, this, [this, path]() { m_launcher->launch(path); hide(); });
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QPushButton::customContextMenuRequested, this, [this, button, path](const QPoint &pos) {
            QMenu menu(this);
            QAction *unpinAction = menu.addAction(tr("Unpin from Start"));
            if (menu.exec(button->mapToGlobal(pos)) == unpinAction) {
                m_launcher->unpinApp(path);
            }
        });
        m_pinnedLayout->addWidget(button);
    }
    m_pinnedLayout->addStretch();
}

bool MainWindow::openPluginView(const QString &command)
{
    IPlugin *plugin = m_pluginManager->plugin(command);
    QWidget *view = m_pluginManager->viewForCommand(command);
    if (!view) return false;

    if (plugin) {
        connect(plugin, &IPlugin::viewCloseRequested, this, &MainWindow::openResults, Qt::UniqueConnection);
    }
    if (m_stack->indexOf(view) < 0) {
        m_stack->addWidget(view);
    }
    m_stack->setCurrentWidget(view);
    m_openPluginCmd = command;
    return true;
}

void MainWindow::activateCommand(const QString &command)
{
    showAndActivate();
    m_searchEdit->setText(command + QStringLiteral(" "));
    performSearch();
    IPlugin *plugin = m_pluginManager->plugin(command);
    if (plugin && plugin->supportsAutoEnter()) {
        openPluginView(command);
    }
}

void MainWindow::openResults()
{
    m_stack->setCurrentWidget(m_resultsList);
    m_openPluginCmd.clear();
}

void MainWindow::updateCommandBar()
{
    if (!m_commandMode) {
        m_commandLabel->hide();
        return;
    }
    const QString desc = m_pluginManager->commandDescription(m_currentCmd);
    m_commandLabel->setText(desc.isEmpty() ? tr("%1 | Unknown command").arg(m_currentCmd) : tr("%1 | %2").arg(m_currentCmd, desc));
    m_commandLabel->show();
}

FloatIcon::FloatIcon(Settings *settings, ThemeManager *themeManager, QWidget *mainWindow, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_themeManager(themeManager)
    , m_mainWindow(mainWindow)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(48, 48);
    connect(m_themeManager, &ThemeManager::paletteChanged, this, qOverload<>(&FloatIcon::update));
}

void FloatIcon::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(m_themeManager->accent());
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(rect().adjusted(2, 2, -2, -2));
    painter.setPen(m_themeManager->bg());
    QFont f = painter.font();
    f.setBold(true);
    f.setPointSize(16);
    painter.setFont(f);
    painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("G"));
}

void FloatIcon::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_dragging = false;
    }
}

void FloatIcon::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        m_dragging = true;
        move(event->globalPosition().toPoint() - m_dragOffset);
    }
}

void FloatIcon::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        QMenu menu(this);
        QAction *showAction = menu.addAction(m_mainWindow && m_mainWindow->isVisible() ? tr("Hide main window") : tr("Show main window"));
        menu.addSeparator();
        QAction *hideIconAction = menu.addAction(tr("Hide float icon"));
        QAction *selected = menu.exec(event->globalPosition().toPoint());
        if (selected == showAction && m_mainWindow) {
            if (m_mainWindow->isVisible()) {
                m_mainWindow->hide();
            } else if (auto *main = qobject_cast<MainWindow *>(m_mainWindow)) {
                main->showAndActivate();
            }
        } else if (selected == hideIconAction) {
            m_settings->setShowFloatIcon(false);
        }
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    m_settings->setFloatIconPos(pos());
    if (!m_dragging && m_mainWindow) {
        if (m_mainWindow->isVisible()) {
            m_mainWindow->hide();
        } else if (auto *main = qobject_cast<MainWindow *>(m_mainWindow)) {
            main->showAndActivate();
        }
    }
}

