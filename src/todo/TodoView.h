#pragma once

#include "Task.h"

#include <QHash>
#include <QList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class TaskService;

class TodoView final : public QWidget {
    Q_OBJECT
public:
    explicit TodoView(TaskService *service, QWidget *parent = nullptr);

    void reload();

signals:
    void closeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Filter {
        Active,
        All,
        Completed,
        HighPriority
    };

    using ChildrenByParent = QHash<QString, QList<Task>>;

    void addTask();
    void addSubTask();
    void removeSelectedTask();
    void toggleSelectedComplete();
    void saveDetails();
    void rebuildTree();
    void appendChildren(QTreeWidgetItem *parentItem, const QString &parentId, const ChildrenByParent &childrenByParent);
    bool applyFilter(QTreeWidgetItem *item);
    bool taskMatchesFilter(const Task &task) const;
    bool taskMatchesSearch(const Task &task) const;
    void updateSummary();
    void updateDetails();
    void setDetailsEnabled(bool enabled);
    QString selectedTaskId() const;
    Task selectedTask() const;
    QString priorityText(int priority) const;

    TaskService *m_service;
    QLineEdit *m_input;
    QLineEdit *m_search;
    QComboBox *m_filter;
    QLabel *m_summary;
    QTreeWidget *m_tree;
    QPushButton *m_addButton;

    QLabel *m_detailTitle;
    QLineEdit *m_titleEdit;
    QTextEdit *m_noteEdit;
    QComboBox *m_priorityCombo;
    QCheckBox *m_completedCheck;
    QPushButton *m_saveButton;
    QPushButton *m_subTaskButton;
    QPushButton *m_deleteButton;
    QPushButton *m_toggleButton;
    QWidget *m_detailPanel;
    QWidget *m_detailEditor;
    QWidget *m_detailActions;
    QLabel *m_emptyDetailLabel;

    bool m_rebuilding = false;
    bool m_loadingDetails = false;
};
