#include "PinyinConverter.h"
#include <QHash>
#include <QChar>

namespace {

#include "PinyinDict.inc"

} // namespace

PinyinConverter &PinyinConverter::instance()
{
    static PinyinConverter inst;
    return inst;
}

PinyinConverter::PinyinConverter()
{
}

QString PinyinConverter::toPinyin(const QString &text) const
{
    QString result;
    result.reserve(text.length() * 4);
    for (const QChar &c : text) {
        uint uc = c.unicode();
        // Handle surrogate pairs for characters outside BMP
        if (c.isHighSurrogate()) {
            continue; // Will be handled with low surrogate
        }
        if (c.isLowSurrogate()) {
            continue; // Skip, already processed or not paired
        }
        auto it = PINYIN_DICT.find(uc);
        if (it != PINYIN_DICT.end()) {
            result.append(it.value());
        } else {
            result.append(c.toLower());
        }
    }
    return result;
}

QString PinyinConverter::toAcronym(const QString &text) const
{
    QString result;
    result.reserve(text.length());
    for (const QChar &c : text) {
        uint uc = c.unicode();
        if (c.isHighSurrogate() || c.isLowSurrogate()) {
            continue;
        }
        auto it = PINYIN_DICT.find(uc);
        if (it != PINYIN_DICT.end()) {
            result.append(it.value().at(0));
        } else if (c.isLetter()) {
            result.append(c.toLower());
        }
    }
    return result;
}
