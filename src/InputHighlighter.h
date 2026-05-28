#pragma once

#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QColor>

class ThemeManager;

class InputHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit InputHighlighter(QObject *parent = nullptr);

    Q_INVOKABLE void setup(QObject *textDocument);
    void setThemeManager(ThemeManager *themeManager);

protected:
    void highlightBlock(const QString &text) override;

private:
    ThemeManager *m_themeManager = nullptr;
};
