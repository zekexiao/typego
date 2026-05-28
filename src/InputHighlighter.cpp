#include "InputHighlighter.h"
#include "ThemeManager.h"
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

void InputHighlighter::setThemeManager(ThemeManager *themeManager)
{
    m_themeManager = themeManager;
}

void InputHighlighter::highlightBlock(const QString &text)
{
    QColor accent = m_themeManager ? m_themeManager->accent() : QColor(QStringLiteral("#89b4fa"));
    QColor txt = m_themeManager ? m_themeManager->text() : QColor(QStringLiteral("#cdd6f4"));

    int spaceIdx = text.indexOf(' ');
    if (spaceIdx > 0) {
        QTextCharFormat cmdFormat;
        cmdFormat.setForeground(accent);
        cmdFormat.setFontWeight(QFont::Bold);
        setFormat(0, spaceIdx, cmdFormat);

        QTextCharFormat argsFormat;
        argsFormat.setForeground(txt);
        setFormat(spaceIdx, text.length() - spaceIdx, argsFormat);
    } else {
        QTextCharFormat defaultFormat;
        defaultFormat.setForeground(txt);
        setFormat(0, text.length(), defaultFormat);
    }
}
