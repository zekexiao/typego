#pragma once
#include <QWidget>
#include "TodoStore.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QDateTimeEdit;
class QCheckBox;
class QDialogButtonBox;
class QPushButton;
class QTreeWidget;

// 待办主窗口：增删改查都在本地即时保存，无未保存状态。
class TodoWindow : public QWidget {
    Q_OBJECT
public:
    explicit TodoWindow(const QString &storageDirectory, QWidget *parent = nullptr);

private slots:
    void addItem();
    void editItem();
    void removeItem();
    void clearDone();

private:
    void reload();
    void persist();
    void updateSummary();
    QString priorityText(int priority) const;
    QString dueText(const TodoItem &item, bool *overdue) const;

    TodoStore m_store;
    QTreeWidget *m_tree = nullptr;
    QComboBox *m_filter = nullptr;
    QLabel *m_summary = nullptr;
    int m_filterIndex = 0; // 0 全部 / 1 未完成 / 2 已完成
    bool m_updating = false; // 程序刷新条目时抑制 itemChanged 落盘
};
