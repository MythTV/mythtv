// -*- Mode: c++ -*-
/*******************************************************************
 * H2645Parser
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

#ifndef H2645PARSER_H
#define H2645PARSER_H

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

#include "libmythbase/compat.h" // for uint on Darwin, MinGW
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/recorders/recorderbase.h" // for ScanType

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
enum class SCAN_t : uint8_t;

class H2645Parser {
  public:
    enum {
        MAX_SLICE_HEADER_SIZE = 256
    };

    enum field_type {
        FRAME = 'F',
        FIELD_TOP = 'T',
        FIELD_BOTTOM = 'B'
    };

    H2645Parser(void);
    H2645Parser(const H2645Parser& rhs);
    virtual ~H2645Parser(void) {delete [] m_rbspBuffer;}

    virtual uint32_t addBytes(const uint8_t  *bytes,
                              uint32_t  byte_count,
                              uint64_t  stream_offset) = 0;
    virtual void Reset(void);

    virtual QString NAL_type_str(int8_t type) = 0;

    bool stateChanged(void) const { return m_stateChanged; }

    bool onFrameStart(void) const { return m_onFrame; }
    bool onKeyFrameStart(void) const { return m_onKeyFrame; }

    uint pictureWidth(void) const { return m_picWidth; }
    uint pictureHeight(void) const { return m_picHeight; }
    virtual uint pictureWidthCropped(void)  const = 0;
    virtual uint pictureHeightCropped(void) const = 0;

    /** \brief Computes aspect ratio from picture size and sample aspect ratio
     */
    uint aspectRatio(void) const;
    virtual void getFrameRate(FrameRate &result) const = 0;
    virtual field_type getFieldType(void) const = 0;

    uint64_t frameAUstreamOffset(void) const {return m_frameStartOffset;}
    uint64_t keyframeAUstreamOffset(void) const {return m_keyframeStartOffset;}
    uint64_t SPSstreamOffset(void) const {return m_spsOffset;}

    uint32_t GetTimeScale(void) const { return m_timeScale; }
    uint32_t GetUnitsInTick(void) const { return m_unitsInTick; }
    SCAN_t GetScanType(void) const { return m_scanType; }

    enum NAL_unit_type {
        UNKNOWN = -1
    };

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

  protected:
    enum constants {EXTENDED_SAR = 255};

    void resetRBSP(void);
    bool fillRBSP(const uint8_t *byteP, uint32_t byte_count,
                  bool found_start_code);

    void vui_parameters(GetBitContext * gb, bool hevc);

    uint64_t   m_framecnt                    {0};
    uint64_t   m_keyframecnt                 {0};
    uint64_t   m_totalframecnt               {0};
    uint64_t   m_totalkeyframecnt            {0};
    uint64_t   m_auOffset                    {0};
    uint64_t   m_frameStartOffset            {0};
    uint64_t   m_keyframeStartOffset         {0};
    uint64_t   m_pktOffset                   {0};
    uint64_t   m_spsOffset                   {0};

    uint32_t   m_consecutiveZeros            {0};
    uint32_t   m_rbspBufferSize              {188 * 2};
    uint32_t   m_rbspIndex                   {0};
    uint32_t   m_syncAccumulator             {0xffffffff};
    uint32_t   m_timeScale                   {0};
    uint32_t   m_unitsInTick                 {0};

    SCAN_t     m_scanType                    {SCAN_t::UNKNOWN_SCAN};

    uint       m_frameCropBottomOffset       {0};
    uint       m_frameCropLeftOffset         {0};
    uint       m_frameCropRightOffset        {0};
    uint       m_frameCropTopOffset          {0};
    uint       m_picHeight                   {0};
    uint       m_picWidth                    {0};
    uint       m_sarHeight                   {0};
    uint       m_sarWidth                    {0};

    uint8_t   *m_rbspBuffer                  {nullptr};
    uint8_t    m_aspectRatioIdc              {0};

    int8_t     m_chromaFormatIdc             {1};

    bool       m_auPending                   {false};
    bool       m_fixedRate                   {false};
    bool       m_haveUnfinishedNAL           {false};
    bool       m_isKeyframe                  {false};
    bool       m_onAU                        {false};
    bool       m_onFrame                     {false};
    bool       m_onKeyFrame                  {false};
    bool       m_seenSPS                     {false};
    bool       m_separateColourPlaneFlag     {false};
    bool       m_stateChanged                {false};
};

#endif /* H2645PARSER_H */
