/*
 * This file is part of libbluray
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

#if !defined(_HDMV_PIDS_H_)
#define _HDMV_PIDS_H_

/*
 * HDMV transport stream PIDs
 */

#define HDMV_PID_PAT              0
#define HDMV_PID_PMT              0x0100
#define HDMV_PID_PCR              0x1001

/* primary streams */

#define HDMV_PID_VIDEO            0x1011
#define HDMV_PID_VIDEO_SS         0x1012

#define HDMV_PID_AUDIO_FIRST      0x1100
#define HDMV_PID_AUDIO_LAST       0x111f

/* graphics streams */

#define HDMV_PID_PG_FIRST         0x1200
#define HDMV_PID_PG_LAST          0x121f

#define HDMV_PID_PG_B_FIRST       0x1220  /* base view */
#define HDMV_PID_PG_B_LAST        0x123f
#define HDMV_PID_PG_E_FIRST       0x1240  /* enhanced view */
#define HDMV_PID_PG_E_LAST        0x125f

#define HDMV_PID_IG_FIRST         0x1400
#define HDMV_PID_IG_LAST          0x141f

#define HDMV_PID_TEXTST           0x1800

/* secondary streams */

#define HDMV_PID_SEC_AUDIO_FIRST  0x1a00
#define HDMV_PID_SEC_AUDIO_LAST   0x1a1f

#define HDMV_PID_SEC_VIDEO_FIRST  0x1b00
#define HDMV_PID_SEC_VIDEO_LAST   0x1b1f

/*
 *
 */

#define IS_HDMV_PID_PG(pid)     ((pid) >= HDMV_PID_PG_FIRST && (pid) <= HDMV_PID_PG_LAST)
#define IS_HDMV_PID_IG(pid)     ((pid) >= HDMV_PID_IG_FIRST && (pid) <= HDMV_PID_IG_LAST)
#define IS_HDMV_PID_TEXTST(pid) ((pid) == HDMV_PID_TEXTST)

/*
 * Extract PID from HDMV MPEG-TS packet
 */

#define TS_PID(buf)                             \
  ((((buf)[4+1] & 0x1f) << 8) | (buf)[4+2])


#endif // _HDMV_PIDS_H_
