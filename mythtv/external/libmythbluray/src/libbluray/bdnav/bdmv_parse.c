/*
 * This file is part of libbluray
 * Copyright (C) 2017  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#include "bdmv_parse.h"

#include "util/bits.h"
#include "util/logging.h"

#include <stdint.h>


#define U32CHARS(u) (char)((u) >> 24), (char)((u) >> 16), (char)((u) >> 8), (char)(u)

int bdmv_parse_header(BITSTREAM *bs, uint32_t type, uint32_t *version)
{
    uint32_t tag, ver;

    if (bs_seek_byte(bs, 0) < 0) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "bdmv_parse_header(%c%c%c%c): seek failed\n", U32CHARS(type));
        return 0;
    }

    /* read and verify magic bytes and version code */

    if (bs_avail(bs)/8 < 8) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "bdmv_parse_header(%c%c%c%c): unexpected EOF\n", U32CHARS(type));
        return 0;
    }

    tag = bs_read(bs, 32);
    ver = bs_read(bs, 32);

    if (tag != type) {
        BD_DEBUG(DBG_NAV | DBG_CRIT, "bdmv_parse_header(%c%c%c%c): invalid signature %c%c%c%c\n",
                 U32CHARS(type), U32CHARS(tag));
        return 0;
    }

    switch (ver) {
        case BDMV_VERSION_0100:
        case BDMV_VERSION_0200:
        case BDMV_VERSION_0240:
        case BDMV_VERSION_0300:
            break;
        default:
            BD_DEBUG(DBG_NAV | DBG_CRIT, "bdmv_parse_header(%c%c%c%c): unsupported file version %c%c%c%c\n",
                     U32CHARS(type), U32CHARS(ver));
            return 0;
    }

    if (version) {
        *version = ver;
    }

    return 1;
}
