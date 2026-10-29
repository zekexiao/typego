#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QLineEdit;
class QRadioButton;

struct SearchOptions {
    QString text;
    bool backward = false;
    bool ignoreCase = false;
    bool wrapAround = true;
    bool useRegex = false;
    bool searchInValue = false;
};

class SearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit SearchDialog(QWidget *parent = nullptr);

    SearchOptions options() const;
    void setOptions(const SearchOptions &options);
    void setDirection(bool backward);
    void focusPattern();

signals:
    void findRequested(const SearchOptions &options);

private slots:
    void onFindNextClicked();

private:
    QLineEdit *m_patternEdit;
    QRadioButton *m_downRadio;
    QRadioButton *m_upRadio;
    QCheckBox *m_ignoreCaseCheck;
    QCheckBox *m_wrapAroundCheck;
    QCheckBox *m_regexCheck;
    QCheckBox *m_searchInValueCheck;
};
