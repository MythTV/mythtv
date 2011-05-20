/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  Obliter0n
 * Copyright (C) 2009-2010  John Stebbins
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

#ifndef BD_LOG_CONTROL_H_
#define BD_LOG_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


enum debug_mask_enum {
    DBG_RESERVED   = 0x00001,
    DBG_CONFIGFILE = 0x00002,
    DBG_FILE       = 0x00004,
    DBG_AACS       = 0x00008,
    DBG_MKB        = 0x00010,
    DBG_MMC        = 0x00020,
    DBG_BLURAY     = 0x00040,
    DBG_DIR        = 0x00080,
    DBG_NAV        = 0x00100,
    DBG_BDPLUS     = 0x00200,
    DBG_DLX        = 0x00400,
    DBG_CRIT       = 0x00800, /* this is libbluray's default debug mask so use this if you want to display critical info */
    DBG_HDMV       = 0x01000,
    DBG_BDJ        = 0x02000,
    DBG_STREAM     = 0x04000,
    DBG_GC         = 0x08000, /* graphics controller */
    DBG_DECODE     = 0x10000, /* PG / IG decoders, m2ts demuxer */
};

typedef enum debug_mask_enum debug_mask_t;

typedef void (*BD_LOG_FUNC)(const char *);

/*
 *
 */

void bd_set_debug_handler(BD_LOG_FUNC);

void bd_set_debug_mask(uint32_t mask);
uint32_t bd_get_debug_mask(void);

#ifdef __cplusplus
};
#endif

#endif /* BD_LOG_CONTROL_H_ */
