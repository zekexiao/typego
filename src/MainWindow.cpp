#include "MainWindow.h"

#include "AppLauncher.h"
#include "Settings.h"
#include "ThemeManager.h"
#include "Updater.h"
#include "plugins/PluginManager.h"

#include <QWKWidgets/widgetwindowagent.h>

#include <QAbstractListModel>
#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QCache>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QEnterEvent>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QFutureWatcher>
#include <QHash>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressDialog>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QScopeGuard>
#include <QStyle>
#include <QTimer>
#include <QTreeView>
#include <QUrl>
#include <QtConcurrent>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

namespace {
constexpr int DefaultWindowHeight = 460;
constexpr int ResultsExtraHeight = 10;
constexpr int InitialVisibleResults = 80;
constexpr int ResultsFetchBatchSize = 120;
constexpr int MaxConcurrentIconLoads = 4;

// 结果项布局常量，配合 32px 图标缓存尺寸，避免上采样发虚。
constexpr int ResultIconSize = 32;
constexpr int ResultItemHPadding = 12;
constexpr int ResultItemVPadding = 8;
constexpr int ResultItemRadius = 8;
constexpr int ResultIconTextGap = 12;
constexpr int ResultTitleSubtitleGap = 2;

// 标题与副标题分开提供，供 ResultItemDelegate 分层绘制。
enum ResultRoles {
    TitleRole = Qt::UserRole + 1,
    SubtitleRole = Qt::UserRole + 2,
};

// 副标题比标题小一档形成层级，下限保证可读。
QFont subtitleFontFor(const QFont &base)
{
    QFont font = base;
    font.setPixelSize(qMax(11, QFontInfo(base).pixelSize() - 2));
    return font;
}

QString titleOf(const QVariantMap &item)
{
    return item.value(QStringLiteral("title"), item.value(QStringLiteral("name"))).toString();
}

QString subtitleOf(const QVariantMap &item)
{
    return item.value(QStringLiteral("subtitle"), item.value(QStringLiteral("desc"))).toString();
}

QString actionTypeOf(const QVariantMap &item)
{
    return item.value(QStringLiteral("action")).toMap().value(QStringLiteral("type")).toString();
}

QString localPathOf(const QVariantMap &item)
{
    const QString path = item.value(QStringLiteral("path")).toString();
    if (!path.isEmpty()) return path;

    const QVariantMap action = item.value(QStringLiteral("action")).toMap();
    const QString type = action.value(QStringLiteral("type")).toString();
    const QString data = action.value(QStringLiteral("data")).toString();
    if ((type == QStringLiteral("openFile") || type == QStringLiteral("openFolder") || type == QStringLiteral("navigate"))
        && !data.isEmpty()) {
        return data;
    }
    return QString();
}

#ifdef Q_OS_WIN
QImage hIconToImage(HICON hIcon)
{
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return QImage();

    BITMAP bmp;
    if (!GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp)) {
        DeleteObject(iconInfo.hbmMask);
        DeleteObject(iconInfo.hbmColor);
        return QImage();
    }

    const int width = bmp.bmWidth;
    const int height = bmp.bmHeight;

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HBITMAP hDIB = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hDIB) {
        DeleteDC(memDC);
        DeleteObject(iconInfo.hbmMask);
        DeleteObject(iconInfo.hbmColor);
        return QImage();
    }

    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, hDIB));
    DrawIconEx(memDC, 0, 0, hIcon, width, height, 0, nullptr, DI_NORMAL);
    SelectObject(memDC, oldBmp);

    QImage image(width, height, QImage::Format_ARGB32);
    memcpy(image.bits(), bits, width * height * 4);

    DeleteObject(hDIB);
    DeleteDC(memDC);
    DeleteObject(iconInfo.hbmMask);
    DeleteObject(iconInfo.hbmColor);

    return image;
}
#endif

QString extractIconAsync(const QString &filePath, const QString &cacheDir)
{
    QDir().mkpath(cacheDir);
    const QString cachePath = QDir(cacheDir).filePath(QString::number(qHash(filePath), 16) + QStringLiteral(".png"));
    if (QFile::exists(cachePath)) return cachePath;

#ifdef Q_OS_WIN
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const auto uninitialize = qScopeGuard([initialized]() {
        if (SUCCEEDED(initialized)) CoUninitialize();
    });
    QString iconSource = filePath;
    if (!QFileInfo(filePath).isAbsolute() && !filePath.contains(QLatin1Char(':'))) {
        const QString executable = QStandardPaths::findExecutable(filePath);
        if (!executable.isEmpty()) iconSource = executable;
    }
    SHFILEINFOW sfi = {};
    if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(iconSource.utf16()), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON)) {
        const HICON hIcon = sfi.hIcon;
        if (hIcon) {
            QImage image = hIconToImage(hIcon);
            DestroyIcon(hIcon);
            if (!image.isNull()) {
                if (image.width() != 32 || image.height() != 32) {
                    image = image.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                if (image.save(cachePath, "PNG")) return cachePath;
            }
        }
    }
#else
    Q_UNUSED(filePath)
#endif

    return QString();
}
}

class ResultListModel final : public QAbstractListModel {
public:
    explicit ResultListModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
        , m_iconCacheDir(QDir::tempPath() + QStringLiteral("/TypeGo/icons"))
    {
        QDir().mkpath(m_iconCacheDir);
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : qMin(m_visibleCount, m_results.size());
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) return QVariant();
        const QVariantMap item = m_results.at(index.row()).toMap();
        if (role == Qt::DisplayRole) {
            const QString title = titleOf(item);
            const QString subtitle = subtitleOf(item);
            return subtitle.isEmpty() ? title : title + QStringLiteral("\n") + subtitle;
        }
        if (role == TitleRole) {
            return titleOf(item);
        }
        if (role == SubtitleRole) {
            return subtitleOf(item);
        }
        if (role == Qt::ToolTipRole) {
            const QString title = titleOf(item);
            const QString subtitle = subtitleOf(item);
            return subtitle.isEmpty() ? title : title + QStringLiteral("\n") + subtitle;
        }
        if (role == Qt::DecorationRole) {
            const QString iconPath = item.value(QStringLiteral("icon")).toString();
            if (!iconPath.isEmpty()) {
                if (const QIcon *icon = m_icons.object(iconPath)) return *icon;
                const QIcon icon(iconPath);
                m_icons.insert(iconPath, new QIcon(icon));
                return icon;
            }

            const QString localPath = localPathOf(item);
            if (localPath.isEmpty()) return QVariant();
            if (const QIcon *icon = m_icons.object(localPath)) return *icon;
            scheduleIconLoad(localPath);
        }
        if (role == Qt::UserRole) return item;
        return QVariant();
    }

    bool canFetchMore(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        return false;
    }

    void fetchMore(const QModelIndex &parent = QModelIndex()) override
    {
        Q_UNUSED(parent)
        fetchNextBatch();
    }

    bool hasMore() const
    {
        return m_visibleCount < m_results.size();
    }

    void fetchNextBatch()
    {
        if (!hasMore()) return;

        const int first = m_visibleCount;
        const int count = qMin(ResultsFetchBatchSize, m_results.size() - m_visibleCount);
        beginInsertRows(QModelIndex(), first, first + count - 1);
        m_visibleCount += count;
        endInsertRows();
    }

    void setResults(const QVariantList &results)
    {
        beginResetModel();
        m_results = results;
        m_visibleCount = qMin(InitialVisibleResults, m_results.size());
        m_iconQueue.clear();
        m_queuedIcons.clear();
        endResetModel();
    }

    QVariantMap itemAt(int row) const
    {
        if (row < 0 || row >= rowCount()) return QVariantMap();
        return m_results.at(row).toMap();
    }

private:
    void scheduleIconLoad(const QString &path) const
    {
        if (m_icons.contains(path) || m_pendingIcons.contains(path) || m_queuedIcons.contains(path)) return;
        m_iconQueue.append(path);
        m_queuedIcons.insert(path);
        startQueuedIconLoads();
    }

    void startQueuedIconLoads() const
    {
        while (!m_iconQueue.isEmpty() && m_pendingIcons.size() < MaxConcurrentIconLoads) {
            const QString path = m_iconQueue.takeFirst();
            m_queuedIcons.remove(path);
            m_pendingIcons.insert(path);

            auto *watcher = new QFutureWatcher<QString>(const_cast<ResultListModel *>(this));
            auto *self = const_cast<ResultListModel *>(this);
            QObject::connect(watcher, &QFutureWatcher<QString>::finished, self, [self, watcher, path]() {
                const QString iconPath = watcher->result();
                watcher->deleteLater();
                self->m_pendingIcons.remove(path);
                // 缓存失败结果，避免不可读取的路径在每次重绘时反复排队。
                self->m_icons.insert(path, new QIcon(iconPath));
                if (!iconPath.isEmpty()) {
                    for (int row = 0; row < self->rowCount(); ++row) {
                        if (localPathOf(self->m_results.at(row).toMap()) == path) {
                            const QModelIndex idx = self->index(row, 0);
                            emit self->dataChanged(idx, idx, {Qt::DecorationRole});
                        }
                    }
                }
                self->startQueuedIconLoads();
            });
            watcher->setFuture(QtConcurrent::run(extractIconAsync, path, m_iconCacheDir));
        }
    }

    QVariantList m_results;
    int m_visibleCount = 0;
    QString m_iconCacheDir;
    mutable QCache<QString, QIcon> m_icons{256};
    mutable QSet<QString> m_pendingIcons;
    mutable QSet<QString> m_queuedIcons;
    mutable QStringList m_iconQueue;
};

ResultItemDelegate::ResultItemDelegate(ThemeManager *themeManager, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_themeManager(themeManager)
{
}

QSize ResultItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)
    const QFont subtitleFont = subtitleFontFor(option.font);
    const int textHeight = QFontMetrics(option.font).height()
        + ResultTitleSubtitleGap + QFontMetrics(subtitleFont).height();
    const int contentHeight = qMax(ResultIconSize, textHeight);
    return QSize(option.rect.width(), contentHeight + ResultItemVPadding * 2);
}

void ResultItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QColor selection = m_themeManager ? m_themeManager->selection() : option.palette.color(QPalette::Highlight);
    const QColor hover = m_themeManager ? m_themeManager->hover() : option.palette.color(QPalette::Mid);
    const QColor titleColor = m_themeManager ? m_themeManager->text() : option.palette.color(QPalette::Text);
    const QColor subtitleColor = m_themeManager ? m_themeManager->textSecondary() : option.palette.color(QPalette::PlaceholderText);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF background = QRectF(option.rect).adjusted(3.0, 1.0, -3.0, -1.0);
    const bool selected = option.state & QStyle::State_Selected;
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selection);
        painter->drawRoundedRect(background, ResultItemRadius, ResultItemRadius);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(hover);
        painter->drawRoundedRect(background, ResultItemRadius, ResultItemRadius);
    }

    // 图标列固定宽度，无图标时留白保证标题对齐。
    QRect iconRect(QPoint(), QSize(ResultIconSize, ResultIconSize));
    iconRect.moveLeft(qRound(background.left() + ResultItemHPadding));
    iconRect.moveCenter(QPoint(iconRect.center().x(), qRound(background.center().y())));
    const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (!icon.isNull()) {
        icon.paint(painter, iconRect, Qt::AlignCenter);
    }

    const QString title = index.data(TitleRole).toString();
    const QString subtitle = index.data(SubtitleRole).toString();
    const int textLeft = iconRect.right() + 1 + ResultIconTextGap;
    const int textRight = qRound(background.right() - ResultItemHPadding);
    const int availableWidth = qMax(0, textRight - textLeft);
    QRect textRect(textLeft, option.rect.top(), availableWidth, option.rect.height());

    const QFont subtitleFont = subtitleFontFor(option.font);
    const QFontMetrics titleMetrics(option.font);
    const QFontMetrics subtitleMetrics(subtitleFont);
    const int blockHeight = subtitle.isEmpty()
        ? titleMetrics.height()
        : titleMetrics.height() + ResultTitleSubtitleGap + subtitleMetrics.height();
    int baselineTop = textRect.center().y() - blockHeight / 2;

    const QColor paintTitle = selected ? QColor(Qt::white) : titleColor;
    painter->setFont(option.font);
    painter->setPen(paintTitle);
    painter->drawText(QRect(textRect.left(), baselineTop, availableWidth, titleMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      titleMetrics.elidedText(title, Qt::ElideRight, availableWidth));

    if (!subtitle.isEmpty()) {
        baselineTop += titleMetrics.height() + ResultTitleSubtitleGap;
        // 路径类副标题从中间省略，保留头尾信息。
        const QColor paintSubtitle = selected ? QColor(255, 255, 255, 205) : subtitleColor;
        painter->setFont(subtitleFont);
        painter->setPen(paintSubtitle);
        painter->drawText(QRect(textRect.left(), baselineTop, availableWidth, subtitleMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          subtitleMetrics.elidedText(subtitle, Qt::ElideMiddle, availableWidth));
    }

    painter->restore();
}

ManagementIcon::ManagementIcon(QWidget *parent)
    : QWidget(parent)
    , m_icon(QStringLiteral(":/assets/icon.png"))
{
    setFixedSize(32, 32);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setCursor(Qt::PointingHandCursor);
    // 让设置入口可键盘到达，不只能鼠标点击。
    setFocusPolicy(Qt::TabFocus);
    setAccessibleName(tr("Open settings"));
}

void ManagementIcon::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
        emit clicked();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ManagementIcon::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void ManagementIcon::leaveEvent(QEvent *event)
{
    m_hovered = false;
    m_pressed = false;
    update();
    QWidget::leaveEvent(event);
}

void ManagementIcon::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }
    QWidget::mousePressEvent(event);
}

void ManagementIcon::mouseReleaseEvent(QMouseEvent *event)
{
    const bool activate = m_pressed && event->button() == Qt::LeftButton
        && rect().contains(event->position().toPoint());
    m_pressed = false;
    update();
    if (activate) emit clicked();
    QWidget::mouseReleaseEvent(event);
}

void ManagementIcon::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const int hoverSize = 30;
    const QRect hoverRect((width() - hoverSize) / 2, (height() - hoverSize) / 2, hoverSize, hoverSize);
    if (m_hovered || m_pressed) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_pressed ? QColor(128, 128, 128, 76) : QColor(128, 128, 128, 45));
        painter.drawEllipse(hoverRect);
    }
    if (hasFocus()) {
        QPen focusPen(palette().highlight().color());
        focusPen.setWidthF(1.6);
        painter.setPen(focusPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(hoverRect.adjusted(-1, -1, 1, 1));
    }
    const int iconSize = 22;
    const QRect iconRect((width() - iconSize) / 2, (height() - iconSize) / 2, iconSize, iconSize);
    m_icon.paint(&painter, iconRect, Qt::AlignCenter, QIcon::Normal);
}

CommandLineEdit::CommandLineEdit(QWidget *parent)
        : QFrame(parent)
        , m_badge(new QLabel(this))
        , m_editor(new QLineEdit(this))
        , m_managementIcon(new ManagementIcon(this))
    {
        setObjectName(QStringLiteral("commandInput"));
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 4, 5, 4);
        layout->setSpacing(6);
        m_badge->setObjectName(QStringLiteral("commandBadge"));
        m_badge->hide();
        m_editor->setObjectName(QStringLiteral("commandArgumentEdit"));
        m_managementIcon->setToolTip(tr("Open settings"));

        layout->addWidget(m_badge);
        layout->addWidget(m_editor, 1);
        layout->addWidget(m_managementIcon, 0, Qt::AlignCenter);
        setFocusProxy(m_editor);
        setProperty("focused", false);
        m_editor->installEventFilter(this);
        connect(m_managementIcon, &ManagementIcon::clicked, this, &CommandLineEdit::managementRequested);

        connect(m_editor, &QLineEdit::textChanged, this, [this](const QString &value) {
            if (m_updating) return;
            if (m_command.isEmpty()) {
                const int separator = value.indexOf(QLatin1Char(' '));
                if (separator < 0) return;
                const QString candidate = separator < 0 ? value : value.left(separator);
                if (m_commands.contains(candidate)) {
                    setToken(candidate, value.mid(separator + 1));
                }
            }
        });
}

void CommandLineEdit::setCommands(const QStringList &commands)
{
        m_commands = QSet<QString>(commands.cbegin(), commands.cend());
}

void CommandLineEdit::setPlaceholderText(const QString &text)
{
        m_defaultPlaceholder = text;
        m_editor->setPlaceholderText(text);
}

QString CommandLineEdit::text() const
{
        if (m_command.isEmpty()) return m_editor->text();
        return m_command + QLatin1Char(' ') + m_editor->text();
}

void CommandLineEdit::setText(const QString &text)
{
        const int separator = text.indexOf(QLatin1Char(' '));
        const QString candidate = separator < 0 ? text : text.left(separator);
        if (separator >= 0 && m_commands.contains(candidate)) {
            setToken(candidate, text.mid(separator + 1));
        } else {
            clearToken(text);
        }
}

QLineEdit *CommandLineEdit::editor() const
{
        return m_editor;
}

bool CommandLineEdit::eventFilter(QObject *watched, QEvent *event)
{
        if (watched == m_editor && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
            setFocused(event->type() == QEvent::FocusIn);
        }
        if (watched == m_editor && event->type() == QEvent::KeyPress && !m_command.isEmpty()) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Backspace && m_editor->cursorPosition() == 0
                && !m_editor->hasSelectedText()) {
                const QString command = m_command;
                clearToken(command);
                return true;
            }
        }
        return QFrame::eventFilter(watched, event);
}

void CommandLineEdit::setFocused(bool focused)
{
        if (property("focused").toBool() == focused) return;
        setProperty("focused", focused);
        // 动态属性变化需要重新 polish 才能命中样式表的 [focused="true"] 规则。
        style()->polish(this);
        update();
}

void CommandLineEdit::setToken(const QString &command, const QString &arguments)
{
        m_updating = true;
        m_command = command;
        m_badge->setText(command);
        m_badge->show();
        m_editor->setText(arguments);
        m_editor->setPlaceholderText(tr("Type arguments..."));
        m_editor->setCursorPosition(arguments.size());
        m_updating = false;
}

void CommandLineEdit::clearToken(const QString &value)
{
        m_updating = true;
        m_command.clear();
        m_badge->hide();
        m_editor->setText(value);
        m_editor->setPlaceholderText(m_defaultPlaceholder);
        m_editor->setCursorPosition(value.size());
        m_updating = false;
}

MainWindow::MainWindow(AppLauncher *launcher, PluginManager *pluginManager, Settings *settings,
                       Updater *updater, ThemeManager *themeManager, QWidget *parent)
    : QWidget(parent)
    , m_launcher(launcher)
    , m_pluginManager(pluginManager)
    , m_settings(settings)
    , m_updater(updater)
    , m_themeManager(themeManager)
{
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setProperty("useWindowMaterial", true);
    setWindowTitle(QStringLiteral("TypeGo"));
    resize(820, DefaultWindowHeight);
    setMinimumWidth(780);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *windowAgent = new QWK::WidgetWindowAgent(this);
    windowAgent->setup(this);

    auto *titleBar = new QWidget(this);
    m_titleBar = titleBar;
    titleBar->setObjectName(QStringLiteral("windowTitleBar"));
    titleBar->setFixedHeight(36);
    titleBar->installEventFilter(this);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(8, 0, 8, 0);
    m_titleBackSlot = new QWidget(titleBar);
    m_titleBackSlot->setObjectName(QStringLiteral("titleBackSlot"));
    m_titleBackSlot->setAutoFillBackground(false);
    m_titleBackSlot->setFixedWidth(32);
    auto *backSlotLayout = new QHBoxLayout(m_titleBackSlot);
    backSlotLayout->setContentsMargins(0, 0, 0, 0);
    m_pluginBackButton = new QPushButton(QStringLiteral("‹"), m_titleBackSlot);
    m_pluginBackButton->setObjectName(QStringLiteral("titleBackButton"));
    m_pluginBackButton->setFixedSize(32, 28);
    m_pluginBackButton->setFlat(true);
    m_pluginBackButton->setCursor(Qt::PointingHandCursor);
    m_pluginBackButton->setToolTip(tr("Back to results"));
    m_pluginBackButton->setAccessibleName(tr("Back"));
    m_pluginBackButton->hide();
    backSlotLayout->addWidget(m_pluginBackButton);
    auto *titleLabel = new QLabel(QStringLiteral("TypeGo"), titleBar);
    titleLabel->setObjectName(QStringLiteral("windowTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLayout->addWidget(m_titleBackSlot);
    titleLayout->addStretch();
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    auto *closeButton = new QPushButton(QStringLiteral("×"), titleBar);
    closeButton->setObjectName(QStringLiteral("windowCloseButton"));
    closeButton->setFixedSize(32, 28);
    titleLayout->addWidget(closeButton);
    windowAgent->setTitleBar(titleBar);
    windowAgent->setSystemButton(QWK::WindowAgentBase::Close, closeButton);
    windowAgent->setHitTestVisible(closeButton, true);
    windowAgent->setHitTestVisible(m_pluginBackButton, true);
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    connect(m_pluginBackButton, &QPushButton::clicked, this, &MainWindow::openResults);
    layout->addWidget(titleBar);
    applyTaskbarVisibility();

    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(20, 8, 20, 20);
    contentLayout->setSpacing(12);
    layout->addWidget(content, 1);

    auto *searchEdit = new CommandLineEdit;
    searchEdit->setCommands(m_pluginManager->commands());
    m_searchEdit = searchEdit;
    m_searchEdit->setPlaceholderText(tr("Search apps, or type a command (e.g. calc 1+2)"));
    connect(m_searchEdit, &CommandLineEdit::managementRequested, this, [this]() {
        openPluginView(QStringLiteral("user"));
    });
    contentLayout->addWidget(m_searchEdit);

    m_commandLabel = new QLabel;
    m_commandLabel->setObjectName(QStringLiteral("muted"));
    m_commandLabel->hide();
    contentLayout->addWidget(m_commandLabel);

    m_pinnedPanel = new QWidget;
    m_pinnedLayout = new QHBoxLayout(m_pinnedPanel);
    m_pinnedLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->addWidget(m_pinnedPanel);

    m_stack = new QStackedWidget;
    m_resultsModel = new ResultListModel(this);
    m_resultsList = new QListView;
    m_resultsList->setModel(m_resultsModel);
    // 搜索结果由委托绘制，需要鼠标跟踪才能拿到 State_MouseOver 做悬停反馈。
    m_resultsList->setObjectName(QStringLiteral("searchResults"));
    m_resultsList->setItemDelegate(new ResultItemDelegate(themeManager, m_resultsList));
    m_resultsList->setMouseTracking(true);
    m_resultsList->viewport()->setMouseTracking(true);
    if (themeManager) {
        // 主题切换时自绘颜色不在样式表里，需主动重绘可见行。
        connect(themeManager, &ThemeManager::paletteChanged, m_resultsList, [this]() {
            m_resultsList->viewport()->update();
        });
    }
    connect(m_resultsModel, &QAbstractItemModel::rowsInserted, this, [this]() {
        scheduleResultsLayout();
    });
    m_resultsList->setIconSize(QSize(ResultIconSize, ResultIconSize));
    m_resultsList->setUniformItemSizes(true);
    m_resultsList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultsList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_resultsList->setTextElideMode(Qt::ElideMiddle);
    m_resultsList->setWordWrap(false);
    connect(m_resultsList->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        auto *bar = m_resultsList->verticalScrollBar();
        if (!bar || !m_resultsModel->hasMore()) return;
        if (bar->maximum() - value <= bar->singleStep() * 2) {
            m_resultsModel->fetchNextBatch();
        }
    });
    m_stack->addWidget(m_resultsList);
    m_pluginPage = new QWidget;
    auto *pluginPageLayout = new QVBoxLayout(m_pluginPage);
    pluginPageLayout->setContentsMargins(0, 0, 0, 0);
    pluginPageLayout->setSpacing(0);
    m_pluginStack = new QStackedWidget;
    pluginPageLayout->addWidget(m_pluginStack, 1);
    m_stack->addWidget(m_pluginPage);
    contentLayout->addWidget(m_stack, 1);

    auto *debounce = new QTimer(this);
    m_searchDebounce = debounce;
    debounce->setInterval(120);
    debounce->setSingleShot(true);
    connect(m_searchEdit->editor(), &QLineEdit::textChanged, debounce, qOverload<>(&QTimer::start));
    connect(debounce, &QTimer::timeout, this, &MainWindow::performSearch);
    if (IPlugin *filePlugin = m_pluginManager->plugin(QStringLiteral("file"))) {
        connect(m_searchEdit->editor(), &QLineEdit::textChanged, filePlugin, &IPlugin::cancelQuery);
        connect(filePlugin, &IPlugin::queryResultsChanged, this, [this](const QString &query) {
            if (query != m_fileSearchQuery || m_searchEdit->text() != m_lastSearchText
                || m_stack->currentWidget() != m_resultsList) return;
            setResults(queryFiles(query));
            scheduleResultsLayout();
        });
    }
    connect(m_searchEdit->editor(), &QLineEdit::returnPressed, this, &MainWindow::launchCurrent);
    connect(m_resultsList, &QListView::clicked, this, [this](const QModelIndex &index) {
        if (m_settings->appLaunchRequiresDoubleClick() || m_commandMode) return;
        if (m_searchEdit->text() != m_lastSearchText) {
            performSearch();
            return;
        }
        if (index.data(Qt::UserRole).toMap().value(QStringLiteral("path")).toString().isEmpty()) return;
        launchCurrent();
    });
    connect(m_resultsList, &QListView::doubleClicked, this, [this](const QModelIndex &) {
        if (m_searchEdit->text() != m_lastSearchText) {
            performSearch();
            return;
        }
        if (m_settings->appLaunchRequiresDoubleClick() || m_commandMode) launchCurrent();
    });
    connect(m_resultsList, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QModelIndex index = m_resultsList->indexAt(pos);
        if (!index.isValid()) return;
        const QVariantMap item = index.data(Qt::UserRole).toMap();
        const QString path = localPathOf(item);
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
    new QShortcut(QKeySequence(Qt::Key_Escape), this, [this]() { handleEscape(); });

    connect(m_launcher, &AppLauncher::pinsChanged, this, &MainWindow::rebuildPinnedApps);
    auto refreshSearch = [this]() {
        if (m_stack->currentWidget() == m_resultsList) performSearch();
    };
    connect(m_launcher, &AppLauncher::appsChanged, this, refreshSearch);
    connect(m_settings, &Settings::showAllAppsWhenSearchEmptyChanged, this, refreshSearch);
    connect(m_settings, &Settings::showInTaskbarChanged, this, &MainWindow::applyTaskbarVisibility);
    if (m_updater) {
        connect(m_updater, &Updater::updateAvailable, this, [this](const QString &version, const QString &changelog, const QString &url, bool) {
            if (QMessageBox::question(this, tr("Update available"), tr("New version %1 is available.\n\n%2\n\nUpdate now?").arg(version, changelog)) == QMessageBox::Yes) {
                m_updater->downloadAndInstall(url);
            }
        });
        connect(m_updater, &Updater::downloadStarted, this, [this]() {
            if (m_downloadDialog) delete m_downloadDialog;
            m_downloadDialog = new QProgressDialog(tr("Downloading update..."), tr("Cancel"), 0, 0, this);
            m_downloadDialog->setWindowModality(Qt::WindowModal);
            connect(m_downloadDialog, &QProgressDialog::canceled, m_updater, &Updater::cancelDownload);
            m_downloadDialog->show();
        });
        connect(m_updater, &Updater::downloadProgress, this, [this](qint64 received, qint64 total) {
            if (!m_downloadDialog) return;
            if (total > 0) {
                m_downloadDialog->setRange(0, 100);
                m_downloadDialog->setValue(static_cast<int>((received * 100) / total));
            }
        });
        connect(m_updater, &Updater::downloadFinished, this, [this]() {
            if (m_downloadDialog) m_downloadDialog->close();
        });
        connect(m_updater, &Updater::downloadError, this, [this](const QString &message) {
            if (m_downloadDialog) m_downloadDialog->close();
            QMessageBox::warning(this, tr("Update failed"), message);
        });
    }

    rebuildPinnedApps();
    performSearch();
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
            if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) return;
            QWidget *active = QApplication::activeWindow();
            if (active && isAncestorOf(active)) return;
            if (!isActiveWindow() && m_settings->autoHideOnFocusLoss()) hide();
        });
    }
    return QWidget::event(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_titleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_titleBarDragging = true;
                m_titleBarDragGlobalPos = mouseEvent->globalPosition().toPoint();
                m_titleBarDragWindowPos = frameGeometry().topLeft();
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (m_titleBarDragging && (mouseEvent->buttons() & Qt::LeftButton)) {
                move(m_titleBarDragWindowPos + mouseEvent->globalPosition().toPoint() - m_titleBarDragGlobalPos);
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && m_titleBarDragging) {
                m_titleBarDragging = false;
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::Leave && m_titleBarDragging) {
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!event->spontaneous() || m_stack->currentWidget() != m_resultsList
        || !m_stack->isVisible() || m_resultsModel->rowCount() == 0) return;

    m_resultsMaximumWindowHeight = event->size().height();
    m_stack->setFixedHeight(resultsHeightForWindow(event->size().height()));
}

void MainWindow::performSearch()
{
    m_searchDebounce->stop();
    m_fileSearchQuery.clear();
    const bool viewingPlugin = m_stack->currentWidget() == m_pluginPage;

    const QString text = m_searchEdit->text();
    m_lastSearchText = text;
    const QString trimmed = text.trimmed();
    QVariantList results;
    m_commandMode = false;
    m_currentCmd.clear();

    QString args;
    commandParts(&m_currentCmd, &args);
    if (!m_currentCmd.isEmpty()) {
        m_commandMode = true;
        const IPlugin::LaunchMode mode = m_pluginManager->launchModeForCommand(m_currentCmd, args);
        if (!viewingPlugin && mode == IPlugin::LaunchMode::FullView && args.trimmed().isEmpty()
            && text.contains(QLatin1Char(' '))
            && m_pluginManager->autoEnterForCommand(m_currentCmd)) {
            openPluginView(m_currentCmd, args);
            return;
        }
        results = m_currentCmd == QStringLiteral("file") && m_settings->isPluginEnabled(QStringLiteral("file")) ? queryFiles(args)
            : m_pluginManager->queryCommand(m_currentCmd, args, IPlugin::QueryContext::Auto);
    } else if (!trimmed.isEmpty() || m_settings->showAllAppsWhenSearchEmpty()) {
        results = m_launcher->search(text);
    }

    if (results.isEmpty() && !trimmed.isEmpty() && m_launcher->isScanning()
        && (!m_commandMode || m_currentCmd == QStringLiteral("app"))) {
        QVariantMap item;
        item["title"] = tr("Scanning applications...");
        item["subtitle"] = tr("Please wait for application indexing to finish.");
        item["action"] = QVariantMap{{"type", "none"}};
        results.append(item);
    }

    if (results.isEmpty() && !trimmed.isEmpty()
        && m_settings->searchEngine() == QStringLiteral("File Search")
        && m_settings->isPluginEnabled(QStringLiteral("file"))
        && m_pluginManager->plugin(QStringLiteral("file"))) {
        results = queryFiles(trimmed);
    }

    if (results.isEmpty() && !trimmed.isEmpty()) {
        const QString url = searchUrl(trimmed);
        if (!url.isEmpty()) {
            QVariantMap searchItem;
            searchItem["title"] = tr("Search \"%1\"").arg(trimmed);
            searchItem["subtitle"] = tr("Search with %1").arg(m_settings->searchEngine());
            QVariantMap action;
            action["type"] = QStringLiteral("openUrl");
            action["data"] = url;
            searchItem["action"] = action;
            results << searchItem;
        }
        if (m_settings->searchEngine() != QStringLiteral("File Search")
            && m_settings->isPluginEnabled(QStringLiteral("file"))
            && m_pluginManager->plugin(QStringLiteral("file"))) {
            QVariantMap fileItem;
            fileItem["title"] = tr("Search files \"%1\"").arg(trimmed);
            fileItem["subtitle"] = tr("Search with Everything");
            fileItem["icon"] = "";
            QVariantMap action;
            action["type"] = QStringLiteral("fileSearch");
            action["data"] = trimmed;
            fileItem["action"] = action;
            results << fileItem;
        }
    }

    setResults(results);
    updateCommandBar();
    const bool keepPluginOpen = viewingPlugin && m_commandMode && m_currentCmd == m_openPluginCmd;
    if (!keepPluginOpen) {
        openResults();
    }
    m_pinnedPanel->setVisible(m_launcher->hasPinnedApps()
                              && text.isEmpty() && !m_commandMode && !keepPluginOpen);
}

void MainWindow::launchCurrent()
{
    // 回车和点击都必须与当前输入一致，且不能重置用户已选中的最新结果。
    if (m_searchEdit->text() != m_lastSearchText) {
        performSearch();
        if (m_stack->currentWidget() != m_resultsList) return;
    }
    if (m_commandMode) {
        QString command;
        QString args;
        commandParts(&command, &args);
        const IPlugin::LaunchMode mode = m_pluginManager->launchModeForCommand(command, args);
        if (mode != IPlugin::LaunchMode::List && openPluginView(command, args)) return;
        // 输入已刷新，直接执行当前选中结果，避免重查后意外跳回首项。
    }

    const QVariantMap item = currentItemData();
    if (item.isEmpty()) return;
    const QVariantMap action = item.value(QStringLiteral("action")).toMap();
    if (action.value(QStringLiteral("type")).toString() == QStringLiteral("fileSearch")) {
        const QString query = action.value(QStringLiteral("data")).toString();
        if (query.isEmpty()) return;
        setResults(queryFiles(query));
        updateCommandBar();
        openResults();
        return;
    }
    if (!action.isEmpty() && action.value(QStringLiteral("type")).toString() != QStringLiteral("none")) {
        if (m_pluginManager->executeAction(action)) hide();
        else QMessageBox::warning(this, tr("Action failed"), tr("The selected item could not be opened or executed."));
        return;
    }
    const QString path = item.value(QStringLiteral("path")).toString();
    if (!path.isEmpty()) {
        launchPath(path);
    }
}

QVariantList MainWindow::queryFiles(const QString &query)
{
    m_fileSearchQuery = query.trimmed();
    return m_pluginManager->queryCommand(QStringLiteral("file"), m_fileSearchQuery);
}

void MainWindow::launchPath(const QString &path)
{
    if (m_launcher->launch(path)) hide();
    else QMessageBox::warning(this, tr("Cannot open item"),
                              tr("Cannot open %1. Check that the file exists and an application is installed to open it.").arg(path));
}

void MainWindow::handleEscape()
{
    // 插件视图优先返回结果页，避免 Esc 直接关掉整个窗口而丢失上下文。
    if (m_stack->currentWidget() == m_pluginPage) {
        openResults();
        m_searchEdit->setFocus();
        return;
    }
    // 结果页上先清空输入，再次 Esc 才隐藏窗口。
    if (!m_searchEdit->text().isEmpty()) {
        m_searchEdit->setText(QString());
        return;
    }
    hide();
}

void MainWindow::moveSelection(int delta)
{
    if (m_searchEdit->text() != m_lastSearchText) performSearch();
    if (m_stack->currentWidget() != m_resultsList || m_resultsModel->rowCount() == 0) return;
    int row = m_resultsList->currentIndex().row();
    if (row < 0) row = 0;
    if (delta > 0 && row >= m_resultsModel->rowCount() - 1 && m_resultsModel->hasMore()) {
        m_resultsModel->fetchNextBatch();
    }
    row = qBound(0, row + delta, m_resultsModel->rowCount() - 1);
    m_resultsList->setCurrentIndex(m_resultsModel->index(row, 0));
}

QString MainWindow::searchUrl(const QString &query) const
{
    const QString encoded = QUrl::toPercentEncoding(query);
    const QString engine = m_settings->searchEngine();
    if (engine == QStringLiteral("Google")) return QStringLiteral("https://www.google.com/search?q=%1").arg(encoded);
    if (engine == QStringLiteral("Baidu")) return QStringLiteral("https://www.baidu.com/s?wd=%1").arg(encoded);
    if (engine == QStringLiteral("DuckDuckGo")) return QStringLiteral("https://duckduckgo.com/?q=%1").arg(encoded);
    if (engine == QStringLiteral("File Search")) return QString();
    return QStringLiteral("https://www.bing.com/search?q=%1").arg(encoded);
}

void MainWindow::commandParts(QString *command, QString *args) const
{
    if (command) command->clear();
    if (args) args->clear();

    const QString text = m_searchEdit->text();
    const QString trimmed = text.trimmed();
    const int firstSpace = text.indexOf(QLatin1Char(' '));
    if (firstSpace > 0) {
        const QString candidate = text.left(firstSpace).trimmed();
        if (m_pluginManager->hasCommand(candidate)) {
            if (command) *command = candidate;
            if (args) *args = text.mid(firstSpace + 1);
        }
        return;
    }

    if (!trimmed.isEmpty() && m_pluginManager->hasCommand(trimmed)) {
        if (command) *command = trimmed;
    }
}

void MainWindow::setResults(const QVariantList &results)
{
    m_results = results;
    m_resultsModel->setResults(results);
    if (m_resultsModel->rowCount() > 0) m_resultsList->setCurrentIndex(m_resultsModel->index(0, 0));
}

void MainWindow::updateResultsLayout()
{
    m_resultsLayoutPending = false;
    if (m_stack->currentWidget() != m_resultsList) return;

    // 先折叠结果区，得到不受结果数量和插件视图影响的稳定基础高度。
    m_stack->hide();
    m_stack->setFixedHeight(0);
    m_stack->updateGeometry();
    if (QWidget *content = m_stack->parentWidget()) content->updateGeometry();
    layout()->activate();
    m_resultsBaseHeight = sizeHint().height();

    const int resultsHeight = resultsHeightForWindow(m_resultsMaximumWindowHeight);
    if (resultsHeight > 0) {
        m_stack->setFixedHeight(resultsHeight);
        m_stack->show();
    }

    layout()->activate();
    const int targetHeight = m_resultsBaseHeight + resultsHeight;
    if (height() != targetHeight) resize(width(), targetHeight);
}

int MainWindow::resultsHeightForWindow(int windowHeight) const
{
    const int count = m_resultsModel->rowCount();
    if (count == 0) return 0;

    const int rowHeight = qMax(1, m_resultsList->sizeHintForIndex(m_resultsModel->index(0, 0)).height());
    const int contentHeight = rowHeight * count + m_resultsList->frameWidth() * 2 + ResultsExtraHeight;
    const int availableHeight = qMax(rowHeight, windowHeight - m_resultsBaseHeight);
    return qMin(contentHeight, availableHeight);
}

void MainWindow::scheduleResultsLayout()
{
    if (m_resultsLayoutPending) return;
    m_resultsLayoutPending = true;
    QTimer::singleShot(0, this, &MainWindow::updateResultsLayout);
}

void MainWindow::applyTaskbarVisibility()
{
    const bool wasVisible = isVisible();
    setWindowFlag(Qt::Tool, !m_settings->showInTaskbar());
    if (wasVisible) show();
}

QVariantMap MainWindow::currentItemData() const
{
    const QModelIndex index = m_resultsList->currentIndex();
    return index.isValid() ? index.data(Qt::UserRole).toMap() : QVariantMap();
}

void MainWindow::rebuildPinnedApps()
{
    while (QLayoutItem *item = m_pinnedLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const QVariantList pins = m_launcher->pinnedApps();
    if (pins.isEmpty()) {
        m_pinnedPanel->hide();
        scheduleResultsLayout();
        return;
    }

    auto *title = new QLabel(tr("Pinned"));
    title->setObjectName(QStringLiteral("muted"));
    m_pinnedLayout->addWidget(title);
    for (const QVariant &value : pins) {
        const QVariantMap pin = value.toMap();
        auto *button = new QPushButton(pin.value(QStringLiteral("name")).toString());
        button->setObjectName(QStringLiteral("pinnedAppButton"));
        const QString icon = pin.value(QStringLiteral("icon")).toString();
        if (!icon.isEmpty()) button->setIcon(QIcon(icon));
        button->setIconSize(QSize(24, 24));
        const QString path = pin.value(QStringLiteral("path")).toString();
        connect(button, &QPushButton::clicked, this, [this, path]() { launchPath(path); });
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
    const bool showPins = m_launcher->hasPinnedApps()
        && m_searchEdit->text().isEmpty() && !m_commandMode && m_openPluginCmd.isEmpty();
    m_pinnedPanel->setVisible(showPins);
    scheduleResultsLayout();
}

bool MainWindow::openPluginView(const QString &command)
{
    return openPluginView(command, QString());
}

bool MainWindow::openPluginView(const QString &command, const QString &args)
{
    IPlugin *plugin = m_pluginManager->plugin(command);
    QWidget *view = m_pluginManager->viewForCommand(command, args);
    if (!view) return false;
    m_searchDebounce->stop();
    m_fileSearchQuery.clear();

    if (plugin) {
        connect(plugin, &IPlugin::viewCloseRequested, this, &MainWindow::openResults, Qt::UniqueConnection);
    }
    if (m_pluginStack->indexOf(view) < 0) {
        m_pluginStack->addWidget(view);
    }
    m_pluginStack->setCurrentWidget(view);
    m_stack->setCurrentWidget(m_pluginPage);
    // 插件模式由插件视图占用可用空间，不继承结果列表的固定高度。
    m_stack->setVisible(true);
    m_stack->setVisible(true);
    m_stack->setMinimumHeight(0);
    m_stack->setMaximumHeight(QWIDGETSIZE_MAX);
    m_searchEdit->hide();
    m_commandLabel->hide();
    m_pinnedPanel->hide();
    m_pluginBackButton->show();
    if (height() != DefaultWindowHeight) resize(width(), DefaultWindowHeight);
    m_openPluginCmd = command;
    return true;
}

void MainWindow::activateCommand(const QString &command)
{
    showAndActivate();
    m_searchEdit->setText(command + QStringLiteral(" "));
    performSearch();
    IPlugin *plugin = m_pluginManager->plugin(command);
    if (plugin && plugin->supportsAutoEnter()
        && m_pluginManager->launchModeForCommand(command, QString()) == IPlugin::LaunchMode::FullView) {
        openPluginView(command, QString());
    }
}

void MainWindow::openResults()
{
    const bool wasPluginPage = m_stack->currentWidget() == m_pluginPage;
    m_stack->setCurrentWidget(m_resultsList);
    // 切换页后先清除插件尺寸提示，下一事件循环统一恢复结果布局。
    m_stack->hide();
    m_stack->setFixedHeight(0);
    m_stack->updateGeometry();
    m_openPluginCmd.clear();
    m_pluginBackButton->hide();
    m_searchEdit->show();
    if (wasPluginPage) {
        m_searchEdit->setText(QString());
        m_searchDebounce->stop();
        m_lastSearchText.clear();
        m_fileSearchQuery.clear();
        m_commandMode = false;
        m_currentCmd.clear();
        setResults(m_settings->showAllAppsWhenSearchEmpty() ? m_launcher->search(QString()) : QVariantList());
    }
    updateCommandBar();
    m_pinnedPanel->setVisible(m_launcher->hasPinnedApps()
                              && m_searchEdit->text().isEmpty() && !m_commandMode);
    scheduleResultsLayout();
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
    , m_icon(QStringLiteral(":/assets/icon.png"))
{
    setProperty("skipWindowMaterial", true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setToolTip(tr("Left-click to show or hide TypeGo, right-click for the menu"));
    resize(52, 52);
    updateIconCache();
    connect(m_themeManager, &ThemeManager::paletteChanged, this, [this]() {
        updateIconCache();
        update();
    });
}

void FloatIcon::updateIconCache()
{
    const qreal dpr = devicePixelRatioF();
    const QSize logicalSize = size();
    const QSize physicalSize = (QSizeF(logicalSize) * dpr).toSize();
    if (physicalSize.isEmpty()) return;
    if (!m_iconCache.isNull()
        && m_iconCacheDpr == dpr
        && m_iconCache.deviceIndependentSize().toSize() == logicalSize) {
        return;
    }

    m_iconCacheDpr = dpr;

    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter cachePainter(&image);
    cachePainter.scale(dpr, dpr);
    cachePainter.setRenderHint(QPainter::Antialiasing, true);
    cachePainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    cachePainter.setRenderHint(QPainter::LosslessImageRendering, true);

    const QRectF iconBounds = iconRect();
    const QPointF center = iconBounds.center();
    const qreal radius = iconBounds.width() / 2.0;
    for (int i = 3; i >= 1; --i) {
        const qreal spread = i * 0.9;
        const int alpha = 6 + (3 - i) * 5;
        cachePainter.setPen(Qt::NoPen);
        cachePainter.setBrush(QColor(0, 0, 0, alpha));
        cachePainter.drawEllipse(center + QPointF(0, 2.0),
                                 radius + spread,
                                 radius + spread);
    }

    cachePainter.end();

    const QRect iconPhysical = QRectF(iconBounds.topLeft() * dpr, iconBounds.size() * dpr).toAlignedRect();
    QImage iconImage = m_icon.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied)
                           .scaled(iconPhysical.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    if (!iconImage.isNull()) {
        const QRect crop((iconImage.width() - iconPhysical.width()) / 2,
                         (iconImage.height() - iconPhysical.height()) / 2,
                         iconPhysical.width(),
                         iconPhysical.height());
        iconImage = iconImage.copy(crop);

        QImage mask(iconPhysical.size(), QImage::Format_ARGB32_Premultiplied);
        mask.fill(Qt::transparent);
        QPainter maskPainter(&mask);
        maskPainter.setRenderHint(QPainter::Antialiasing, true);
        maskPainter.setPen(Qt::NoPen);
        maskPainter.setBrush(Qt::white);
        const qreal inset = 0.75 * dpr;
        maskPainter.drawEllipse(QRectF(mask.rect()).adjusted(inset, inset, -inset, -inset));
        maskPainter.end();

        QPainter iconPainter(&iconImage);
        iconPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        iconPainter.drawImage(0, 0, mask);
        iconPainter.end();

        QPainter imagePainter(&image);
        imagePainter.drawImage(iconPhysical.topLeft(), iconImage);
    }

    m_iconCache = QPixmap::fromImage(image);
    m_iconCache.setDevicePixelRatio(dpr);
}

QRectF FloatIcon::iconRect() const
{
    return QRectF(rect()).adjusted(4.0, 4.0, -4.0, -5.0);
}

void FloatIcon::paintEvent(QPaintEvent *)
{
    updateIconCache();
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(QPointF(0, 0), m_iconCache);
}

void FloatIcon::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_pressGlobalPos = event->globalPosition().toPoint();
        m_dragging = false;
    }
}

void FloatIcon::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        const QPoint globalPos = event->globalPosition().toPoint();
        // 超过系统拖动阈值才算拖动，否则手抖会让单击失效。
        if (!m_dragging && (globalPos - m_pressGlobalPos).manhattanLength() <= QApplication::startDragDistance()) {
            return;
        }
        m_dragging = true;
        move(globalPos - m_dragOffset);
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

