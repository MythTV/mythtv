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

#include "util/macro.h"
#include "file/file.h"
#include "util/bits.h"
#include "extdata_parse.h"
#include "clpi_parse.h"

#include <stdlib.h>
#include <string.h>

#define CLPI_SIG1  ('H' << 24 | 'D' << 16 | 'M' << 8 | 'V')
#define CLPI_SIG2A ('0' << 24 | '2' << 16 | '0' << 8 | '0')
#define CLPI_SIG2B ('0' << 24 | '1' << 16 | '0' << 8 | '0')

static int clpi_verbose = 0;

static void
_human_readable_sig(char *sig, uint32_t s1, uint32_t s2)
{
    sig[0] = (s1 >> 24) & 0xFF;
    sig[1] = (s1 >> 16) & 0xFF;
    sig[2] = (s1 >>  8) & 0xFF;
    sig[3] = (s1      ) & 0xFF;
    sig[4] = (s2 >> 24) & 0xFF;
    sig[5] = (s2 >> 16) & 0xFF;
    sig[6] = (s2 >>  8) & 0xFF;
    sig[7] = (s2      ) & 0xFF;
    sig[8] = 0;
}

static int
_parse_stream_attr(BITSTREAM *bits, CLPI_PROG_STREAM *ss)
{
    int len, pos;

    if (!bs_is_align(bits, 0x07)) {
        fprintf(stderr, "_parse_stream_attr: Stream alignment error\n");
    }

    len = bs_read(bits, 8);
    pos = bs_pos(bits) >> 3;

    ss->lang[0] = '\0';
    ss->coding_type = bs_read(bits, 8);
    switch (ss->coding_type) {
        case 0x01:
        case 0x02:
        case 0xea:
        case 0x1b:
        case 0x20:
            ss->format = bs_read(bits, 4);
            ss->rate   = bs_read(bits, 4);
            ss->aspect = bs_read(bits, 4);
            bs_skip(bits, 2);
            ss->oc_flag = bs_read(bits, 1);
            bs_skip(bits, 1);
            break;

        case 0x03:
        case 0x04:
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0xa1:
        case 0xa2:
            ss->format = bs_read(bits, 4);
            ss->rate   = bs_read(bits, 4);
            bs_read_bytes(bits, ss->lang, 3);
            break;

        case 0x90:
        case 0x91:
        case 0xa0:
            bs_read_bytes(bits, ss->lang, 3);
            break;

        case 0x92:
            ss->char_code = bs_read(bits, 8);
            bs_read_bytes(bits, ss->lang, 3);
            break;

        default:
            fprintf(stderr, "stream attr: unrecognized coding type %02x\n", ss->coding_type);
            break;
    };
    ss->lang[3] = '\0';

    // Skip over any padding
    bs_seek_byte(bits, pos + len);
    return 1;
}

static int
_parse_header(BITSTREAM *bits, CLPI_CL *cl)
{
    bs_seek_byte(bits, 0);
    cl->type_indicator  = bs_read(bits, 32);
    cl->type_indicator2 = bs_read(bits, 32);
    if (cl->type_indicator != CLPI_SIG1 || 
        (cl->type_indicator2 != CLPI_SIG2A &&
         cl->type_indicator2 != CLPI_SIG2B)) {

        char sig[9];
        char expect[9];

        _human_readable_sig(sig, cl->type_indicator, cl->type_indicator2);
        _human_readable_sig(expect, CLPI_SIG1, CLPI_SIG2A);
        fprintf(stderr, "failed signature match expected (%s) got (%s)\n", 
                expect, sig);
        return 0;
    }
    cl->sequence_info_start_addr = bs_read(bits, 32);
    cl->program_info_start_addr = bs_read(bits, 32);
    cl->cpi_start_addr = bs_read(bits, 32);
    cl->clip_mark_start_addr = bs_read(bits, 32);
    cl->ext_data_start_addr = bs_read(bits, 32);
    return 1;
}

static int
_parse_clipinfo(BITSTREAM *bits, CLPI_CL *cl)
{
    int len, pos;
    int ii;

    bs_seek_byte(bits, 40);
    // ClipInfo len
    bs_skip(bits, 32);
    // reserved
    bs_skip(bits, 16);
    cl->clip.clip_stream_type = bs_read(bits, 8);
    cl->clip.application_type = bs_read(bits, 8);
    // skip reserved 31 bits
    bs_skip(bits, 31);
    cl->clip.is_atc_delta       = bs_read(bits, 1);
    cl->clip.ts_recording_rate  = bs_read(bits, 32);
    cl->clip.num_source_packets = bs_read(bits, 32);

    // Skip reserved 128 bytes
    bs_skip(bits, 128 * 8);

    // ts type info block
    len = bs_read(bits, 16);
    pos = bs_pos(bits) >> 3;
    if (len) {
        cl->clip.ts_type_info.validity = bs_read(bits, 8);
        bs_read_bytes(bits, cl->clip.ts_type_info.format_id, 4);
        cl->clip.ts_type_info.format_id[4] = '\0';
        // Seek past the stuff we don't know anything about
        bs_seek_byte(bits, pos + len);
    }
    if (cl->clip.is_atc_delta) {
        // Skip reserved bytes
        bs_skip(bits, 8);
        cl->clip.atc_delta_count = bs_read(bits, 8);
        cl->clip.atc_delta = 
            malloc(cl->clip.atc_delta_count * sizeof(CLPI_ATC_DELTA));
        for (ii = 0; ii < cl->clip.atc_delta_count; ii++) {
            cl->clip.atc_delta[ii].delta = bs_read(bits, 32);
            bs_read_bytes(bits, cl->clip.atc_delta[ii].file_id, 5);
            cl->clip.atc_delta[ii].file_id[5] = '\0';
            bs_read_bytes(bits, cl->clip.atc_delta[ii].file_code, 4);
            cl->clip.atc_delta[ii].file_code[4] = '\0';
            bs_skip(bits, 8);
        }
    }
    return 1;
}

static int
_parse_sequence(BITSTREAM *bits, CLPI_CL *cl)
{
    int ii, jj;

    bs_seek_byte(bits, cl->sequence_info_start_addr);

    // Skip the length field, and a reserved byte
    bs_skip(bits, 5 * 8);
    // Then get the number of sequences
    cl->sequence.num_atc_seq = bs_read(bits, 8);

    CLPI_ATC_SEQ *atc_seq;
    atc_seq = malloc(cl->sequence.num_atc_seq * sizeof(CLPI_ATC_SEQ));
    cl->sequence.atc_seq = atc_seq;
    for (ii = 0; ii < cl->sequence.num_atc_seq; ii++) {
        atc_seq[ii].spn_atc_start = bs_read(bits, 32);
        atc_seq[ii].num_stc_seq   = bs_read(bits, 8);
        atc_seq[ii].offset_stc_id = bs_read(bits, 8);

        CLPI_STC_SEQ *stc_seq;
        stc_seq = malloc(atc_seq[ii].num_stc_seq * sizeof(CLPI_STC_SEQ));
        atc_seq[ii].stc_seq = stc_seq;
        for (jj = 0; jj < atc_seq[ii].num_stc_seq; jj++) {
            stc_seq[jj].pcr_pid                 = bs_read(bits, 16);
            stc_seq[jj].spn_stc_start           = bs_read(bits, 32);
            stc_seq[jj].presentation_start_time = bs_read(bits, 32);
            stc_seq[jj].presentation_end_time   = bs_read(bits, 32);
        }
    }
    return 1;
}

static int
_parse_program(BITSTREAM *bits, CLPI_PROG_INFO *program)
{
    int ii, jj;

    // Skip the length field, and a reserved byte
    bs_skip(bits, 5 * 8);
    // Then get the number of sequences
    program->num_prog = bs_read(bits, 8);

    CLPI_PROG *progs;
    progs = malloc(program->num_prog * sizeof(CLPI_PROG));
    program->progs = progs;
    for (ii = 0; ii < program->num_prog; ii++) {
        progs[ii].spn_program_sequence_start = bs_read(bits, 32);
        progs[ii].program_map_pid            = bs_read(bits, 16);
        progs[ii].num_streams                = bs_read(bits, 8);
        progs[ii].num_groups                 = bs_read(bits, 8);

        CLPI_PROG_STREAM *ps;
        ps = malloc(progs[ii].num_streams * sizeof(CLPI_PROG_STREAM));
        progs[ii].streams = ps;
        for (jj = 0; jj < progs[ii].num_streams; jj++) {
            ps[jj].pid = bs_read(bits, 16);
            if (!_parse_stream_attr(bits, &ps[jj])) {
                return 0;
            }
        }
    }
    return 1;
}

static int
_parse_program_info(BITSTREAM *bits, CLPI_CL *cl)
{
    bs_seek_byte(bits, cl->program_info_start_addr);

    return _parse_program(bits, &cl->program);
}

static int
_parse_ep_map_stream(BITSTREAM *bits, CLPI_EP_MAP_ENTRY *ee)
{
    uint32_t          fine_start;
    int               ii;
    CLPI_EP_COARSE   * coarse;
    CLPI_EP_FINE     * fine;

    bs_seek_byte(bits, ee->ep_map_stream_start_addr);
    fine_start = bs_read(bits, 32);

    coarse = malloc(ee->num_ep_coarse * sizeof(CLPI_EP_COARSE));
    ee->coarse = coarse;
    for (ii = 0; ii < ee->num_ep_coarse; ii++) {
        coarse[ii].ref_ep_fine_id = bs_read(bits, 18);
        coarse[ii].pts_ep         = bs_read(bits, 14);
        coarse[ii].spn_ep         = bs_read(bits, 32);
    }

    bs_seek_byte(bits, ee->ep_map_stream_start_addr+fine_start);

    fine = malloc(ee->num_ep_fine * sizeof(CLPI_EP_FINE));
    ee->fine = fine;
    for (ii = 0; ii < ee->num_ep_fine; ii++) {
        fine[ii].is_angle_change_point = bs_read(bits, 1);
        fine[ii].i_end_position_offset = bs_read(bits, 3);
        fine[ii].pts_ep                =  bs_read(bits, 11);
        fine[ii].spn_ep                =  bs_read(bits, 17);
    }
    return 1;
}

static int
_parse_cpi(BITSTREAM *bits, CLPI_CPI *cpi)
{
    int ii;
    uint32_t ep_map_pos, len;

    len = bs_read(bits, 32);
    if (len == 0) {
        return 1;
    }

    bs_skip(bits, 12);
    cpi->type = bs_read(bits, 4);
    ep_map_pos = bs_pos(bits) >> 3;

    // EP Map starts here
    bs_skip(bits, 8);
    cpi->num_stream_pid = bs_read(bits, 8);

    CLPI_EP_MAP_ENTRY *entry;
    entry = malloc(cpi->num_stream_pid * sizeof(CLPI_EP_MAP_ENTRY));
    cpi->entry = entry;
    for (ii = 0; ii < cpi->num_stream_pid; ii++) {
        entry[ii].pid                      = bs_read(bits, 16);
        bs_skip(bits, 10);
        entry[ii].ep_stream_type           = bs_read(bits, 4);
        entry[ii].num_ep_coarse            = bs_read(bits, 16);
        entry[ii].num_ep_fine              = bs_read(bits, 18);
        entry[ii].ep_map_stream_start_addr = bs_read(bits, 32) + ep_map_pos;
    }
    for (ii = 0; ii < cpi->num_stream_pid; ii++) {
        _parse_ep_map_stream(bits, &cpi->entry[ii]);
    }
    return 1;
}

static int
_parse_cpi_info(BITSTREAM *bits, CLPI_CL *cl)
{
    bs_seek_byte(bits, cl->cpi_start_addr);

    return _parse_cpi(bits, &cl->cpi);
}

static uint32_t
_find_stc_spn(const CLPI_CL *cl, uint8_t stc_id)
{
    int ii;
    CLPI_ATC_SEQ *atc;

    for (ii = 0; ii < cl->sequence.num_atc_seq; ii++) {
        atc = &cl->sequence.atc_seq[ii];
        if (stc_id < atc->offset_stc_id + atc->num_stc_seq) {
            return atc->stc_seq[stc_id - atc->offset_stc_id].spn_stc_start;
        }
    }
    return 0;
}

// Looks up the start packet number for the timestamp
// Returns the spn for the entry that is closest to but
// before the given timestamp
uint32_t
clpi_lookup_spn(const CLPI_CL *cl, uint32_t timestamp, int before, uint8_t stc_id)
{
    const CLPI_EP_MAP_ENTRY *entry;
    const CLPI_CPI *cpi = &cl->cpi;
    int ii, jj;
    uint32_t coarse_pts, pts; // 45khz timestamps
    uint32_t spn, coarse_spn, stc_spn;
    int start, end;
    int ref;

    if (cpi->num_stream_pid < 1 || !cpi->entry) {
        if (before) {
            return 0;
        }
        return cl->clip.num_source_packets;
    }

    // Assumes that there is only one pid of interest
    entry = &cpi->entry[0];

    // Use sequence info to find spn_stc_start before doing
    // PTS search. The spn_stc_start defines the point in
    // the EP map to start searching.
    stc_spn = _find_stc_spn(cl, stc_id);
    for (ii = 0; ii < entry->num_ep_coarse; ii++) {
        ref = entry->coarse[ii].ref_ep_fine_id;
        if (entry->coarse[ii].spn_ep >= stc_spn) {
            // The desired starting point is either after this point
            // or in the middle of the previous coarse entry
            break;
        }
    }
    if (ii >= entry->num_ep_coarse) {
        return cl->clip.num_source_packets;
    }
    pts = ((uint64_t)(entry->coarse[ii].pts_ep & ~0x01) << 18) +
          ((uint64_t)entry->fine[ref].pts_ep << 8);
    if (pts > timestamp && ii) {
        // The starting point and desired PTS is in the previous coarse entry
        ii--;
        coarse_pts = (uint32_t)(entry->coarse[ii].pts_ep & ~0x01) << 18;
        coarse_spn = entry->coarse[ii].spn_ep;
        start = entry->coarse[ii].ref_ep_fine_id;
        end = entry->coarse[ii+1].ref_ep_fine_id;
        // Find a fine entry that has bothe spn > stc_spn and ptc > timestamp
        for (jj = start; jj < end; jj++) {

            pts = coarse_pts + ((uint32_t)entry->fine[jj].pts_ep << 8);
            spn = (coarse_spn & ~0x1FFFF) + entry->fine[jj].spn_ep;
            if (stc_spn >= spn && pts > timestamp)
                break;
        }
        goto done;
    }

    // If we've gotten this far, the desired timestamp is somewhere
    // after the coarse entry we found the stc_spn in.
    start = ii;
    for (ii = start; ii < entry->num_ep_coarse; ii++) {
        ref = entry->coarse[ii].ref_ep_fine_id;
        pts = ((uint64_t)(entry->coarse[ii].pts_ep & ~0x01) << 18) +
                ((uint64_t)entry->fine[ref].pts_ep << 8);
        if (pts > timestamp) {
            break;
        }
    }
    // If the timestamp is before the first entry, then return
    // the beginning of the clip
    if (ii == 0) {
        return 0;
    }
    ii--;
    coarse_pts = (uint32_t)(entry->coarse[ii].pts_ep & ~0x01) << 18;
    start = entry->coarse[ii].ref_ep_fine_id;
    if (ii < entry->num_ep_coarse - 1) {
        end = entry->coarse[ii+1].ref_ep_fine_id;
    } else {
        end = entry->num_ep_fine;
    }
    for (jj = start; jj < end; jj++) {

        pts = coarse_pts + ((uint32_t)entry->fine[jj].pts_ep << 8);
        if (pts > timestamp)
            break;
    }

done:
    if (before) {
        jj--;
    }
    if (jj == end) {
        ii++;
        if (ii >= entry->num_ep_coarse) {
            // End of file
            return cl->clip.num_source_packets;
        }
        jj = entry->coarse[ii].ref_ep_fine_id;
    }
    spn = (entry->coarse[ii].spn_ep & ~0x1FFFF) + entry->fine[jj].spn_ep;
    return spn;
}

// Looks up the start packet number that is closest to the requested packet
// Returns the spn for the entry that is closest to but
// before the given packet
uint32_t
clpi_access_point(const CLPI_CL *cl, uint32_t pkt, int next, int angle_change, uint32_t *time)
{
    const CLPI_EP_MAP_ENTRY *entry;
    const CLPI_CPI *cpi = &cl->cpi;
    int ii, jj;
    uint32_t coarse_spn, spn;
    int start, end;
    int ref;

    // Assumes that there is only one pid of interest
    entry = &cpi->entry[0];

    for (ii = 0; ii < entry->num_ep_coarse; ii++) {
        ref = entry->coarse[ii].ref_ep_fine_id;
        spn = (entry->coarse[ii].spn_ep & ~0x1FFFF) + entry->fine[ref].spn_ep;
        if (spn > pkt) {
            break;
        }
    }
    // If the timestamp is before the first entry, then return
    // the beginning of the clip
    if (ii == 0) {
        *time = 0;
        return 0;
    }
    ii--;
    coarse_spn = (entry->coarse[ii].spn_ep & ~0x1FFFF);
    start = entry->coarse[ii].ref_ep_fine_id;
    if (ii < entry->num_ep_coarse - 1) {
        end = entry->coarse[ii+1].ref_ep_fine_id;
    } else {
        end = entry->num_ep_fine;
    }
    for (jj = start; jj < end; jj++) {

        spn = coarse_spn + entry->fine[jj].spn_ep;
        if (spn >= pkt) {
            break;
        }
    }
    if (jj == end && next) {
        ii++;
        jj = 0;
    } else if (spn != pkt && !next) {
        jj--;
    }
    if (ii == entry->num_ep_coarse) {
        *time = 0;
        return cl->clip.num_source_packets;
    }
    coarse_spn = (entry->coarse[ii].spn_ep & ~0x1FFFF);
    if (angle_change) {
        // Keep looking till there's an angle change point
        for (; jj < end; jj++) {

            if (entry->fine[jj].is_angle_change_point) {
                *time = ((uint64_t)(entry->coarse[ii].pts_ep & ~0x01) << 18) +
                        ((uint64_t)entry->fine[jj].pts_ep << 8);
                return coarse_spn + entry->fine[jj].spn_ep;
            }
        }
        for (ii++; ii < entry->num_ep_coarse; ii++) {
            start = entry->coarse[ii].ref_ep_fine_id;
            if (ii < entry->num_ep_coarse - 1) {
                end = entry->coarse[ii+1].ref_ep_fine_id;
            } else {
                end = entry->num_ep_fine;
            }
            for (jj = start; jj < end; jj++) {

                if (entry->fine[jj].is_angle_change_point) {
                    *time = ((uint64_t)(entry->coarse[ii].pts_ep & ~0x01) << 18) +
                            ((uint64_t)entry->fine[jj].pts_ep << 8);
                    return coarse_spn + entry->fine[jj].spn_ep;
                }
            }
        }
        *time = 0;
        return cl->clip.num_source_packets;
    }
    *time = ((uint64_t)(entry->coarse[ii].pts_ep & ~0x01) << 18) +
            ((uint64_t)entry->fine[jj].pts_ep << 8);
    return coarse_spn + entry->fine[jj].spn_ep;
}

static int
_parse_extent_start_points(BITSTREAM *bits, CLPI_EXTENT_START *es)
{
    unsigned int ii;

    bs_skip(bits, 32); // length
    es->num_point = bs_read(bits, 32);

    es->point = malloc(es->num_point * sizeof(uint32_t));

    for (ii = 0; ii < es->num_point; ii++) {
        es->point[ii] = bs_read(bits, 32);
    }

    return 1;
}

static int _parse_clpi_extension(BITSTREAM *bits, int id1, int id2, void *handle)
{
    CLPI_CL *cl = (CLPI_CL*)handle;

    if (id1 == 2) {
        if (id2 == 4) {
            // Extent start point
            return _parse_extent_start_points(bits, &cl->extent_start);
        }
        if (id2 == 5) {
            // ProgramInfo SS
            return _parse_program(bits, &cl->program_ss);
        }
        if (id2 == 6) {
            // CPI SS
            return _parse_cpi(bits, &cl->cpi_ss);
        }
    }

    return 0;
}

static void
_clean_program(CLPI_PROG_INFO *p)
{
    int ii;

    for (ii = 0; ii < p->num_prog; ii++) {
        if (p->progs[ii].streams != NULL) {
            X_FREE(p->progs[ii].streams);
        }
    }
    X_FREE(p->progs);
}

static void
_clean_cpi(CLPI_CPI *cpi)
{
    int ii;

    for (ii = 0; ii < cpi->num_stream_pid; ii++) {
        if (cpi->entry[ii].coarse != NULL) {
            X_FREE(cpi->entry[ii].coarse);
        }
        if (cpi->entry[ii].fine != NULL) {
            X_FREE(cpi->entry[ii].fine);
        }
    }
    X_FREE(cpi->entry);
}

void
clpi_free(CLPI_CL *cl)
{
    int ii;

    if (cl == NULL) {
        return;
    }
    if (cl->clip.atc_delta != NULL) {
        X_FREE(cl->clip.atc_delta);
    }
    for (ii = 0; ii < cl->sequence.num_atc_seq; ii++) {
        if (cl->sequence.atc_seq[ii].stc_seq != NULL) {
            X_FREE(cl->sequence.atc_seq[ii].stc_seq);
        }
    }
    if (cl->sequence.atc_seq != NULL) {
        X_FREE(cl->sequence.atc_seq);
    }

    _clean_program(&cl->program);
    _clean_cpi(&cl->cpi);

    X_FREE(cl->extent_start.point);

    _clean_program(&cl->program_ss);
    _clean_cpi(&cl->cpi_ss);

    X_FREE(cl);
}

static CLPI_CL*
_clpi_parse(const char *path, int verbose)
{
    BITSTREAM  bits;
    BD_FILE_H *fp;
    CLPI_CL   *cl;

    clpi_verbose = verbose;

    cl = calloc(1, sizeof(CLPI_CL));
    if (cl == NULL) {
        return NULL;
    }

    fp = file_open(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open %s\n", path);
        X_FREE(cl);
        return NULL;
    }

    bs_init(&bits, fp);
    if (!_parse_header(&bits, cl)) {
        file_close(fp);
        clpi_free(cl);
        return NULL;
    }

    if (cl->ext_data_start_addr > 0) {
        bdmv_parse_extension_data(&bits,
                                   cl->ext_data_start_addr,
                                   _parse_clpi_extension,
                                   cl);
    }

    if (!_parse_clipinfo(&bits, cl)) {
        file_close(fp);
        clpi_free(cl);
        return NULL;
    }
    if (!_parse_sequence(&bits, cl)) {
        file_close(fp);
        clpi_free(cl);
        return NULL;
    }
    if (!_parse_program_info(&bits, cl)) {
        file_close(fp);
        clpi_free(cl);
        return NULL;
    }
    if (!_parse_cpi_info(&bits, cl)) {
        file_close(fp);
        clpi_free(cl);
        return NULL;
    }
    file_close(fp);
    return cl;
}

CLPI_CL*
clpi_parse(const char *path, int verbose)
{
    CLPI_CL *cl = _clpi_parse(path, verbose);

    /* if failed, try backup file */
    if (!cl) {
        size_t len   = strlen(path);
        char *backup = malloc(len + 8);

        strncpy(backup, path, len - 18);
        strcpy(backup + len - 18, "BACKUP/");
        strcpy(backup + len - 18 + 7, path + len - 18);

        cl = _clpi_parse(backup, verbose);

        X_FREE(backup);
    }

    return cl;
}

CLPI_CL*
clpi_copy(const CLPI_CL* src_cl)
{
    CLPI_CL* dest_cl = NULL;
    int ii, jj;

    if (src_cl) {
        dest_cl = (CLPI_CL*) calloc(1, sizeof(CLPI_CL));

        dest_cl->clip.clip_stream_type = src_cl->clip.clip_stream_type;
        dest_cl->clip.application_type = src_cl->clip.application_type;
        dest_cl->clip.is_atc_delta = src_cl->clip.is_atc_delta;
        dest_cl->clip.atc_delta_count = src_cl->clip.atc_delta_count;
        dest_cl->clip.ts_recording_rate = src_cl->clip.ts_recording_rate;
        dest_cl->clip.num_source_packets = src_cl->clip.num_source_packets;
        dest_cl->clip.ts_type_info.validity = src_cl->clip.ts_type_info.validity;
        memcpy(dest_cl->clip.ts_type_info.format_id, src_cl->clip.ts_type_info.format_id, 5);
        dest_cl->clip.atc_delta = malloc(src_cl->clip.atc_delta_count * sizeof(CLPI_ATC_DELTA));
        for (ii = 0; ii < src_cl->clip.atc_delta_count; ii++) {
            dest_cl->clip.atc_delta[ii].delta =  src_cl->clip.atc_delta[ii].delta;
            memcpy(dest_cl->clip.atc_delta[ii].file_id, src_cl->clip.atc_delta[ii].file_id, 6);
            memcpy(dest_cl->clip.atc_delta[ii].file_code, src_cl->clip.atc_delta[ii].file_code, 5);
        }

        dest_cl->sequence.num_atc_seq = src_cl->sequence.num_atc_seq;
        dest_cl->sequence.atc_seq = malloc(src_cl->sequence.num_atc_seq * sizeof(CLPI_ATC_SEQ));
        for (ii = 0; ii < src_cl->sequence.num_atc_seq; ii++) {
            dest_cl->sequence.atc_seq[ii].spn_atc_start = src_cl->sequence.atc_seq[ii].spn_atc_start;
            dest_cl->sequence.atc_seq[ii].offset_stc_id = src_cl->sequence.atc_seq[ii].offset_stc_id;
            dest_cl->sequence.atc_seq[ii].num_stc_seq = src_cl->sequence.atc_seq[ii].num_stc_seq;
            dest_cl->sequence.atc_seq[ii].stc_seq = malloc(src_cl->sequence.atc_seq[ii].num_stc_seq * sizeof(CLPI_STC_SEQ));
            for (jj = 0; jj < src_cl->sequence.atc_seq[ii].num_stc_seq; jj++) {
                dest_cl->sequence.atc_seq[ii].stc_seq[jj].spn_stc_start = src_cl->sequence.atc_seq[ii].stc_seq[jj].spn_stc_start;
                dest_cl->sequence.atc_seq[ii].stc_seq[jj].pcr_pid = src_cl->sequence.atc_seq[ii].stc_seq[jj].pcr_pid;
                dest_cl->sequence.atc_seq[ii].stc_seq[jj].presentation_start_time = src_cl->sequence.atc_seq[ii].stc_seq[jj].presentation_start_time;
                dest_cl->sequence.atc_seq[ii].stc_seq[jj].presentation_end_time = src_cl->sequence.atc_seq[ii].stc_seq[jj].presentation_end_time;
            }
        }

        dest_cl->program.num_prog = src_cl->program.num_prog;
        dest_cl->program.progs = malloc(src_cl->program.num_prog * sizeof(CLPI_PROG));
        for (ii = 0; ii < src_cl->program.num_prog; ii++) {
            dest_cl->program.progs[ii].spn_program_sequence_start = src_cl->program.progs[ii].spn_program_sequence_start;
            dest_cl->program.progs[ii].program_map_pid = src_cl->program.progs[ii].program_map_pid;
            dest_cl->program.progs[ii].num_streams = src_cl->program.progs[ii].num_streams;
            dest_cl->program.progs[ii].num_groups = src_cl->program.progs[ii].num_groups;
            dest_cl->program.progs[ii].streams = malloc(src_cl->program.progs[ii].num_streams * sizeof(CLPI_PROG_STREAM));
            for (jj = 0; jj < src_cl->program.progs[ii].num_streams; jj++) {
                dest_cl->program.progs[ii].streams[jj].coding_type = src_cl->program.progs[ii].streams[jj].coding_type;
                dest_cl->program.progs[ii].streams[jj].pid = src_cl->program.progs[ii].streams[jj].pid;
                dest_cl->program.progs[ii].streams[jj].format = src_cl->program.progs[ii].streams[jj].format;
                dest_cl->program.progs[ii].streams[jj].rate = src_cl->program.progs[ii].streams[jj].rate;
                dest_cl->program.progs[ii].streams[jj].aspect = src_cl->program.progs[ii].streams[jj].aspect;
                dest_cl->program.progs[ii].streams[jj].oc_flag = src_cl->program.progs[ii].streams[jj].oc_flag;
                dest_cl->program.progs[ii].streams[jj].char_code = src_cl->program.progs[ii].streams[jj].char_code;
                memcpy(dest_cl->program.progs[ii].streams[jj].lang,src_cl->program.progs[ii].streams[jj].lang,4);
            }
        }

        dest_cl->cpi.num_stream_pid = src_cl->cpi.num_stream_pid;
        dest_cl->cpi.entry = malloc(src_cl->cpi.num_stream_pid * sizeof(CLPI_EP_MAP_ENTRY));
        for (ii = 0; ii < dest_cl->cpi.num_stream_pid; ii++) {
            dest_cl->cpi.entry[ii].pid = src_cl->cpi.entry[ii].pid;
            dest_cl->cpi.entry[ii].ep_stream_type = src_cl->cpi.entry[ii].ep_stream_type;
            dest_cl->cpi.entry[ii].num_ep_coarse = src_cl->cpi.entry[ii].num_ep_coarse;
            dest_cl->cpi.entry[ii].num_ep_fine = src_cl->cpi.entry[ii].num_ep_fine;
            dest_cl->cpi.entry[ii].ep_map_stream_start_addr = src_cl->cpi.entry[ii].ep_map_stream_start_addr;
            dest_cl->cpi.entry[ii].coarse = malloc(src_cl->cpi.entry[ii].num_ep_coarse * sizeof(CLPI_EP_COARSE));
            for (jj = 0; jj < src_cl->cpi.entry[ii].num_ep_coarse; jj++) {
                dest_cl->cpi.entry[ii].coarse[jj].ref_ep_fine_id = src_cl->cpi.entry[ii].coarse[jj].ref_ep_fine_id;
                dest_cl->cpi.entry[ii].coarse[jj].pts_ep = src_cl->cpi.entry[ii].coarse[jj].pts_ep;
                dest_cl->cpi.entry[ii].coarse[jj].spn_ep = src_cl->cpi.entry[ii].coarse[jj].spn_ep;
            }
            dest_cl->cpi.entry[ii].fine = malloc(src_cl->cpi.entry[ii].num_ep_fine * sizeof(CLPI_EP_FINE));
            for (jj = 0; jj < src_cl->cpi.entry[ii].num_ep_fine; jj++) {
                dest_cl->cpi.entry[ii].fine[jj].is_angle_change_point = src_cl->cpi.entry[ii].fine[jj].is_angle_change_point;
                dest_cl->cpi.entry[ii].fine[jj].i_end_position_offset = src_cl->cpi.entry[ii].fine[jj].i_end_position_offset;
                dest_cl->cpi.entry[ii].fine[jj].pts_ep = src_cl->cpi.entry[ii].fine[jj].pts_ep;
                dest_cl->cpi.entry[ii].fine[jj].spn_ep = src_cl->cpi.entry[ii].fine[jj].spn_ep;
            }
        }
    }

    return dest_cl;
}
