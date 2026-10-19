#include "Updater.h"
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

namespace {
// 更新源固定为 GitHub 官方仓库的 latest release。
constexpr auto kLatestReleaseUrl = "https://api.github.com/repos/zekexiao/typego/releases/latest";

// 解析 "v1.2.3" / "1.2" 形式的版本号为可比较的整数段。
QList<int> parseVersion(const QString &text)
{
    QString cleaned = text.trimmed();
    if (cleaned.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) cleaned.remove(0, 1);
    QList<int> parts;
    const auto segments = cleaned.split(QLatin1Char('.'));
    for (const QString &segment : segments) {
        bool ok = false;
        const int value = segment.toInt(&ok);
        parts.append(ok ? value : 0);
    }
    return parts;
}

// 逐段比较语义化版本，remote 更新时返回 true。
bool isNewerVersion(const QString &remote, const QString &local)
{
    const QList<int> a = parseVersion(remote);
    const QList<int> b = parseVersion(local);
    const int count = qMax(a.size(), b.size());
    for (int i = 0; i < count; ++i) {
        const int x = i < a.size() ? a.at(i) : 0;
        const int y = i < b.size() ? b.at(i) : 0;
        if (x != y) return x > y;
    }
    return false;
}
}

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
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
            || !isNewerVersion(tagName, currentVersion())) {
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
    QString downloadPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                               .filePath(QStringLiteral("TypeGo_update.exe"));

    m_downloadFile = new QFile(downloadPath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        emit downloadError(QStringLiteral("Failed to create download file"));
        return;
    }

    QNetworkRequest request(url);
    m_downloadReply = m_manager->get(request);

    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &Updater::downloadProgress);
    connect(m_downloadReply, &QNetworkReply::readyRead,
            this, &Updater::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::finished,
            this, &Updater::onDownloadFinished);
}

void Updater::onDownloadReadyRead()
{
    if (m_downloadFile && m_downloadReply) {
        m_downloadFile->write(m_downloadReply->readAll());
    }
}

void Updater::onDownloadFinished()
{
    if (!m_downloadReply || !m_downloadFile)
        return;

    if (m_downloadReply->error() != QNetworkReply::NoError) {
        emit downloadError(m_downloadReply->errorString());
        m_downloadFile->close();
        m_downloadFile->remove();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        return;
    }

    m_downloadFile->write(m_downloadReply->readAll());
    m_downloadFile->close();

    QString downloadPath = m_downloadFile->fileName();
    m_downloadFile->deleteLater();
    m_downloadFile = nullptr;
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;

    emit downloadFinished();

    // Write updater bat and launch it
    QString exePath = QCoreApplication::applicationFilePath();
    QString batPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                          .filePath(QStringLiteral("TypeGo_updater.bat"));

    QFile bat(batPath);
    if (bat.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString script = QStringLiteral(
            "@echo off\r\n"
            "timeout /t 2 /nobreak >nul\r\n"
            "copy /Y \"%1\" \"%2\" >nul\r\n"
            "if %errorlevel% neq 0 (\r\n"
            "  echo Failed to copy update file\r\n"
            "  pause\r\n"
            "  exit /b 1\r\n"
            ")\r\n"
            "del \"%1\" >nul\r\n"
            "start \"\" \"%2\"\r\n"
            "del \"%~f0\" >nul\r\n"
        ).arg(downloadPath, exePath);
        bat.write(script.toUtf8());
        bat.close();

        QProcess::startDetached(QStringLiteral("cmd"), QStringList{QStringLiteral("/c"), batPath});
        QCoreApplication::quit();
    } else {
        emit downloadError(QStringLiteral("Failed to create updater script"));
    }
}
