#ifndef STRINGUTIL_H_
#define STRINGUTIL_H_

#include <QByteArray>
#include <QString>

#include "mythbaseexp.h"

namespace StringUtil
{
MBASE_PUBLIC bool isValidUTF8(const QByteArray &data);

inline QString indentSpaces(unsigned int level, unsigned int size = 4)
{
    return QString(level * size, QChar(' '));
}

/**
This is equivalent to QVariant(bool).toString()
*/
inline QString bool_to_string(bool val)
{
    return (val) ? QStringLiteral("true") : QStringLiteral("false");
}

MBASE_PUBLIC
int naturalCompare(const QString &_a, const QString &_b,
                   Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive);

} // namespace StringUtil

#endif // STRINGUTIL_H_
