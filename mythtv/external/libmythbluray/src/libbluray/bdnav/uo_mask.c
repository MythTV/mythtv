/*
 * This file is part of libbluray
 * Copyright (C) 2009-2010  John Stebbins
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

#include "uo_mask.h"
#include "uo_mask_table.h"

#include "util/bits.h"

#include <string.h>

int
uo_mask_parse(const uint8_t *buf, BD_UO_MASK *uo)
{
    BITBUFFER bb;
    bb_init(&bb, buf, 8);

    memset(uo, 0, sizeof(BD_UO_MASK));

    uo->menu_call                       = bb_read(&bb, 1);
    uo->title_search                    = bb_read(&bb, 1);
    uo->chapter_search                  = bb_read(&bb, 1);
    uo->time_search                     = bb_read(&bb, 1);
    uo->skip_to_next_point              = bb_read(&bb, 1);
    uo->skip_to_prev_point              = bb_read(&bb, 1);
    uo->play_firstplay                  = bb_read(&bb, 1);
    uo->stop                            = bb_read(&bb, 1);
    uo->pause_on                        = bb_read(&bb, 1);
    uo->pause_off                       = bb_read(&bb, 1);
    uo->still_off                       = bb_read(&bb, 1);
    uo->forward                         = bb_read(&bb, 1);
    uo->backward                        = bb_read(&bb, 1);
    uo->resume                          = bb_read(&bb, 1);
    uo->move_up                         = bb_read(&bb, 1);
    uo->move_down                       = bb_read(&bb, 1);
    uo->move_left                       = bb_read(&bb, 1);
    uo->move_right                      = bb_read(&bb, 1);
    uo->select                          = bb_read(&bb, 1);
    uo->activate                        = bb_read(&bb, 1);
    uo->select_and_activate             = bb_read(&bb, 1);
    uo->primary_audio_change            = bb_read(&bb, 1);
    bb_skip(&bb, 1);
    uo->angle_change                    = bb_read(&bb, 1);
    uo->popup_on                        = bb_read(&bb, 1);
    uo->popup_off                       = bb_read(&bb, 1);
    uo->pg_enable_disable               = bb_read(&bb, 1);
    uo->pg_change                       = bb_read(&bb, 1);
    uo->secondary_video_enable_disable  = bb_read(&bb, 1);
    uo->secondary_video_change          = bb_read(&bb, 1);
    uo->secondary_audio_enable_disable  = bb_read(&bb, 1);
    uo->secondary_audio_change          = bb_read(&bb, 1);
    bb_skip(&bb, 1);
    uo->pip_pg_change                   = bb_read(&bb, 1);
    bb_skip(&bb, 30);
    return 1;
}

