#include "UnitConversion.h"

#include <QRegularExpression>

#include <cmath>

namespace UnitConversion {

const QList<LengthUnit> &lengthUnits()
{
    static const QList<LengthUnit> units = {
        {QStringLiteral("mm"), QStringLiteral("Millimeter"), 0.001},
        {QStringLiteral("cm"), QStringLiteral("Centimeter"), 0.01},
        {QStringLiteral("m"), QStringLiteral("Meter"), 1.0},
        {QStringLiteral("km"), QStringLiteral("Kilometer"), 1000.0},
        {QStringLiteral("in"), QStringLiteral("Inch"), 0.0254},
        {QStringLiteral("ft"), QStringLiteral("Foot"), 0.3048},
        {QStringLiteral("yd"), QStringLiteral("Yard"), 0.9144},
        {QStringLiteral("mi"), QStringLiteral("Mile"), 1609.344},
    };
    return units;
}

static QString normalizedUnit(QString unit)
{
    unit = unit.trimmed().toLower();
    if (unit == QStringLiteral("meter") || unit == QStringLiteral("meters")) return QStringLiteral("m");
    if (unit == QStringLiteral("kilometer") || unit == QStringLiteral("kilometers")) return QStringLiteral("km");
    if (unit == QStringLiteral("centimeter") || unit == QStringLiteral("centimeters")) return QStringLiteral("cm");
    if (unit == QStringLiteral("millimeter") || unit == QStringLiteral("millimeters")) return QStringLiteral("mm");
    if (unit == QStringLiteral("inch") || unit == QStringLiteral("inches")) return QStringLiteral("in");
    if (unit == QStringLiteral("foot") || unit == QStringLiteral("feet")) return QStringLiteral("ft");
    if (unit == QStringLiteral("yard") || unit == QStringLiteral("yards")) return QStringLiteral("yd");
    if (unit == QStringLiteral("mile") || unit == QStringLiteral("miles")) return QStringLiteral("mi");
    return unit;
}

bool parseLengthQuery(const QString &query, double *value, LengthUnit *unit)
{
    static const QRegularExpression pattern(QStringLiteral("^\\s*([+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+))\\s*([a-zA-Z]+)\\s*$"));
    const QRegularExpressionMatch match = pattern.match(query);
    if (!match.hasMatch()) return false;

    bool ok = false;
    const double amount = match.captured(1).toDouble(&ok);
    if (!ok || !std::isfinite(amount)) return false;

    const QString key = normalizedUnit(match.captured(2));
    for (const LengthUnit &candidate : lengthUnits()) {
        if (candidate.key == key) {
            *value = amount;
            *unit = candidate;
            return true;
        }
    }
    return false;
}

QString formatNumber(double value)
{
    QString text = QString::number(value, 'g', 12);
    if (text == QStringLiteral("-0")) return QStringLiteral("0");
    return text;
}

}
