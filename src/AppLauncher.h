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
    QString pinyin;
    QString pinyinAcronym;
};

struct PinItem {
    QString name;
    QString path;
    QString iconPath;
};

class AppLauncher : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList pinnedApps READ pinnedApps NOTIFY pinsChanged)
public:
    explicit AppLauncher(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList search(const QString &query);
    Q_INVOKABLE bool launch(const QString &path);

    QVariantList pinnedApps() const;
    Q_INVOKABLE bool pinApp(const QString &path, const QString &name, const QString &iconPath);
    Q_INVOKABLE bool unpinApp(const QString &path);
    Q_INVOKABLE bool isPinned(const QString &path) const;

signals:
    void pinsChanged();

private:
    void scanStartMenu();
    void addBuiltinApps();
    QString extractIcon(const QString &filePath);
    void loadPins();
    void savePins() const;
    QString pinFilePath() const;

    QList<AppItem> m_apps;
    QString m_iconCacheDir;
    QList<PinItem> m_pins;
};
