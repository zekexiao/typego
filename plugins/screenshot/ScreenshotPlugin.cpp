#include "ScreenshotPlugin.h"

#include "PinEditor.h"
#include "PinWindow.h"
#include "ScrollResultWindow.h"
#include "ScreenshotSettingsWidget.h"
#include "ScreenshotStore.h"
#include "WinCapture.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QWidget>
#include <utility>

namespace Api = TypeGo::PluginApi;

namespace {
QString formatSize(qint64 bytes)
{
    if (bytes >= 1024 * 1024) return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 KB").arg(qMax<qint64>(1, bytes / 1024));
}
} // namespace

Api::Error ScreenshotPlugin::initialize(Api::Host *host)
{
    m_host = host;
    const QString translations = QDir(host->directory(Api::Directory::Package))
        .filePath(QStringLiteral("translations"));
    if (m_translator.load(QLocale(), QStringLiteral("Screenshot"), QStringLiteral("_"), translations))
        QCoreApplication::installTranslator(&m_translator);

    const QString dataDir = host->directory(Api::Directory::Data);
    const QString cacheDir = host->directory(Api::Directory::Cache);
    if (dataDir.isEmpty())
        return {Api::ErrorCode::IoError, QStringLiteral("Screenshot data directory is unavailable")};
    m_store = std::make_unique<ScreenshotStore>(dataDir, cacheDir);
    applyConfig();
    return {};
}

QString ScreenshotPlugin::defaultSaveDir() const
{
    QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty()) pictures = QDir::homePath();
    return QDir(pictures).filePath(QStringLiteral("TypeGo Screenshots"));
}

void ScreenshotPlugin::applyConfig()
{
    if (!m_host) return;
    m_saveDir = m_host->configValue(QStringLiteral("saveDir"), defaultSaveDir()).toString();
    if (m_saveDir.isEmpty()) m_saveDir = defaultSaveDir();
    m_historyLimit = qBound(0, m_host->configValue(QStringLiteral("historyLimit"), 30).toInt(), 200);
    m_magnifier = m_host->configValue(QStringLiteral("magnifier"), true).toBool();
    m_magnifierZoom = qBound(2, m_host->configValue(QStringLiteral("magnifierZoom"), 8).toInt(), 20);
    if (m_store) m_store->setHistoryLimit(m_historyLimit);
}

Api::QueryResult ScreenshotPlugin::query(const Api::QueryRequest &request)
{
    if (request.invocation.commandId != QLatin1String("history")) {
        const QString command = request.invocation.commandId;
        if (command == QLatin1String("capture") || command == QLatin1String("fullscreen")
            || command == QLatin1String("scroll")) {
            const bool fullscreen = command == QLatin1String("fullscreen");
            const bool scrolling = command == QLatin1String("scroll");
            Api::ResultItem hint;
            hint.id = QStringLiteral("capture");
            hint.title = scrolling ? tr("Scrolling screenshot") : tr("Capture the screen");
            if (scrolling) {
                hint.subtitle = tr("Select the scrolling area; capture starts after you release the mouse");
            } else if (fullscreen) {
                hint.subtitle = tr("Start with the current display selected");
            } else {
                hint.subtitle = tr("Drag to select an area, click a highlighted window, or press Enter to copy");
            }
            return {request.id, {hint}, false};
        }
        return {request.id, {}};
    }

    QList<Api::ResultItem> items;
    const int limit = qMin(request.limit, Api::MaxResults);
    const QString filter = request.invocation.arguments.trimmed().toLower();
    const auto entries = m_store ? m_store->entries() : QList<ShotHistoryEntry>();
    for (const ShotHistoryEntry &entry : entries) {
        const QString time = QLocale().toString(QDateTime::fromMSecsSinceEpoch(entry.modified),
                                                QLocale::ShortFormat);
        if (!filter.isEmpty() && !time.toLower().contains(filter)
            && !entry.id.toLower().contains(filter)) continue;
        Api::ResultItem item;
        item.id = entry.id;
        item.title = tr("Screenshot %1").arg(time);
        item.subtitle = tr("%1 × %2 · %3 · Enter to pin")
                            .arg(entry.size.width()).arg(entry.size.height())
                            .arg(formatSize(QFileInfo(entry.path).size()));
        if (m_store) item.icon = m_store->ensureThumbnail(entry);
        item.actionId = QStringLiteral("pin");
        item.parameters = {{QStringLiteral("id"), entry.id}};
        items.append(item);
        if (items.size() >= limit) break;
    }
    if (items.isEmpty()) {
        items.append(Api::ResultItem{QStringLiteral("empty"),
            entries.isEmpty() ? tr("No screenshots yet") : tr("No matching screenshot"),
            entries.isEmpty() ? tr("Press F1 or run \"shot\" to capture the screen")
                              : tr("Try another keyword"),
            {}, {}, {}});
    }
    return {request.id, items, false};
}

Api::ExecutionResult ScreenshotPlugin::execute(const Api::Invocation &invocation)
{
    if (invocation.commandId == QLatin1String("capture")
        || invocation.commandId == QLatin1String("fullscreen")
        || invocation.commandId == QLatin1String("scroll")) {
        // 已在截图或标注编辑中时，重新触发截图会丢掉当前内容，先让用户确认。
        if (hasEditingInProgress() && !confirmRestartCapture())
            return {Api::Status::Succeeded, Api::HostAction::Keep};
        // 必须在宿主显示覆盖层之前记录前台窗口，完成或取消后据此恢复焦点。
        m_previousForeground = ShotWin::foregroundWindow();
        m_pendingFullscreen = invocation.commandId == QLatin1String("fullscreen");
        m_pendingScroll = invocation.commandId == QLatin1String("scroll");
        m_pendingStart = true;
        return {Api::Status::Succeeded, Api::HostAction::OpenView};
    }
    if (invocation.commandId == QLatin1String("history")) {
        if (invocation.actionId != QLatin1String("pin"))
            return {Api::Status::Succeeded, Api::HostAction::Keep};
        const QString id = invocation.parameters.value(QStringLiteral("id")).toString();
        const ShotHistoryEntry entry = m_store ? m_store->find(id) : ShotHistoryEntry{};
        if (entry.path.isEmpty())
            return {Api::Status::Failed, Api::HostAction::Keep,
                    {Api::ErrorCode::InvalidArgument, tr("The screenshot is no longer available")}};
        const QImage image(entry.path);
        if (image.isNull())
            return {Api::Status::Failed, Api::HostAction::Keep,
                    {Api::ErrorCode::IoError, tr("Cannot read the saved screenshot")}};
        addPin(image, {}, QCursor::pos());
        // 隐藏启动器，让贴图直接出现在刚选中的位置。
        return {Api::Status::Succeeded, Api::HostAction::Hide};
    }
    return {Api::Status::Failed, Api::HostAction::Keep,
            {Api::ErrorCode::InvalidArgument, QStringLiteral("Unsupported action")}};
}

QWidget *ScreenshotPlugin::createView(const Api::Invocation &, QWidget *parent)
{
    if (!parent) return nullptr;
    // 宿主为覆盖层创建的全屏窗口只作为生命周期锚点，真正的覆盖层按屏幕单独创建。
    parent->setAttribute(Qt::WA_TranslucentBackground, true);
    parent->setAttribute(Qt::WA_NoSystemBackground, true);
    parent->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    parent->setWindowFlag(Qt::FramelessWindowHint, true);
    parent->setWindowFlag(Qt::Tool, true);
    auto *view = new QWidget(parent);
    view->setAttribute(Qt::WA_NoSystemBackground, true);
    view->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    view->setAttribute(Qt::WA_TranslucentBackground, true);
    view->resize(1, 1);
    m_view = view;
    return view;
}

QWidget *ScreenshotPlugin::createSettingsView(QWidget *parent)
{
    if (!m_host || !m_store) return nullptr;
    ScreenshotSettingsWidget::Values values;
    values.saveDir = m_saveDir;
    values.historyLimit = m_historyLimit;
    values.magnifier = m_magnifier;
    values.magnifierZoom = m_magnifierZoom;
    values.historyCount = m_store->entries().size();
    return new ScreenshotSettingsWidget(values, parent,
        [this](const ScreenshotSettingsWidget::Values &next) {
            m_saveDir = next.saveDir;
            m_historyLimit = qBound(0, next.historyLimit, 200);
            m_magnifier = next.magnifier;
            m_magnifierZoom = qBound(2, next.magnifierZoom, 20);
            if (m_store) m_store->setHistoryLimit(m_historyLimit);
            if (m_host) {
                m_host->setConfigValue(QStringLiteral("saveDir"), m_saveDir);
                m_host->setConfigValue(QStringLiteral("historyLimit"), m_historyLimit);
                m_host->setConfigValue(QStringLiteral("magnifier"), m_magnifier);
                m_host->setConfigValue(QStringLiteral("magnifierZoom"), m_magnifierZoom);
            }
        },
        [this]() { return m_store ? m_store->clear() : 0; });
}

void ScreenshotPlugin::activate(const Api::Invocation &)
{
    if (!m_pendingStart) return;
    m_pendingStart = false;
    // 宿主在 activate 之后才创建/显示覆盖层窗口，延后一拍再隐藏并捕获。
    QTimer::singleShot(0, this, &ScreenshotPlugin::beginCapture);
}

void ScreenshotPlugin::beginCapture()
{
    if (!m_host || m_shuttingDown || !m_view) return;
    abandonSession();

    ScreenshotSession::Options options;
    options.fullscreen = m_pendingFullscreen;
    options.magnifier = m_magnifier;
    options.magnifierZoom = m_magnifierZoom;
    options.autoScroll = m_pendingScroll;
    m_pendingFullscreen = false;
    m_pendingScroll = false;

    auto session = std::make_unique<ScreenshotSession>(m_previousForeground, options);
    session->setController(m_view);
    connect(session.get(), &ScreenshotSession::completed, this,
            [this](ScreenshotSession::Finish action, const QImage &image, const QRect &selection) {
                onCompleted(action, image, selection);
            });
    connect(session.get(), &ScreenshotSession::scrollFinished, this, &ScreenshotPlugin::onScrollFinished);
    connect(session.get(), &ScreenshotSession::failed, this, &ScreenshotPlugin::onFailed);
    m_session = std::move(session);
    m_session->start();
}

void ScreenshotPlugin::onCompleted(ScreenshotSession::Finish action, const QImage &image,
                                   const QRect &selection)
{
    // 合成与贴图都以裁剪区为基准，原始选区不再参与定位。
    Q_UNUSED(selection)
    ScreenshotSession *session = m_session.release();
    // 贴图保留未标注的底图和矢量标注，方便之后继续修改。
    QImage pinBase;
    QVector<ShotAnnotation> pinAnnotations;
    QPoint pinOrigin;
    if (session && action == ScreenshotSession::Finish::Pin) {
        pinBase = session->composeImage(false);
        pinAnnotations = session->relativeAnnotations();
        pinOrigin = session->cropRect().topLeft();
    }
    if (session) session->deleteLater();
    if (!m_shuttingDown && m_host) m_host->closeView();
    switch (action) {
    case ScreenshotSession::Finish::Copy:
        copyImage(image);
        recordImage(image);
        break;
    case ScreenshotSession::Finish::Save:
        recordImage(image);
        saveImageAs(image);
        break;
    case ScreenshotSession::Finish::Pin:
        recordImage(image);
        if (pinBase.isNull()) pinBase = image;
        addPin(pinBase, pinAnnotations, pinOrigin);
        break;
    case ScreenshotSession::Finish::Cancel:
        break;
    }
}

void ScreenshotPlugin::onScrollFinished(const QImage &image)
{
    ScreenshotSession *session = m_session.release();
    if (session) session->deleteLater();
    if (!m_shuttingDown && m_host) m_host->closeView();
    recordImage(image);
    showScrollResult(image);
}

void ScreenshotPlugin::onFailed(const QString &message)
{
    ScreenshotSession *session = m_session.release();
    if (session) session->deleteLater();
    if (m_host) m_host->log(Api::LogLevel::Warning, message);
    if (!m_shuttingDown && m_host) m_host->closeView();
    if (!m_shuttingDown)
        QMessageBox::warning(nullptr, tr("Screenshot failed"), message);
}

void ScreenshotPlugin::abandonSession()
{
    if (!m_session) return;
    // 不触发 completed，避免在禁用/退出或重新截图时再次走宿主界面流程。
    ScreenshotSession *session = m_session.release();
    session->deleteLater();
}

void ScreenshotPlugin::recordImage(const QImage &image)
{
    if (!m_store || m_historyLimit <= 0 || image.isNull()) return;
    QString error;
    if (m_store->record(image, &error).isEmpty() && m_host && !error.isEmpty())
        m_host->log(Api::LogLevel::Warning, error);
}

void ScreenshotPlugin::copyImage(const QImage &image)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) return;
    clipboard->setImage(image);
}

void ScreenshotPlugin::saveImageAs(const QImage &image)
{
    QString directory = m_saveDir;
    if (directory.isEmpty()) directory = defaultSaveDir();
    QDir().mkpath(directory);
    const QString defaultPath = QDir(directory).filePath(
        QStringLiteral("Screenshot_%1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
    QString path = QFileDialog::getSaveFileName(nullptr, tr("Save screenshot"), defaultPath,
                                                tr("PNG image (*.png);;JPEG image (*.jpg *.jpeg)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".png");
    const QString format = QFileInfo(path).suffix().toLower() == QLatin1String("jpg")
            || QFileInfo(path).suffix().toLower() == QLatin1String("jpeg")
        ? QStringLiteral("jpg") : QStringLiteral("png");
    if (!image.save(path, format.toLatin1().constData())) {
        QMessageBox::warning(nullptr, tr("Save failed"), tr("Cannot save the image to %1").arg(path));
        return;
    }
    m_saveDir = QFileInfo(path).absolutePath();
    if (m_host) m_host->setConfigValue(QStringLiteral("saveDir"), m_saveDir);
}

void ScreenshotPlugin::addPin(const QImage &base, const QVector<ShotAnnotation> &annotations,
                              const QPoint &position)
{
    if (base.isNull()) return;
    auto *pin = new PinWindow(base, annotations, position);
    m_pins.append(pin);
    connect(pin, &PinWindow::closed, this, [this, pin]() { m_pins.removeAll(pin); });
    pin->show();
    pin->raise();
    pin->activateWindow();
}

void ScreenshotPlugin::closePins()
{
    const auto pins = m_pins;
    m_pins.clear();
    for (const QPointer<PinWindow> &pin : pins)
        if (pin) pin->close();
}

bool ScreenshotPlugin::hasEditingInProgress() const
{
    // 只有已画了标注的截图会话或打开的贴图编辑窗口才算“正在编辑”，
    // 单纯还在框选阶段重新截图属于正常操作，不需要打断。
    if (m_session && !m_session->annotations().isEmpty()) return true;
    for (const QPointer<PinWindow> &pin : m_pins) {
        if (pin && pin->findChild<PinEditor *>(QString(), Qt::FindDirectChildrenOnly))
            return true;
    }
    return false;
}

bool ScreenshotPlugin::confirmRestartCapture()
{
    // 截图覆盖层是置顶全屏窗口，对话框必须挂到它（或贴图编辑窗口）上才不会被盖住。
    QWidget *parent = nullptr;
    if (m_session) {
        parent = m_session->dialogParent();
    } else {
        for (const QPointer<PinWindow> &pin : m_pins) {
            if (!pin) continue;
            if (auto *editor = pin->findChild<PinEditor *>(QString(), Qt::FindDirectChildrenOnly)) {
                parent = editor;
                break;
            }
        }
    }
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Screenshot in progress"));
    box.setText(tr("A screenshot or annotation edit is still in progress."));
    box.setInformativeText(tr("Start over and discard the current one?"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    box.show();
    box.raise();
    box.activateWindow();
    const bool accepted = box.exec() == QMessageBox::Yes;
    if (parent) {
        parent->raise();
        parent->activateWindow();
        parent->setFocus(Qt::OtherFocusReason);
    }
    return accepted;
}

void ScreenshotPlugin::showScrollResult(const QImage &image)
{
    if (image.isNull()) return;
    auto *window = new ScrollResultWindow(image, QCursor::pos());
    m_scrollResults.append(window);
    connect(window, &ScrollResultWindow::closed, this,
            [this, window]() { m_scrollResults.removeAll(window); });
    window->show();
    window->raise();
    window->activateWindow();
}

void ScreenshotPlugin::closeScrollResults()
{
    const auto windows = m_scrollResults;
    m_scrollResults.clear();
    for (const QPointer<ScrollResultWindow> &window : windows)
        if (window) window->close();
}

void ScreenshotPlugin::deactivate()
{
    abandonSession();
}

Api::LeaveDecision ScreenshotPlugin::prepareToLeave(Api::LeaveReason reason, quint64 token)
{
    Q_UNUSED(token)
    if (reason == Api::LeaveReason::Disable || reason == Api::LeaveReason::Shutdown) {
        if (reason == Api::LeaveReason::Shutdown) m_shuttingDown = true;
        abandonSession();
        closePins();
        closeScrollResults();
    }
    return Api::LeaveDecision::Ready;
}

void ScreenshotPlugin::shutdown()
{
    m_shuttingDown = true;
    abandonSession();
    closePins();
    closeScrollResults();
    m_store.reset();
    m_host = nullptr;
    QCoreApplication::removeTranslator(&m_translator);
}