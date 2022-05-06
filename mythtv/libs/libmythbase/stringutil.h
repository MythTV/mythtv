#ifndef STRINGUTIL_H_
#define STRINGUTIL_H_

#if __has_include(<format>) // C++20
#include <format>
#endif

#include <QByteArray>
#include <QString>

#include "mythbaseexp.h"

namespace StringUtil
{
MBASE_PUBLIC bool isValidUTF8(const QByteArray &data);

/**
Creates a zero padded string representation of an integer
\param n integer to convert
\param width minimum string length including sign, if any

\note per the QString::arg() documentation the padding may prefix the sign of a negative number
*/
inline QString intToPaddedString(int n, int width = 2)
{
#ifdef __cpp_lib_format // C++20 with <format>
    return QString::fromStdString(std::format("{:0{}d}", n, width));
#else
    return QString("%1").arg(n, width, 10, QChar('0'));
#endif // __cpp_lib_format
}

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

/**
naturalCompare as a std::sort compatible function (ignoring the third parameter,
which is never used).
*/
inline bool naturalSortCompare(const QString &a, const QString &b,
                   Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive)
{
    return naturalCompare(a, b, caseSensitivity) < 0;
}

} // namespace StringUtil

#endif // STRINGUTIL_H_
