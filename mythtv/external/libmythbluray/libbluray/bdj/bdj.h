/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
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

#ifndef BDJ_H_
#define BDJ_H_

#include "common.h"

#include <util/attributes.h>

typedef enum {
    BDJ_EVENT_NONE = 0,
    BDJ_EVENT_CHAPTER,
    BDJ_EVENT_PLAYITEM,
    BDJ_EVENT_ANGLE,
    BDJ_EVENT_SUBTITLE,
    BDJ_EVENT_PIP,
    BDJ_EVENT_END_OF_PLAYLIST,
    BDJ_EVENT_PTS,
    BDJ_EVENT_VK_KEY,
} BDJ_EVENT;

typedef struct bdjava_s BDJAVA;

struct bluray;
struct bd_registers_s;
struct indx_root_s;

BD_PRIVATE BDJAVA* bdj_open(const char *path, struct bluray *bd,
                            struct bd_registers_s *registers, struct indx_root_s *index);
BD_PRIVATE int  bdj_start(BDJAVA *bdjava, unsigned title);
BD_PRIVATE int  bdj_stop(BDJAVA *bdjava);
BD_PRIVATE void bdj_close(BDJAVA *bdjava);
BD_PRIVATE void bdj_process_event(BDJAVA *bdjava, unsigned ev, unsigned param);

#endif
