#include "Updater.h"
#include "utils/ReleaseParser.h"
#include <QNetworkRequest>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace {
// 更新源用 releases.atom 订阅源而不是 api.github.com：匿名 API 每 IP 每小时 60 次限流，
// 共享出口 IP 的用户会 403；atom 是普通页面接口，不限流，且同样跟随系统代理。
constexpr auto kReleasesFeedUrl = "https://github.com/zekexiao/typego/releases.atom";

#ifdef Q_OS_WIN
bool startInstallerAfterExit(const QString &installerPath, QString *error)
{
    QTemporaryFile helper(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                              .filePath(QStringLiteral("TypeGo_handoff_XXXXXX.ps1")));
    if (!helper.open()) {
        *error = helper.errorString();
        return false;
    }
    helper.setAutoRemove(false);
    const QByteArray script = QByteArrayLiteral(
        "$ErrorActionPreference = 'Stop'\r\n"
        "$parentPid = [int]$args[0]\r\n"
        "$installerPath = $args[1]\r\n"
        "$installDir = $args[2]\r\n"
        "$appPath = $args[3]\r\n"
        "try { Wait-Process -Id $parentPid -ErrorAction SilentlyContinue } catch {}\r\n"
        "try {\r\n"
        "  $setup = Start-Process -FilePath $installerPath -ArgumentList ('/DIR=\"' + $installDir + '\"') -Verb RunAs -Wait -PassThru\r\n"
        "  if ($setup.ExitCode -ne 0) { Start-Process -FilePath $appPath }\r\n"
        "} catch { Start-Process -FilePath $appPath }\r\n"
        "Remove-Item -LiteralPath $installerPath -Force -ErrorAction SilentlyContinue\r\n"
        "Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue\r\n");
    if (helper.write(script) != script.size() || !helper.flush()) {
        *error = helper.errorString();
        const QString helperPath = helper.fileName();
        helper.close();
        QFile::remove(helperPath);
        return false;
    }
    const QString helperPath = helper.fileName();
    helper.close();

    const QString powershell = QStandardPaths::findExecutable(QStringLiteral("powershell.exe"));
    if (powershell.isEmpty()) {
        *error = QStringLiteral("powershell.exe was not found");
        QFile::remove(helperPath);
        return false;
    }

    const QStringList arguments{
        QStringLiteral("-NoProfile"), QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-File"), helperPath,
        QString::number(QCoreApplication::applicationPid()), installerPath,
        QDir::toNativeSeparators(QCoreApplication::applicationDirPath()),
        QCoreApplication::applicationFilePath()};
    if (!QProcess::startDetached(powershell, arguments)) {
        *error = QStringLiteral("Cannot start the update handoff process");
        QFile::remove(helperPath);
        return false;
    }
    return true;
}
#endif
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
    QNetworkRequest request{QUrl(QString::fromLatin1(kReleasesFeedUrl))};
    request.setRawHeader("User-Agent", "TypeGo-Updater");
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

        const ReleaseParser::Result release = ReleaseParser::parseAtomFeed(reply->readAll(), currentVersion());
        switch (release.outcome) {
        case ReleaseParser::Outcome::UpToDate:
            emit noUpdateAvailable();
            return;
        case ReleaseParser::Outcome::InvalidResponse:
            emit checkError(release.message);
            return;
        case ReleaseParser::Outcome::UpdateReady:
            emit updateAvailable(release.version, release.changelog, release.installerUrl, false);
            return;
        }
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

    // 先启动等待进程，待当前进程完全退出后再运行安装包，避免安装器看到旧互斥量。
#ifdef Q_OS_WIN
    QString error;
    if (!startInstallerAfterExit(downloadPath, &error)) {
        qWarning() << "Cannot prepare the update installer:" << error;
        finishDownload(tr("Cannot start the update installer."));
        return;
    }
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
