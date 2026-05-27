#pragma once

#include <QString>
#include <QJsonArray>

class TaskRepository;

/**
 * @brief JSON 文件持久化
 */
class JsonTaskStorage {
public:
    explicit JsonTaskStorage(const QString &filePath);

    bool load(TaskRepository *repo) const;
    bool save(const TaskRepository *repo) const;

    QString filePath() const { return m_filePath; }

private:
    QString m_filePath;
};
