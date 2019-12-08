// -*- Mode: c++ -*-
/*******************************************************************
 * H264Parser
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

#ifndef H264PARSER_H
#define H264PARSER_H

// OMG this is a hack.  Buried several layers down in FFmpeg includes
// is an include of unistd.h that using GCC will foricibly redefine
// NULL back to the wrong value.  (Maybe just on ARM?)  Include
// unistd.h up front so that the subsequent inclusions will be
// skipped, and then define NULL to the right value.
#include <unistd.h>
#undef NULL
#define NULL nullptr

#include <QString>
#include <cstdint>
#include "mythconfig.h"
#include "compat.h" // for uint on Darwin, MinGW

#ifndef INT_BIT
#define INT_BIT (CHAR_BIT * sizeof(int))
#endif

// copied from libavutil/internal.h
extern "C" {
// Grr. NULL keeps getting redefined back to 0
#undef NULL
#define NULL nullptr
#include "libavutil/common.h" // for AV_GCC_VERSION_AT_LEAST()
}
#ifndef av_alias
#if HAVE_ATTRIBUTE_MAY_ALIAS && (!defined(__ICC) || __ICC > 1110) && AV_GCC_VERSION_AT_LEAST(3,3)
#   define av_alias __attribute__((may_alias))
#else
#   define av_alias
#endif
#endif

extern "C" {
// Grr. NULL keeps getting redefined back to 0
#undef NULL
#define NULL nullptr
#include "libavcodec/get_bits.h"
}

class FrameRate;

class H264Parser {
  public:

    enum {
        MAX_SLICE_HEADER_SIZE = 256
    };

    // ITU-T Rec. H.264 table 7-1
    enum NAL_unit_type {
        UNKNOWN         = 0,
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

    enum SEI_type {
        SEI_TYPE_PIC_TIMING             = 1,
        SEI_FILLER_PAYLOAD              = 3,
        SEI_TYPE_USER_DATA_UNREGISTERED = 5,
        SEI_TYPE_RECOVERY_POINT         = 6
    };

    /*
      slice_type values in the range 5..9 specify, in addition to the
      coding type of the current slice, that all other slices of the
      current coded picture shall have a value of slice_type equal to
      the current value of slice_type or equal to the current value of
      slice_type – 5.
    */
    enum SLICE_type {
        SLICE_P = 0,
        SLICE_B = 1,
        SLICE_I = 2,
        SLICE_SP = 3,
        SLICE_SI = 4,
        SLICE_P_a = 5,
        SLICE_B_a = 6,
        SLICE_I_a = 7,
        SLICE_SP_a = 8,
        SLICE_SI_a = 9,
        SLICE_UNDEF = 10
    };

    enum frame_type {
        FRAME = 'F',
        FIELD_TOP = 'T',
        FIELD_BOTTOM = 'B'
    };

    H264Parser(void);
    H264Parser(const H264Parser& rhs);
    ~H264Parser(void) {delete [] m_rbspBuffer;}

    uint32_t addBytes(const uint8_t  *bytes,
                      const uint32_t  byte_count,
                      const uint64_t  stream_offset);
    void Reset(void);

    static QString NAL_type_str(uint8_t type);

    bool stateChanged(void) const { return m_stateChanged; }

    uint8_t lastNALtype(void) const { return m_nalUnitType; }

    frame_type FieldType(void) const
        {
            if (m_bottomFieldFlag == -1)
                return FRAME;
            else
                return m_bottomFieldFlag ? FIELD_BOTTOM : FIELD_TOP;
        }

    bool onFrameStart(void) const { return m_onFrame; }
    bool onKeyFrameStart(void) const { return m_onKeyFrame; }

    uint pictureWidth(void) const { return m_picWidth; }
    uint pictureHeight(void) const { return m_picHeight; }
    uint pictureWidthCropped(void) const;
    uint pictureHeightCropped(void) const;

    /** \brief Computes aspect ratio from picture size and sample aspect ratio
     */
    uint aspectRatio(void) const;
    double frameRate(void) const;
    void getFrameRate(FrameRate &result) const;
    uint  getRefFrames(void) const;

    uint64_t frameAUstreamOffset(void) const {return m_frameStartOffset;}
    uint64_t keyframeAUstreamOffset(void) const {return m_keyframeStartOffset;}
    uint64_t SPSstreamOffset(void) const {return m_spsOffset;}

    // == NAL_type AU_delimiter: primary_pic_type = 5
    static bool isKeySlice(uint slice_type)
        {
            return (slice_type == SLICE_I   ||
                    slice_type == SLICE_SI  ||
                    slice_type == SLICE_I_a ||
                    slice_type == SLICE_SI_a);
        }

    static bool NALisSlice(uint8_t nal_type)
        {
            return (nal_type == SLICE ||
                    nal_type == SLICE_DPA ||
                    nal_type == SLICE_IDR);
        }

    void use_I_forKeyframes(bool val) { m_iIsKeyframe = val; }
    bool using_I_forKeyframes(void) const { return m_iIsKeyframe; }

    uint32_t GetTimeScale(void) const { return m_timeScale; }

    uint32_t GetUnitsInTick(void) const { return m_unitsInTick; }

    void parse_SPS(uint8_t *sps, uint32_t sps_size,
                   bool& interlaced, int32_t& max_ref_frames);

    void reset_SPS(void) { m_seenSps = false; }
    bool seen_SPS(void) const { return m_seenSps; }

    bool found_AU(void) const { return m_auPending; }

  private:
    enum constants {EXTENDED_SAR = 255};

    inline void set_AU_pending(void)
        {
            if (!m_auPending)
            {
                m_auPending = true;
                m_auOffset = m_pktOffset;
                m_auContainsKeyframeMessage = false;
            }
        }

    bool new_AU(void);
    void resetRBSP(void);
    bool fillRBSP(const uint8_t *byteP, uint32_t byte_count,
                  bool found_start_code);
    void processRBSP(bool rbsp_complete);
    bool decode_Header(GetBitContext *gb);
    void decode_SPS(GetBitContext *gb);
    void decode_PPS(GetBitContext * gb);
    void decode_SEI(GetBitContext * gb);
    void vui_parameters(GetBitContext * gb);

    bool       m_auPending                   {false};
    bool       m_stateChanged                {false};
    bool       m_seenSps                     {false};
    bool       m_auContainsKeyframeMessage   {false};
    bool       m_isKeyframe                  {false};
    bool       m_iIsKeyframe                 {true};

    uint32_t   m_syncAccumulator             {0xffffffff};
    uint8_t   *m_rbspBuffer                  {nullptr};
    uint32_t   m_rbspBufferSize              {188 * 2};
    uint32_t   m_rbspIndex                   {0};
    uint32_t   m_consecutiveZeros            {0};
    bool       m_haveUnfinishedNAL           {false};

    int        m_prevFrameNum                {-1};
    int        m_frameNum                    {-1};
    uint       m_sliceType                   {SLICE_UNDEF};
    int        m_prevPicParameterSetId       {-1};
    int        m_picParameterSetId           {-1};
    int8_t     m_prevFieldPicFlag            {-1};
    int8_t     m_fieldPicFlag                {-1};
    int8_t     m_prevBottomFieldFlag         {-1};
    int8_t     m_bottomFieldFlag             {-1};
    uint8_t    m_prevNALRefIdc               {111}; // != [0|1|2|3]
    uint8_t    m_nalRefIdc                   {111}; // != [0|1|2|3]
    uint8_t    m_prevPicOrderCntType         {0};
    uint8_t    m_picOrderCntType             {0};
    int        m_prevPicOrderCntLsb          {0};
    int        m_picOrderCntLsb              {0};
    int        m_prevDeltaPicOrderCntBottom  {0};
    int        m_deltaPicOrderCntBottom      {0};
    int        m_prevDeltaPicOrderCnt[2]     {0};
    int        m_deltaPicOrderCnt[2]         {0};
    uint8_t    m_prevNalUnitType             {UNKNOWN};
    uint8_t    m_nalUnitType                 {UNKNOWN};
    uint       m_prevIdrPicId                {65536};
    uint       m_idrPicId                    {65536};

    uint       m_log2MaxFrameNum             {0};
    uint       m_log2MaxPicOrderCntLsb       {0};
    uint       m_seqParameterSetId           {0};

    uint8_t    m_deltaPicOrderAlwaysZeroFlag {0};
    uint8_t    m_separateColourPlaneFlag     {0};
    int8_t     m_frameMbsOnlyFlag            {-1};
    int8_t     m_picOrderPresentFlag         {-1};
    int8_t     m_redundantPicCntPresentFlag  {0};
    int8_t     m_chromaFormatIdc             {1};

    uint       m_numRefFrames                {0};
    uint       m_redundantPicCnt             {0};
//  uint       m_picWidthInMbs               {0};
//  uint       m_picHeightInMapUnits         {0};
    uint       m_picWidth                    {0};
    uint       m_picHeight                   {0};
    uint       m_frameCropLeftOffset         {0};
    uint       m_frameCropRightOffset        {0};
    uint       m_frameCropTopOffset          {0};
    uint       m_frameCropBottomOffset       {0};
    uint8_t    m_aspectRatioIdc              {0};
    uint       m_sarWidth                    {0};
    uint       m_sarHeight                   {0};
    uint32_t   m_unitsInTick                 {0};
    uint32_t   m_timeScale                   {0};
    bool       m_fixedRate                   {false};

    uint64_t   m_pktOffset                   {0};
    uint64_t   m_auOffset                    {0};
    uint64_t   m_frameStartOffset            {0};
    uint64_t   m_keyframeStartOffset         {0};
    uint64_t   m_spsOffset                   {0};
    bool       m_onFrame                     {false};
    bool       m_onKeyFrame                  {false};
};

#endif /* H264PARSER_H */
