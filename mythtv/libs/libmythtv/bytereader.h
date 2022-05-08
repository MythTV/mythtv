/*
 * find_start_code from libavcodec/FFmpeg
 * Copyright (c) 2022 Scott Theisen
 *
 * This file is part of MythTV.  The functions have been adapted to use C++.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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

} // namespace ByteReader

#endif // BYTEREADER_H
