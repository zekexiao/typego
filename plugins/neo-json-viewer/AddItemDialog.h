#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QStackedWidget>
#include "JsonTreeItem.h"

class AddItemDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddItemDialog(bool isArrayParent, QWidget *parent = nullptr);

    QString key() const;
    JsonNodeType nodeType() const;
    QVariant value() const;

private slots:
    void onTypeChanged(int index);

private:
    QComboBox *m_typeCombo;
    QLineEdit *m_keyEdit;
    QLineEdit *m_valueEdit;
    QCheckBox *m_boolCheck;
    QStackedWidget *m_valueStack;
};
