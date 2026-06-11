#pragma once

#include "IPlugin.h"

/**
 * @brief 鐩綍璺宠浆鎻掍欢
 *
 * 鍛戒护: cd
 * 绀轰緥: cd .., cd D:\\, cd Documents
 */
class DirectoryPlugin : public IPlugin {
    Q_OBJECT
public:
    explicit DirectoryPlugin(QObject *parent = nullptr);

    QString command() const override { return QStringLiteral("cd"); }
    QString description() const override { return tr("Directory browser and navigation"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;

private:
    QString resolvePath(const QString &args) const;
};

