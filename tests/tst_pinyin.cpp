#include <QtTest>

#include "utils/PinyinConverter.h"

// 拼音用于应用搜索的中文匹配，字典缺字时会退化成原字符，测试要固定这两种行为。
class TstPinyin : public QObject
{
    Q_OBJECT

private slots:
    void toPinyinConvertsKnownHan();
    void toPinyinKeepsUnknownCharAsLowercase();
    void toAcronymKeepsFirstLetterOnly();
    void toAcronymSkipsNonLetters();
    void handlesSurrogatePairsWithoutGarbage();
};

void TstPinyin::toPinyinConvertsKnownHan()
{
    const PinyinConverter &c = PinyinConverter::instance();
    QCOMPARE(c.toPinyin(QStringLiteral("中文")), QStringLiteral("zhongwen"));
    QCOMPARE(c.toPinyin(QStringLiteral("焊接")), QStringLiteral("hanjie"));
    QCOMPARE(c.toPinyin(QStringLiteral("软件")), QStringLiteral("ruanjian"));
}

void TstPinyin::toPinyinKeepsUnknownCharAsLowercase()
{
    const PinyinConverter &c = PinyinConverter::instance();
    QCOMPARE(c.toPinyin(QStringLiteral("A中B")), QStringLiteral("azhongb"));
    QCOMPARE(c.toPinyin(QStringLiteral("TypeGo")), QStringLiteral("typego"));
    // 字典命中的汉字不再保留原字符，这是搜索用拼音而非汉字匹配的原因。
    QVERIFY(!c.toPinyin(QStringLiteral("中")).contains(QStringLiteral("中")));
}

void TstPinyin::toAcronymKeepsFirstLetterOnly()
{
    const PinyinConverter &c = PinyinConverter::instance();
    QCOMPARE(c.toAcronym(QStringLiteral("中文")), QStringLiteral("zw"));
    QCOMPARE(c.toAcronym(QStringLiteral("焊接")), QStringLiteral("hj"));
    QCOMPARE(c.toAcronym(QStringLiteral("软件")), QStringLiteral("rj"));
}

void TstPinyin::toAcronymSkipsNonLetters()
{
    const PinyinConverter &c = PinyinConverter::instance();
    // 数字与标点在全拼里保留原样，在首字母里直接丢弃。
    QCOMPARE(c.toPinyin(QStringLiteral("1中")), QStringLiteral("1zhong"));
    QCOMPARE(c.toAcronym(QStringLiteral("1中")), QStringLiteral("z"));
    QCOMPARE(c.toAcronym(QStringLiteral("")), QString());
}

void TstPinyin::handlesSurrogatePairsWithoutGarbage()
{
    const PinyinConverter &c = PinyinConverter::instance();
    // 字典只覆盖 BMP，补充平面字符必须被跳过而不是留下半个代理项。
    const QString rare = QStringLiteral("中\U00020000文");
    QCOMPARE(c.toPinyin(rare), QStringLiteral("zhongwen"));
    QCOMPARE(c.toAcronym(rare), QStringLiteral("zw"));
}

QTEST_GUILESS_MAIN(TstPinyin)

#include "tst_pinyin.moc"
