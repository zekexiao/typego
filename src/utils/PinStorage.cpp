#include "PinStorage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <utility>

PinStorage::PinStorage(QString path) : m_path(std::move(path)) {}

bool PinStorage::load()
{
    m_loaded = false;
    m_missing = false;
    m_error.clear();
    QFile file(m_path);
    if (!file.exists()) {
        m_missing = true;
        m_loaded = true;
        m_items.clear();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) { m_error = file.errorString(); return false; }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFile::NoError) { m_error = file.errorString(); return false; }
    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (!doc.isArray()) { m_error = QStringLiteral("Invalid pins.json"); return false; }
    QList<PinItem> items;
    items.reserve(doc.array().size());
    for (const QJsonValue &value : doc.array()) {
        const QJsonObject item = value.toObject();
        if (!value.isObject() || !item.value("path").isString() || item.value("path").toString().isEmpty()
            || !item.value("name").isString()) {
            m_error = QStringLiteral("Invalid pins.json entry");
            return false;
        }
        items.append({item.value("name").toString(), item.value("path").toString(), item.value("iconPath").toString()});
    }
    m_items = std::move(items);
    m_loaded = true;
    return true;
}

bool PinStorage::save(const QList<PinItem> &items)
{
    if (!m_loaded) return false;
    QJsonArray array;
    for (const PinItem &item : items)
        array.append(QJsonObject{{"name", item.name}, {"path", item.path}, {"iconPath", item.iconPath}});
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    const QByteArray bytes = QJsonDocument(array).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        m_error = file.errorString();
        return false;
    }
    m_items = items;
    m_error.clear();
    m_missing = false;
    return true;
}
