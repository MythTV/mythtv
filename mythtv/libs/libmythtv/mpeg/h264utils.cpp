// -*- Mode: c++ -*-
/*******************************************************************
 * h264utils
 *
 * Copyright (C) Her Majesty the Queen in Right of Canada, 2006
 * Communications Research Centre (CRC)
 *
 * Distributed as part of MythTV (www.mythtv.org)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Contact:
 * Francois Lefebvre <francois [dot] lefebvre [at] crc [dot] ca>
 * Web: http://www.crc.ca
 *
 * 2006/04 Jean-Francois Roy for CRC
 *    Initial release
 *
 ********************************************************************/

// C headers
#include <cstring>

// MythTV headers
#include "mythverbose.h"
#include "h264utils.h"

extern "C" {
// from libavcodec
extern const uint8_t *ff_find_start_code(const uint8_t * p, const uint8_t *end, uint32_t * state);
#include "avcodec.h"
}

namespace H264
{

KeyframeSequencer::KeyframeSequencer() /* throw() */
{
    Reset();
}

void KeyframeSequencer::Reset(void) /* throw() */
{
    errored = false;
    state_changed = false;

    sync_accumulator = 0xffffffff;
    sync_stream_offset = 0;

    first_NAL_byte = H264::NALUnitType::UNKNOWN;

    log2_max_frame_num = -1;
    frame_num = 0;
    prev_frame_num = -1;

    saw_AU_delimiter = false;
    saw_first_VCL_NAL_unit = false;
    saw_sps = false;
    separate_colour_plane_flag = false;
    frame_mbs_only_flag = true;
    new_VLC_NAL = false;

    did_evaluate_once = false;
    keyframe = false;
    keyframe_sync_stream_offset = 0;
}

void KeyframeSequencer::KeyframePredicate(
    const uint8_t new_first_NAL_byte) /* throw() */
{
    // new_NAL_type will be current after this method executes
    uint8_t new_NAL_type = new_first_NAL_byte & 0x1f;
    uint8_t current_NAL_type = first_NAL_byte & 0x1f;

    // stage 1: if we're starting a new AU, save the offset
    //          in case it's an IDR AU
    if ((saw_first_VCL_NAL_unit || !did_evaluate_once) &&
        !saw_AU_delimiter)
    {
        did_evaluate_once = true;

        // stage 1.1: this particular check follows
        //            ITU-T Rec. H.264 (03/2005)
        if (new_NAL_type     == NALUnitType::AU_DELIMITER ||
            current_NAL_type == NALUnitType::END_SEQUENCE)
        {
            saw_first_VCL_NAL_unit = false;
            saw_AU_delimiter = true;
            keyframe_sync_stream_offset = sync_stream_offset;
        }

        /* stage 1.2 is a hack. The correct method would be to
         * write down when see those types (and others, such as
         * a VCL NAL unit), wait until the next VCL NAL unit,
         * and compare it to the last seen VCL NAL unit according
         * to ITU-T Rec. H.264 (03/2005) 7.4.1.2.4
         */

        // stage 1.2: HACK: this is a fugly guesstimate based on
        //            Figure 7-1 in 7.4.1.2.3
        if (new_NAL_type == NALUnitType::SEI || 
            new_NAL_type == NALUnitType::SPS ||
            new_NAL_type == NALUnitType::PPS || 
            (new_NAL_type > NALUnitType::SPS_EXT &&
             new_NAL_type < NALUnitType::AUXILIARY_SLICE))
        {
            saw_first_VCL_NAL_unit = false;
            saw_AU_delimiter = true;
            keyframe_sync_stream_offset = sync_stream_offset;
        }
    }

    // stage 2: determine if it's an IDR AU
    if (!saw_first_VCL_NAL_unit && !saw_sps && new_NAL_type == NALUnitType::SPS)
    {
        saw_sps = true;
        state_changed = true;
        keyframe = false;
    }

    // stage 3: did we see the AU's first VCL NAL unit yet?
    if (!saw_first_VCL_NAL_unit && NALUnitType::IsVCLType(new_NAL_type))
    {
        saw_first_VCL_NAL_unit = new_VLC_NAL;
        saw_AU_delimiter = false;
        state_changed = true;
        if (saw_sps)
            keyframe = true;
        saw_sps = false;
    }
}

uint32_t KeyframeSequencer::AddBytes(
    const uint8_t  *bytes,
    const uint32_t  byte_count,
    const int64_t   stream_offset) /* throw() */
{
    const uint8_t *local_bytes = bytes;
    const uint8_t *local_bytes_end = bytes + byte_count;

    state_changed = false;

    while (local_bytes < local_bytes_end)
    {
        local_bytes = ff_find_start_code(local_bytes, local_bytes_end,
                                         &sync_accumulator);

        if ((sync_accumulator & 0xffffff00) == 0x00000100)
        {
            uint8_t k = *(local_bytes-1);
            uint8_t NAL_type = k & 0x1f;
            sync_stream_offset = stream_offset;
            keyframe = false;

            if (NAL_type == NALUnitType::SPS ||
                NAL_type == NALUnitType::SLICE ||
                NAL_type == NALUnitType::SLICE_DPA)
            {
                /*
                  bitstream buffer, must be FF_INPUT_BUFFER_PADDING_SIZE
                  bytes larger then the actual read bits
                */
                if (local_bytes + 20 + FF_INPUT_BUFFER_PADDING_SIZE <
                    local_bytes_end)
                {
                    init_get_bits(&gb, local_bytes,
                                  8 * (local_bytes_end - local_bytes));

                    if (NAL_type == NALUnitType::SPS)
                        decode_SPS(&gb);
                    else
                        decode_Header(&gb);
                }
            }
            else if (NAL_type == NALUnitType::SLICE_IDR)
            {
                frame_num = 0;
            }

            KeyframePredicate(k);
            first_NAL_byte = k;

            return local_bytes - bytes;
        }
    }

    return local_bytes - bytes;
}

bool KeyframeSequencer::IsOnFrame(void) const /* throw() */
{
    // HACK: This could be improved, but it does work if you're
    //       interested in knowing when you just hit a frame
    //       (as opposed to still being on one).
    return saw_first_VCL_NAL_unit;
}

void KeyframeSequencer::decode_Header(GetBitContext *gb)
{
    uint first_mb_in_slice;
    uint slice_type;
    uint pic_parameter_set_id;
    bool field_pic_flag;
    bool bottom_field_flag;

    if (log2_max_frame_num < 1)
    {
        VERBOSE(VB_IMPORTANT, "KeyframeSequencer::decode_Header: "
                "SPS has not been parsed!");
        return;
    }

    new_VLC_NAL = false;

    prev_frame_num = frame_num;

    first_mb_in_slice = get_ue_golomb(gb);
    slice_type = get_ue_golomb(gb);

    pic_parameter_set_id = get_ue_golomb(gb);
    if (pic_parameter_set_id != prev_pic_parameter_set_id)
    {
        new_VLC_NAL = true;
        prev_pic_parameter_set_id = pic_parameter_set_id;
    }
    
    if (separate_colour_plane_flag)
        get_bits(gb, 2);  // colour_plane_id
    
    frame_num = get_bits(gb, log2_max_frame_num);

    if (frame_mbs_only_flag)
    {
        new_VLC_NAL = true;
    }
    else
    {
        /* From section 7.3.3 Slice header syntax
           if (!frame_mbs_only_flag)
           {
               field_pic_flag = get_bits1(gb);
               if (field_pic_flag)
                   bottom_field_flag = get_bits1(gb);
           }
        */

        field_pic_flag = get_bits1(gb);
        if (field_pic_flag != prev_field_pic_flag)
        {
            new_VLC_NAL = true;
            prev_field_pic_flag = field_pic_flag;
        }

        if (field_pic_flag)
        {
            bottom_field_flag = get_bits1(gb);
            if (bottom_field_flag != prev_bottom_field_flag)
            {
                new_VLC_NAL = !bottom_field_flag;
                prev_bottom_field_flag = bottom_field_flag;
            }
        }
        else
            new_VLC_NAL = true; 
    }
}

/*
 * libavcodec used for example
 */
void KeyframeSequencer::decode_SPS(GetBitContext * gb)
{
    int profile_idc, chroma_format_idc;

    profile_idc = get_bits(gb, 8); // profile_idc
    get_bits1(gb);   // constraint_set0_flag
    get_bits1(gb);   // constraint_set1_flag
    get_bits1(gb);   // constraint_set2_flag
    get_bits1(gb);   // constraint_set3_flag
    get_bits(gb, 4); // reserved
    get_bits(gb, 8); // level_idc
    get_ue_golomb(gb);  // sps_id

    if (profile_idc >= 100)
    { // high profile
        if ((chroma_format_idc = get_ue_golomb(gb)) == 3) // chroma_format_idc
            separate_colour_plane_flag = (get_bits1(gb) == 1);

        get_ue_golomb(gb);  // bit_depth_luma_minus8
        get_ue_golomb(gb);  // bit_depth_chroma_minus8
        get_bits1(gb);      // qpprime_y_zero_transform_bypass_flag

        if (get_bits1(gb))      // seq_scaling_matrix_present_flag
        {
            for (int idx = 0; idx < ((chroma_format_idc != 3) ? 8 : 12); ++idx)
            {
                get_bits1(gb);  // scaling_list
            }
        }
    }

    log2_max_frame_num = get_ue_golomb(gb) + 4;

    uint pic_order_cnt_type;
    uint log2_max_pic_order_cnt_lsb;
    bool delta_pic_order_always_zero_flag;
    int  offset_for_non_ref_pic;
    int  offset_for_top_to_bottom_field;
    uint tmp;
    uint num_ref_frames;
    bool gaps_in_frame_num_allowed_flag;
    uint pic_width_in_mbs;
    uint pic_height_in_map_units;

    pic_order_cnt_type = get_ue_golomb(gb);
    if (pic_order_cnt_type == 0)
    {
        log2_max_pic_order_cnt_lsb = get_ue_golomb(gb) + 4;
    }
    else if (pic_order_cnt_type == 1)
    {
        delta_pic_order_always_zero_flag = get_bits1(gb);
        offset_for_non_ref_pic = get_se_golomb(gb);
        offset_for_top_to_bottom_field = get_se_golomb(gb);
        tmp = get_ue_golomb(gb);
        for (uint idx = 0; idx < tmp; ++idx)
            get_se_golomb(gb);  // offset_for_ref_frame[i]
    }

    num_ref_frames = get_ue_golomb(gb);
    gaps_in_frame_num_allowed_flag = get_bits1(gb);
    pic_width_in_mbs = get_ue_golomb(gb) + 1;
    pic_height_in_map_units = get_ue_golomb(gb) + 1;
    frame_mbs_only_flag = get_bits1(gb);
}

} // namespace H264
