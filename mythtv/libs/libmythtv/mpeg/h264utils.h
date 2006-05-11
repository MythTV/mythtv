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

#ifndef H264UTILS_H_
#define H264UTILS_H_

#include <stdint.h>

namespace H264
{

/** \class NALUnitType
 *  \brief Contains a listing of valid NAL unit types for H.264 bitstreams.
 *
 *   The class also provides a toString method taking an unsigned 
 *   char and returning a string representation of the NAL unit 
 *   type code as a const char *.
 */
class NALUnitType
{
  public:
    /** According to ITU-T Rec. H.264 (03/2005) 7.4.1, table 7-1. */
    enum
    {
        UNKNOWN         = 0,
        SLICE           = 1,
        SLICE_DPA       = 2,
        SLICE_DPB       = 3,
        SLICE_DPC       = 4,
        SLICE_IDR       = 5,
        SEI             = 6,
        SPS             = 7,
        PPS             = 8,
        AU_DELIMITER    = 9,
        END_SEQUENCE    = 10,
        END_STREAM      = 11,
        FILLER_DATA     = 12,
        SPS_EXT         = 13,
        AUXILIARY_SLICE = 19,
    };

    /** According to ITU-T Rec. H.264 (03/2005) 7.4.1, nal_unit_type. */
    static bool IsVCLType(const uint8_t type)
        { return (type > UNKNOWN && type < SEI); /* SLICE types */ }

    static const char *toString(const uint8_t type)
    {
        switch (type)
        {
            case SLICE:
                return "SLICE";

            case SLICE_DPA:
                return "SLICE_DPA";

            case SLICE_DPB:
                return "SLICE_DPB";

            case SLICE_DPC:
                return "SLICE_DPC";

            case SLICE_IDR:
                return "SLICE_IDR";

            case SEI:
                return "SEI";

            case SPS:
                return "SPS";

            case PPS:
                return "PPS";

            case AU_DELIMITER:
                return "AU_DELIMITER";

            case END_SEQUENCE:
                return "END_SEQUENCE";

            case END_STREAM:
                return "END_STREAM";

            case FILLER_DATA:
                return "FILLER_DATA";

            case SPS_EXT:
                return "SPS_EXT";

            case AUXILIARY_SLICE:
                return "AUXILIARY_SLICE";

            default:
                return "UNKNOWN";
        }
    }
};

/** \class KeyframeSequencer
 *  \brief Minimalist state machine class used to detect H.264 keyframes.
 *
 *   Theory of operation: A true H.264 keyframe is an IDR 
 *   (instantaneous decoding refresh) slice.
 *
 *   You submit bytes to instances of this class, and it will tell 
 *   you if you're synced on a NAL unit. More bytes and you get the type.
 *   Keep going and you'll be know when you're on keyframes.
 *
 *   As a bonus, if you submit stream offsets, it will track access unit 
 *   offsets such that you can seek in a stream and please H.264 decoders.
 *
 *   This class only applies to the byte stream format as specified by 
 *   ITU-T Rec. H.264 (03/2005) Annex B.
 */
class KeyframeSequencer
{
  public:
    KeyframeSequencer(); /* throw() */

    inline bool IsErrored(void) const /* throw() */
        { return errored; }
    inline bool HasStateChanged(void) const /* throw() */
        { return state_changed; }

    void Reset(void); /* throw() */

    /** 
     *  This function more or less follows the method in ITU-T Rec. H.264
     *  (03/2005) B.2. It returns the number of bytes it has consumed when
     *  the sequencer's state has changed or when there are no bytes left
     *  to process.
     */
    uint32_t AddBytes(const uint8_t *bytes, const uint32_t byte_count,
                      const int64_t stream_offset); /* throw() */

    /**
     *  This function returns true when the sequencer read the first data
     *  byte (past the start code) of a new NAL unit.
     */
    inline bool DidReadNALHeaderByte(void) const /* throw() */
        { return read_first_NAL_byte; }

    /// This function returns the NAL unit type of the last synced unit.
    inline uint8_t LastSyncedType(void) const /* throw() */
        { return first_NAL_byte & 0x1f; }

    bool IsOnFrame(void) const; /* throw() */
    inline bool IsOnKeyframe(void) const /* throw() */
        { return keyframe; }

    inline int64_t KeyframeAUStreamOffset(void) const /* throw() */
        { return keyframe_sync_stream_offset; }

  private:
    void KeyframePredicate(const uint8_t new_first_NAL_byte); /* throw() */

    bool    errored;
    bool    state_changed;

    bool    synced;
    uint8_t sync_accumulator[3];
    uint8_t sync_accumulator_index;
    int64_t sync_stream_offset;

    bool    read_first_NAL_byte;
    uint8_t first_NAL_byte;

    bool    saw_AU_delimiter;
    bool    saw_first_VCL_NAL_unit;

    bool    did_evaluate_once;
    bool    keyframe;
    int64_t keyframe_sync_stream_offset;
};

} // namespace H264

#endif // H264UTILS_H_
