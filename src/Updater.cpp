#include "Updater.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QStandardPaths>

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

QString Updater::currentVersion() const
{
    return QStringLiteral("1.0.0");
}

void Updater::checkForUpdate()
{
    QString platform = QStringLiteral("windows");
    QString arch = QStringLiteral("amd64");

    QString apiUrl = QStringLiteral("http://127.0.0.1:8080/api/v1/update/latest?platform=%1&arch=%2")
                         .arg(platform, arch);

    QNetworkRequest request(apiUrl);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit checkError(reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            emit checkError(QStringLiteral("Invalid response"));
            return;
        }

        QJsonObject obj = doc.object();
        if (!obj.value(QStringLiteral("has_update")).toBool()) {
            emit noUpdateAvailable();
            return;
        }

        QString version = obj.value(QStringLiteral("version")).toString();
        QString changelog = obj.value(QStringLiteral("changelog")).toString();
        QString url = obj.value(QStringLiteral("download_url")).toString();
        bool mandatory = obj.value(QStringLiteral("mandatory")).toBool(false);

        m_updateUrl = url;
        emit updateAvailable(version, changelog, url, mandatory);
    });
}

void Updater::downloadAndInstall(const QString &url)
{
    QString downloadPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                               .filePath(QStringLiteral("qingzhou_update.exe"));

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
                          .filePath(QStringLiteral("qingzhou_updater.bat"));

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
