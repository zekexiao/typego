#pragma once

#include "IPlugin.h"
#include "../AppLauncher.h"

/**
 * @brief 搴旂敤鎼滅储鎻掍欢
 *
 * 鍛戒护: app
 * 绀轰緥: app vscode
 */
class AppPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit AppPlugin(AppLauncher *launcher, QObject *parent = nullptr);

    QString command() const override { return QStringLiteral("app"); }
    QString description() const override { return tr("Search and launch applications"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;

private:
    AppLauncher *m_launcher;
};

