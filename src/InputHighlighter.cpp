#include "InputHighlighter.h"
#include <QQuickTextDocument>
#include <QTextDocument>
#include <QFont>

InputHighlighter::InputHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
}

void InputHighlighter::setup(QObject *textDocument)
{
    QQuickTextDocument *quickDoc = qobject_cast<QQuickTextDocument *>(textDocument);
    if (quickDoc && quickDoc->textDocument()) {
        setDocument(quickDoc->textDocument());
    }
}

void InputHighlighter::highlightBlock(const QString &text)
{
    int spaceIdx = text.indexOf(' ');
    if (spaceIdx > 0) {
        QTextCharFormat cmdFormat;
        cmdFormat.setForeground(QColor("#89b4fa"));
        cmdFormat.setFontWeight(QFont::Bold);
        setFormat(0, spaceIdx, cmdFormat);

        // 参数部分保持默认颜色（cdd6f4）
        QTextCharFormat argsFormat;
        argsFormat.setForeground(QColor("#cdd6f4"));
        setFormat(spaceIdx, text.length() - spaceIdx, argsFormat);
    } else {
        // 无空格时全部默认颜色
        QTextCharFormat defaultFormat;
        defaultFormat.setForeground(QColor("#cdd6f4"));
        setFormat(0, text.length(), defaultFormat);
    }
}
