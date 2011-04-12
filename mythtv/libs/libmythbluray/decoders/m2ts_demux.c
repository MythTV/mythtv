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

#include "m2ts_demux.h"
#include "pes_buffer.h"

#include "util/logging.h"
#include "util/macro.h"

#include <stdlib.h>
#include <string.h>

/*#define M2TS_TRACE(...) BD_DEBUG(DBG_CRIT,__VA_ARGS__)*/
#define M2TS_TRACE(...) do {} while(0)

/*
 *
 */

struct m2ts_demux_s
{
    uint16_t    pid;
    uint32_t    pes_length;
    PES_BUFFER *buf;
};

M2TS_DEMUX *m2ts_demux_init(uint16_t pid)
{
    M2TS_DEMUX *p = calloc(1, sizeof(*p));

    if (p) {
        p->pid = pid;
    }

    return p;
}

void m2ts_demux_free(M2TS_DEMUX **p)
{
    if (p && *p) {
        pes_buffer_free(&(*p)->buf);
        X_FREE(*p);
    }
}

static int64_t _parse_timestamp(uint8_t *p)
{
    int64_t ts;
    ts  = ((int64_t)(p[0] & 0x0E)) << 29;
    ts |=  p[1]         << 22;
    ts |= (p[2] & 0xFE) << 14;
    ts |=  p[3]         <<  7;
    ts |= (p[4] & 0xFE) >>  1;
    return ts;
}

/*
 * _add_ts()
 * - add ts payload to buffer.
 * - parse PES header if pusi is set.
 * - return:
 *   < 0   error (incorrect PES header)
 *   = 0   PES packet continue
 *   > 0   PES packet payload length from PES header
 */
static int _add_ts(PES_BUFFER *p, unsigned pusi, uint8_t *buf, unsigned len)
{
    int result = 0;

    if (pusi) {
        // Parse PES header
        unsigned pes_length = buf[4] << 8 | buf[5];
        unsigned pts_exists = buf[7] & 0x80;
        unsigned dts_exists = buf[7] & 0x40;
        unsigned hdr_len    = buf[8] + 9;

        if (buf[0] || buf[1] || buf[2] != 1) {
            BD_DEBUG(DBG_DECODE, "invalid PES header (00 00 01)");
            return -1;
        }

        if (len < hdr_len) {
            BD_DEBUG(DBG_DECODE, "invalid BDAV TS (PES header not in single TS packet)\n");
            return -1;
        }

        if (pts_exists) {
            p->pts = _parse_timestamp(buf + 9);
        }
        if (dts_exists) {
            p->dts = _parse_timestamp(buf + 14);
        }

        buf += hdr_len;
        len -= hdr_len;

        result = pes_length + 6 - hdr_len;
    }

    // realloc
    if (p->size < p->len + len) {
        p->size *= 2;
        p->buf   = realloc(p->buf, p->size);
    }

    // append
    memcpy(p->buf + p->len, buf, len);
    p->len += len;

    return result;
}

PES_BUFFER *m2ts_demux(M2TS_DEMUX *p, uint8_t *buf)
{
    uint8_t   *end = buf + 6144;
    PES_BUFFER *result = NULL;

    if (!buf) {
        // flush
        result = p->buf;
        p->buf = NULL;
        return result;
    }

    for (; buf < end; buf += 192) {

        unsigned tp_error       = buf[4+1] & 0x80;
        unsigned pusi           = buf[4+1] & 0x40;
        uint16_t pid            = ((buf[4+1] & 0x1f) << 8) | buf[4+2];
        unsigned payload_exists = buf[4+3] & 0x10;
        int      payload_offset = (buf[4+3] & 0x20) ? buf[4+4] + 5 : 4;

        if (buf[4] != 0x47) {
            BD_DEBUG(DBG_DECODE, "missing sync byte. scrambled data ?\n");
            return NULL;
        }
        if (pid != p->pid) {
            M2TS_TRACE("skipping packet (pid %d)\n", pid);
            continue;
        }
        if (tp_error) {
            BD_DEBUG(DBG_DECODE, "skipping packet (transport error)\n");
            continue;
        }
        if (!payload_exists) {
            M2TS_TRACE("skipping packet (no payload)\n");
            continue;
        }
        if (payload_offset >= 188) {
            BD_DEBUG(DBG_DECODE, "skipping packet (invalid payload start address)\n");
            continue;
        }

        if (pusi) {
            if (p->buf) {
                BD_DEBUG(DBG_DECODE, "PES length mismatch: have %d, expected %d\n",
                      p->buf->len, p->pes_length);
                pes_buffer_free(&p->buf);
            }
            p->buf = pes_buffer_alloc(0xffff);
        }

        if (!p->buf) {
            BD_DEBUG(DBG_DECODE, "skipping packet (no pusi seen)\n");
            continue;
        }

        int r = _add_ts(p->buf, pusi, buf + 4 + payload_offset, 188 - payload_offset);
        if (r) {
            if (r < 0) {
                BD_DEBUG(DBG_DECODE, "skipping block (PES header error)\n");
                pes_buffer_free(&p->buf);
                continue;
            }
            p->pes_length = r;
        }

        if (p->buf->len == p->pes_length) {
            M2TS_TRACE("PES complete (%d bytes)\n", p->pes_length);
            pes_buffer_append(&result, p->buf);
            p->buf = NULL;
        }
    }

    return result;
}
