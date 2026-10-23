#pragma once

#include <QList>
#include <QString>

// 长度单位换算的纯规则：单位表、查询解析与数值格式化。
// 从 ConvertPlugin 抽离，放在 typego_core 供界面与单元测试共用。
namespace UnitConversion {

struct LengthUnit {
    QString key;
    QString label;
    double factorToMeter;
};

const QList<LengthUnit> &lengthUnits();

// 解析 "10km"、"1 mile" 这类输入；成功时写入 value 与 unit 并返回 true。
bool parseLengthQuery(const QString &query, double *value, LengthUnit *unit);

// 换算结果统一按 12 位有效数字输出，并消除 "-0"。
QString formatNumber(double value);

}
