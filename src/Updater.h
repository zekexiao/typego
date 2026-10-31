#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QMetaType>
#include <QString>

#include "utils/ReleaseParser.h"
#include "utils/UpdateCheckGate.h"

// 负责检查、下载和安装应用更新。
class Updater : public QObject
{
    Q_OBJECT
public:
    using Mode = UpdateCheckGate::Mode;

    explicit Updater(QObject *parent = nullptr);
    ~Updater() override;

    // 指定来源的检查：自动检查与手动检查撞车时只发一次请求，不会弹两个提示。
    // Mode::Manual 用于用户点击「检查更新」，结果一定要有反馈。
    void check(Mode mode);
    Q_INVOKABLE void downloadAndInstall(const QString &url);
    void cancelDownload();
    Q_INVOKABLE QString currentVersion() const;
    bool isChecking() const;

signals:
    void updateAvailable(const ReleaseParser::Result &result, UpdateCheckGate::Mode mode);
    void noUpdateAvailable(UpdateCheckGate::Mode mode);
    void checkError(const QString &message, UpdateCheckGate::Mode mode);

    void downloadStarted();
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished();
    void downloadError(const QString &message);

private slots:
    void onDownloadFinished();
    void onDownloadReadyRead();

private:
    void finishDownload(const QString &error, bool cancelled = false);

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_checkReply = nullptr;
    UpdateCheckGate::Gate m_gate;
    QNetworkReply *m_downloadReply = nullptr;
    QFile *m_downloadFile = nullptr;
    QString m_writeError;
    bool m_cancelled = false;
};
