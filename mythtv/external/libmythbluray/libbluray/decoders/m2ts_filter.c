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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "m2ts_filter.h"

#include "hdmv_pids.h"

#include "util/logging.h"
#include "util/macro.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <stdio.h>
#endif

#define M2TS_TRACE(...) BD_DEBUG(DBG_STREAM,__VA_ARGS__)
//#define M2TS_TRACE(...) do {} while(0)

/*
 *
 */

struct m2ts_filter_s
{
    uint16_t *wipe_pid;
    uint16_t *pass_pid;

    int64_t  in_pts;
    int64_t  out_pts;
    uint32_t pat_packets; /* how many packets to search for PAT (seeked pat_packets packets before the actual seek point) */
    uint8_t  pat_seen;
};

M2TS_FILTER *m2ts_filter_init(int64_t in_pts, int64_t out_pts,
                              unsigned num_video, unsigned num_audio,
                              unsigned num_ig, unsigned num_pg)
{
    M2TS_FILTER *p = calloc(1, sizeof(*p));

    if (p) {
        unsigned ii, npid;
        uint16_t *pid;

        p->in_pts   = in_pts;
        p->out_pts  = out_pts;
        p->wipe_pid = calloc(num_audio + num_video + num_ig + num_pg + 1, sizeof(uint16_t));
        p->pass_pid = calloc(num_audio + num_video + num_ig + num_pg + 1, sizeof(uint16_t));
        if (!p->pass_pid || !p->wipe_pid) {
            m2ts_filter_close(&p);
            return NULL;
        }

        pid = (in_pts >= 0) ? p->wipe_pid : p->pass_pid;

        for (ii = 0, npid = 0; ii < num_video; ii++) {
            pid[npid++] = HDMV_PID_VIDEO + ii;
        }
        for (ii = 0; ii < num_audio; ii++) {
            pid[npid++] = HDMV_PID_AUDIO_FIRST + ii;
        }
        for (ii = 0; ii < num_ig; ii++) {
            pid[npid++] = HDMV_PID_IG_FIRST + ii;
        }
        for (ii = 0; ii < num_pg; ii++) {
            pid[npid++] = HDMV_PID_PG_FIRST + ii;
        }
    }

    return p;
}

void m2ts_filter_close(M2TS_FILTER **p)
{
    if (p && *p) {
        X_FREE((*p)->wipe_pid);
        X_FREE((*p)->pass_pid);
        X_FREE(*p);
    }
}

/*
 *
 */

#define DUMPLIST(msg,list)                          \
    {                                               \
        unsigned ii = 0;                            \
        fprintf(stderr, "list " msg " : ");         \
        for (ii = 0; list[ii]; ii++) {              \
            fprintf(stderr, " 0x%04x", list[ii]);   \
        }                                           \
        fprintf(stderr, "\n");                      \
    }

static int _pid_in_list(uint16_t *list, uint16_t pid)
{
    for (; *list && *list <= pid; list++) {
        if (*list == pid) {
          return 1;
        }
    }
    return 0;
}

static void _remove_pid(uint16_t *list, uint16_t pid)
{
    for (; *list && *list != pid; list++) ;

    for (; *list; list++) {
        list[0] = list[1];
    }
}

static void _add_pid(uint16_t *list, uint16_t pid)
{
    for (; *list && *list < pid; list++) ;

    for (; *list; list++) {
        uint16_t tmp = *list;
        *list = pid;
        pid = tmp;
    }
    *list = pid;
}

static int64_t _parse_timestamp(const uint8_t *p)
{
    int64_t ts;
    ts  = ((int64_t)(p[0] & 0x0E)) << 29;
    ts |=  p[1]         << 22;
    ts |= (p[2] & 0xFE) << 14;
    ts |=  p[3]         <<  7;
    ts |= (p[4] & 0xFE) >>  1;
    return ts;
}

static int64_t _es_timestamp(const uint8_t *buf, unsigned len)
{
    if (buf[0] || buf[1] || buf[2] != 1) {
        BD_DEBUG(DBG_DECODE, "invalid BDAV TS\n");
        return -1;
    }

    if (len < 9) {
        BD_DEBUG(DBG_DECODE, "invalid BDAV TS (no payload ?)\n");
        return -1;
    }

    /* Parse PES header */
    unsigned pes_pid = buf[3];
    if (pes_pid != 0xbf) {

        unsigned pts_exists = buf[7] & 0x80;
        if (pts_exists) {
            int64_t pts = _parse_timestamp(buf + 9);

            return pts;
        }
    }

    return -1;
}

void m2ts_filter_seek(M2TS_FILTER *p, uint32_t pat_packets, int64_t in_pts)
{
    M2TS_TRACE("seek notify\n");

    /* move all pids to wipe list */
    uint16_t *pid = p->pass_pid;
    while (*pid) {
        _add_pid(p->wipe_pid, *pid);
        *pid = 0;
        pid++;
    }

    p->in_pts   = in_pts;
    p->pat_seen = 0;
    p->pat_packets = pat_packets;
}

static int _filter_es_pts(M2TS_FILTER *p, const uint8_t *buf, uint16_t pid)
{
    unsigned tp_error       = buf[4+1] & 0x80;
    unsigned payload_exists = buf[4+3] & 0x10;
    int      payload_offset = (buf[4+3] & 0x20) ? buf[4+4] + 5 : 4;

    if (buf[4] != 0x47) {
        BD_DEBUG(DBG_DECODE | DBG_CRIT, "missing sync byte. scrambled data ? Filtering aborted.\n");
        return -1;
    }
    if (tp_error || !payload_exists || payload_offset >= 188) {
        M2TS_TRACE("skipping packet (no payload)\n");
        return 0;
    }

    if (_pid_in_list(p->wipe_pid, pid)) {

        int64_t pts = _es_timestamp(buf + 4 + payload_offset, 188 - payload_offset);
        if (pts >= p->in_pts && (p->out_pts < 0 || pts <= p->out_pts)) {
            M2TS_TRACE("Pid 0x%04x pts %"PRId64" passed IN timestamp %"PRId64" (pts %"PRId64")\n",
                       pid, pts, p->in_pts, pts);
            _remove_pid(p->wipe_pid, pid);
            _add_pid(p->pass_pid, pid);

        } else {
            M2TS_TRACE("Pid 0x%04x pts %"PRId64" outside of clip (%"PRId64"-%"PRId64" -> keep wiping out\n",
                       pid, pts, p->in_pts, p->out_pts);
        }
    }
    if (p->out_pts >= 0) {
        /*
         * Note: we can't compare against in_pts here (after passing it once):
         * PG / IG streams can have timestamps before in_time (except for composition segments), and those are valid.
         */
        if (_pid_in_list(p->pass_pid, pid)) {

            int64_t pts = _es_timestamp(buf + 4 + payload_offset, 188 - payload_offset);
            if (pts >= p->out_pts) {
                /*
                 * audio/video streams are cutted after out_time (unit with pts==out_time is included in the clip).
                 * PG/IG streams are cutted before out_time (unit with pts==out_time is dropped out).
                 */
                if (pts > p->out_pts ||
                    IS_HDMV_PID_PG(pid) ||
                    IS_HDMV_PID_IG(pid)) {
                M2TS_TRACE("Pid 0x%04x passed OUT timestamp %"PRId64" (pts %"PRId64") -> start wiping\n", pid, p->out_pts, pts);
                _remove_pid(p->pass_pid, pid);
                _add_pid(p->wipe_pid, pid);
                }
            }
        }
    }

    return 0;
}

static void _wipe_packet(uint8_t *p)
{
    /* set pid to 0x1fff (padding) */
    p[4 + 2] = 0xff;
    p[4 + 1] |= 0x1f;
}

int m2ts_filter(M2TS_FILTER *p, uint8_t *buf)
{
    uint8_t *end    = buf + 6144;
    int      result = 0;

    for (; buf < end; buf += 192) {

        uint16_t pid = ((buf[4+1] & 0x1f) << 8) | buf[4+2];
        if (pid == HDMV_PID_PAT) {
            p->pat_seen = 1;
            p->pat_packets = 0;
            continue;
        }
        if (p->pat_packets) {
            p->pat_packets--;
            if (!p->pat_seen) {
                M2TS_TRACE("Wiping pid 0x%04x (inside seek buffer, no PAT)\n", pid);
                _wipe_packet(buf);
                continue;
            }
            M2TS_TRACE("NOT Wiping pid 0x%04x (inside seek buffer, PAT seen)\n", pid);
        }
        if (pid < HDMV_PID_VIDEO) {
            /* pass PMT, PCR, SIT */
            /*M2TS_TRACE("NOT Wiping pid 0x%04x (< 0x1011)\n", pid);*/
            continue;
        }
#if 0
        /* no PAT yet ? */
        if (!p->pat_seen) {
            /* Wipe packet (pid -> padding stream) */
            M2TS_TRACE("Wiping pid 0x%04x before PAT\n", pid);
            _wipe_packet(buf);
            continue;
        }
#endif
        /* payload start indicator ? check ES timestamp */
        unsigned pusi = buf[4+1] & 0x40;
        if (pusi) {
            if (_filter_es_pts(p, buf, pid) < 0)
                return -1;
        }

        if (_pid_in_list(p->wipe_pid, pid)) {
            /* Wipe packet (pid -> padding stream) */
            M2TS_TRACE("Wiping pid 0x%04x\n", pid);
            _wipe_packet(buf);
        }
    }

    /*result = !!(p->after ? p->wipe_pid[0] : p->pass_pid[0]);*/

    return result;
}
