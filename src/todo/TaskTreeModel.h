#pragma once

#include <QAbstractItemModel>
#include <QMap>

class TaskRepository;
class TaskService;

/**
 * @brief 任务树模型，用于 QML TreeView
 */
class TaskTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit TaskTreeModel(TaskRepository *repo, TaskService *service, QObject *parent = nullptr);

    enum Roles {
        TitleRole = Qt::UserRole + 1,
        CompletedRole,
        NoteRole,
        PriorityRole,
        TaskIdRole
    };

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addTask(const QString &title, const QModelIndex &parent = QModelIndex());
    Q_INVOKABLE void addSubTask(const QString &parentId, const QString &title);
    Q_INVOKABLE void removeTask(const QString &id);
    Q_INVOKABLE void toggleCompleted(const QString &id);
    Q_INVOKABLE void refresh();

    // QModelIndex-based helpers for QML TreeView delegate
    Q_INVOKABLE void updateTitle(const QString &id, const QString &title);

private:
    struct Node {
        QString taskId;
    };

    QString taskIdAt(const QModelIndex &index) const;
    QModelIndex indexForTask(const QString &taskId) const;

    TaskRepository *m_repo;
    TaskService *m_service;
    mutable QMap<QString, Node> m_nodes;
};
