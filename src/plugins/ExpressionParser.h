#pragma once

#include <QList>
#include <QString>

// 递归下降表达式解析器：支持 +-*/%^、括号、科学计数法、pi/e 常量与常见函数。
// 纯逻辑模块，放在 typego_core 供 CalcPlugin 与单元测试共用。
class ExpressionParser {
public:
    explicit ExpressionParser(const QString &text);

    // 解析成功时写入 result 并返回 true；失败时写入 error（含位置）并返回 false。
    bool parse(double *result, QString *error);

private:
    double parseExpression();
    double parseTerm();
    double parseUnary();
    double parsePower();
    double parsePrimary();
    double parseNumber();
    QString parseIdentifier();
    double callFunction(const QString &name, const QList<double> &args);
    bool consume(QChar token);
    bool peek(QChar token);
    void skipSpaces();
    void fail(const QString &message);

    const QString &m_text;
    int m_pos = 0;
    QString m_error;
};
