#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QList>

struct AppItem {
    QString name;
    QString path;
    QString desc;
    QString iconPath;
};

class AppLauncher : public QObject {
    Q_OBJECT
public:
    explicit AppLauncher(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList search(const QString &query);
    Q_INVOKABLE bool launch(const QString &path);

private:
    void scanStartMenu();
    void addBuiltinApps();
    QString extractIcon(const QString &filePath);

    QList<AppItem> m_apps;
    QString m_iconCacheDir;
};
