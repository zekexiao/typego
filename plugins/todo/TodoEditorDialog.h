#pragma once
#include "TodoStore.h"
#include <QDialog>

class QLineEdit;
class QComboBox;
class QDateTimeEdit;
class QCheckBox;

// 新建 / 编辑共用的表单：标题、优先级和可选截止时间。
class TodoEditorDialog : public QDialog {
    Q_OBJECT
public:
    TodoEditorDialog(QWidget *parent = nullptr);
    void setItem(const TodoItem &item);
    TodoItem item() const;

private:
    QLineEdit *m_title = nullptr;
    QComboBox *m_priority = nullptr;
    QCheckBox *m_hasDue = nullptr;
    QDateTimeEdit *m_due = nullptr;
};
