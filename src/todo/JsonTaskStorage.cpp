#include "JsonTaskStorage.h"
#include "TaskRepository.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QDebug>

JsonTaskStorage::JsonTaskStorage(const QString &filePath)
    : m_filePath(filePath)
{
}

bool JsonTaskStorage::load(TaskRepository *repo) const
{
    QFile file(m_filePath);
    if (!file.exists()) {
        qDebug() << "Task storage not found, starting fresh:" << m_filePath;
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open task storage:" << m_filePath;
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        qWarning() << "Invalid task storage format";
        return false;
    }
    repo->load(doc.array());
    qDebug() << "Loaded" << repo->tasks().size() << "tasks from" << m_filePath;
    return true;
}

bool JsonTaskStorage::save(const TaskRepository *repo) const
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot write task storage:" << m_filePath;
        return false;
    }
    QJsonDocument doc(repo->save());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}
