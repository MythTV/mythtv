/*
 * This file is part of libbluray
 * Copyright (C) 2010  hpi1
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

#if !defined(_GRAPHICS_PROCESSOR_H_)
#define _GRAPHICS_PROCESSOR_H_

#include "ig.h"

#include <util/attributes.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct graphics_processor_s GRAPHICS_PROCESSOR;
struct pes_buffer_s;

/*
 * PG_DISPLAY_SET
 */

typedef struct {
    int64_t       valid_pts;
    uint8_t       complete;     /* set complete: last decoded segment was END_OF_DISPLAY */
    uint8_t       epoch_start;

    unsigned      num_palette;
    unsigned      num_object;
    unsigned      num_window;

    BD_PG_PALETTE *palette;
    BD_PG_OBJECT  *object;
    BD_PG_WINDOW  *window;

    /* only one of the following segments can be present */
    BD_IG_INTERACTIVE   *ics;
    /*BD_PG_PRESENTATON_COMPOSITION_SEGMENT  *pcs;*/

} PG_DISPLAY_SET;

BD_PRIVATE void pg_display_set_free(PG_DISPLAY_SET **s);

/*
 * graphics processor
 */

BD_PRIVATE GRAPHICS_PROCESSOR *graphics_processor_init(void);
BD_PRIVATE void                graphics_processor_free(GRAPHICS_PROCESSOR **p);

/*
 *  stc:      current STC
 *
 *  return:   0 : wait for more data
 *            1 : display set complete
 *
 *  Only segments where DTS <= STC are decoded.
 *  If STC < 0, all segments are immediately decoded to display set.
 *
 *  All decoded PES packets are removed from buffer.
 */

BD_PRIVATE int
graphics_processor_decode_pes(PG_DISPLAY_SET **s,
                              struct pes_buffer_s **buf,
                              int64_t stc);

BD_PRIVATE int
graphics_processor_decode_ts(GRAPHICS_PROCESSOR *p,
                             PG_DISPLAY_SET **s,
                             uint16_t pid, uint8_t *unit, unsigned num_units,
                             int64_t stc);

#ifdef __cplusplus
};
#endif

#endif // _GRAPHICS_PROCESSOR_H_
