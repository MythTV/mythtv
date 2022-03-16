#include "stringutil.h"

#if __has_include(<bit>) // C++20
#include <bit>
#endif

#include <climits> // for CHAR_BIT

#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
using std::countl_one;
#else
// 8 bit LUT based count leading ones
static int countl_one(unsigned char x)
{
#if CHAR_BIT != 8
    if (x > 256)
        return 8; // works for our purposes even if not correct
#endif
    static constexpr int leading_ones[256] =
    {
#define REPEAT_4(x) (x), (x), (x), (x)
#define REPEAT_8(x)   REPEAT_4(x),  REPEAT_4(x)
#define REPEAT_16(x)  REPEAT_8(x),  REPEAT_8(x)
#define REPEAT_32(x)  REPEAT_16(x), REPEAT_16(x)
#define REPEAT_64(x)  REPEAT_32(x), REPEAT_32(x)
#define REPEAT_128(x) REPEAT_64(x), REPEAT_64(x)
        REPEAT_128(0), REPEAT_64(1), REPEAT_32(2), REPEAT_16(3), REPEAT_8(4), REPEAT_4(5), 6, 6, 7, 8
#undef REPEAT_4
#undef REPEAT_8
#undef REPEAT_16
#undef REPEAT_32
#undef REPEAT_64
#undef REPEAT_128
    };
    return leading_ones[x];
}
#endif // C++20 feature test macro

bool StringUtil::isValidUTF8(const QByteArray& data)
{
    const unsigned char* p = (const unsigned char*)data.data();
    const unsigned char* const end = p + data.size();
    while (p < end)
    {
        int code_point_length = countl_one(*p);

        switch (code_point_length)
        {
        case 0: p++; continue; // ASCII
        case 1: return false;  // invalid, continuation byte
        case 2:
        case 3:
        case 4: break;
        default: return false;
        /* the variable length code is limited to 4 bytes by RFC3629 §3 to match
           the range of UTF-16, i.e. the maximum code point is U+10FFFF
        */
        }

        if (end < code_point_length + p)
        {
            return false; // truncated codepoint at end
        }

        // verify each starting byte is followed by the correct number of continuation bytes
        switch (code_point_length)
        {
        case 4:
            if (countl_one(p[3]) != 1)
            {
                return false;
            }
            [[fallthrough]];
        case 3:
            if (countl_one(p[2]) != 1)
            {
                return false;
            }
            [[fallthrough]];
        case 2:
            if (countl_one(p[1]) != 1)
            {
                return false;
            }
            break;
        default: break; // should never be reached
        }

        // all continuation bytes are in the range 0x80 to 0xBF
        switch (code_point_length)
        {
        case 2:
            // overlong encoding of single byte character
            if (*p == 0xC0 || *p == 0xC1)
            {
                return false;
            }
            break;
        case 3:
            // U+D800–U+DFFF are invalid; UTF-16 surrogate halves
            // 0xED'A0'80 to 0xED'BF'BF
            if (p[0] == 0xED && p[1] >= 0xA0)
            {
                return false;
            }
            // overlong encoding of 2 byte character
            if (p[0] == 0xE0 && p[1] < 0xA0)
            {
                return false;
            }
            break;
        case 4:
            // code points > U+10FFFF are invalid
            // U+10FFFF in UTF-8 is 0xF4'8F'BF'BF
            // U+110000 in UTF-8 is 0xF4'90'80'80
            if (*p > 0xF4 || (p[0] == 0xF4 && p[1] >= 0x90))
            {
                return false;
            }
            // overlong encoding of 3 byte character
            if (p[0] == 0xF0 && p[1] < 0x90)
            {
                return false;
            }
            break;
        default: break; // should never be reached
        }

        p += code_point_length;
    }

    return true;
}

/**
This method chops the input a and b into pieces of
digits and non-digits (a1.05 becomes a | 1 | . | 05)
and compares these pieces of a and b to each other
(first with first, second with second, ...).

This is based on the natural sort order code code by Martin Pool
http://sourcefrog.net/projects/natsort/
Martin Pool agreed to license this under LGPL or GPL.

\todo FIXME: Using toLower() to implement case insensitive comparison is
sub-optimal, but is needed because we compare strings with
localeAwareCompare(), which does not know about case sensitivity.
A task has been filled for this in Qt Task Tracker with ID 205990.
http://trolltech.com/developer/task-tracker/index_html?method=entry&id=205990
Dead link.  QCollator might be of relevance.
*/
int StringUtil::naturalCompare(const QString &_a, const QString &_b, Qt::CaseSensitivity caseSensitivity)
{
    QString a;
    QString b;

    if (caseSensitivity == Qt::CaseSensitive)
    {
        a = _a;
        b = _b;
    }
    else
    {
        a = _a.toLower();
        b = _b.toLower();
    }

    const QChar* currA = a.unicode(); // iterator over a
    const QChar* currB = b.unicode(); // iterator over b

    if (currA == currB)
    {
        return 0;
    }

    while (!currA->isNull() && !currB->isNull())
    {
        const QChar* begSeqA = currA; // beginning of a new character sequence of a
        const QChar* begSeqB = currB;

        if (currA->unicode() == QChar::ObjectReplacementCharacter)
        {
            return 1;
        }

        if (currB->unicode() == QChar::ObjectReplacementCharacter)
        {
            return -1;
        }

        if (currA->unicode() == QChar::ReplacementCharacter)
        {
            return 1;
        }

        if (currB->unicode() == QChar::ReplacementCharacter)
        {
            return -1;
        }

        // find sequence of characters ending at the first non-character
        while (!currA->isNull() && !currA->isDigit() && !currA->isPunct() &&
               !currA->isSpace())
        {
            ++currA;
        }

        while (!currB->isNull() && !currB->isDigit() && !currB->isPunct() &&
               !currB->isSpace())
        {
            ++currB;
        }

        // compare these sequences
        const QString& subA(a.mid(begSeqA - a.unicode(), currA - begSeqA));
        const QString& subB(b.mid(begSeqB - b.unicode(), currB - begSeqB));
        const int cmp = QString::localeAwareCompare(subA, subB);

        if (cmp != 0)
        {
            return cmp < 0 ? -1 : +1;
        }

        if (currA->isNull() || currB->isNull())
        {
            break;
        }

        // find sequence of characters ending at the first non-character
        while ((currA->isPunct() || currA->isSpace()) &&
               (currB->isPunct() || currB->isSpace()))
        {
            if (*currA != *currB)
            {
                return (*currA < *currB) ? -1 : +1;
            }
            ++currA;
            ++currB;
            if (currA->isNull() || currB->isNull())
            {
                break;
            }
        }

        // now some digits follow...
        if ((*currA == QLatin1Char('0')) || (*currB == QLatin1Char('0')))
        {
            // one digit-sequence starts with 0 -> assume we are in a fraction part
            // do left aligned comparison (numbers are considered left aligned)
            while (true)
            {
                if (!currA->isDigit() && !currB->isDigit())
                {
                    break;
                }
                if (!currA->isDigit())
                {
                    return +1;
                }
                if (!currB->isDigit())
                {
                    return -1;
                }
                if (*currA < *currB)
                {
                    return -1;
                }
                if (*currA > *currB)
                {
                    return + 1;
                }
                ++currA;
                ++currB;
            }
        }
        else
        {
            // No digit-sequence starts with 0 -> assume we are looking at some integer
            // do right aligned comparison.
            //
            // The longest run of digits wins. That aside, the greatest
            // value wins, but we can't know that it will until we've scanned
            // both numbers to know that they have the same magnitude.

            bool isFirstRun = true;
            int weight = 0;

            while (true)
            {
                if (!currA->isDigit() && !currB->isDigit())
                {
                    if (weight != 0)
                    {
                        return weight;
                    }
                    break;
                }
                if (!currA->isDigit())
                {
                    if (isFirstRun)
                    {
                        return *currA < *currB ? -1 : +1;
                    }
                    return -1;
                }
                if (!currB->isDigit())
                {
                    if (isFirstRun)
                    {
                        return *currA < *currB ? -1 : +1;
                    }
                    return +1;
                }
                if ((*currA < *currB) && (weight == 0))
                {
                    weight = -1;
                }
                else if ((*currA > *currB) && (weight == 0))
                {
                    weight = + 1;
                }
                ++currA;
                ++currB;
                isFirstRun = false;
            }
        }
    }

    if (currA->isNull() && currB->isNull())
    {
        return 0;
    }

    return currA->isNull() ? -1 : + 1;
}
