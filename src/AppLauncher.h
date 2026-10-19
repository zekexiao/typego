#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QList>
#include <QSet>

class Settings;

struct AppItem {
    QString name;
    QString path;
    QString desc;
    QString lowerName;
    QString lowerDesc;
    QString iconPath;
    QString pinyin;
    QString pinyinAcronym;
};

struct PinItem {
    QString name;
    QString path;
    QString iconPath;
};

// 负责发现、搜索、启动和固定应用程序。
class AppLauncher : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList pinnedApps READ pinnedApps NOTIFY pinsChanged)
public:
    explicit AppLauncher(Settings *settings = nullptr, QObject *parent = nullptr);

    Q_INVOKABLE QVariantList search(const QString &query);
    Q_INVOKABLE bool launch(const QString &path);

    QVariantList pinnedApps() const;
    bool hasPinnedApps() const { return !m_pins.isEmpty(); }
    Q_INVOKABLE bool pinApp(const QString &path, const QString &name, const QString &iconPath);
    Q_INVOKABLE bool unpinApp(const QString &path);
    Q_INVOKABLE bool isPinned(const QString &path) const;

signals:
    void pinsChanged();
    void appsChanged();

private:
    void rebuildApps();
    void scanStartMenu();
    void scanCustomDirs();
    void scanDirectory(const QString &dirPath, const QString &desc, bool recursive, int depth = 0, int maxDepth = 0);
    bool shouldHideEntry(const QString &name, const QString &path) const;
    void addBuiltinApps();
    QString extractIcon(const QString &filePath);
    bool loadPins();
    void savePins() const;
    QString pinFilePath() const;

    QList<AppItem> m_apps;
    QSet<QString> m_appPaths;
    QString m_iconCacheDir;
    QList<PinItem> m_pins;
    Settings *m_settings = nullptr;
};
