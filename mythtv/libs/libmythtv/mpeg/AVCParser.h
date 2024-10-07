// -*- Mode: c++ -*-
/*******************************************************************
 * AVCParser
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
 ********************************************************************/

#ifndef AVCPARSER_H
#define AVCPARSER_H

#include <array>

#include "mpeg/H2645Parser.h"

class AVCParser : public H2645Parser
{
  public:

    // ITU-T Rec. H.264 table 7-1
    enum NAL_unit_type : std::int8_t {
        UNKOWN          = -1,
        SLICE           = 1,   // 1 - 5 are VCL NAL units
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
        NALU_prefix     = 14,
        SPS_subset      = 15,
        AUXILIARY_SLICE = 19,
        SLICE_EXTENSION = 20
    };

    enum SEI_type : std::uint8_t {
        SEI_TYPE_PIC_TIMING             = 1,
        SEI_FILLER_PAYLOAD              = 3,
        SEI_TYPE_USER_DATA_UNREGISTERED = 5,
        SEI_TYPE_RECOVERY_POINT         = 6
    };

    field_type getFieldType(void) const override;

    /*
      slice_type values in the range 5..9 specify, in addition to the
      coding type of the current slice, that all other slices of the
      current coded picture shall have a value of slice_type equal to
      the current value of slice_type or equal to the current value of
      slice_type – 5.
    */
    AVCParser(void) { ; }
    AVCParser(const AVCParser& rhs);
    ~AVCParser(void) override { ; }

    uint32_t addBytes(const uint8_t  *bytes,
                      uint32_t  byte_count,
                      uint64_t  stream_offset) override;
    void Reset(void) override;

    QString NAL_type_str(int8_t type) override;

    // == NAL_type AU_delimiter: primary_pic_type = 5
    static bool isKeySlice(uint slice_type)
        {
            return (slice_type == SLICE_I   ||
                    slice_type == SLICE_SI  ||
                    slice_type == SLICE_I_a ||
                    slice_type == SLICE_SI_a);
        }

    static bool NALisSlice(int8_t nal_type)
        {
            return (nal_type == SLICE ||
                    nal_type == SLICE_DPA ||
                    nal_type == SLICE_IDR);
        }

    void use_I_forKeyframes(bool val) { m_iIsKeyframe = val; }
    bool using_I_forKeyframes(void) const { return m_iIsKeyframe; }

    void parse_SPS(uint8_t *sps, uint32_t sps_size,
                   bool& interlaced, int32_t& max_ref_frames);

    void reset_SPS(void) { m_seenSPS = false; }
    bool seen_SPS(void) const { return m_seenSPS; }

    bool found_AU(void) const { return m_auPending; }

    uint8_t lastNALtype(void) const { return m_nalUnitType; }

    uint  getRefFrames(void) const { return m_numRefFrames; }

    uint pictureWidthCropped(void)  const override;
    uint pictureHeightCropped(void) const override;

    double frameRate(void) const;
    MythAVRational getFrameRate() const override;

    void set_AU_pending(void)
        {
            if (!m_auPending)
            {
                m_auPending = true;
                m_auOffset = m_pktOffset;
                m_auContainsKeyframeMessage = false;
            }
        }

  protected:
    bool new_AU(void);
    void processRBSP(bool rbsp_complete);

  private:
    bool decode_Header(BitReader& br);
    void decode_SPS(BitReader& br);
    void decode_PPS(BitReader& br);
    void decode_SEI(BitReader& br);


    int        m_deltaPicOrderCntBottom      {0};
    std::array<int,2> m_deltaPicOrderCnt     {0};
    int        m_frameNum                    {-1};
    int        m_picOrderCntLsb              {0};
    int        m_picParameterSetId           {-1};
    int        m_prevDeltaPicOrderCntBottom  {0};
    std::array<int,2> m_prevDeltaPicOrderCnt {0};
    int        m_prevFrameNum                {-1};
    int        m_prevPicOrderCntLsb          {0};
    int        m_prevPicParameterSetId       {-1};

    uint       m_idrPicId                    {65536};
    uint       m_log2MaxFrameNum             {0};
    uint       m_log2MaxPicOrderCntLsb       {0};
    uint       m_numRefFrames                {0};
    uint       m_prevIdrPicId                {65536};
    uint       m_redundantPicCnt             {0};
    uint       m_seqParameterSetId           {0};
    uint       m_sliceType                   {SLICE_UNDEF};

    int8_t     m_bottomFieldFlag             {-1};
    int8_t     m_fieldPicFlag                {-1};
    int8_t     m_frameMbsOnlyFlag            {-1};
    int8_t     m_nalUnitType                 {UNKNOWN};
    int8_t     m_picOrderPresentFlag         {-1};
    int8_t     m_prevBottomFieldFlag         {-1};
    int8_t     m_prevFieldPicFlag            {-1};
    int8_t     m_prevNalUnitType             {UNKNOWN};
    int8_t     m_redundantPicCntPresentFlag  {0};

    uint8_t    m_deltaPicOrderAlwaysZeroFlag {0};
    uint8_t    m_nalRefIdc                   {111}; // != [0|1|2|3]
    uint8_t    m_picOrderCntType             {0};
    uint8_t    m_prevNALRefIdc               {111}; // != [0|1|2|3]
    uint8_t    m_prevPicOrderCntType         {0};

    bool       m_auContainsKeyframeMessage   {false};
    bool       m_iIsKeyframe                 {true};
};

#endif /* AVCPARSER_H */
