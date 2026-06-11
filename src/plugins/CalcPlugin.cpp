#include "CalcPlugin.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QVariantMap>

#include <cmath>
#include <limits>

namespace {
class ExpressionParser {
public:
    explicit ExpressionParser(const QString &text)
        : m_text(text)
    {
    }

    bool parse(double *result, QString *error)
    {
        m_error.clear();
        m_pos = 0;
        const double value = parseExpression();
        skipSpaces();
        if (m_error.isEmpty() && m_pos != m_text.size()) fail(QStringLiteral("Unexpected token"));
        if (m_error.isEmpty() && !std::isfinite(value)) fail(QStringLiteral("Result is not finite"));
        if (!m_error.isEmpty()) {
            *error = QStringLiteral("%1 at position %2").arg(m_error).arg(m_pos + 1);
            return false;
        }
        *result = value;
        return true;
    }

private:
    double parseExpression()
    {
        double value = parseTerm();
        while (m_error.isEmpty()) {
            if (consume(QLatin1Char('+'))) value += parseTerm();
            else if (consume(QLatin1Char('-'))) value -= parseTerm();
            else break;
        }
        return value;
    }

    double parseTerm()
    {
        double value = parseUnary();
        while (m_error.isEmpty()) {
            if (consume(QLatin1Char('*'))) {
                value *= parseUnary();
            } else if (consume(QLatin1Char('/'))) {
                const double divisor = parseUnary();
                if (divisor == 0.0) fail(QStringLiteral("Division by zero"));
                else value /= divisor;
            } else if (consume(QLatin1Char('%'))) {
                const double divisor = parseUnary();
                if (divisor == 0.0) fail(QStringLiteral("Division by zero"));
                else value = std::fmod(value, divisor);
            } else {
                break;
            }
        }
        return value;
    }

    double parseUnary()
    {
        if (consume(QLatin1Char('+'))) return parseUnary();
        if (consume(QLatin1Char('-'))) return -parseUnary();
        return parsePower();
    }

    double parsePower()
    {
        double value = parsePrimary();
        if (consume(QLatin1Char('^'))) value = std::pow(value, parseUnary());
        return value;
    }

    double parsePrimary()
    {
        skipSpaces();
        if (consume(QLatin1Char('('))) {
            const double value = parseExpression();
            if (!consume(QLatin1Char(')'))) fail(QStringLiteral("Expected ')'"));
            return value;
        }

        if (m_pos < m_text.size() && (m_text[m_pos].isDigit() || m_text[m_pos] == QLatin1Char('.'))) {
            return parseNumber();
        }

        if (m_pos < m_text.size() && (m_text[m_pos].isLetter() || m_text[m_pos] == QLatin1Char('_'))) {
            const QString name = parseIdentifier().toLower();
            if (name == QStringLiteral("pi")) return 3.14159265358979323846;
            if (name == QStringLiteral("e")) return 2.71828182845904523536;
            if (!consume(QLatin1Char('('))) {
                fail(QStringLiteral("Expected '(' after function name"));
                return 0.0;
            }

            QList<double> args;
            if (!peek(QLatin1Char(')'))) {
                args.append(parseExpression());
                while (consume(QLatin1Char(','))) args.append(parseExpression());
            }
            if (!consume(QLatin1Char(')'))) fail(QStringLiteral("Expected ')'"));
            return callFunction(name, args);
        }

        fail(QStringLiteral("Expected number, constant, or function"));
        return 0.0;
    }

    double parseNumber()
    {
        skipSpaces();
        const int start = m_pos;
        bool hasDigits = false;
        while (m_pos < m_text.size() && m_text[m_pos].isDigit()) {
            ++m_pos;
            hasDigits = true;
        }
        if (m_pos < m_text.size() && m_text[m_pos] == QLatin1Char('.')) {
            ++m_pos;
            while (m_pos < m_text.size() && m_text[m_pos].isDigit()) {
                ++m_pos;
                hasDigits = true;
            }
        }
        if (!hasDigits) {
            fail(QStringLiteral("Invalid number"));
            return 0.0;
        }
        if (m_pos < m_text.size() && (m_text[m_pos] == QLatin1Char('e') || m_text[m_pos] == QLatin1Char('E'))) {
            ++m_pos;
            if (m_pos < m_text.size() && (m_text[m_pos] == QLatin1Char('+') || m_text[m_pos] == QLatin1Char('-'))) ++m_pos;
            const int exponentStart = m_pos;
            while (m_pos < m_text.size() && m_text[m_pos].isDigit()) ++m_pos;
            if (m_pos == exponentStart) fail(QStringLiteral("Invalid exponent"));
        }

        bool ok = false;
        const double value = m_text.mid(start, m_pos - start).toDouble(&ok);
        if (!ok) fail(QStringLiteral("Invalid number"));
        return value;
    }

    QString parseIdentifier()
    {
        skipSpaces();
        const int start = m_pos;
        while (m_pos < m_text.size() && (m_text[m_pos].isLetterOrNumber() || m_text[m_pos] == QLatin1Char('_'))) ++m_pos;
        return m_text.mid(start, m_pos - start);
    }

    double callFunction(const QString &name, const QList<double> &args)
    {
        auto unary = [&](auto fn) {
            if (args.size() != 1) {
                fail(QStringLiteral("Function '%1' expects one argument").arg(name));
                return 0.0;
            }
            return fn(args[0]);
        };
        auto binary = [&](auto fn) {
            if (args.size() != 2) {
                fail(QStringLiteral("Function '%1' expects two arguments").arg(name));
                return 0.0;
            }
            return fn(args[0], args[1]);
        };

        if (name == QStringLiteral("sqrt")) return unary([](double x) { return std::sqrt(x); });
        if (name == QStringLiteral("sin")) return unary([](double x) { return std::sin(x); });
        if (name == QStringLiteral("cos")) return unary([](double x) { return std::cos(x); });
        if (name == QStringLiteral("tan")) return unary([](double x) { return std::tan(x); });
        if (name == QStringLiteral("abs")) return unary([](double x) { return std::fabs(x); });
        if (name == QStringLiteral("floor")) return unary([](double x) { return std::floor(x); });
        if (name == QStringLiteral("ceil")) return unary([](double x) { return std::ceil(x); });
        if (name == QStringLiteral("exp")) return unary([](double x) { return std::exp(x); });
        if (name == QStringLiteral("ln")) return unary([](double x) { return std::log(x); });
        if (name == QStringLiteral("log10")) return unary([](double x) { return std::log10(x); });
        if (name == QStringLiteral("log")) {
            if (args.size() == 1) return std::log(args[0]);
            return binary([](double value, double base) { return std::log(value) / std::log(base); });
        }
        if (name == QStringLiteral("min")) return binary([](double a, double b) { return std::fmin(a, b); });
        if (name == QStringLiteral("max")) return binary([](double a, double b) { return std::fmax(a, b); });
        if (name == QStringLiteral("pow")) return binary([](double a, double b) { return std::pow(a, b); });

        fail(QStringLiteral("Unknown function '%1'").arg(name));
        return 0.0;
    }

    bool consume(QChar token)
    {
        skipSpaces();
        if (m_pos < m_text.size() && m_text[m_pos] == token) {
            ++m_pos;
            return true;
        }
        return false;
    }

    bool peek(QChar token)
    {
        skipSpaces();
        return m_pos < m_text.size() && m_text[m_pos] == token;
    }

    void skipSpaces()
    {
        while (m_pos < m_text.size() && m_text[m_pos].isSpace()) ++m_pos;
    }

    void fail(const QString &message)
    {
        if (m_error.isEmpty()) m_error = message;
    }

    const QString &m_text;
    int m_pos = 0;
    QString m_error;
};
}

CalcPlugin::CalcPlugin(QObject *parent)
    : IPlugin(parent)
{
}

QVariantList CalcPlugin::query(const QString &args)
{
    QVariantMap item;
    QVariantMap action;
    action["plugin"] = command();

    if (args.trimmed().isEmpty()) {
        item["title"] = tr("Enter an expression to calculate");
        item["subtitle"] = tr("e.g. 1+2, sqrt(16), 2^10");
        item["icon"] = "";
        action["type"] = QStringLiteral("none");
        item["action"] = action;
        return QVariantList() << item;
    }

    double value = 0.0;
    QString error;
    ExpressionParser parser(args);
    if (!parser.parse(&value, &error)) {
        item["title"] = tr("Expression Error");
        item["subtitle"] = error;
        item["icon"] = "";
        action["type"] = QStringLiteral("none");
        item["action"] = action;
        return QVariantList() << item;
    }

    const QString result = QString::number(value, 'g', 15);
    item["title"] = result;
    item["subtitle"] = args + QStringLiteral(" = ") + result;
    item["icon"] = "";
    action["type"] = QStringLiteral("copy");
    action["data"] = result;
    item["action"] = action;
    return QVariantList() << item;
}

bool CalcPlugin::execute(const QVariantMap &action)
{
    if (action.value("type").toString() != QStringLiteral("copy")) return false;
    const QString text = action.value("data").toString();
    if (text.isEmpty()) return false;
    QGuiApplication::clipboard()->setText(text);
    return true;
}

