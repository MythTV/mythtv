/*
 * ProRes RAW decoder
 *
 * Copyright (c) 2025 Lynne <dev@lynne.ee>
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

#pragma shader_stage(compute)
#extension GL_GOOGLE_include_directive : require

#define GET_BITS_SMEM 4
#include "common.glsl"

struct TileData {
   ivec2 pos;
   uint offset;
   uint size;
   uint log2_nb_blocks;
};

layout (set = 0, binding = 0, r16ui) uniform writeonly uimage2D dst;
layout (set = 0, binding = 1, scalar) readonly buffer frame_data_buf {
    TileData tile_data[];
};

layout (push_constant, scalar) uniform pushConstants {
   u8buf pkt_data;
};

#define COMP_ID (gl_LocalInvocationID.y)

GetBitContext gb;

#define DC_CB_MAX 12
const uint8_t dc_cb[DC_CB_MAX + 1] = {
    U8(16), U8(33), U8(50), U8(51), U8(51), U8(51),
    U8(68), U8(68), U8(68), U8(68), U8(68), U8(68), U8(118)
};

#define AC_CB_MAX 94
const int16_t ac_cb[AC_CB_MAX + 1] = {
    I16(  0), I16(529), I16(273), I16(273), I16(546), I16(546),
    I16(546), I16(290), I16(290), I16(290), I16(563), I16(563),
    I16(563), I16(563), I16(563), I16(563), I16(563), I16(563),
    I16(307), I16(307), I16(580), I16(580), I16(580), I16(580),
    I16(580), I16(580), I16(580), I16(580), I16(580), I16(580),
    I16(580), I16(580), I16(580), I16(580), I16(580), I16(580),
    I16(580), I16(580), I16(580), I16(580), I16(580), I16(580),
    I16(853), I16(853), I16(853), I16(853), I16(853), I16(853),
    I16(853), I16(853), I16(853), I16(853), I16(853), I16(853),
    I16(853), I16(853), I16(853), I16(853), I16(853), I16(853),
    I16(853), I16(853), I16(853), I16(853), I16(853), I16(853),
    I16(853), I16(853), I16(853), I16(853), I16(853), I16(853),
    I16(853), I16(853), I16(853), I16(853), I16(853), I16(853),
    I16(853), I16(853), I16(853), I16(853), I16(853), I16(853),
    I16(853), I16(853), I16(853), I16(853), I16(853), I16(853),
    I16(853), I16(853), I16(853), I16(853), I16(358)
};

#define RN_CB_MAX 27
const int16_t rn_cb[RN_CB_MAX + 1] = {
    I16(512), I16(256), I16(  0), I16(  0), I16(529), I16(529), I16(273),
    I16(273), I16( 17), I16( 17), I16( 33), I16( 33), I16(546), I16( 34),
    I16( 34), I16( 34), I16( 34), I16( 34), I16( 34), I16( 34), I16( 34),
    I16( 34), I16( 34), I16( 34), I16( 34), I16( 50), I16( 50), I16( 68),
};

#define LN_CB_MAX 14
const int16_t ln_cb[LN_CB_MAX + 1] = {
    I16( 256), I16( 273), I16( 546), I16( 546), I16( 290), I16( 290), I16( 1075),
    I16(1075), I16( 563), I16( 563), I16( 563), I16( 563), I16( 563), I16( 563),
    I16( 51)
};

int get_value(int16_t codebook)
{
    const int switch_bits = int(codebook >> 8);
    const int rice_order  = int(codebook & I16(0xf));
    const int exp_order   = int((codebook >> 4) & I16(0xf));

    uint32_t b = show_bits(gb, 32);
    if (expectEXT(b == 0, false))
        return 0;
    int q = 31 - findMSB(b);

    if (q <= switch_bits) {
        skip_bits_unchecked(gb, q + rice_order + 1);
        return int((q << rice_order) +
                   (((b << (q + 1)) >> 1) >> (31 - rice_order)));
    }

    int bits = exp_order + (q << 1) - switch_bits;
    skip_bits(gb, bits);
    return int((b >> (32 - bits)) +
               ((switch_bits + 1) << rice_order) -
               (1 << exp_order));
}

#define TODCCODEBOOK(x) ((x + 1) >> 1)

void store_val(ivec2 offs, int blk, int c, int16_t v)
{
    imageStore(dst, offs + 2*ivec2(blk*8 + (c & 7), c >> 3),
               ivec4(v & 0xFFFF));
}

void read_dc_vals(ivec2 offs, int nb_blocks)
{
    int dc;
    int dc_add;
    int prev_dc = 0;
    int sign = 0;

    /* Special handling for first block */
    dc = get_value(I16(700));
    prev_dc = (dc >> 1) ^ -(dc & 1);
    store_val(offs, 0, 0, I16(prev_dc));

    for (int n = 1; n < nb_blocks; n++) {
        if (expectEXT(left_bits(gb) <= 0, false))
            break;

        int16_t dc_codebook;
        if ((n & 15) == 1)
            dc_codebook = I16(100);
        else
            dc_codebook = I16(dc_cb[min(TODCCODEBOOK(dc), 13 - 1)]);

        dc = get_value(dc_codebook);

        sign ^= dc & 1;
        dc_add = (-sign ^ TODCCODEBOOK(dc)) + sign;
        sign = int(dc_add < 0);
        prev_dc += dc_add;

        store_val(offs, n, 0, I16(prev_dc));
    }
}

void read_ac_vals(ivec2 offs, int nb_blocks)
{
    const int nb_codes = nb_blocks << 6;
    const int log2_nb_blocks = findMSB(nb_blocks);
    const int block_mask = (1 << log2_nb_blocks) - 1;

    int ac, rn, ln;
    int16_t ac_codebook = I16(49);
    int16_t rn_codebook = I16( 0);
    int16_t ln_codebook = I16(66);
    int sign;
    int16_t val;

    for (int n = nb_blocks; n <= nb_codes;) {
        if (expectEXT(left_bits(gb) <= 0, false))
            break;

        ln = get_value(ln_codebook);
        int loop_end = min(ln, nb_codes - n);
        for (int i = 0; i < loop_end; i++) {
            if (expectEXT(left_bits(gb) <= 0, false))
                break;

            ac = get_value(ac_codebook);
            ac_codebook = ac_cb[min(ac, 95 - 1)];
            sign = -int(get_bit(gb));

            val = I16(((ac + 1) ^ sign) - sign);
            store_val(offs, n & block_mask, n >> log2_nb_blocks, val);

            n++;
        }

        if (expectEXT(n >= nb_codes, false))
            break;

        rn = get_value(rn_codebook);
        rn_codebook = rn_cb[min(rn, 28 - 1)];

        n += rn + 1;
        if (expectEXT(n >= nb_codes, false))
            break;

        if (expectEXT(left_bits(gb) <= 0, false))
            break;

        ac = get_value(ac_codebook);
        sign = -int(get_bit(gb));

        val = I16(((ac + 1) ^ sign) - sign);
        store_val(offs, n & block_mask, n >> log2_nb_blocks, val);

        ac_codebook = ac_cb[min(ac, 95 - 1)];
        ln_codebook = ln_cb[min(ac, 15 - 1)];

        n++;
    }
}

void main(void)
{
    const uint tile_idx = gl_WorkGroupID.y*gl_NumWorkGroups.x + gl_WorkGroupID.x;
    TileData td = tile_data[tile_idx];

    uint64_t pkt_offset = uint64_t(pkt_data) + td.offset;
    u8vec2buf hdr_data = u8vec2buf(pkt_offset);
    int header_len = hdr_data[0].v.x >> 3;

    ivec4 size = ivec4(td.size,
                       pack16(hdr_data[2].v.yx),
                       pack16(hdr_data[1].v.yx),
                       pack16(hdr_data[3].v.yx));
    size[0] = size[0] - size[1] - size[2] - size[3] - header_len;
    if (expectEXT(size[0] < 0, false))
        return;

    const ivec2 offs = td.pos + ivec2(COMP_ID & 1, COMP_ID >> 1);
    const int nb_blocks = 1 << td.log2_nb_blocks;

    const ivec4 comp_offset = ivec4(size[2] + size[1] + size[3],
                                    size[2],
                                    0,
                                    size[2] + size[1]);

    init_get_bits(gb, u8buf(pkt_offset + header_len + comp_offset[COMP_ID]),
                  size[COMP_ID]);

    read_dc_vals(offs, nb_blocks);
    read_ac_vals(offs, nb_blocks);
}
