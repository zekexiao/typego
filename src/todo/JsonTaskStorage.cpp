#include "JsonTaskStorage.h"
#include "TaskRepository.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QDebug>
#include <QSaveFile>

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

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (!doc.isArray()) {
        qWarning() << "Invalid task storage format:" << error.errorString();
        return false;
    }
    repo->load(doc.array());
    qDebug() << "Loaded" << repo->tasks().size() << "tasks from" << m_filePath;
    return true;
}

bool JsonTaskStorage::save(const TaskRepository *repo) const
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot write task storage:" << m_filePath;
        return false;
    }
    QJsonDocument doc(repo->save());
    if (file.write(doc.toJson(QJsonDocument::Compact)) < 0) return false;
    return file.commit();
}
