#ifndef STRINGUTIL_H_
#define STRINGUTIL_H_

#if __has_include(<format>) // C++20
#include <format>
#endif

#include <string_view>
#include <vector>

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
    return {static_cast<int>(level * size), QChar(' ')};
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

MBASE_PUBLIC QString formatKBytes(int64_t sizeKB, int prec=1);
MBASE_PUBLIC QString formatBytes(int64_t sizeB, int prec=1);

/**
Split a `std::string_view` into a `std::vector` of `std::string_view`s.

@param s String to split, may be empty.
@param delimiter String to determine where to split.
@return Will always have a size >= 1.  Only valid as long as the data
        referenced by s remains valid.
*/
inline std::vector<std::string_view> split_sv(const std::string_view s, const std::string_view delimiter)
{
    // There are infinitely many empty strings at each position, avoid infinite loop
    if (delimiter.empty())
        return {s};
    std::vector<std::string_view> tokens;
    size_t last_pos = 0;
    size_t pos = s.find(delimiter);
    while (pos != std::string_view::npos)
    {
        tokens.emplace_back(s.substr(last_pos, pos - last_pos));
        last_pos = pos + delimiter.size();
        pos = s.find(delimiter, last_pos);
    }
    tokens.emplace_back(s.substr(last_pos));
    return tokens;
}

} // namespace StringUtil

#endif // STRINGUTIL_H_
