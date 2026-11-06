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

QString TodoStore::makeDue(const QDate &date, const QTime &time)
{
    if (!date.isValid()) return QString();
    // 纯日期与带时间两种粒度显式分开，展示层据此决定是否显示时刻。
    return time.isValid()
        ? QDateTime(date, time).toString(QStringLiteral("yyyy-MM-ddTHH:mm"))
        : date.toString(QStringLiteral("yyyy-MM-dd"));
}

bool TodoStore::dueHasTime(const QString &due)
{
    return due.size() > 10;
}

QDateTime TodoStore::dueDateTime(const QString &due)
{
    if (due.isEmpty()) return QDateTime();
    const QDate date = QDate::fromString(due.left(10), QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid()) return QDateTime();
    if (!dueHasTime(due)) return QDateTime(date, QTime(0, 0));
    const QTime time = QTime::fromString(due.mid(11), QStringLiteral("HH:mm"));
    if (!time.isValid()) return QDateTime();
    // 本地墙钟：排序与逾期判定都在本时区内进行。
    return QDateTime(date, time);
}

QList<TodoItem> TodoStore::sorted(const QList<TodoItem> &items)
{
    QList<TodoItem> result;
    result.reserve(items.size());
    for (const auto &item : items)
        if (item.deletedAt <= 0) result.append(item);
    std::sort(result.begin(), result.end(), [](const TodoItem &a, const TodoItem &b) {
        if (a.done != b.done) return !a.done;
        if (a.done) {
            // 已完成组：最近完成的排最上，方便反悔勾回去。
            if (a.completedAt != b.completedAt) return a.completedAt > b.completedAt;
        } else {
            if (a.priority != b.priority) return a.priority > b.priority;
            const QDateTime dueA = dueDateTime(a.due);
            const QDateTime dueB = dueDateTime(b.due);
            const bool hasA = dueA.isValid();
            const bool hasB = dueB.isValid();
            // 有截止的按时间升序且排在无截止之前；纯日期即当天 0 点，天然排在同日带时间之前。
            if (hasA != hasB) return hasA;
            if (hasA && dueA != dueB) return dueA < dueB;
        }
        if (a.createdAt != b.createdAt) return a.createdAt < b.createdAt;
        return a.id < b.id; // 同毫秒创建时用 id 定序，保证多次加载结果一致
    });
    return result;
}

bool TodoStore::load(QString *error)
{
    m_items.clear();
    m_unreadable = false;
    QFile file(m_filePath);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        // 保留原文件内容，m_unreadable 让首次保存先备份，避免覆盖唯一的数据副本。
        m_unreadable = true;
        if (error) *error = parseError.errorString();
        return false;
    }
    const QJsonArray array = doc.object().value(QLatin1String("items")).toArray();
    m_items.reserve(array.size());
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const auto &value : array) {
        const QJsonObject object = value.toObject();
        TodoItem item;
        item.id = object.value(QLatin1String("id")).toString();
        if (item.id.isEmpty()) item.id = newId(); // 手工编辑缺 id 时补齐，维持锚点不变式
        item.title = object.value(QLatin1String("title")).toString().trimmed();
        if (item.title.isEmpty()) continue;
        if (item.title.size() > MaxTitleLength) item.title.truncate(MaxTitleLength);
        item.done = object.value(QLatin1String("done")).toBool();
        item.completedAt = object.value(QLatin1String("completedAt")).toInteger();
        item.priority = object.value(QLatin1String("priority")).toInt(1);
        if (item.priority < 0 || item.priority > 2) item.priority = 1;
        // 截止时间重新归一化，无法解析或缺失的存空串。
        const QString dueRaw = object.value(QLatin1String("due")).toString();
        const QDateTime dueParsed = dueDateTime(dueRaw);
        item.due = dueParsed.isValid()
            ? makeDue(dueParsed.date(), dueHasTime(dueRaw) ? dueParsed.time() : QTime())
            : QString();
        item.createdAt = object.value(QLatin1String("createdAt")).toInteger();
        item.updatedAt = object.value(QLatin1String("updatedAt")).toInteger();
        item.deletedAt = object.value(QLatin1String("deletedAt")).toInteger();
        // 过期墓碑对同步已无意义，加载时物理清除。
        if (item.deletedAt > 0 && now - item.deletedAt > TombstoneRetentionMs) continue;
        m_items.append(item);
    }
    return true;
}

bool TodoStore::save(QString *error)
{
    QJsonObject root;
    root.insert(QLatin1String("version"), 1);
    QJsonArray array;
    for (const auto &item : m_items) {
        QJsonObject object;
        object.insert(QLatin1String("id"), item.id);
        object.insert(QLatin1String("title"), item.title);
        object.insert(QLatin1String("done"), item.done);
        object.insert(QLatin1String("completedAt"), item.completedAt);
        object.insert(QLatin1String("priority"), item.priority);
        object.insert(QLatin1String("due"), item.due);
        object.insert(QLatin1String("createdAt"), item.createdAt);
        object.insert(QLatin1String("updatedAt"), item.updatedAt);
        if (item.deletedAt > 0) object.insert(QLatin1String("deletedAt"), item.deletedAt);
        array.append(object);
    }
    root.insert(QLatin1String("items"), array);

    // 原文件解析失败时先把坏文件备份成 .bak，再落新内容，给用户留一条恢复路径。
    if (m_unreadable && QFile::exists(m_filePath))
        QFile::copy(m_filePath, m_filePath + QStringLiteral(".bak"));
    // 先写临时文件再原子提交，避免写一半崩溃损坏数据。
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
    m_unreadable = false;
    return true;
}
