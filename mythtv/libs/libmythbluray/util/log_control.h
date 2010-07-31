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

#include <stdint.h>


enum debug_mask_enum {
    DBG_RESERVED = 1,
    DBG_CONFIGFILE = 2,
    DBG_FILE = 4,
    DBG_AACS = 8,
    DBG_MKB = 16,
    DBG_MMC = 32,
    DBG_BLURAY = 64,
    DBG_DIR = 128,
    DBG_NAV = 256,
    DBG_BDPLUS = 512,
    DBG_DLX = 1024,
    DBG_CRIT = 2048,         // this is libbluray's default debug mask so use this if you want to display critical info
    DBG_HDMV = 4096,
    DBG_BDJ = 8192,
};

typedef enum debug_mask_enum debug_mask_t;

typedef void (*BD_LOG_FUNC)(const char *);

/*
 *
 */

void bd_set_debug_handler(BD_LOG_FUNC);

void bd_set_debug_mask(debug_mask_t mask);
debug_mask_t bd_get_debug_mask(void);


#endif /* BD_LOG_CONTROL_H_ */
