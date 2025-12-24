// SPDX-License-Identifier: Unlicense
// Created by Scott Theisen, 2022-2025
/* The Unlicense
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>
*/

#include "bytereader.h"

#include <algorithm>

const uint8_t* ByteReader::find_start_code(const uint8_t * p,
                                           const uint8_t * const end,
                                           uint32_t * const start_code)
{
    // minimum length for a start code
    if (p + 4 > end)
    {
        *start_code = ~0; // set to an invalid start code
        return end;
    }

    p += 3; // offset for negative indices in while loop

    /* with memory address increasing left to right, we are looking for (in hexadecimal):
     * 00 00 01 XX
     * p points at the address which should have the value of XX
     */
    while (p < end)
    {
        if      (/* UU UU UU */ p[-1]  < 1) // equivalently p[-1] == 0
        {
            p++;
            // could be in a start code, so check next byte
        }
        else if (/* UU UU UN */ p[-1]  > 1 ||
                 /* UU UU 01 */ p[-2] != 0 ||
                 /* UU 00 01 */ p[-3] != 0)
        {
            // start check over with 3 new bytes
            p += 3;
        }
        else /* 00 00 01 */
        {
            p++;
            // p now points at the address following the start code value XX
            break;
        }
    }

    p = std::min(p, end);
    // read the previous 4 bytes, i.e. bytes {p - 4, p - 3, p - 2, p - 1}
    *start_code = readBigEndianU32(p - 4);
    return p;
}

const uint8_t* ByteReader::find_start_code_truncated(const uint8_t * p,
                                         const uint8_t * const end,
                                         uint32_t * const start_code)
{
    if (p >= end)
    {
        return end;
    }

    if (*start_code == 0x100)
    {
        *start_code = ~0;
    // invalidate byte 0 so overlapping start codes are not erroneously detected
    }

    // read up to the first three bytes in p to enable reading a start code across
    // two (to four) buffers
    for (int i = 0; i < 3; i++)
    {
        *start_code <<= 8;
        *start_code += *p;
        p++;
        if (start_code_is_valid(*start_code) || p == end)
        {
            return p;
        }
    }
    // buffer length is at least 4
    return find_start_code(p - 3, end, start_code);
}
