#pragma once

#include <QList>
#include <QString>

struct PinItem {
    QString name;
    QString path;
    QString iconPath;
};

// 固定项写盘成功才发布到界面，读取异常时禁止覆盖原文件。
class PinStorage {
public:
    explicit PinStorage(QString path);
    bool load();
    bool save(const QList<PinItem> &items);
    const QList<PinItem> &items() const { return m_items; }
    QString error() const { return m_error; }
    bool missing() const { return m_missing; }
private:
    QString m_path;
    QString m_error;
    QList<PinItem> m_items;
    bool m_loaded = false;
    bool m_missing = false;
};
