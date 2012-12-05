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

#if !defined(_M2TS_DEMUX_H_)
#define _M2TS_DEMUX_H_

#include <util/attributes.h>

#include <stdint.h>

/*
 * simple single-pid demuxer for BDAV m2ts.
 */

struct pes_buffer_s;
typedef struct m2ts_demux_s M2TS_DEMUX;

BD_PRIVATE M2TS_DEMUX *m2ts_demux_init(uint16_t pid);
BD_PRIVATE void        m2ts_demux_free(M2TS_DEMUX **);

/*
 *   Demux aligned unit (mpeg-ts + pes).
 *   input:  aligned unit (6144 bytes). NULL to flush demuxer buffer.
 *   output: PES payload
 *   Flush demuxer internal cache if block == NULL.
 */
BD_PRIVATE struct pes_buffer_s *m2ts_demux(M2TS_DEMUX *, uint8_t *block);


#endif // _M2TS_DEMUX_H_
