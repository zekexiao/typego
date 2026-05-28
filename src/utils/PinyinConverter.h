#pragma once

#include <QString>
#include <QStringList>

class PinyinConverter {
public:
    static PinyinConverter &instance();

    QString toPinyin(const QString &text) const;
    QString toAcronym(const QString &text) const;

private:
    PinyinConverter();
    ~PinyinConverter() = default;
    PinyinConverter(const PinyinConverter &) = delete;
    PinyinConverter &operator=(const PinyinConverter &) = delete;
};
