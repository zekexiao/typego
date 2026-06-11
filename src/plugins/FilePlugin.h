#pragma once

#include "IPlugin.h"

/**
 * @brief 鏂囦欢鎼滅储鎻掍欢锛堝熀浜?Voidtools Everything SDK锛? *
 * 鍛戒护: file
 * 绀轰緥: file readme.txt
 *
 * 瑕佹眰锛欵verything 鏈嶅姟姝ｅ湪杩愯锛屼笖 Everything64.dll 宸查儴缃? */
class FilePlugin : public IPlugin {
    Q_OBJECT
public:
    explicit FilePlugin(QObject *parent = nullptr);
    ~FilePlugin() override;

    QString command() const override { return QStringLiteral("file"); }
    QString description() const override { return tr("File search (Everything)"); }
    QVariantList query(const QString &args) override;
    bool execute(const QVariantMap &action) override;

private:
    QString m_iconCacheDir;
    QString extractFileIcon(const QString &filePath);
};

