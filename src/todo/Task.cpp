#include "Task.h"
#include <QJsonValue>

QJsonObject Task::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["parentId"] = parentId;
    obj["title"] = title;
    obj["note"] = note;
    obj["completed"] = completed;
    obj["priority"] = priority;
    obj["dueTime"] = dueTime.toString(Qt::ISODate);
    obj["createdTime"] = createdTime.toString(Qt::ISODate);
    obj["updatedTime"] = updatedTime.toString(Qt::ISODate);
    return obj;
}

Task Task::fromJson(const QJsonObject &obj)
{
    Task t;
    t.id = obj["id"].toString();
    t.parentId = obj["parentId"].toString();
    t.title = obj["title"].toString();
    t.note = obj["note"].toString();
    t.completed = obj["completed"].toBool(false);
    t.priority = obj["priority"].toInt(0);
    t.dueTime = QDateTime::fromString(obj["dueTime"].toString(), Qt::ISODate);
    t.createdTime = QDateTime::fromString(obj["createdTime"].toString(), Qt::ISODate);
    t.updatedTime = QDateTime::fromString(obj["updatedTime"].toString(), Qt::ISODate);
    return t;
}
