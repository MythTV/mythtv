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

#include "graphics_processor.h"

#include "ig_decode.h"
#include "pg_decode.h"
#include "pes_buffer.h"
#include "m2ts_demux.h"

#include "util/macro.h"
#include "util/logging.h"
#include "util/bits.h"

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define GP_TRACE(...) do {} while (0)


/*
 * segment types
 */

typedef enum {
    PGS_PALETTE        = 0x14,
    PGS_OBJECT         = 0x15,
    PGS_PG_COMPOSITION = 0x16,
    PGS_WINDOW         = 0x17,
    PGS_IG_COMPOSITION = 0x18,
    PGS_END_OF_DISPLAY = 0x80,
} pgs_segment_type_e;

/*
 * PG_DISPLAY_SET
 */

void pg_display_set_free(PG_DISPLAY_SET **s)
{
    if (s && *s) {
        unsigned ii;
        for (ii = 0; ii < (*s)->num_object; ii++) {
            pg_clean_object(&(*s)->object[ii]);
        }
        ig_free_interactive(&(*s)->ics);

        X_FREE((*s)->window);
        X_FREE((*s)->object);
        X_FREE((*s)->palette);

        X_FREE(*s);
    }
}

/*
 * segment handling
 */

static PES_BUFFER *_find_segment_by_idv(PES_BUFFER *p,
                                        uint8_t seg_type, unsigned idv_pos,
                                        uint8_t *idv, unsigned idv_len)
{
    while (p && (p->buf[0] != seg_type || memcmp(p->buf + idv_pos, idv, idv_len))) {
        p = p->next;
    }
    return p;
}

static void _join_fragments(PES_BUFFER *p1, PES_BUFFER *p2, int data_pos)
{
    unsigned new_len = p1->len + p2->len - data_pos;

    if (p1->size < new_len) {
        p1->size = new_len + 1;
        p1->buf  = realloc(p1->buf, p1->size);
    }

    memcpy(p1->buf + p1->len, p2->buf + data_pos, p2->len - data_pos);
    p1->len = new_len;
    p2->len = 0;
}

/* return 1 if segment is ready for decoding, 0 if more data is needed */
static int _join_segment_fragments(struct pes_buffer_s *p)
{
    uint8_t type = p->buf[0];
    unsigned id_pos = 0, id_len = 3, sd_pos = 6, data_pos = 0;

    if (p->len < 3) {
        return 1;
    }

    /* check segment type */

    if (type == PGS_OBJECT) {
        id_pos = 3;
        sd_pos = 6;
        data_pos = 7;
    }
    else if (type == PGS_IG_COMPOSITION) {
        id_pos = 8;
        sd_pos = 11;
        data_pos = 12;
    }
    else {
        return 1;
    }

    /* check sequence descriptor - is segment complete ? */

    BD_PG_SEQUENCE_DESCRIPTOR sd;
    BITBUFFER bb;
    bb_init(&bb, p->buf + sd_pos, 3);
    pg_decode_sequence_descriptor(&bb, &sd);

    if (sd.last_in_seq) {
        return 1;
    }
    if (!sd.first_in_seq) {
        return 1;
    }

    /* find next fragment(s) */

    PES_BUFFER *next;
    while (NULL != (next = _find_segment_by_idv(p->next, p->buf[0], id_pos, p->buf + id_pos, id_len))) {

        bb_init(&bb, next->buf + sd_pos, 3);
        pg_decode_sequence_descriptor(&bb, &sd);

        _join_fragments(p, next, data_pos);

        pes_buffer_remove(&p, next);

        if (sd.last_in_seq) {
            /* set first + last in sequence descriptor */
            p->buf[sd_pos] = 0xff;
            return 1;
        }
    }

    /* do not delay decoding if there are other segments queued (missing fragment ?) */
    return !!p->next;
}

/*
 * segment decoding
 */

static int _decode_wds(PG_DISPLAY_SET *s, BITBUFFER *bb, PES_BUFFER *p)
{
    (void)s;
    (void)bb;
    (void)p;

    BD_DEBUG(DBG_DECODE | DBG_CRIT, "unhandled segment type (PGS_WINDOW)\n");
    return 0;
}

static int _decode_ods(PG_DISPLAY_SET *s, BITBUFFER *bb, PES_BUFFER *p)
{
    /* search for object to be updated */

    if (s->object) {
        BITBUFFER bb_tmp = *bb;
        uint16_t  id     = bb_read(&bb_tmp, 16);
        unsigned  ii;

        for (ii = 0; ii < s->num_object; ii++) {
            if (s->object[ii].id == id) {
                if (pg_decode_object(bb, &s->object[ii])) {
                    s->object[ii].pts = p->pts;
                    return 1;
                }
                return 0;
            }
        }
    }

    /* add and decode new object */

    s->object = realloc(s->object, sizeof(s->object[0]) * (s->num_object + 1));
    memset(&s->object[s->num_object], 0, sizeof(s->object[0]));

    if (pg_decode_object(bb, &s->object[s->num_object])) {
        s->object[s->num_object].pts = p->pts;
        s->num_object++;
        return 1;
    }

    return 0;
}

static int _decode_pds(PG_DISPLAY_SET *s, BITBUFFER *bb, PES_BUFFER *p)
{
    /* search for palette to be updated */

    if (s->palette) {
        BITBUFFER bb_tmp = *bb;
        uint8_t   id     = bb_read(&bb_tmp, 8);
        unsigned  ii;

        for (ii = 0; ii < s->num_palette; ii++) {
            if (s->palette[ii].id == id) {
                int rr;
                if ( (s->ics && s->ics->composition_descriptor.state == 0) ||
                     (s->pcs && s->pcs->composition_descriptor.state == 0)) {
                    /* 8.8.3.1.1 */
                    rr = pg_decode_palette_update(bb, &s->palette[ii]);
                } else {
                    rr = pg_decode_palette(bb, &s->palette[ii]);
                }
                if (rr) {
                    s->palette[ii].pts = p->pts;
                    return 1;
                }
                return 0;
            }
        }
    }

    /* add and decode new palette */

    s->palette = realloc(s->palette, sizeof(s->palette[0]) * (s->num_palette + 1));
    memset(&s->palette[s->num_palette], 0, sizeof(s->palette[0]));

    if (pg_decode_palette(bb, &s->palette[s->num_palette])) {
        s->palette[s->num_palette].pts = p->pts;
        s->num_palette++;
        return 1;
    }

    return 0;
}

static void _check_epoch_start(PG_DISPLAY_SET *s)
{
    if ((s->pcs && s->pcs->composition_descriptor.state == 2) ||
        (s->ics && s->ics->composition_descriptor.state == 2)) {
        /* epoch start, drop all cached data */

        unsigned ii;
        for (ii = 0; ii < s->num_object; ii++) {
            pg_clean_object(&s->object[ii]);
        }

        s->num_palette = 0;
        s->num_window  = 0;
        s->num_object  = 0;

        s->epoch_start = 1;

    } else {
        s->epoch_start = 0;
    }
}

static int _decode_pcs(PG_DISPLAY_SET *s, BITBUFFER *bb, PES_BUFFER *p)
{
    pg_free_composition(&s->pcs);
    s->pcs = calloc(1, sizeof(*s->pcs));

    if (!pg_decode_composition(bb, s->pcs)) {
        pg_free_composition(&s->pcs);
        return 0;
    }

    s->pcs->pts  = p->pts;
    s->valid_pts = p->pts;

    _check_epoch_start(s);

    return 1;
}

static int _decode_ics(PG_DISPLAY_SET *s, BITBUFFER *bb, PES_BUFFER *p)
{
    ig_free_interactive(&s->ics);
    s->ics = calloc(1, sizeof(*s->ics));

    if (!ig_decode_interactive(bb, s->ics)) {
        ig_free_interactive(&s->ics);
        return 0;
    }

    s->ics->pts  = p->pts;
    s->valid_pts = p->pts;

    _check_epoch_start(s);

    return 1;
}

static int _decode_segment(PG_DISPLAY_SET *s, PES_BUFFER *p)
{
    BITBUFFER bb;
    bb_init(&bb, p->buf, p->len);

    uint8_t type   =    bb_read(&bb, 8);
    /*uint16_t len = */ bb_read(&bb, 16);

    switch (type) {
        case PGS_OBJECT:
            return _decode_ods(s, &bb, p);

        case PGS_PALETTE:
            return _decode_pds(s, &bb, p);

        case PGS_WINDOW:
            return _decode_wds(s, &bb, p);

        case PGS_PG_COMPOSITION:
            return _decode_pcs(s, &bb, p);

        case PGS_IG_COMPOSITION:
            return _decode_ics(s, &bb, p);

        case PGS_END_OF_DISPLAY:
            s->complete = 1;
            return 1;

        default:
            BD_DEBUG(DBG_DECODE | DBG_CRIT, "unknown segment type 0x%x\n", type);
            break;
    }

    return 0;
}

/*
 * mpeg-pes interface
 */

int graphics_processor_decode_pes(PG_DISPLAY_SET **s, PES_BUFFER **p, int64_t stc)
{
    if (!s) {
        return 0;
    }

    if (*s == NULL) {
        *s = calloc(1, sizeof(PG_DISPLAY_SET));
    }

    while (*p) {

        /* time to decode next segment ? */
        if (stc >= 0 && (*p)->dts > stc) {
            GP_TRACE("Segment dts > stc (%"PRId64" > %"PRId64" ; diff %"PRId64")\n",
                     (*p)->dts, stc, (*p)->dts - stc);
            return 0;
        }

        /* all fragments present ? */
        if (!_join_segment_fragments(*p)) {
            GP_TRACE("splitted segment not complete, waiting for next fragment\n");
            return 0;
        }

        if ((*p)->len <= 2) {
            BD_DEBUG(DBG_DECODE, "segment too short, skipping (%d bytes)\n", (*p)->len);
            pes_buffer_remove(p, *p);
            continue;
        }

        /* decode segment */

        GP_TRACE("Decoding segment, dts %010"PRId64" pts %010"PRId64" len %d\n",
                 (*p)->dts, (*p)->pts, (*p)->len);

        (*s)->complete = 0;

        _decode_segment(*s, *p);

        pes_buffer_remove(p, *p);

        if ((*s)->complete) {
            return 1;
        }

    }

    return 0;
}

/*
 * mpeg-ts interface
 */

struct graphics_processor_s {
    uint16_t    pid;
    M2TS_DEMUX  *demux;
    PES_BUFFER  *queue;
};

GRAPHICS_PROCESSOR *graphics_processor_init(void)
{
    GRAPHICS_PROCESSOR *p = calloc(1, sizeof(*p));

    return p;
}

void graphics_processor_free(GRAPHICS_PROCESSOR **p)
{
    if (p && *p) {
        m2ts_demux_free(&(*p)->demux);
        pes_buffer_free(&(*p)->queue);

        X_FREE(*p);
    }
}

int graphics_processor_decode_ts(GRAPHICS_PROCESSOR *p,
                                 PG_DISPLAY_SET **s,
                                 uint16_t pid, uint8_t *unit, unsigned num_units,
                                 int64_t stc)
{
    unsigned ii;

    if (pid != p->pid) {
        m2ts_demux_free(&p->demux);
        pes_buffer_free(&p->queue);
    }
    if (!p->demux) {
        p->demux = m2ts_demux_init(pid);
        p->pid   = pid;
    }

    for (ii = 0; ii < num_units; ii++) {
        pes_buffer_append(&p->queue, m2ts_demux(p->demux, unit));
        unit += 6144;
    }

    if (p->queue) {
        return graphics_processor_decode_pes(s, &p->queue, stc);
    }

    return 0;
}
