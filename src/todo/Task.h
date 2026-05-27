#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>

/**
 * @brief 任务数据结构
 */
struct Task {
    QString id;
    QString parentId;   // 空字符串表示顶层任务
    QString title;
    QString note;
    bool completed = false;
    int priority = 0;   // 0=普通, 1=高, 2=紧急
    QDateTime dueTime;
    QDateTime createdTime;
    QDateTime updatedTime;

    QJsonObject toJson() const;
    static Task fromJson(const QJsonObject &obj);
};
