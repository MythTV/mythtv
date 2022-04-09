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
