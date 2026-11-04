#include "TodoStore.h"
#include <algorithm>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

TodoStore::TodoStore(QString filePath) : m_filePath(std::move(filePath)) {}

QString TodoStore::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QList<TodoItem> TodoStore::sorted(const QList<TodoItem> &items)
{
    QList<TodoItem> result = items;
    std::sort(result.begin(), result.end(), [](const TodoItem &a, const TodoItem &b) {
        if (a.done != b.done) return !a.done;
        if (a.priority != b.priority) return a.priority > b.priority;
        // 有时间的排前面并按时间升序；同优先级下无时间的保留在最后。
        if (a.due.isValid() != b.due.isValid()) return a.due.isValid();
        if (a.due.isValid() && a.due != b.due) return a.due < b.due;
        return false;
    });
    return result;
}

bool TodoStore::load(QString *error)
{
    m_items.clear();
    QFile file(m_filePath);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) *error = parseError.errorString();
        return false;
    }
    const QJsonObject root = doc.object();
    const QJsonArray array = root.value(QLatin1String("items")).toArray();
    m_items.reserve(array.size());
    for (const auto &value : array) {
        const QJsonObject object = value.toObject();
        TodoItem item;
        item.id = object.value(QLatin1String("id")).toString();
        item.title = object.value(QLatin1String("title")).toString();
        item.done = object.value(QLatin1String("done")).toBool();
        item.priority = object.value(QLatin1String("priority")).toInt(1);
        if (item.priority < 0 || item.priority > 2) item.priority = 1;
        const QString due = object.value(QLatin1String("due")).toString();
        if (!due.isEmpty()) item.due = QDateTime::fromString(due, Qt::ISODate);
        item.title = item.title.trimmed();
        if (!item.title.isEmpty()) m_items.append(item);
    }
    return true;
}

bool TodoStore::save(QString *error) const
{
    QJsonObject root;
    root.insert(QLatin1String("version"), 1);
    QJsonArray array;
    for (const auto &item : m_items) {
        QJsonObject object;
        object.insert(QLatin1String("id"), item.id);
        object.insert(QLatin1String("title"), item.title);
        object.insert(QLatin1String("done"), item.done);
        object.insert(QLatin1String("priority"), item.priority);
        if (item.due.isValid())
            object.insert(QLatin1String("due"), item.due.toString(Qt::ISODate));
        array.append(object);
    }
    root.insert(QLatin1String("items"), array);

    // 先写临时文件再提交，避免写一半崩溃损坏数据。
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
