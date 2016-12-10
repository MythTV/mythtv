/*
 * This file is part of libbluray
 * Copyright (C) 2012  Petri Hintukainen <phintuka@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "util/bits.h"
#include "extdata_parse.h"

#include <stdint.h>

int bdmv_parse_extension_data(BITSTREAM *bits,
                              int start_address,
                              int (*handler)(BITSTREAM*, int, int, void*),
                              void *handle)
{
    int64_t length;
    int num_entries, n;

    if (start_address < 1) return 0;
    if (start_address > bits->end - 12) return 0;

    if (bs_seek_byte(bits, start_address) < 0) {
        return 0;
    }

    length      = bs_read(bits, 32); /* length of extension data block */
    if (length < 1) return 0;
    bs_skip(bits, 32); /* relative start address of extension data */
    bs_skip(bits, 24); /* padding */
    num_entries = bs_read(bits, 8);

    if (start_address > bits->end - 12 - num_entries * 12) return 0;

    for (n = 0; n < num_entries; n++) {
        uint16_t id1       = bs_read(bits, 16);
        uint16_t id2       = bs_read(bits, 16);
        int64_t  ext_start = bs_read(bits, 32);
        int64_t  ext_len   = bs_read(bits, 32);

        int64_t  saved_pos = bs_pos(bits) >> 3;

        if (ext_start + start_address + ext_len > bits->end) return 0;

        if (bs_seek_byte(bits, start_address + ext_start) >= 0) {
            (handler)(bits, id1, id2, handle);
        }

        if (bs_seek_byte(bits, saved_pos) < 0) {
            return 0;
        }
    }

    return 1;
}
