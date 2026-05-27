#pragma once

#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QColor>

class InputHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit InputHighlighter(QObject *parent = nullptr);

    Q_INVOKABLE void setup(QObject *textDocument);

protected:
    void highlightBlock(const QString &text) override;
};
