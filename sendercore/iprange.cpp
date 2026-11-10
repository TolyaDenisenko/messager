#include "iprange.h"

#include <QHostAddress>
#include <QRegularExpression>
#include <QSet>

static constexpr qint64 kMaxHosts = 65536;

static bool parseV4(const QString &s, quint32 *out)
{
    QHostAddress a;
    if (!a.setAddress(s.trimmed()) || a.protocol() != QAbstractSocket::IPv4Protocol)
        return false;
    *out = a.toIPv4Address();
    return true;
}

QStringList parseIpRanges(const QString &text, QString *error)
{
    QStringList result;
    QSet<quint32> seen;
    auto fail = [&](const QString &msg) {
        if (error)
            *error = msg;
        return QStringList();
    };

    const auto parts = text.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return fail(QStringLiteral("Укажите диапазон IP-адресов"));

    for (const QString &part : parts) {
        quint32 first = 0, last = 0;
        if (part.contains('/')) {
            const auto x = part.split('/');
            bool okMask = false;
            const int n = x.value(1).toInt(&okMask);
            if (x.size() != 2 || !parseV4(x[0], &first) || !okMask || n < 0 || n > 32)
                return fail(QStringLiteral("Неверная подсеть: ") + part);
            const quint32 mask = n == 0 ? 0 : (0xFFFFFFFFu << (32 - n));
            const quint32 net = first & mask;
            first = net;
            last = net | ~mask;
            if (n <= 30) {
                ++first;
                --last;
            }
        } else if (part.contains('-')) {
            const auto x = part.split('-');
            if (x.size() != 2 || !parseV4(x[0], &first))
                return fail(QStringLiteral("Неверный диапазон: ") + part);
            if (x[1].contains('.')) {
                if (!parseV4(x[1], &last))
                    return fail(QStringLiteral("Неверный диапазон: ") + part);
            } else {
                bool okN = false;
                const uint n = x[1].toUInt(&okN);
                if (!okN || n > 255)
                    return fail(QStringLiteral("Неверный диапазон: ") + part);
                last = (first & 0xFFFFFF00u) | n;
            }
            if (last < first)
                return fail(QStringLiteral("Конец диапазона меньше начала: ") + part);
        } else {
            if (!parseV4(part, &first))
                return fail(QStringLiteral("Неверный IP-адрес: ") + part);
            last = first;
        }

        if (qint64(last) - qint64(first) + 1 + result.size() > kMaxHosts)
            return fail(QStringLiteral("Слишком много адресов (максимум 65536)"));
        for (quint64 a = first; a <= last; ++a)
            if (!seen.contains(quint32(a))) {
                seen.insert(quint32(a));
                result << QHostAddress(quint32(a)).toString();
            }
    }
    return result;
}
