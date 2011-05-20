/*
 * This file is part of libbluray
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

#if !defined(_BD_UO_MASK_TABLE_H_)
#define _BD_UO_MASK_TABLE_H_

#include <stdint.h>

typedef struct bd_uo_mask_table_s
{
    uint8_t         menu_call : 1;
    uint8_t         title_search : 1;
    uint8_t         chapter_search : 1;
    uint8_t         time_search : 1;
    uint8_t         skip_to_next_point : 1;
    uint8_t         skip_to_prev_point : 1;
    uint8_t         play_firstplay : 1;
    uint8_t         stop : 1;
    uint8_t         pause_on : 1;
    uint8_t         pause_off : 1;
    uint8_t         still : 1;
    uint8_t         forward : 1;
    uint8_t         backward : 1;
    uint8_t         resume : 1;
    uint8_t         move_up : 1;
    uint8_t         move_down : 1;
    uint8_t         move_left : 1;
    uint8_t         move_right : 1;
    uint8_t         select : 1;
    uint8_t         activate : 1;
    uint8_t         select_and_activate : 1;
    uint8_t         primary_audio_change : 1;
    uint8_t         angle_change : 1;
    uint8_t         popup_on : 1;
    uint8_t         popup_off : 1;
    uint8_t         pg_enable_disable : 1;
    uint8_t         pg_change : 1;
    uint8_t         secondary_video_enable_disable : 1;
    uint8_t         secondary_video_change : 1;
    uint8_t         secondary_audio_enable_disable : 1;
    uint8_t         secondary_audio_change : 1;
    uint8_t         pip_pg_change : 1;
} BD_UO_MASK;

static inline BD_UO_MASK bd_uo_mask_combine(BD_UO_MASK a, BD_UO_MASK b)
{
    BD_UO_MASK o;
    uint8_t   *pa = (uint8_t*)&a;
    uint8_t   *pb = (uint8_t*)&b;
    uint8_t   *po = (uint8_t*)&o;
    unsigned   i;

    for (i = 0; i < sizeof(BD_UO_MASK); i++) {
        po[i] = pa[i] | pb[i];
    }

    return o;
}

#endif // _BD_UO_MASK_TABLE_H_
