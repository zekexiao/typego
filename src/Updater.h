#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QString>

// 负责检查、下载和安装应用更新。
class Updater : public QObject
{
    Q_OBJECT
public:
    explicit Updater(QObject *parent = nullptr);

    Q_INVOKABLE void checkForUpdate();
    Q_INVOKABLE void downloadAndInstall(const QString &url);
    Q_INVOKABLE QString currentVersion() const;

signals:
    void updateAvailable(const QString &version, const QString &changelog, const QString &url, bool mandatory);
    void noUpdateAvailable();
    void checkError(const QString &message);

    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished();
    void downloadError(const QString &message);

private slots:
    void onDownloadFinished();
    void onDownloadReadyRead();

private:
    bool installUpdate();

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_downloadReply = nullptr;
    QFile *m_downloadFile = nullptr;
    QString m_updateUrl;
};
