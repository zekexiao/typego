#include "Updater.h"
#include "utils/Version.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
// 更新源固定为 GitHub 官方仓库的 latest release。
constexpr auto kLatestReleaseUrl = "https://api.github.com/repos/zekexiao/typego/releases/latest";
}

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

Updater::~Updater()
{
    if (m_downloadReply) {
        m_downloadReply->disconnect(this);
        m_downloadReply->abort();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->remove();
    }
}

QString Updater::currentVersion() const
{
    // 统一使用应用版本（由 VERSION 文件注入），不要再硬编码，否则与发布版本串不等会反复提示更新。
    return QCoreApplication::applicationVersion();
}

void Updater::checkForUpdate()
{
    QNetworkRequest request{QUrl(QString::fromLatin1(kLatestReleaseUrl))};
    // GitHub API 要求 User-Agent，否则直接 403。
    request.setRawHeader("User-Agent", "TypeGo-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit checkError(reply->errorString());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit checkError(QStringLiteral("Invalid GitHub release response"));
            return;
        }

        const QJsonObject obj = doc.object();
        const QString tagName = obj.value(QStringLiteral("tag_name")).toString();
        // 草稿/预发布版本不纳入自动更新提示。
        if (tagName.isEmpty() || obj.value(QStringLiteral("draft")).toBool()
            || obj.value(QStringLiteral("prerelease")).toBool()
            || !Version::isNewer(tagName, currentVersion())) {
            emit noUpdateAvailable();
            return;
        }

        const QString version = tagName.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)
                                    ? tagName.mid(1) : tagName;
        const QString changelog = obj.value(QStringLiteral("body")).toString();

        // 在 release assets 中挑选 Windows 安装包（命名约定 TypeGo_x.y_x64.exe）。
        QString url;
        const auto assets = obj.value(QStringLiteral("assets")).toArray();
        for (const auto &assetValue : assets) {
            const QJsonObject asset = assetValue.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
                url = asset.value(QStringLiteral("browser_download_url")).toString();
                if (name.contains(QStringLiteral("x64"), Qt::CaseInsensitive)) break;
            }
        }
        if (url.isEmpty()) {
            emit checkError(QStringLiteral("No Windows installer found in release assets"));
            return;
        }

        m_updateUrl = url;
        emit updateAvailable(version, changelog, url, false);
    });
}

void Updater::downloadAndInstall(const QString &url)
{
    if (m_downloadReply) return;
    const QUrl downloadUrl(url);
    if (!downloadUrl.isValid() || downloadUrl.scheme() != QStringLiteral("https")) {
        emit downloadError(tr("Invalid update download URL"));
        return;
    }
    auto *file = new QTemporaryFile(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                       .filePath(QStringLiteral("TypeGo_update_XXXXXX.exe")), this);
    if (!file->open()) {
        const QString error = file->errorString();
        delete file;
        emit downloadError(tr("Cannot create the update download: %1").arg(error));
        return;
    }
    file->setAutoRemove(false);
    m_downloadFile = file;
    m_writeError.clear();
    m_cancelled = false;
    QNetworkRequest request(downloadUrl);
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_downloadReply = m_manager->get(request);

    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &Updater::downloadProgress);
    connect(m_downloadReply, &QNetworkReply::readyRead,
            this, &Updater::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::finished,
            this, &Updater::onDownloadFinished);
    emit downloadStarted();
}

void Updater::cancelDownload()
{
    if (!m_downloadReply) return;
    m_cancelled = true;
    m_downloadReply->abort();
}

void Updater::onDownloadReadyRead()
{
    if (m_downloadFile && m_downloadReply) {
        const QByteArray data = m_downloadReply->readAll();
        if (m_downloadFile->write(data) != data.size()) {
            m_writeError = tr("Cannot save the update: %1").arg(m_downloadFile->errorString());
            m_downloadReply->abort();
        }
    }
}

void Updater::onDownloadFinished()
{
    if (!m_downloadReply || !m_downloadFile)
        return;

    if (m_cancelled || !m_writeError.isEmpty() || m_downloadReply->error() != QNetworkReply::NoError) {
        finishDownload(m_writeError.isEmpty() ? m_downloadReply->errorString() : m_writeError, m_cancelled);
        return;
    }
    const QByteArray tail = m_downloadReply->readAll();
    if (m_downloadFile->write(tail) != tail.size() || !m_downloadFile->flush()
        || !m_downloadFile->seek(0) || m_downloadFile->read(2) != QByteArrayLiteral("MZ")) {
        finishDownload(tr("The update download is incomplete or invalid. Please try again."));
        return;
    }
    const QString downloadPath = QDir::toNativeSeparators(m_downloadFile->fileName());
    m_downloadFile->close();

    // 安装包负责提权、替换文件和重启，只有成功交接后才退出当前程序。
#ifdef Q_OS_WIN
    const QString parameters = QStringLiteral(R"(/DIR="%1")")
                                   .arg(QDir::toNativeSeparators(QCoreApplication::applicationDirPath()));
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.lpVerb = L"open";
    info.lpFile = reinterpret_cast<LPCWSTR>(downloadPath.utf16());
    info.lpParameters = reinterpret_cast<LPCWSTR>(parameters.utf16());
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        const DWORD error = GetLastError();
        finishDownload(error == ERROR_CANCELLED ? QString()
                       : tr("Cannot start the update installer (error %1).").arg(error), error == ERROR_CANCELLED);
        return;
    }
    if (info.hProcess) CloseHandle(info.hProcess);
#else
    if (!QProcess::startDetached(downloadPath, QStringList())) {
        finishDownload(tr("Cannot start the update installer."));
        return;
    }
#endif
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
    delete m_downloadFile;
    m_downloadFile = nullptr;
    emit downloadFinished();
    QCoreApplication::quit();
}

void Updater::finishDownload(const QString &error, bool cancelled)
{
    m_downloadFile->close();
    m_downloadFile->remove();
    delete m_downloadFile;
    m_downloadFile = nullptr;
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
    if (cancelled) emit downloadFinished();
    else emit downloadError(error);
}
