#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QXmlStreamReader>

// 翻译完整性检查：译文留空时 Qt 会直接回退成英文原文，界面上就会露出没翻译的英文。
// 这里只读仓库里的 .ts 源文件，不碰用户数据目录。
class TstTranslations : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void everyMessageHasTranslation();
    void everySourceTextIsCatalogued();

private:
    static QStringList translationFiles();
    static QString relativeToSource(const QString &path);
    // 去掉注释，但保留字符串字面量里的 "//"（例如网址）。
    static QString stripComments(const QString &text);
    static QString unescapeLiteral(const QString &literal);

    QStringList m_files;
};

QStringList TstTranslations::translationFiles()
{
    const QDir root(QStringLiteral(TYPEGO_SOURCE_DIR));
    QStringList files;
    // 主程序翻译在 i18n/，插件翻译在 plugins/<插件>/translations/。
    // website 下同名后缀的 .ts 是 TypeScript 源码，不属于翻译文件，因此不扫描。
    QDirIterator appIt(root.filePath(QStringLiteral("i18n")), QStringList() << QStringLiteral("*_zh_CN.ts"),
                       QDir::Files);
    while (appIt.hasNext()) files << appIt.next();
    QDirIterator pluginIt(root.filePath(QStringLiteral("plugins")), QStringList() << QStringLiteral("*_zh_CN.ts"),
                           QDir::Files, QDirIterator::Subdirectories);
    while (pluginIt.hasNext()) files << pluginIt.next();
    files.sort();
    return files;
}

QString TstTranslations::relativeToSource(const QString &path)
{
    const QDir root(QStringLiteral(TYPEGO_SOURCE_DIR));
    return root.relativeFilePath(path);
}

QString TstTranslations::stripComments(const QString &text)
{
    QString out;
    out.reserve(text.size());
    int i = 0;
    const int n = text.size();
    while (i < n) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('"')) {
            // 字符串字面量原样保留，其中的 "//" 不是注释。
            out += c;
            ++i;
            while (i < n) {
                const QChar s = text.at(i);
                out += s;
                ++i;
                if (s == QLatin1Char('\\') && i < n) {
                    out += text.at(i);
                    ++i;
                    continue;
                }
                if (s == QLatin1Char('"')) break;
            }
            continue;
        }
        if (c == QLatin1Char('/') && i + 1 < n && text.at(i + 1) == QLatin1Char('/')) {
            while (i < n && text.at(i) != QLatin1Char('\n')) ++i;
            continue;
        }
        if (c == QLatin1Char('/') && i + 1 < n && text.at(i + 1) == QLatin1Char('*')) {
            i += 2;
            while (i + 1 < n && !(text.at(i) == QLatin1Char('*') && text.at(i + 1) == QLatin1Char('/'))) ++i;
            i = (i + 1 < n) ? i + 2 : n;
            out += QLatin1Char(' ');
            continue;
        }
        out += c;
        ++i;
    }
    return out;
}

QString TstTranslations::unescapeLiteral(const QString &literal)
{
    QString out;
    out.reserve(literal.size());
    for (int i = 0; i < literal.size(); ++i) {
        const QChar c = literal.at(i);
        if (c != QLatin1Char('\\') || i + 1 >= literal.size()) {
            out += c;
            continue;
        }
        const QChar next = literal.at(++i);
        switch (next.unicode()) {
        case 'n': out += QLatin1Char('\n'); break;
        case 't': out += QLatin1Char('\t'); break;
        case 'r': out += QLatin1Char('\r'); break;
        default: out += next; break;
        }
    }
    return out;
}

void TstTranslations::initTestCase()
{
    m_files = translationFiles();
    // 翻译文件缺失时用例必须失败，否则空跑会被当成通过。
    QVERIFY2(!m_files.isEmpty(), "未找到任何 *_zh_CN.ts 翻译文件");
    QVERIFY2(m_files.contains(QStringLiteral(TYPEGO_SOURCE_DIR "/i18n/TypeGo_zh_CN.ts")),
             "主程序翻译文件 i18n/TypeGo_zh_CN.ts 缺失");
}

void TstTranslations::everyMessageHasTranslation()
{
    QStringList failures;
    int total = 0;
    for (const QString &path : m_files) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            failures << QStringLiteral("%1: 无法打开").arg(relativeToSource(path));
            continue;
        }
        QXmlStreamReader xml(&file);
        QString context;
        QString source;
        QString translation;
        bool unfinished = false;
        bool vanished = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                const QStringView name = xml.name();
                if (name == QLatin1String("context")) {
                    context.clear();
                } else if (name == QLatin1String("name")) {
                    context = xml.readElementText();
                } else if (name == QLatin1String("message")) {
                    source.clear();
                    translation.clear();
                    unfinished = false;
                    vanished = false;
                } else if (name == QLatin1String("source")) {
                    source = xml.readElementText();
                } else if (name == QLatin1String("translation")) {
                    const auto type = xml.attributes().value(QLatin1String("type"));
                    unfinished = type == QLatin1String("unfinished");
                    vanished = type == QLatin1String("vanished");
                    // 复数条目是 <numerusform> 子节点，这里一并取其文本。
                    translation = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                }
            } else if (xml.isEndElement() && xml.name() == QLatin1String("message")) {
                ++total;
                if (vanished) continue;
                if (unfinished || translation.trimmed().isEmpty()) {
                    failures << QStringLiteral("%1 :: %2 :: \"%3\"%4")
                                    .arg(relativeToSource(path), context, source,
                                         unfinished ? QStringLiteral("（未完成）") : QString());
                }
            }
        }
        if (xml.hasError()) {
            failures << QStringLiteral("%1: XML 解析失败 %2")
                            .arg(relativeToSource(path), xml.errorString());
        }
    }
    QVERIFY2(total > 0, "翻译文件里没有解析到任何条目");
    QVERIFY2(failures.isEmpty(),
             qPrintable(QStringLiteral("存在未翻译条目：\n%1").arg(failures.join(QLatin1Char('\n')))));
}

// 插件的 .ts 不会随构建自动跑 lupdate，源码里新增的文案很容易漏进翻译文件。
// 这里反向核对：源码里每个 tr()/translate() 文案都必须在对应 .ts 里登记过。
void TstTranslations::everySourceTextIsCatalogued()
{
    const QDir root(QStringLiteral(TYPEGO_SOURCE_DIR));
    const QRegularExpression trPattern(QStringLiteral("\\btr\\s*\\(\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*[,)]"));
    const QRegularExpression translatePattern(
        QStringLiteral("\\btranslate\\s*\\(\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*,\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*[,)]"));

    QStringList failures;
    int checked = 0;
    for (const QString &tsPath : m_files) {
        const QString relative = root.relativeFilePath(tsPath);
        QString sourceDir;
        if (relative.startsWith(QStringLiteral("i18n/"))) {
            sourceDir = root.filePath(QStringLiteral("src"));
        } else {
            const int marker = relative.indexOf(QStringLiteral("/translations/"));
            if (marker < 0) continue;
            sourceDir = root.filePath(relative.left(marker));
        }
        if (!QDir(sourceDir).exists()) continue;

        QSet<QString> sources;
        QSet<QString> contextual;
        {
            QFile file(tsPath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QXmlStreamReader xml(&file);
            QString context;
            QString source;
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement()) {
                    if (xml.name() == QLatin1String("name")) {
                        context = xml.readElementText();
                    } else if (xml.name() == QLatin1String("source")) {
                        source = xml.readElementText();
                        sources.insert(source);
                        contextual.insert(context + QLatin1Char('\t') + source);
                    }
                }
            }
        }

        QDirIterator it(sourceDir, QStringList() << QStringLiteral("*.cpp") << QStringLiteral("*.h"), QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString code = stripComments(QString::fromUtf8(file.readAll()));
            const QString relativeCode = root.relativeFilePath(path);

            auto matches = trPattern.globalMatch(code);
            while (matches.hasNext()) {
                const QString text = unescapeLiteral(matches.next().captured(1));
                ++checked;
                if (!sources.contains(text)) {
                    failures << QStringLiteral("%1: tr(\"%2\") 未登记到 %3")
                                    .arg(relativeCode, text, relative);
                }
            }
            matches = translatePattern.globalMatch(code);
            while (matches.hasNext()) {
                const QRegularExpressionMatch match = matches.next();
                const QString context = unescapeLiteral(match.captured(1));
                const QString text = unescapeLiteral(match.captured(2));
                ++checked;
                if (!contextual.contains(context + QLatin1Char('\t') + text)) {
                    failures << QStringLiteral("%1: translate(\"%2\", \"%3\") 未登记到 %4")
                                    .arg(relativeCode, context, text, relative);
                }
            }
        }
    }
    QVERIFY2(checked > 0, "没有扫描到任何 tr()/translate() 文案");
    QVERIFY2(failures.isEmpty(),
             qPrintable(QStringLiteral("存在未登记到翻译文件的文案：\n%1").arg(failures.join(QLatin1Char('\n')))));
}

QTEST_GUILESS_MAIN(TstTranslations)

#include "tst_translations.moc"
