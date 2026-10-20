#include "utils/Version.h"

#include <QStringList>

namespace Version {

QList<int> parse(const QString &text)
{
    QString cleaned = text.trimmed();
    if (cleaned.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) cleaned.remove(0, 1);
    QList<int> parts;
    const QStringList segments = cleaned.split(QLatin1Char('.'));
    for (const QString &segment : segments) {
        bool ok = false;
        const int value = segment.toInt(&ok);
        parts.append(ok ? value : 0);
    }
    return parts;
}

bool isNewer(const QString &remote, const QString &local)
{
    const QList<int> a = parse(remote);
    const QList<int> b = parse(local);
    const int count = qMax(a.size(), b.size());
    for (int i = 0; i < count; ++i) {
        const int x = i < a.size() ? a.at(i) : 0;
        const int y = i < b.size() ? b.at(i) : 0;
        if (x != y) return x > y;
    }
    return false;
}

} // namespace Version
