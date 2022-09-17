/*
 * MPEG-1/2 decoder header
 * Copyright (c) 2007 Aurelien Jacobs <aurel@gnuage.org>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_MPEG12DEC_H
#define AVCODEC_MPEG12DEC_H

#include "get_bits.h"
#include "mpeg12vlc.h"
#include "rl.h"

#define INIT_2D_VLC_RL(rl, static_size, flags)\
{\
    static RL_VLC_ELEM rl_vlc_table[static_size];\
    rl.rl_vlc[0] = rl_vlc_table;\
    ff_init_2d_vlc_rl(&rl, static_size, flags);\
}

void ff_init_2d_vlc_rl(RLTable *rl, unsigned static_size, int flags);

static inline int decode_dc(GetBitContext *gb, int component)
{
    int code, diff;

    if (component == 0) {
        code = get_vlc2(gb, ff_dc_lum_vlc.table, DC_VLC_BITS, 2);
    } else {
        code = get_vlc2(gb, ff_dc_chroma_vlc.table, DC_VLC_BITS, 2);
    }
    if (code == 0) {
        diff = 0;
    } else {
        diff = get_xbits(gb, code);
    }
    return diff;
}

int ff_mpeg1_decode_block_intra(GetBitContext *gb,
                                const uint16_t *quant_matrix,
                                const uint8_t *scantable, int last_dc[3],
                                int16_t *block, int index, int qscale);

#endif /* AVCODEC_MPEG12DEC_H */
