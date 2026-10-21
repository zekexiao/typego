#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QList>
#include <QSet>
#include <QFutureWatcher>

#include "utils/AppMatcher.h"

class Settings;

struct AppItem {
    QString name;
    QString path;
    QString desc;
    QString iconPath;
    AppMatcher::Entry match;   // 扫描期构造好的匹配键，搜索时只读
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
    bool isScanning() const { return !m_scanReady || m_scanning; }

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
    static QList<AppItem> scanApps(const QStringList &customDirs, bool recursive, bool hideUninstall);
    static void scanDirectory(const QString &dirPath, const QString &desc, bool recursive, int depth, int maxDepth,
                              bool hideUninstall, QList<AppItem> &apps, QSet<QString> &paths);
    static bool shouldHideEntry(const QString &name, const QString &path);
    void addBuiltinApps();
    QString extractIcon(const QString &filePath);
    bool loadPins();
    void savePins() const;
    QString pinFilePath() const;

    QList<AppItem> m_apps;
    QFutureWatcher<QList<AppItem>> m_scanWatcher;
    bool m_scanning = false;
    bool m_scanPending = false;
    bool m_scanReady = false;
    QString m_iconCacheDir;
    QList<PinItem> m_pins;
    Settings *m_settings = nullptr;
};
