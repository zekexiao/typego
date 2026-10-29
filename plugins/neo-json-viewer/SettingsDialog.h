#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(int maxTextLength, bool prettyPrint, QWidget *parent = nullptr);

    int maxTextLength() const;
    bool prettyPrint() const;

private:
    QSpinBox *m_maxTextSpin;
    QCheckBox *m_prettyCheck;
};
