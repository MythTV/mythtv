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

#if !defined(_PG_DECODE_H_)
#define _PG_DECODE_H_

#include "pg.h"

#include <util/attributes.h>
#include <util/bits.h>

/*
 * elements
 */

BD_PRIVATE void pg_decode_video_descriptor(BITBUFFER *bb, BD_PG_VIDEO_DESCRIPTOR *p);
BD_PRIVATE void pg_decode_composition_descriptor(BITBUFFER *bb, BD_PG_COMPOSITION_DESCRIPTOR *p);
BD_PRIVATE void pg_decode_sequence_descriptor(BITBUFFER *bb, BD_PG_SEQUENCE_DESCRIPTOR *p);
BD_PRIVATE void pg_decode_window(BITBUFFER *bb, BD_PG_WINDOW *p);
BD_PRIVATE void pg_decode_composition_object(BITBUFFER *bb, BD_PG_COMPOSITION_OBJECT *p);

/*
 * segments
 */

BD_PRIVATE int pg_decode_palette_update(BITBUFFER *bb, BD_PG_PALETTE *p);
BD_PRIVATE int pg_decode_palette(BITBUFFER *bb, BD_PG_PALETTE *p);
BD_PRIVATE int pg_decode_object(BITBUFFER *bb, BD_PG_OBJECT *p);
BD_PRIVATE int pg_decode_composition(BITBUFFER *bb, BD_PG_COMPOSITION *p);
BD_PRIVATE int pg_decode_windows(BITBUFFER *bb, BD_PG_WINDOWS *p);

/*
 * cleanup
 */

BD_PRIVATE void pg_clean_object(BD_PG_OBJECT *p);
BD_PRIVATE void pg_clean_composition(BD_PG_COMPOSITION *p);
BD_PRIVATE void pg_clean_windows(BD_PG_WINDOWS *p);

BD_PRIVATE void pg_free_palette(BD_PG_PALETTE **p);
BD_PRIVATE void pg_free_object(BD_PG_OBJECT **p);
BD_PRIVATE void pg_free_composition(BD_PG_COMPOSITION **p);
BD_PRIVATE void pg_free_windows(BD_PG_WINDOWS **p);

#endif // _PG_DECODE_H_
