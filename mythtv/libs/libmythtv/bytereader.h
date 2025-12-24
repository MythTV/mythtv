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

#ifndef BYTEREADER_H
#define BYTEREADER_H

#include <cstdint>

#include "mythtvexp.h"

/**
@file
@ingroup libmythtv

This is in libmythtv because that is where the parsers, which are its main users,
are.
*/

/**
These functions are rewritten copies from FFmpeg's libavcodec, where
find_start_code() is prefixed with <b>@c avpriv_ </b>.
*/
namespace ByteReader
{
/**
 * @brief Test whether a start code found by find_start_code() is valid.
 *
 * Use this to test the validity of a start code especially if a start code can
 * be at the end of the buffer, where testing the return value of find_start_code()
 * would incorrectly imply that the start code is invalid (since the returned value
 * equals <b>@c end </b>).
 *
 * @param[in] start_code The start code to test.
 * @return A boolean that is true if and only if <b>@p start_code</b> is valid
 */
inline bool start_code_is_valid(uint32_t start_code)
{
    return (start_code & 0xFFFFFF00) == 0x100;
}

/**
 * @brief Find the first start code in the buffer @p p.
 *
 * A start code is a sequence of 4 bytes with the hexadecimal value <b><tt> 00 00 01 XX </tt></b>,
 * where <b><tt> XX </tt></b> represents any value and memory address increases left to right.
 *
 * @param[in] p     A pointer to the start of the memory buffer to scan.
 * @param[in] end   A pointer to the past-the-end memory address for the buffer
 *                  given by @p p.  <b>@p p</b> must be ≤ <b>@p end</b>.
 *
 * @param[out] start_code A pointer to a mutable @c uint32_t.<br>
 *             Set to the found start code if it exists or an invalid start code
 *             (the 4 bytes prior to the returned value or <b>@c ~0 </b> if
 *             @f$ end - p < 4 @f$).
 *
 * @return A pointer to the memory address following the found start code, or <b>@p end</b>
 *         if no start code was found.
 */
MTV_PUBLIC
const uint8_t* find_start_code(const uint8_t *p,
                               const uint8_t *end,
                               uint32_t *start_code);

/**
 * By preserving the <b>@p start_code</b> value between subsequent calls, the caller can
 * detect start codes across buffer boundaries.
 *
 * @param[in,out] start_code A pointer to a mutable @c uint32_t.<br>
 *          As input: For no history preset to <b>@c ~0 </b>, otherwise preset to the last
 *                    returned start code to enable detecting start codes across
 *                    buffer boundaries.<br>
 *          On output: Set to the found start code if it exists or an invalid
 *                     start code (the 4 bytes prior to the returned value,
 *                     using the input history if @f$ end - p < 4 @f$).
 *
 * @sa find_start_code()
 */
MTV_PUBLIC
const uint8_t *find_start_code_truncated(const uint8_t * p,
                                         const uint8_t * end,
                                         uint32_t * start_code);

inline uint32_t readBigEndianU32(const uint8_t* x)
{
    return uint32_t{x[0]} << 24 |
           uint32_t{x[1]} << 16 |
           uint32_t{x[2]} <<  8 |
           uint32_t{x[3]};
}

inline uint32_t readBigEndianU24(const uint8_t* x)
{
    return uint32_t{x[0]} << 16 |
           uint32_t{x[1]} <<  8 |
           uint32_t{x[2]};
}
} // namespace ByteReader

#endif // BYTEREADER_H
