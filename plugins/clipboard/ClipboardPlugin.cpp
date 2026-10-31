#include "ClipboardPlugin.h"
#include "ClipboardIcons.h"
#include "ClipboardSettingsWidget.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QMimeData>
#include <QTimer>
#include <QUrl>
#include <utility>

namespace Api = TypeGo::PluginApi;

namespace {
QString timeText(qint64 msecs)
{
    return QLocale().toString(QDateTime::fromMSecsSinceEpoch(msecs), QLocale::ShortFormat);
}

// 图片签名：按像素算，用于识别剪贴板回声时确认内容一致。
QByteArray imageSignature(const QImage &image)
{
    if (image.isNull()) return {};
    const QImage normalized = image.convertToFormat(QImage::Format_ARGB32);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (int y = 0; y < normalized.height(); ++y) {
        hash.addData(reinterpret_cast<const char *>(normalized.constScanLine(y)),
                     normalized.bytesPerLine());
    }
    return hash.result();
}

// 剪贴板里的本地文件路径；网页链接之类没有本地路径，返回空。
QStringList localFilePaths(const QMimeData *mime)
{
    QStringList paths;
    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    return paths;
}
} // namespace

Api::Error ClipboardPlugin::initialize(Api::Host *host)
{
    m_host = host;
    const QString translations = QDir(host->directory(Api::Directory::Package))
        .filePath(QStringLiteral("translations"));
    if (m_translator.load(QLocale(), QStringLiteral("Clipboard"), QStringLiteral("_"), translations))
        QCoreApplication::installTranslator(&m_translator);

    const QString dataDir = host->directory(Api::Directory::Data);
    if (dataDir.isEmpty())
        return {Api::ErrorCode::IoError, QStringLiteral("Clipboard data directory is unavailable")};
    m_store = std::make_unique<ClipboardStore>(dataDir, host->directory(Api::Directory::Cache));
    applyConfig();
    m_store->load();
    // 索引损坏时历史读不出来，但磁盘文件已经保留；提示用户而不是静默丢弃。
    if (!m_store->lastError().isEmpty()) {
        const QString message = m_store->indexCorrupted()
            ? QStringLiteral("%1. The damaged index was kept and a new history was started.")
                  .arg(m_store->lastError())
            : m_store->lastError();
        host->log(Api::LogLevel::Warning, message);
    }

    // 连续复制会重复触发 dataChanged，合并后再读取剪贴板。
    m_captureTimer.setSingleShot(true);
    m_captureTimer.setInterval(120);
    connect(&m_captureTimer, &QTimer::timeout, this, &ClipboardPlugin::captureClipboard);
    startMonitoring();
    // 记录启动前已经在剪贴板中的内容；重启后历史仍以最新内容开头。
    QTimer::singleShot(0, this, [this]() { captureClipboard(); });
    return {};
}

void ClipboardPlugin::applyConfig()
{
    if (!m_host || !m_store) return;
    m_captureText = m_host->configValue(QStringLiteral("captureText"), true).toBool();
    m_captureImages = m_host->configValue(QStringLiteral("captureImages"), true).toBool();
    m_captureFiles = m_host->configValue(QStringLiteral("captureFiles"), true).toBool();
    m_store->setMaxItems(m_host->configValue(QStringLiteral("maxItems"), 50).toInt());
}

void ClipboardPlugin::startMonitoring()
{
    if (m_clipboard) return;
    m_clipboard = QGuiApplication::clipboard();
    if (!m_clipboard) return;
    connect(m_clipboard, &QClipboard::dataChanged, this, [this]() {
        if (m_captureText || m_captureImages || m_captureFiles) m_captureTimer.start();
    });
}

void ClipboardPlugin::stopMonitoring()
{
    m_captureTimer.stop();
    m_echo = ClipboardEcho{};
    if (!m_clipboard) return;
    disconnect(m_clipboard, &QClipboard::dataChanged, this, nullptr);
    m_clipboard = nullptr;
}

bool ClipboardPlugin::isSelfEcho(const QMimeData *mime) const
{
    switch (m_echo.type) {
    case ClipboardItem::Type::Text:
        return mime->hasText() && mime->text() == m_echo.text;
    case ClipboardItem::Type::Files:
        return mime->hasUrls() && localFilePaths(mime) == m_echo.files;
    case ClipboardItem::Type::Image:
        return mime->hasImage()
            && imageSignature(qvariant_cast<QImage>(mime->imageData())) == m_echo.imageSignature;
    }
    return false;
}

void ClipboardPlugin::captureClipboard()
{
    if (!m_store || !m_clipboard) return;
    const QMimeData *mime = m_clipboard->mimeData();
    if (!mime) return;
    // 系统回声：内容与刚写入的一致才算自身回声，否则按用户的真实复制处理。
    if (m_echo.active) {
        const bool echo = isSelfEcho(mime);
        m_echo = ClipboardEcho{};
        if (echo) return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    // 文件优先：从资源管理器复制文件时，剪贴板同时可能带有路径文本。
    if (m_captureFiles && mime->hasUrls()) {
        // 只有存在本地文件才算文件复制；网页超链接也带 uri-list，但文本才是有效内容。
        const QStringList paths = localFilePaths(mime);
        if (!paths.isEmpty()) changed = m_store->captureFiles(paths, now);
    }
    if (!changed && m_captureImages && mime->hasImage())
        changed = m_store->captureImage(qvariant_cast<QImage>(mime->imageData()), now);
    if (!changed && m_captureText && mime->hasText())
        changed = m_store->captureText(mime->text(), now);
    if (changed) {
        saveHistory();
    } else if (!m_store->lastError().isEmpty() && m_host) {
        m_host->log(Api::LogLevel::Warning, m_store->lastError());
    }
}

void ClipboardPlugin::saveHistory()
{
    if (!m_store || m_store->save() || !m_host) return;
    m_host->log(Api::LogLevel::Warning, m_store->lastError());
}

bool ClipboardPlugin::matches(const ClipboardItem &item, const QString &filter) const
{
    switch (item.type) {
    case ClipboardItem::Type::Text:
        return item.text.contains(filter, Qt::CaseInsensitive);
    case ClipboardItem::Type::Files:
        for (const QString &file : item.files)
            if (file.contains(filter, Qt::CaseInsensitive)) return true;
        return false;
    case ClipboardItem::Type::Image:
        return QStringLiteral("image").contains(filter, Qt::CaseInsensitive)
            || tr("image").contains(filter, Qt::CaseInsensitive)
            || filter.contains(tr("image"), Qt::CaseInsensitive);
    }
    return false;
}

Api::ResultItem ClipboardPlugin::resultItem(const ClipboardItem &item) const
{
    Api::ResultItem result;
    result.id = item.id;
    result.actionId = QStringLiteral("copy");
    result.parameters = {{QStringLiteral("id"), item.id}};
    const QString time = timeText(item.timestamp);
    switch (item.type) {
    case ClipboardItem::Type::Text: {
        QString title = item.text.simplified();
        if (title.size() > 160) title = title.left(157) + QStringLiteral("...");
        result.title = title;
        result.subtitle = tr("Text · %1 characters · %2").arg(item.text.size()).arg(time);
        result.icon = clipboardTypeIcon(item.type, m_store->cacheDir());
        break;
    }
    case ClipboardItem::Type::Files: {
        QStringList names;
        for (const QString &file : item.files) {
            names.append(QFileInfo(file).fileName());
            if (names.size() == 3) break;
        }
        QString title = names.join(QStringLiteral(", "));
        if (item.files.size() > names.size()) title += QStringLiteral(", ...");
        result.title = title;
        result.subtitle = (item.files.size() == 1 ? tr("1 file") : tr("%1 files").arg(item.files.size()))
            + QStringLiteral(" · ") + time;
        result.icon = clipboardTypeIcon(item.type, m_store->cacheDir());
        break;
    }
    case ClipboardItem::Type::Image: {
        result.title = tr("Image %1 × %2").arg(item.imageSize.width()).arg(item.imageSize.height());
        const qint64 bytes = QFileInfo(m_store->imagePath(item)).size();
        result.subtitle = tr("%1 KB · %2").arg(qMax<qint64>(1, bytes / 1024)).arg(time);
        const QString thumbnail = m_store->thumbnailPath(item);
        if (!thumbnail.isEmpty() && QFileInfo::exists(thumbnail)) result.icon = thumbnail;
        else result.icon = clipboardTypeIcon(item.type, m_store->cacheDir());
        break;
    }
    }
    return result;
}

Api::QueryResult ClipboardPlugin::query(const Api::QueryRequest &request)
{
    if (!m_store)
        return {request.id, {}, false, {Api::ErrorCode::Internal, QStringLiteral("Clipboard is not initialized")}};
    const QString filter = request.invocation.arguments.trimmed();
    QList<Api::ResultItem> items;
    const int limit = qMin(request.limit, Api::MaxResults);
    for (const ClipboardItem &entry : m_store->items()) {
        if (!filter.isEmpty() && !matches(entry, filter)) continue;
        // 缓存被清理后按需重建缩略图，避免启动时批量解码图片。
        if (entry.type == ClipboardItem::Type::Image) m_store->ensureThumbnail(entry);
        items.append(resultItem(entry));
        if (items.size() >= limit) break;
    }
    if (items.isEmpty()) {
        items.append(Api::ResultItem{QStringLiteral("empty"),
            filter.isEmpty() ? tr("Clipboard history is empty") : tr("No matching clipboard item"),
            filter.isEmpty() ? tr("Copy text, images or files and they will appear here")
                             : tr("Try another keyword"),
            {}, {}, {}});
    }
    return {request.id, items, false};
}

Api::ExecutionResult ClipboardPlugin::execute(const Api::Invocation &invocation)
{
    if (invocation.actionId != QLatin1String("copy"))
        return {Api::Status::Failed, Api::HostAction::Keep,
                {Api::ErrorCode::InvalidArgument, QStringLiteral("Select a clipboard item first")}};
    const ClipboardItem *entry = m_store ? m_store->find(invocation.parameters.value(QStringLiteral("id")).toString()) : nullptr;
    if (!entry)
        return {Api::Status::Failed, Api::HostAction::Keep,
                {Api::ErrorCode::InvalidArgument, QStringLiteral("Clipboard item is no longer available")}};
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return {Api::Status::Failed, Api::HostAction::Keep,
                {Api::ErrorCode::Internal, QStringLiteral("Cannot access the system clipboard")}};

    // 粘贴自身内容会再次触发 dataChanged；记下写入内容，回声到达时按内容比对跳过。
    m_echo = ClipboardEcho{};
    m_echo.active = true;
    switch (entry->type) {
    case ClipboardItem::Type::Text:
        m_echo.type = ClipboardItem::Type::Text;
        m_echo.text = entry->text;
        clipboard->setText(entry->text);
        break;
    case ClipboardItem::Type::Image: {
        const QImage image(m_store->imagePath(*entry));
        if (image.isNull()) {
            m_echo = ClipboardEcho{};
            return {Api::Status::Failed, Api::HostAction::Keep,
                    {Api::ErrorCode::IoError, QStringLiteral("Cannot read the saved clipboard image")}};
        }
        m_echo.type = ClipboardItem::Type::Image;
        m_echo.imageSignature = imageSignature(image);
        clipboard->setImage(image);
        break;
    }
    case ClipboardItem::Type::Files: {
        QList<QUrl> urls;
        QStringList paths;
        for (const QString &file : entry->files) {
            urls.append(QUrl::fromLocalFile(file));
            paths.append(file);
        }
        auto *mime = new QMimeData;
        mime->setUrls(urls);
        mime->setText(paths.join(QLatin1Char('\n')));
        m_echo.type = ClipboardItem::Type::Files;
        m_echo.files = paths;
        clipboard->setMimeData(mime);
        break;
    }
    }
    if (m_store->touch(entry->id, QDateTime::currentMSecsSinceEpoch())) saveHistory();
    return {Api::Status::Succeeded, Api::HostAction::Hide};
}

int ClipboardPlugin::clearHistory()
{
    if (!m_store) return 0;
    m_store->clear();
    saveHistory();
    return 0;
}

QWidget *ClipboardPlugin::createSettingsView(QWidget *parent)
{
    if (!m_host || !m_store) return nullptr;
    ClipboardSettingsWidget::Values values;
    values.maxItems = m_store->maxItems();
    values.captureText = m_captureText;
    values.captureImages = m_captureImages;
    values.captureFiles = m_captureFiles;
    values.count = m_store->items().size();
    return new ClipboardSettingsWidget(values, parent,
        [this](const ClipboardSettingsWidget::Values &next) {
            m_captureText = next.captureText;
            m_captureImages = next.captureImages;
            m_captureFiles = next.captureFiles;
            if (m_host) {
                m_host->setConfigValue(QStringLiteral("maxItems"), next.maxItems);
                m_host->setConfigValue(QStringLiteral("captureText"), next.captureText);
                m_host->setConfigValue(QStringLiteral("captureImages"), next.captureImages);
                m_host->setConfigValue(QStringLiteral("captureFiles"), next.captureFiles);
            }
            if (m_store) {
                m_store->setMaxItems(next.maxItems);
                saveHistory();
            }
        },
        [this]() { return clearHistory(); });
}

void ClipboardPlugin::activate(const Api::Invocation &)
{
    // 禁用后再次使用时恢复监听；initialize 之后重复调用无副作用。
    startMonitoring();
}

Api::LeaveDecision ClipboardPlugin::prepareToLeave(Api::LeaveReason reason, quint64 token)
{
    Q_UNUSED(token)
    // 隐藏启动器不影响后台采集；禁用与退出时停止监听，避免继续读取剪贴板。
    if (reason == Api::LeaveReason::Disable || reason == Api::LeaveReason::Shutdown) stopMonitoring();
    return Api::LeaveDecision::Ready;
}

void ClipboardPlugin::shutdown()
{
    stopMonitoring();
    m_store.reset();
    m_host = nullptr;
    QCoreApplication::removeTranslator(&m_translator);
}