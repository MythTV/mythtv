/*
 * This file is part of libbluray
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#if !defined(_M2TS_FILTER_H_)
#define _M2TS_FILTER_H_

#include "util/attributes.h"

#include <stdint.h>

/*
 * simple timestamp filter for BDAV m2ts.
 * Used to cut stream at specific timestamps.
 */

typedef struct m2ts_filter_s M2TS_FILTER;

BD_PRIVATE M2TS_FILTER *m2ts_filter_init(int64_t in_pts, int64_t out_pts,
                                         unsigned num_video, unsigned num_audio,
                                         unsigned num_ig, unsigned num_pg);
BD_PRIVATE void         m2ts_filter_close(M2TS_FILTER **);

/*
 *   Filter aligned unit (mpeg-ts + pes).
 *   - drop incomplete PES packets at start of clip
 *   - drop PES packets where timestamp is outside of clip range
 *
 *   - drop packets before PAT in seek buffer
 *
 */
BD_PRIVATE int m2ts_filter(M2TS_FILTER *, uint8_t *block);

/*
 * Notify seek. All streams are discarded until next PUSI.
 * Wait at most pat_packets for first PAT.
 */
BD_PRIVATE void  m2ts_filter_seek(M2TS_FILTER *, uint32_t pat_packets, int64_t in_pts);


#endif // _M2TS_FILTER_H_
