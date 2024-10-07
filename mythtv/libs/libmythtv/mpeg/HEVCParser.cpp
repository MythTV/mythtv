// MythTV headers
#include "HEVCParser.h"
#include <iostream>

#include "libmythbase/mythlogging.h"
#include "recorders/dtvrecorder.h" // for FrameRate

#include <cmath>
#include <strings.h>

#include "bitreader.h"
#include "bytereader.h"

static const QString LOC { QStringLiteral("HEVCParser ") };

/*
  References:
  http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.352.3388&rep=rep1&type=pdf
  https://www.itu.int/rec/T-REC-H.265-201911-I/en
  https://ieeexplore.ieee.org/stamp/stamp.jsp?arnumber=6316136
 */

static uint ceil_log2 (uint32_t v)
{
    uint r = 0;
    uint shift = 0;

    --v;
    r = (v > 0xFFFF) << 4;
    v >>= r;
    shift = (v > 0xFF) << 3;
    v >>= shift;
    r |= shift;
    shift = (v > 0xF) << 2;
    v >>= shift;
    r |= shift;
    shift = (v > 0x3) << 1;
    v >>= shift;
    r |= shift;
    r |= (v >> 1);

    return r + 1;
}

void HEVCParser::Reset(void)
{
    H2645Parser::Reset();
}

QString HEVCParser::NAL_type_str(int8_t type)
{
    switch (type)
    {
        case UNKNOWN:
          return "UNKNOWN";
        case TAIL_N:
          return "TAIL_N";
        case TAIL_R:
          return "TAIL_R";
        case TSA_N:
          return "TSA_N";
        case TSA_R:
          return "TSA_R";
        case STSA_N:
          return "STSA_N";
        case STSA_R:
          return "STSA_R";
        case RADL_N:
          return "RADL_N";
        case RADL_R:
          return "RADL_R";
        case RASL_N:
          return "RASL_N";
        case RASL_R:
          return "RASL_R";
        case RSV_VCL_N10:
          return "RSV_VCL_N10";
        case RSV_VCL_N12:
          return "RSV_VCL_N12";
        case RSV_VCL_N14:
          return "RSV_VCL_N14";
        case RSV_VCL_R11:
          return "RSV_VCL_R11";
        case RSV_VCL_R13:
          return "RSV_VCL_R13";
        case RSV_VCL_R15:
          return "RSV_VCL_R15";
        case BLA_W_LP:
          return "BLA_W_LP";
        case BLA_W_RADL:
          return "BLA_W_RADL";
        case BLA_N_LP:
          return "BLA_N_LP";
        case IDR_W_RADL:
          return "IDR_W_RADL";
        case IDR_N_LP:
          return "IDR_N_LP";
        case CRA_NUT:
          return "CRA_NUT";
        case RSV_IRAP_VCL22:
          return "RSV_IRAP_VCL22";
        case RSV_IRAP_VCL23:
          return "RSV_IRAP_VCL23";
        case VPS_NUT:
          return "VPS_NUT";
        case SPS_NUT:
          return "SPS_NUT";
        case PPS_NUT:
          return "PPS_NUT";
        case AUD_NUT:
          return "AUD_NUT";
        case EOS_NUT:
          return "EOS_NUT";
        case EOB_NUT:
          return "EOB_NUT";
        case FD_NUT:
          return "FD_NUT";
        case PREFIX_SEI_NUT:
          return "PREFIX_SEI_NUT";
        case SUFFIX_SEI_NUT:
          return "SUFFIX_SEI_NUT";
    }
    return "OTHER";
}

uint32_t HEVCParser::addBytes(const uint8_t  *bytes,
                              const uint32_t  byte_count,
                              const uint64_t  stream_offset)
{
    const uint8_t *startP = bytes;

    m_stateChanged = false;
    m_onFrame      = false;
    m_onKeyFrame   = false;

#if 0
    static MythTimer timer(MythTimer::kStartRunning);
    static int nexttime = 60000;

    if (timer.elapsed() > nexttime)
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Frames %1 KeyFrames %2 | Total Frames %3 KeyFrames %4")
            .arg(m_framecnt)
            .arg(m_keyframecnt)
            .arg(m_totalframecnt)
            .arg(m_totalkeyframecnt));

        m_framecnt = 0;
        m_keyframecnt = 0;
        nexttime += 60000;
    }
#endif

    while (!m_onFrame && (startP < bytes + byte_count))
    {
        const uint8_t *endP =
            ByteReader::find_start_code_truncated(startP, bytes + byte_count, &m_syncAccumulator);

        bool found_start_code = ByteReader::start_code_is_valid(m_syncAccumulator);

        /* Between startP and endP we potentially have some more
         * bytes of a NAL that we've been parsing (plus some bytes of
         * start code)
         */
        if (m_haveUnfinishedNAL)
        {
            if (!fillRBSP(startP, endP - startP, found_start_code))
            {
                resetRBSP();
                return endP - bytes;
            }
        }
        processRBSP(found_start_code); /* Call may set have_uinfinished_NAL
                                        * to false */

        /* Dealt with everything up to endP */
        startP = endP;

        if (found_start_code)
        {
            if (m_haveUnfinishedNAL)
            {
                /* We've found a new start code, without completely
                 * parsing the previous NAL. Either there's a
                 * problem with the stream or with this parser.
                 */
                LOG(VB_GENERAL, LOG_ERR,
                    "HEVCParser::addBytes: Found new start "
                    "code, but previous NAL is incomplete!");
            }

            /* Prepare for accepting the new NAL */
            resetRBSP();

            /* If we find the start of an AU somewhere from here
             * to the next start code, the offset to associate with
             * it is the one passed in to this call, not any of the
             * subsequent calls.
             */
            m_pktOffset = stream_offset; // + (startP - bytes);

            uint16_t nal_unit_header = ((m_syncAccumulator & 0xff) << 8)
                                       | *startP;
            ++startP;

            // nal_unit header
            if (nal_unit_header & 0x8000)
            {
                LOG(VB_GENERAL, LOG_ERR, "HEVCParser::parseNAL: "
                    "NAL header forbidden_zero_bit is not zero!");
                return false;
            }

            m_nalTemperalId = (nal_unit_header & 0x7) - 1;
            m_nuhLayerId  = (nal_unit_header >> 3) & 0x3f;
            m_nalUnitType = (nal_unit_header >> 9) & 0x3f;

#if 0
            LOG(VB_RECORD, LOG_INFO,
                QString("nalTemperalId: %1, "
                        "nuhLayerId: %2, "
                        "nalUnitType: %3 %4")
                .arg(m_nalTemperalId)
                .arg(m_nuhLayerId)
                .arg(m_nalUnitType)
                .arg(NAL_type_str(m_nalUnitType)));
#endif

            if (m_nalUnitType == SPS_NUT ||
                m_nalUnitType == VPS_NUT ||
                NALisVCL(m_nalUnitType))
            {
                /* This is a NAL we need to parse. We may have the body
                 * of it in the part of the stream past to us this call,
                 * or we may get the rest in subsequent calls to addBytes.
                 * Either way, we set m_haveUnfinishedNAL, so that we
                 * start filling the rbsp buffer
                 */
                m_haveUnfinishedNAL = true;
            }
        } //found start code
    }

    return startP - bytes;
}

bool HEVCParser::newAU(void)
{
    /*
      3.1 access unit: A set of NAL units that are associated with
          each other according to a specified classification rule, are
          consecutive in decoding order, and contain exactly one coded
          picture with nuh_layer_id equal to 0.
          NOTE 1 – In addition to containing the video coding layer
                   (VCL) NAL units of the coded picture with
                   nuh_layer_id equal to 0, an access unit may also
                   contain non-VCL NAL units. The decoding of an
                   access unit with the decoding process specified in
                   clause 8 always results in a decoded picture with
                   nuh_layer_id equal to 0.
          NOTE 2 – An access unit is defined differently in Annex F
                   and does not need to contain a coded picture with
                   nuh_layer_id equal to 0.
    */

    /*
      F.3.1 access unit: A set of NAL units that are associated with
            each other according to a specified classification rule,
            are consecutive in decoding order and contain at most one
            coded picture with any specific value of nuh_layer_id.
    */

    /*
      F.7.4.2.4.4

      The first of any of the following NAL units preceding the first VCL
      NAL unit firstVclNalUnitInAu and succeeding the last VCL NAL unit
      preceding firstVclNalUnitInAu, if any, specifies the start of a new
      access unit:

      – Access unit delimiter NAL unit (when present).
      – VPS NAL unit (when present)
      – SPS NAL unit (when present)
      – PPS NAL unit (when present)
      – Prefix SEI NAL unit (when present)
      – NAL units with nal_unit_type in the range of
        RSV_NVCL41..RSV_NVCL44 (when present)
      – NAL units with nal_unit_type in the range of
        UNSPEC48..UNSPEC55 (when present)

      When there is none of the above NAL units preceding
      firstVclNalUnitInAu and succeeding the last VCL NAL unit
      preceding firstVclNalUnitInAu, if any, firstVclNalUnitInAu
      starts a new access unit.
    */


    /*
      7.4.2.4.4

      An access unit consists of one coded picture with nuh_layer_id
      equal to 0, zero or more VCL NAL units with nuh_layer_id greater
      than 0 and zero or more non-VCL NAL units. The association of
      VCL NAL units to coded pictures is described in clause
      7.4.2.4.5.

      The first access unit in the bitstream starts with the first NAL
      unit of the bitstream.

      Let firstBlPicNalUnit be the first VCL NAL unit of a coded
      picture with nuh_layer_id equal to 0. The first of any of the
      following NAL units preceding firstBlPicNalUnit and succeeding
      the last VCL NAL unit preceding firstBlPicNalUnit, if any,
      specifies the start of a new access unit:

      NOTE 1 – The last VCL NAL unit preceding firstBlPicNalUnit in
               decoding order may have nuh_layer_id greater than 0.

      – access unit delimiter NAL unit with nuh_layer_id equal to 0
        (when present),
      – VPS NAL unit with nuh_layer_id equal to 0 (when present),
      – SPS NAL unit with nuh_layer_id equal to 0 (when present),
      – PPS NAL unit with nuh_layer_id equal to 0 (when present),
      – Prefix SEI NAL unit with nuh_layer_id equal to 0 (when present),
      – NAL units with nal_unit_type in the range of
        RSV_NVCL41..RSV_NVCL44 with nuh_layer_id equal to 0 (when
        present),
      – NAL units with nal_unit_type in the range of
        UNSPEC48..UNSPEC55 with nuh_layer_id equal to 0 (when
        present).

      NOTE 2 – The first NAL unit preceding firstBlPicNalUnit and
               succeeding the last VCL NAL unit preceding
               firstBlPicNalUnit, if any, can only be one of the
               above-listed NAL units.

      When there is none of the above NAL units preceding
      firstBlPicNalUnit and succeeding the last VCL NAL preceding
      firstBlPicNalUnit, if any, firstBlPicNalUnit starts a new access
      unit.

      The order of the coded pictures and non-VCL NAL units within an
      access unit shall obey the following constraints:

      – When an access unit delimiter NAL unit with nuh_layer_id equal
        to 0 is present, it shall be the first NAL unit. There shall
        be at most one access unit delimiter NAL unit with
        nuh_layer_id equal to 0 in any access unit.
      – When any VPS NAL units, SPS NAL units, PPS NAL units, prefix
        SEI NAL units, NAL units with nal_unit_type in the range of
        RSV_NVCL41..RSV_NVCL44, or NAL units with nal_unit_type in the
        range of UNSPEC48..UNSPEC55 are present, they shall not follow
        the last VCL NAL unit of the access unit.
      – NAL units having nal_unit_type equal to FD_NUT or
        SUFFIX_SEI_NUT or in the range of RSV_NVCL45..RSV_NVCL47 or
        UNSPEC56..UNSPEC63 shall not precede the first VCL NAL unit of
        the access unit.
      – When an end of sequence NAL unit with nuh_layer_id equal to 0
        is present, it shall be the last NAL unit among all NAL units
        with nuh_layer_id equal to 0 in the access unit other than an
        end of bitstream NAL unit (when present).
      – When an end of bitstream NAL unit is present, it shall be the
        last NAL unit in the access unit.

      NOTE 3 – Decoders conforming to profiles specified in Annex A do
               not use NAL units with nuh_layer_id greater than 0,
               e.g., access unit delimiter NAL units with nuh_layer_id
               greater than 0, for access unit boundary detection,
               except for identification of a NAL unit as a VCL or
               non-VCL NAL unit.

      The structure of access units not containing any NAL units with
      nal_unit_type equal to FD_NUT, VPS_NUT, SPS_NUT, PPS_NUT,
      RSV_VCL_N10, RSV_VCL_R11, RSV_VCL_N12, RSV_VCL_R13, RSV_VCL_N14
      or RSV_VCL_R15, RSV_IRAP_VCL22 or RSV_IRAP_VCL23, or in the
      range of RSV_VCL24..RSV_VCL31, RSV_NVCL41..RSV_NVCL47 or
      UNSPEC48..UNSPEC63 is shown in Figure 7-1.
    */

    if (m_nuhLayerId == 0)
    {
        if (m_nalUnitType == AUD_NUT)
            return true;

        if (m_nalUnitType == EOS_NUT)
        {
            // The next NAL is the start of a new AU
            m_nextNALisAU = true;
            m_seenEOS = true;
            m_onAU = false;
            m_noRaslOutputFlag = true;
            return false;
        }

        if (m_nextNALisAU)
        {
            m_nextNALisAU = false;
            return true;
        }
    }

    if (m_nalUnitType == SUFFIX_SEI_NUT ||
        (RSV_NVCL45 <= m_nalUnitType && m_nalUnitType <= RSV_NVCL47) ||
        (UNSPEC56 <= m_nalUnitType && m_nalUnitType <= UNSPEC63))
    {
        m_auPending = false;
        m_onAU = false;
        return false;
    }

    if (m_auPending || m_onAU)
        return false;

    if (m_nalUnitType == VPS_NUT ||
        m_nalUnitType == SPS_NUT ||
        m_nalUnitType == PPS_NUT ||
        m_nalUnitType == PREFIX_SEI_NUT ||
        (m_nalUnitType >= RSV_NVCL41 && m_nalUnitType <= RSV_NVCL44) ||
        (m_nalUnitType >= UNSPEC48   && m_nalUnitType <= UNSPEC55))
    {
        return true;
    }


    /*
      7.4.2.4.5 Order of VCL NAL units and association to coded pictures

      This clause specifies the order of VCL NAL units and association
      to coded pictures.

      Each VCL NAL unit is part of a coded picture.  The order of the
      VCL NAL units within a coded picture is constrained as follows:

      A VCL NAL unit is the first VCL NAL unit of an access unit, when
      all of the following conditions are true:
      – first_slice_segment_in_pic_flag is equal to 1.

      and F.7.4.2.4.4 Common specifications for multi-layer extensions

      – At least one of the following conditions is true:
        – The previous picture in decoding order belongs to a
          different picture order count (POC) resetting period than
          the picture containing the VCL NAL unit.
        – PicOrderCntVal derived for the VCL NAL unit differs from the
          PicOrderCntVal of the previous picture in decoding order.
    */
    if (m_firstSliceSegmentInPicFlag && NALisVCL(m_nalUnitType))
    {
        return true;
    }

    return false;
}


void HEVCParser::processRBSP(bool rbsp_complete)
{
    auto br = BitReader(m_rbspBuffer, m_rbspIndex);

    if (m_nalUnitType == SPS_NUT ||
        m_nalUnitType == VPS_NUT ||
        NALisVCL(m_nalUnitType))
    {
        /* Best wait until we have the whole thing */
        if (!rbsp_complete)
            return;

        if (!m_seenSPS)
            m_spsOffset = m_pktOffset;

        if (m_nalUnitType == SPS_NUT)
            parseSPS(br);
        else if (m_nalUnitType == VPS_NUT)
            parseVPS(br);
        else if (NALisVCL(m_nalUnitType))
            parseSliceSegmentLayer(br);
    }

    /* If we got this far, we managed to parse a sufficient
     * prefix of the current NAL. We can go onto the next. */
    m_haveUnfinishedNAL = false;

    if (newAU())
    {
        m_auPending = true;
        m_auOffset  = m_pktOffset;
    }

    if (m_auPending && NALisVCL(m_nalUnitType))
    {
        m_onAU = true;
        m_auPending = false;
        m_stateChanged = m_seenSPS;

        m_onFrame = true;
        m_frameStartOffset = m_auOffset;

        if (NALisIRAP(m_nalUnitType))
        {
            m_onKeyFrame = true;
            m_keyframeStartOffset = m_auOffset;

            ++m_keyframecnt;
            ++m_totalkeyframecnt;
        }

        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("On %2Frame").arg(m_onKeyFrame ? "Key" : ""));

        ++m_framecnt;
        ++m_totalframecnt;
    }

}

/*
  7.4.4 Profile, tier and level semantics

  When the profile_tier_level( ) syntax structure is included in an
  SPS or is the first profile_tier_level( ) syntax structure in a VPS,
  and any of the syntax elements

      sub_layer_profile_space[ i ],
      sub_layer_profile_idc[ i ],
      sub_layer_profile_compatibility_flag[ i ][j ],
      sub_layer_progressive_source_flag[ i ],
      sub_layer_interlaced_source_ flag[ i ],
      sub_layer_non_packed_constraint_flag[ i ],
      sub_layer_frame_only_constraint_flag[ i ],
      sub_layer_max_12bit_constraint_flag[ i ],
      sub_layer_max_10bit_constraint_flag[ i ],
      sub_layer_max_8bit_constraint_flag[ i ],
      sub_layer_max_422chroma_constraint_flag[ i ],
      sub_layer_max_420chroma_constraint_flag[ i ],
      sub_layer_max_monochrome_ constraint_flag[ i ],
      sub_layer_intra_constraint_flag[ i ],
      sub_layer_one_picture_only_constraint_flag[ i ],
      sub_layer_lower_bit_rate_constraint_flag[ i ],
      sub_layer_max_14bit_constraint_flag,
      sub_layer_reserved_zero_33bits[ i ],
      sub_layer_reserved_zero_34bits[ i ],
      sub_layer_reserved_zero_43bits[ i ],
      sub_layer_inbld_flag[ i ],
      sub_layer_reserved_zero_1bit[ i ] and
      sub_layer_level_idc[ i ]

      is not present for any value of i in the range of 0 to
      maxNumSubLayersMinus1 − 1, inclusive, in the profile_tier_level(
      ) syntax structure, the value of the syntax element is inferred
      as follows (in decreasing order of i values from
      maxNumSubLayersMinus1 − 1 to 0):

      – If the value of i is equal to maxNumSubLayersMinus1, the value
        of the syntax element is inferred to be equal to the value of
        the corresponding syntax element prefixed with "general_" of
        the same profile_tier_level( ) syntax structure.

        NOTE 9 – For example, in this case, if
                 sub_layer_profile_space[ i ] is not present, the
                 value is inferred to be equal to
                 general_profile_space of the same profile_tier_level(
                 ) syntax structure.

      – Otherwise (the value of i is less than maxNumSubLayersMinus1),
        the value of the syntax element is inferred to be equal to the
        corresponding syntax element with i being replaced with i + 1
        of the same profile_tier_level( ) syntax structure.

        NOTE 10 – For example, in this case, if
                  sub_layer_profile_space[ i ] is not present, the
                  value is inferred to be equal to
                  sub_layer_profile_space[ i + 1 ] of the same
                  profile_tier_level( ) syntax structure.
*/
bool HEVCParser::profileTierLevel(BitReader& br,
                                  bool profilePresentFlag,
                                  int  maxNumSubLayersMinus1)
{
    int i = 0;

    if (profilePresentFlag)
    {
        br.get_bits(2); // general_profile_space u(2);
        br.get_bits(1); // general_tier_flag u(1)
        br.get_bits(5); // general_profile_idc u(5);
        for (int j = 0; j < 32; ++j)
            br.get_bits(1); // general_profile_compatibility_flag[j] u(1);

        /*
          general_progressive_source_flag and
          general_interlaced_source_flag are interpreted as follows:
          – If general_progressive_source_flag is equal to 1 and
            general_interlac ed_source_flag is equal to 0, the source
            scan type of the pictures in the CVS should be interpreted
            as progressive only.
          – Otherwise, if general_progressive_source_flag is equal to
            0 and general_interlaced_source_flag is equal to 1, the
            source scan type of the pictures in the CVS should be
            interpreted as interlaced only.
          – Otherwise, if general_progressive_source_flag is equal to
            0 and general_interlaced_source_flag is equal to 0, the
            source scan type of the pictures in the CVS should be
            interpreted as unknown or unspecified.
          – Otherwise (general_progressive_source_flag is equal to 1
            and general_interlaced_source_flag is equal to 1), the
            source scan type of each picture in the CVS is indicated
            at the picture level using the syntax element
            source_scan_type in a picture timing SEI message.

          NOTE 1 – Decoders may ignore the values of
                   general_progressive_source_flag and
                   general_interlaced_source_flag for purposes other
                   than determining the value to be inferred for
                   frame_field_info_present_flag when
                   vui_parameters_present_flag is equal to 0, as there
                   are no other decoding process requirements
                   associated with the values of these
                   flags. Moreover, the actual source scan type of the
                   pictures is outside the scope of this Specification
                   and the method by which the encoder selects the
                   values of general_progressive_source_flag and
                   general_interlaced_source_flag is unspecified.
        */
        bool general_progressive_source_flag = br.get_bits(1); // u(1)
        bool general_interlaced_source_flag  = br.get_bits(1); // u(1)
        if (!general_progressive_source_flag &&
            general_interlaced_source_flag)
            m_scanType = SCAN_t::INTERLACED;
        else
            m_scanType = SCAN_t::PROGRESSIVE;

        br.get_bits(1); // general_non_packed_constraint_flag u(1)
        br.get_bits(1); // general_frame_only_constraint_flag u(1)

#if 0
        /* The number of bits in this syntax structure is not
         * affected by this condition */
        if (general_profile_idc == 4 ||
            general_profile_compatibility_flag[4] ||
            general_profile_idc == 5 ||
            general_profile_compatibility_flag[5] ||
            general_profile_idc == 6 ||
            general_profile_compatibility_flag[6] ||
            general_profile_idc == 7 ||
            general_profile_compatibility_flag[7] ||
            general_profile_idc == 8 ||
            general_profile_compatibility_flag[8] ||
            general_profile_idc == 9 ||
            general_profile_compatibility_flag[9] ||
            general_profile_idc == 10 ||
            general_profile_compatibility_flag[10] ||
            general_profile_idc == 11 ||
            general_profile_compatibility_flag[11])
        {
            br.get_bits(1); //general_max_12bit_constraint_flag u(1)
            br.get_bits(1); //general_max_10bit_constraint_flag u(1)
            br.get_bits(1); //general_max_8bit_constraint_flag u(1)
            br.get_bits(1); //general_max_422chroma_constraint_flag u(1)
            br.get_bits(1); //general_max_420chroma_constraint_flag u(1)
            br.get_bits(1); //general_max_monochrome_constraint_flag u(1)
            br.get_bits(1); //general_intra_constraint_flag u(1)
            br.get_bits(1); //general_one_picture_only_constraint_flag u(1)
            br.get_bits(1); //general_lower_bit_rate_constraint_flag u(1)

            if (general_profile_idc == 5 ||
                general_profile_compatibility_flag[5] ||
                general_profile_idc == 9 ||
                general_profile_compatibility_flag[9] ||
                general_profile_idc == 10 ||
                general_profile_compatibility_flag[10] ||
                general_profile_idc == 11 ||
                general_profile_compatibility_flag[11])
            {
                br.get_bits(1); //general_max_14bit_constraint_flag u(1)
                // general_reserved_zero_33bits
                br.skip_bits(16); // bits[0..15]
                br.skip_bits(16); // bits[16..31]
                br.skip_bits(1);  // bits[32]
            }
            else
            {
                // general_reserved_zero_34bits u(34);
                br.skip_bits(16); // bits[0..15]
                br.skip_bits(16); // bits[16..31]
                br.skip_bits(2);  // bits[32..33]
            }
        }
        else if (general_profile_idc == 2 ||
                 general_profile_compatibility_flag[2])
        {
            br.get_bits(7);   // general_reserved_zero_7bits u(7);
            br.get_bits(1);   //general_one_picture_only_constraint_flag u(1)
            // general_reserved_zero_35bits u(35);
            br.skip_bits(16); // bits[0..15]
            br.skip_bits(16); // bits[16..31]
            br.skip_bits(3);  // bits[32..34]
        }
        else
#endif
        {
            // general_reserved_zero_43bits
            br.skip_bits(16); // bits[0..15]
            br.skip_bits(16); // bits[16..31]
            br.skip_bits(11); // bits[32..42]
        }

#if 0
        /* The number of bits in this syntax structure is not
         * affected by this condition */
        if (general_profile_idc == 1 ||
            general_profile_compatibility_flag[1] ||
            general_profile_idc == 2 ||
            general_profile_compatibility_flag[2] ||
            general_profile_idc == 3 ||
            general_profile_compatibility_flag[3] ||
            general_profile_idc == 4 ||
            general_profile_compatibility_flag[4] ||
            general_profile_idc == 5 ||
            general_profile_compatibility_flag[5] ||
            general_profile_idc == 9 ||
            general_profile_compatibility_flag[9] ||
            general_profile_idc == 11 ||
            general_profile_compatibility_flag[11])
            br.get_bits(1); //general_inbld_flag u(1)
        else
#endif
            br.get_bits(1); //general_reserved_zero_bit u(1)
    }

    br.get_bits(8); // general_level_idc u(8);

    /*
       sub_layer_profile_present_flag[i] equal to 1, specifies that
       profile information is present in the profile_tier_level()
       syntax structure for the sub-layer representation with
       TemporalId equal to i. sub_layer_profile_present_flag[i]
       equal to 0 specifies that profile information is not present in
       the profile_tier_level() syntax structure for the sub-layer
       representation with TemporalId equal to i. When
       profilePresentFlag is equal to 0,
       sub_layer_profile_present_flag[i] shall be equal to 0.
    */

    std::vector<bool> sub_layer_profile_present_flag;
    std::vector<bool> sub_layer_level_present_flag;
    for (i = 0; i < maxNumSubLayersMinus1; ++i)
    {
        sub_layer_profile_present_flag.push_back(br.get_bits(1)); // u(1)
        sub_layer_level_present_flag.push_back(  br.get_bits(1)); // u(1)
    }

    if (maxNumSubLayersMinus1 > 0)
    {
        for (i = maxNumSubLayersMinus1; i < 8; ++i)
            br.get_bits(2); // reserved_zero_2bits[i] u(2);
    }

    for (i = 0; i < maxNumSubLayersMinus1; ++i)
    {
        if (sub_layer_profile_present_flag[i])
        {
            br.get_bits(2); // sub_layer_profile_space[i] u(2);
            br.get_bits(1); // sub_layer_tier_flag[i]     u(1)
            br.get_bits(5); // sub_layer_profile_idc[i]   u(5);

            for (int j = 0; j < 32; ++j)
                br.get_bits(1); //sub_layer_profile_compatibility_flag[i][j] u(1)

            br.get_bits(1); //sub_layer_progressive_source_flag[i]    u(1)
            br.get_bits(1); //sub_layer_interlaced_source_flag[i]     u(1)
            br.get_bits(1); //sub_layer_non_packed_constraint_flag[i] u(1)
            br.get_bits(1); //sub_layer_frame_only_constraint_flag[i] u(1)

#if 0
            /* The number of bits in this syntax structure is not
             * affected by this condition */
            if (sub_layer_profile_idc[i] == 4 ||
                sub_layer_profile_compatibility_flag[i][4] ||
                sub_layer_profile_idc[i] == 5 ||
                sub_layer_profile_compatibility_flag[i][5] ||
                sub_layer_profile_idc[i] == 6 ||
                sub_layer_profile_compatibility_flag[i][6] ||
                sub_layer_profile_idc[i] == 7 ||
                sub_layer_profile_compatibility_flag[i][7] ||
                sub_layer_profile_idc[i] == 8 ||
                sub_layer_profile_compatibility_flag[i][8] ||
                sub_layer_profile_idc[i] == 9 ||
                sub_layer_profile_compatibility_flag[i][9] ||
                sub_layer_profile_idc[i] == 10 ||
                sub_layer_profile_compatibility_flag[i][10] ||
                sub_layer_profile_idc[i] == 11 ||
                sub_layer_profile_compatibility_flag[i][11])
            {
                br.get_bits(1); //sub_layer_max_12bit_constraint_flag[i] u(1)
                br.get_bits(1); //sub_layer_max_10bit_constraint_flag[i] u(1)
                br.get_bits(1); //sub_layer_max_8bit_constraint_flag[i] u(1)
                br.get_bits(1); //sub_layer_max_422chroma_constraint_flag[i] u(1)
                br.get_bits(1); //sub_layer_max_420chroma_constraint_flag[i] u(1)
                br.get_bits(1); //sub_layer_max_monochrome_constraint_flag[i] u(1)
                br.get_bits(1); //sub_layer_intra_constraint_flag[i] u(1)
                br.get_bits(1); //sub_layer_one_picture_only_constraint_flag[i] u(1)
                br.get_bits(1); //sub_layer_lower_bit_rate_constraint_flag[i] u(1)
                if (sub_layer_profile_idc[i] == 5 ||
                    sub_layer_profile_compatibility_flag[i][5] ||
                    sub_layer_profile_idc[i] == 9 ||
                    sub_layer_profile_compatibility_flag[i][9] ||
                    sub_layer_profile_idc[i] == 10 ||
                    sub_layer_profile_compatibility_flag[i][10] ||
                    sub_layer_profile_idc[i] == 11 ||
                    sub_layer_profile_compatibility_flag[i][11])
                {
                    br.get_bits(1); //sub_layer_max_14bit_constraint_flag[i] u(1)
                    // sub_layer_reserved_zero_33bits[i] u(33);
                    br.skip_bits(16); // bits[ 0..15]
                    br.skip_bits(16); // bits[16..31]
                    br.skip_bits( 1); // bits[32..32]
                }
                else
                {
                    // sub_layer_reserved_zero_34bits[i] u(34);
                    br.skip_bits(16); // bits[ 0..15]
                    br.skip_bits(16); // bits[16..31]
                    br.skip_bits( 2); // bits[32..33]
                }
            }
            else if(sub_layer_profile_idc[i] == 2 ||
                     sub_layer_profile_compatibility_flag[i][2])
            {
                br.get_bits(7); // sub_layer_reserved_zero_7bits[i] u(7);
                br.get_bits(1); // sub_layer_one_picture_only_constraint_flag[i] u(1)
                // sub_layer_reserved_zero_35bits[i] u(35);
                br.skip_bits(16); // bits[ 0..15]
                br.skip_bits(16); // bits[16..31]
                br.skip_bits( 3); // bits[32..34]
            }
            else
#endif
            {
                // sub_layer_reserved_zero_43bits[i] u(43);
                br.skip_bits(16); // bits[ 0..15]
                br.skip_bits(16); // bits[16..31]
                br.skip_bits(12); // bits[32..43]
            }

#if 0
            /* The number of bits in this syntax structure is not
             * affected by this condition */
            if (sub_layer_profile_idc[i] == 1 ||
                sub_layer_profile_compatibility_flag[i][1] ||
                sub_layer_profile_idc[i] == 2 ||
                sub_layer_profile_compatibility_flag[i][2] ||
                sub_layer_profile_idc[i] == 3 ||
                sub_layer_profile_compatibility_flag[i][3] ||
                sub_layer_profile_idc[i] == 4 ||
                sub_layer_profile_compatibility_flag[i][4] ||
                sub_layer_profile_idc[i] == 5 ||
                sub_layer_profile_compatibility_flag[i][5] ||
                sub_layer_profile_idc[i] == 9 ||
                sub_layer_profile_compatibility_flag[i][9] ||
                sub_layer_profile_idc[i] == 11 ||
                sub_layer_profile_compatibility_flag[i][11])

                br.get_bits(1); //sub_layer_inbld_flag[i] u(1)
            else
#endif
                br.get_bits(1); //sub_layer_reserved_zero_bit[i] u(1)
        }

        if (sub_layer_level_present_flag[i])
            br.get_bits(8); // sub_layer_level_idc[i] u(8);
    }

    return true;
}

static bool getScalingListParams(uint8_t sizeId, uint8_t matrixId,
                                 HEVCParser::ScalingList & dest_scaling_list,
                                 uint8_t* &sl, uint8_t &size,
                                 std::vector<int16_t> &scaling_list_dc_coef_minus8)
{
    switch (sizeId)
    {
        case HEVCParser::QUANT_MATIX_4X4:
          sl = dest_scaling_list.scaling_lists_4x4[matrixId].data();
          size = dest_scaling_list.scaling_lists_4x4[matrixId].size();;
          break;
        case HEVCParser::QUANT_MATIX_8X8:
          sl = dest_scaling_list.scaling_lists_8x8[matrixId].data();
          size = dest_scaling_list.scaling_lists_8x8[matrixId].size();
          break;
        case HEVCParser::QUANT_MATIX_16X16:
          sl = dest_scaling_list.scaling_lists_16x16[matrixId].data();
          size = dest_scaling_list.scaling_lists_16x16[matrixId].size();
          scaling_list_dc_coef_minus8 =
                  dest_scaling_list.scaling_list_dc_coef_minus8_16x16;
          break;
        case HEVCParser::QUANT_MATIX_32X32:
          sl = dest_scaling_list.scaling_lists_32x32[matrixId].data();
          size = dest_scaling_list.scaling_lists_32x32[matrixId].size();
          scaling_list_dc_coef_minus8 =
                  dest_scaling_list.scaling_list_dc_coef_minus8_32x32;
          break;
        default:
          return false;
    }
    return true;
}

/*
  7.3.4 Scaling list data syntax
  We dont' need any of this data. We just need to get past the bits.
*/
static bool scalingListData(BitReader& br,
                            HEVCParser::ScalingList & dest_scaling_list,
                            bool use_default)
{
    uint8_t sizeId   = 0;
    uint8_t size     = 0;

    for (sizeId = 0; sizeId < 4; ++sizeId)
    {
        for (uint matrixId = 0; matrixId < ((sizeId == 3) ? 2 : 6); ++matrixId)
        {
            std::vector<int16_t> scaling_list_dc_coef_minus8 {};
            uint8_t *sl = nullptr;

            if (!getScalingListParams(sizeId, matrixId,
                                      dest_scaling_list, sl, size,
                                      scaling_list_dc_coef_minus8))
            {
                LOG(VB_RECORD, LOG_WARNING, LOC +
                    QString("Failed to process scaling list params"));
                return false;
            }

            /* use_default_scaling_matrices forcefully which means,
             * sps_scaling_list_enabled_flag=TRUE,
             * sps_scaling_list_data_present_flag=FALSE,
             * pps_scaling_list_data_present_falg=FALSE */
            if (use_default)
            {
#if 0 // Unneeded
                if (!getDefaultScalingLists(&sl, sizeId, matrixId))
                {
                    LOG(VB_RECORD, LOG_WARNING, LOC +
                        QString("Failed to process default scaling lists"));
                    return false;
                }

                if (sizeId > 1)
                    /* Inferring the value of scaling_list_dc_coef_minus8 */
                    scaling_list_dc_coef_minus8[matrixId] = 8;
#endif
            }
            else
            {
                if (!br.get_bits(1)) // scaling_list_pred_mode_flag u(1)
                {
                    br.get_ue_golomb(); // scaling_list_pred_matrix_id_delta ue(v)
#if 0 // Unneeded
                    if (!scaling_list_pred_matrix_id_delta)
                    {
                        if (!getDefaultScalingLists(&sl, sizeId, matrixId))
                        {
                            LOG(VB_RECORD, LOG_WARNING, LOC +
                                QString("Failed to process default "
                                        "scaling list"));
                            return false;
                        }

                        /* Inferring the value of scaling_list_dc_coef_minus8 */
                        if (sizeId > 1)
                            scaling_list_dc_coef_minus8[matrixId] = 8;
                    }
                    else
                    {
                        uint8_t *temp_sl;
                        uint8_t refMatrixId = matrixId -
                                              scaling_list_pred_matrix_id_delta;
                        if (!getScalingListParams(dest_scaling_list, sizeId,
                                                  refMatrixId, &temp_sl,
                                                  NULL, {}))
                        {
                            LOG(VB_RECORD, LOG_WARNING, LOC +
                                QString("Failed to process scaling "
                                        "list params"));
                            return false;
                        }

                        for (i = 0; i < size; ++i)
                            sl[i] = temp_sl[i];

                        /* Inferring the value of scaling_list_dc_coef_minus8 */
                        if (sizeId > 1)
                            scaling_list_dc_coef_minus8[matrixId] =
                                scaling_list_dc_coef_minus8[refMatrixId];
                    }
#endif
                }
                else
                {
//                    uint nextCoef = 8;

                    if (sizeId > 1)
                    {
//                        scaling_list_dc_coef_minus8[matrixId] =
                        br.get_se_golomb(); // se(v)
#if 0 // Unneeded
                        if (scaling_list_dc_coef_minus8[matrixId] < -7 ||
                            247 < scaling_list_dc_coef_minus8[matrixId])
                        {
                            LOG(VB_RECORD, LOG_WARNING, LOC +
                                QString("scaling_list_dc_coef_minus8[%1] %2 "
                                        "outside -7 and 247")
                                .arg(matrixId)
                                .arg(scaling_list_dc_coef_minus8[matrixId]));
                        }
                        nextCoef = scaling_list_dc_coef_minus8[matrixId] + 8;
#endif
                    }

                    for (uint8_t i = 0; i < size; ++i)
                    {
                        br.get_se_golomb(); // scaling_list_delta_coef  se(v)
#if 0 // Unneeded
                        if (scaling_list_delta_coef < -128 ||
                            scaling_list_delta_coef > 127)
                        {
                            LOG(VB_RECORD, LOG_WARNING, LOC +
                                QString("scaling_list_delta_coef %1 "
                                        "outside -128 and 127")
                                .arg(scaling_list_delta_coef));
                        }
                        nextCoef = (nextCoef + scaling_list_delta_coef) & 0xff;
                        sl[i] = nextCoef;
#endif
                    }

                }
            }
        }
    }

    return true;
}

/*
  7.3.7 Short-term reference picture set syntax
  We don't any of this data, but we have to get past the bits
*/
static bool shortTermRefPicSet(BitReader& br, int stRPSIdx,
                               int num_short_term_ref_pic_sets,
                               std::array<HEVCParser::ShortTermRefPicSet,65> & stRPS,
                               uint8_t max_dec_pic_buffering_minus1)
{
    std::array<bool,16>     use_delta_flag          { false };
    std::array<bool,16>     used_by_curr_pic_flag   { false };
    std::array<uint32_t,16> delta_poc_s0_minus1     { 0 };
    std::array<uint32_t,16> delta_poc_s1_minus1     { 0 };
    uint i   = 0;

    /* 7.4.8 inter_ref_pic_set_prediction_flag equal to 1 specifies
       that the stRPSIdx-th candidate short-term RPS is predicted from
       another candidate short-term RPS, which is referred to as the
       source candidate short-term RPS. When
       inter_ref_pic_set_prediction_flag is not present, it is
       inferred to be equal to 0.
    */
    bool inter_ref_pic_set_prediction_flag = (stRPSIdx != 0) ?
                                             br.get_bits(1) : false; // u(1)

    if (inter_ref_pic_set_prediction_flag)
    {
        /*
          delta_idx_minus1 plus 1 specifies the difference between the
          value of stRPSIdx and the index, into the list of the
          candidate short-term RPSs specified in the SPS, of the
          source candidate short-term RPS. The value of
          delta_idx_minus1 shall be in the range of 0 to stRPSIdx − 1,
          inclusive. When delta_idx_minus1 is not present, it is
          inferred to be equal to 0.
        */
        int delta_idx_minus1 = (stRPSIdx == num_short_term_ref_pic_sets) ?
                               br.get_ue_golomb() : 0; // ue(v)
        if (delta_idx_minus1 > stRPSIdx - 1)
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Invalid delta_idx_minus1? %1").arg(delta_idx_minus1));

        int8_t delta_rps_sign = br.get_bits(1); // u(1)
        int abs_delta_rps_minus1 = br.get_ue_golomb();  // ue(v)
        if (abs_delta_rps_minus1 > 32767)
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Invalid abs_delta_rps_minus1"));
        int deltaRPS = ( 1 - 2 * delta_rps_sign ) * ( abs_delta_rps_minus1 + 1 );

        /*
          The variable RefRPSIdx is derived as follows:
          RefRPSIdx = stRPSIdx − ( delta_idx_minus1 + 1)
        */
        int RefRPSIdx = stRPSIdx - (delta_idx_minus1 + 1);
        HEVCParser::ShortTermRefPicSet *RefRPS = &stRPS[RefRPSIdx];

        for (int j = 0; j <= RefRPS->NumDeltaPocs; ++j)
        {
            used_by_curr_pic_flag[j] = br.get_bits(1); // u(1)
            /*
              use_delta_flag[ j ] equal to 1 specifies that the
              j-th entry in the source candidate short-term RPS
              is included in the stRPSIdx-th candidate short-term
              RPS. use_delta_flag[ j ] equal to 0 specifies that
              the j-th entry in the source candidate short-term
              RPS is not included in the stRPSIdx-th candidate
              short-term RPS. When use_delta_flag[ j ] is not
              present, its value is inferred to be equal to 1.
            */
            if (!used_by_curr_pic_flag[j])
            {
                use_delta_flag[j] = br.get_bits(1); // u(1)
            }
            else
            {
                use_delta_flag[j] = true;
            }
        }


        /* 7.4.8 Short-term reference picture set semantics */
        i = 0;
        for (int k = (RefRPS->NumPositivePics - 1); k >= 0; --k)
        {
            int dPoc = RefRPS->DeltaPocS1[k] + deltaRPS;
            if (dPoc < 0 && use_delta_flag[RefRPS->NumNegativePics + k])
            {
                stRPS[stRPSIdx].DeltaPocS0[i] = dPoc;
                stRPS[stRPSIdx].UsedByCurrPicS0[i++] =
                    used_by_curr_pic_flag[RefRPS->NumNegativePics + k];
            }
        }

        if (deltaRPS < 0 && use_delta_flag[RefRPS->NumDeltaPocs])
        {
            stRPS[stRPSIdx].DeltaPocS0[i] = deltaRPS;
            stRPS[stRPSIdx].UsedByCurrPicS0[i++] =
                used_by_curr_pic_flag[RefRPS->NumDeltaPocs];
        }

        for (int j = 0; j < RefRPS->NumNegativePics; ++j)
        {
            int dPoc = RefRPS->DeltaPocS0[j] + deltaRPS;
            if (dPoc < 0 && use_delta_flag[j])
            {
                stRPS[stRPSIdx].DeltaPocS0[i] = dPoc;
                stRPS[stRPSIdx].UsedByCurrPicS0[i++] = used_by_curr_pic_flag[j];
            }
        }
        stRPS[stRPSIdx].NumNegativePics = i;

        i = 0;
        for (int k = (RefRPS->NumNegativePics - 1); k >= 0; --k)
        {
            int dPoc = RefRPS->DeltaPocS0[k] + deltaRPS;
            if (dPoc > 0 && use_delta_flag[k])
            {
                stRPS[stRPSIdx].DeltaPocS1[i] = dPoc;
                stRPS[stRPSIdx].UsedByCurrPicS1[i++] = used_by_curr_pic_flag[k];
            }
        }

        if (deltaRPS > 0 && use_delta_flag[RefRPS->NumDeltaPocs])
        {
            stRPS[stRPSIdx].DeltaPocS1[i] = deltaRPS;
            stRPS[stRPSIdx].UsedByCurrPicS1[i++] =
                used_by_curr_pic_flag[RefRPS->NumDeltaPocs];
        }

        for (int j = 0; j < RefRPS->NumPositivePics; ++j)
        {
            int dPoc = RefRPS->DeltaPocS1[j] + deltaRPS;
            if (dPoc > 0 && use_delta_flag[RefRPS->NumNegativePics + j])
            {
                stRPS[stRPSIdx].DeltaPocS1[i] = dPoc;
                stRPS[stRPSIdx].UsedByCurrPicS1[i++] =
                    used_by_curr_pic_flag[RefRPS->NumNegativePics + j];
            }
        }
        stRPS[stRPSIdx].NumPositivePics= i;
    }
    else
    {
        stRPS[stRPSIdx].NumNegativePics = std::min((uint8_t)br.get_ue_golomb(), // ue(v)
                                          max_dec_pic_buffering_minus1);
        stRPS[stRPSIdx].NumPositivePics = std::min((uint8_t)br.get_ue_golomb(), // ue(v)
                                          max_dec_pic_buffering_minus1);

        for (i = 0; i < stRPS[stRPSIdx].NumNegativePics; ++i)
        {
            delta_poc_s0_minus1[i] = br.get_ue_golomb(); // ue(v)
            br.get_bits(1); // used_by_curr_pic_s0_flag[i]; u(1)

            if (i == 0)
                stRPS[stRPSIdx].DeltaPocS0[i] = -(delta_poc_s0_minus1[i] + 1);
            else
                stRPS[stRPSIdx].DeltaPocS0[i] = stRPS[stRPSIdx].DeltaPocS0[i - 1] -
                                       (delta_poc_s0_minus1[i] + 1);
        }
        for (i = 0; i < stRPS[stRPSIdx].NumPositivePics; ++i)
        {
            delta_poc_s1_minus1[i] = br.get_ue_golomb(); // ue(v)
            br.get_bits(1); // used_by_curr_pic_s1_flag[i]; u(1)

            if (i == 0)
                stRPS[stRPSIdx].DeltaPocS1[i] = delta_poc_s1_minus1[i] + 1;
            else
                stRPS[stRPSIdx].DeltaPocS1[i] = stRPS[stRPSIdx].DeltaPocS1[i - 1] +
                                       (delta_poc_s1_minus1[i] + 1);
        }
    }

    /*
      The variable NumDeltaPocs[ stRPSIdx ] is derived as follows:
      NumDeltaPocs[ stRPSIdx ] = NumNegativePics[ stRPSIdx ] +
      NumPositivePics[ stRPSIdx ]
    */
    stRPS[stRPSIdx].NumDeltaPocs = stRPS[stRPSIdx].NumNegativePics +
                          stRPS[stRPSIdx].NumPositivePics;
    return true;
}

/* 7.3.2.9 Slice segment layer RBSP syntax */
bool HEVCParser::parseSliceSegmentLayer(BitReader& br)
{
    if (!m_seenSPS)
        return false;

    parseSliceSegmentHeader(br);
#if 0
    slice_segment_data(br);
    rbsp_slice_segment_trailing_bits(br);
#endif
    return true;
}

/*
   7.3.6.1 General slice segment header syntax
   All we are after is the pic order count
*/
bool HEVCParser::parseSliceSegmentHeader(BitReader& br)
{
    bool dependent_slice_segment_flag = false; // check!

    m_firstSliceSegmentInPicFlag = br.get_bits(1);

    if (m_nalUnitType >= BLA_W_LP && m_nalUnitType <= RSV_IRAP_VCL23 )
    {
        br.get_bits(1); // no_output_of_prior_pics_flag; u(1)
    }

    int pps_id = br.get_ue_golomb(); // slice_pic_parameter_set_id; ue(v)
    if (m_pps.find(pps_id) == m_pps.end())
    {
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("PPS Id %1 not valid yet. Skipping parsing of slice.")
            .arg(pps_id));
        return false;
    }
    PPS* pps = &m_pps[pps_id];
    SPS* sps = &m_sps[pps->sps_id];

    if (!m_firstSliceSegmentInPicFlag)
    {
        if (pps->dependent_slice_segments_enabled_flag)
        {
            dependent_slice_segment_flag = br.get_bits(1); // u(1)
        }

        /* Figure out how many bits are in the slice_segment_address */
        uint32_t MinCbLog2SizeY = sps->log2_min_luma_coding_block_size;
        uint32_t CtbLog2SizeY = MinCbLog2SizeY +
                                sps->log2_diff_max_min_luma_coding_block_size;
        uint32_t CtbSizeY = 1 << CtbLog2SizeY;
        uint32_t PicHeightInCtbsY =
            ceil (static_cast<double>(m_picHeight) /
                  static_cast<double>(CtbSizeY));
        uint32_t PicWidthInCtbsY =
            ceil (static_cast<double>(m_picWidth) /
                  static_cast<double>(CtbSizeY));

        uint address_size = ceil_log2(PicWidthInCtbsY *
                                      PicHeightInCtbsY);

        br.get_bits(address_size); // slice_segment_address u(v)
    }

    // CuQpDeltaVal = 0;
    if (!dependent_slice_segment_flag)
    {
        for (int i = 0; i < pps->num_extra_slice_header_bits; ++i)
        {
            br.get_bits(1); // slice_reserved_flag[i]; // u(1)
        }
        br.get_ue_golomb(); // slice_type; // ue(v)
        if (pps->output_flag_present_flag)
        {
            br.get_bits(1); // pic_output_flag; // u(1)
        }
        if (sps->separate_colour_plane_flag)
        {
            br.get_bits(2); // colour_plane_id; // u(2)
        }
        if (m_nalUnitType == IDR_W_RADL || m_nalUnitType == IDR_N_LP)
        {
            m_picOrderCntMsb = 0;
            m_prevPicOrderCntLsb = 0;
            m_prevPicOrderCntMsb = 0;
        }
        else
        {
            uint16_t slice_pic_order_cnt_lsb =
                br.get_bits(sps->log2_max_pic_order_cnt_lsb); // u(v)

            /*
              8.1.3 Decoding process for a coded picture with
              nuh_layer_id equal to 0

                When the current picture is an IRAP picture, the
                following applies:
                – If the current picture is an IDR picture, a BLA
                  picture, the first picture in the bitstream in
                  decoding order, or the first picture that follows an
                  end of sequence NAL unit in decoding order, the
                  variable NoRaslOutputFlag is set equal to 1.
                - Otherwise, it is magic!
                - If not magic, then NoRaslOutputFlag equals 0.

              Meanwhile...
              F.8.1.3 Common decoding process for a coded picture

              When the current picture is an IRAP picture, the following applies:
              – If the current picture with a particular value of
                nuh_layer_id is an IDR picture, a BLA picture, the
                first picture with that particular value of
                nuh_layer_id in the bitstream in decoding order or the
                first picture with that particular value of
                nuh_layer_id that follows an end of sequence NAL unit
                with that particular value of nuh_layer_id in decoding
                order, the variable NoRaslOutputFlag is set equal to
                1.
            */
            m_noRaslOutputFlag |= (BLA_W_LP <= m_nalUnitType &&
                                   m_nalUnitType <= IDR_N_LP);

            /*
              8.3.1 Decoding process for picture order count
              The variable PicOrderCntMsb of the current picture is
              derived as follows:
               – If the current picture is an IRAP picture with
                 NoRaslOutputFlag equal to 1, PicOrderCntMsb is set
                 equal to 0.

               – Otherwise...

               NOTE 1 – All IDR pictures will have PicOrderCntVal
                        equal to 0 since slice_pic_order_cnt_lsb is
                        inferred to be 0 for IDR pictures and
                        prevPicOrderCntLsb and prevPicOrderCntMsb are
                        both set equal to 0.
            */

            if (NALisIRAP(m_nalUnitType) && m_noRaslOutputFlag)
            {
                m_picOrderCntMsb = 0;
            }
            else
            {
                /* 8.3.1 Decoding process for picture order count */
                uint MaxPicOrderCntLsb = pow(2, sps->log2_max_pic_order_cnt_lsb);

                if ((slice_pic_order_cnt_lsb < m_prevPicOrderCntLsb) &&
                    ((m_prevPicOrderCntLsb - slice_pic_order_cnt_lsb) >=
                     (MaxPicOrderCntLsb / 2)))
                {
                    m_picOrderCntMsb = m_prevPicOrderCntMsb +
                                       MaxPicOrderCntLsb;
                }
                else if ((slice_pic_order_cnt_lsb > m_prevPicOrderCntLsb) &&
                         ((slice_pic_order_cnt_lsb - m_prevPicOrderCntLsb) >
                          (MaxPicOrderCntLsb / 2)))
                {
                    m_picOrderCntMsb = m_prevPicOrderCntMsb -
                                       MaxPicOrderCntLsb;
                }
                else
                {
                    m_picOrderCntMsb = m_prevPicOrderCntMsb;
                }
            }
            m_picOrderCntVal = m_picOrderCntMsb + slice_pic_order_cnt_lsb;

            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("picOrderCntVal: %1 = %2 + %3")
                .arg(m_picOrderCntVal)
                .arg(m_picOrderCntMsb)
                .arg(slice_pic_order_cnt_lsb));

            m_noRaslOutputFlag = false;

#if 0 // We dont' need the rest
            br.get_bits(1); // short_term_ref_pic_set_sps_flag; // u(1)
            if (!short_term_ref_pic_set_sps_flag)
            {
                shortTermRefPicSet(num_short_term_ref_pic_sets);
            }
            else if(num_short_term_ref_pic_sets > 1)
            {
                br.get_bits(??? ); // short_term_ref_pic_set_idx; // u(v)
            }
            if (long_term_ref_pics_present_flag)
            {
                if (num_long_term_ref_pics_sps > 0)
                {
                    br.get_ue_golomb(); // num_long_term_sps; // ue(v)
                }
                br.get_ue_golomb(); // num_long_term_pics; // ue(v)
                for (i = 0; i < num_long_term_sps + num_long_term_pics; ++i)
                {
                    if (i < num_long_term_sps)
                    {
                        if (num_long_term_ref_pics_sps > 1)
                            br.get_bits(??? ); // lt_idx_sps[i]; // u(v)
                    }
                    else
                    {
                        poc_lsb_lt[i] =
                            br.get_bits(sps->Log2MaxPicOrderCntLsb); // u(v)
                        br.get_bits(1); // used_by_curr_pic_lt_flag[i]; // u(1)
                    }
                    br.get_bits(1); // delta_poc_msb_present_flag[i]; // u(1)
                    if (delta_poc_msb_present_flag[i])
                    {
                        br.get_ue_golomb(); // delta_poc_msb_cycle_lt[i]; // ue(v)
                    }
                }
            }
            if (sps_temporal_mvp_enabled_flag)
            {
                br.get_bits(1); // slice_temporal_mvp_enabled_flag; // u(1)
            }
#endif
        }
#if 0 // We don't need the rest
        if (sample_adaptive_offset_enabled_flag)
        {
            br.get_bits(1); // slice_sao_luma_flag; // u(1)
            if (ChromaArrayType != 0)
            {
                br.get_bits(1); // slice_sao_chroma_flag; // u(1)
            }
        }
        if (slice_type == P || slice_type == B)
        {
            br.get_bits(1); // num_ref_idx_active_override_flag; // u(1)
            if (num_ref_idx_active_override_flag)
            {
                br.get_ue_golomb(); // num_ref_idx_l0_active_minus1; // ue(v)
                if (slice_type == B)
                {
                    br.get_ue_golomb(); // num_ref_idx_l1_active_minus1; // ue(v)
                }
            }
            if (lists_modification_present_flag && NumPicTotalCurr > 1)
            {
                ref_pic_lists_modification();
                if (slice_type == B)
                {
                    br.get_bits(1); // mvd_l1_zero_flag; // u(1)
                }
            }
            if (cabac_init_present_flag)
            {
                br.get_bits(1); // cabac_init_flag; // u(1)
            }
            if (slice_temporal_mvp_enabled_flag)
            {
                if (slice_type == B)
                {
                    br.get_bits(1); // collocated_from_l0_flag; // u(1)
                }
                if (( collocated_from_l0_flag &&
                      num_ref_idx_l0_active_minus1 > 0) ||
                    (!collocated_from_l0_flag &&
                     num_ref_idx_l1_active_minus1 > 0))
                {
                    br.get_ue_golomb(); // collocated_ref_idx; // ue(v)
                }
            }
            if ((weighted_pred_flag && slice_type == P) ||
                (weighted_bipred_flag && slice_type == B))
            {
                pred_weight_table();
            }
            br.get_ue_golomb(); // five_minus_max_num_merge_cand; // ue(v)
            if (motion_vector_resolution_control_idc == 2)
            {
                br.get_bits(1); // use_integer_mv_flag; // u(1)
            }
        }
        br.get_se_golomb(); // slice_qp_delta; //se(v)
        if (pps_slice_chroma_qp_offsets_present_flag)
        {
            br.get_se_golomb(); // slice_cb_qp_offset; //se(v)
            br.get_se_golomb(); // slice_cr_qp_offset; //se(v)
        }
        if (pps_slice_act_qp_offsets_present_flag)
        {
            br.get_se_golomb(); // slice_act_y_qp_offset;  //se(v)
            br.get_se_golomb(); // slice_act_cb_qp_offset; //se(v)
            br.get_se_golomb(); // slice_act_cr_qp_offset; //se(v)
        }
        if (chroma_qp_offset_list_enabled_flag)
        {
            br.get_bits(1); // cu_chroma_qp_offset_enabled_flag; // u(1)
        }
        if (deblocking_filter_override_enabled_flag)
        {
            br.get_bits(1); // deblocking_filter_override_flag; // u(1)
        }
        if (deblocking_filter_override_flag)
        {
            br.get_bits(1); // slice_deblocking_filter_disabled_flag; // u(1)
            if (!slice_deblocking_filter_disabled_flag)
            {
                br.get_se_golomb(); // slice_beta_offset_div2; //se(v)
                br.get_se_golomb(); // slice_tc_offset_div2; //se(v)
            }
        }
        if (pps_loop_filter_across_slices_enabled_flag &&
            ( slice_sao_luma_flag || slice_sao_chroma_flag ||
              !slice_deblocking_filter_disabled_flag))
        {
            br.get_bits(1); // slice_loop_filter_across_slices_enabled_flag; // u(1)
        }
#endif
    }
#if 0 // We don't need the rest
    if (tiles_enabled_flag || entropy_coding_sync_enabled_flag)
    {
        br.get_ue_golomb(); // num_entry_point_offsets; // ue(v)
        if (num_entry_point_offsets > 0)
        {
            br.get_ue_golomb(); // offset_len_minus1; // ue(v)
            for (i = 0; i < num_entry_point_offsets; ++i)
            {
                br.get_bits(??? ); // entry_point_offset_minus1[i]; // u(v)
            }
        }
    }
    if (slice_segment_header_extension_present_flag)
    {
        br.get_ue_golomb(); // slice_segment_header_extension_length; // ue(v)
        for (i = 0; i < slice_segment_header_extension_length; ++i)
        {
            slice_segment_header_extension_data_byte[i]; // u(8)
        }
    }
    byte_alignment();
#endif

    return true;
}

/*
  F.7.3.2.2.1 General sequence parameter set RBSP
*/
bool HEVCParser::parseSPS(BitReader& br)
{
    uint i = 0;
    static std::array<ShortTermRefPicSet,65> short_term_ref_pic_set;

    static uint     sub_layer_size = 0;
    static uint8_t* max_dec_pic_buffering_minus1 = nullptr;

    m_seenSPS = true;

    uint vps_id = br.get_bits(4); // sps_video_parameter_set_id u(4)

    uint ext_or_max_sub_layers_minus1 = br.get_bits(3); // u(3)
    uint max_sub_layers_minus1 = 0;

    if (m_nuhLayerId == 0)
        max_sub_layers_minus1 = ext_or_max_sub_layers_minus1;
    else
    {
        if (m_vps.find(vps_id) == m_vps.end())
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Could not find VPS[%1]").arg(vps_id));
            max_sub_layers_minus1 = ext_or_max_sub_layers_minus1;
        }
        else
        {
            /*
              When not present, the value of sps_max_sub_layers_minus1 is
              inferred to be equal to ( sps_ext_or_max_sub_layers_minus1
              == 7) ? vps_max_sub_layers_minus1 :
              sps_ext_or_max_sub_layers_minus1
            */
            max_sub_layers_minus1 = (ext_or_max_sub_layers_minus1 == 7) ?
                                    m_vps[vps_id].max_sub_layers :
                                    ext_or_max_sub_layers_minus1;
        }
    }

    if (sub_layer_size <= max_sub_layers_minus1)
    {
        delete[] max_dec_pic_buffering_minus1;
        sub_layer_size = max_sub_layers_minus1 + 1;
        max_dec_pic_buffering_minus1 = new uint8_t[sub_layer_size];
    }

    bool MultiLayerExtSpsFlag =
        (m_nuhLayerId != 0 && ext_or_max_sub_layers_minus1 == 7);

    if (!MultiLayerExtSpsFlag)
    {
        br.get_bits(1); // sps_temporal_id_nesting_flag u(1)
        if (!profileTierLevel(br, true, max_sub_layers_minus1))
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("Failed to parse SPS profiel tier level."));
            return false;
        }
    }

    uint sps_id = br.get_ue_golomb(); // sps_seq_parameter_set_id ue(v);
    SPS* sps = &m_sps[sps_id];

    if (MultiLayerExtSpsFlag)
    {
        if (br.get_bits(1)) // update_rep_format_flag u(1)
        {
            br.get_bits(8); // sps_rep_format_idx
        }
    }
    else
    {
        m_chromaFormatIdc = br.get_ue_golomb(); // ue(v);
        if (m_chromaFormatIdc == 3)
            m_separateColourPlaneFlag = br.get_bits(1); // u(1)

        /*
          pic_width_in_luma_samples specifies the width of each decoded
          picture in units of luma samples.

          pic_width_in_luma_samples shall not be equal to 0 and shall be
          an integer multiple of MinCbSizeY.
        */
        m_picWidth = br.get_ue_golomb(); // pic_width_in_luma_samples ue(v);

        /*
          pic_height_in_luma_samples specifies the height of each decoded
          picture in units of luma samples.

          pic_height_in_luma_samples shall not be equal to 0 and shall be
          an integer multiple of MinCbSizeY.
        */
        m_picHeight = br.get_ue_golomb(); // pic_height_in_luma_samples ue(v);

        if (br.get_bits(1)) //conformance_window_flag u(1)
        {
            m_frameCropLeftOffset   = br.get_ue_golomb(); // ue(v);
            m_frameCropRightOffset  = br.get_ue_golomb(); // ue(v);
            m_frameCropTopOffset    = br.get_ue_golomb(); // ue(v);
            m_frameCropBottomOffset = br.get_ue_golomb(); // ue(v);
        }

        br.get_ue_golomb(); // bit_depth_luma_minus8   ue(v);
        br.get_ue_golomb(); // bit_depth_chroma_minus8 ue(v);
    }

#if 1
    /* Potentially a lot more bits to wade through to get to the VUI
       information, so only do it if it looks like the info has changed.
    */
    if (m_sarWidth != 0 && m_sarHeight != 0 &&
        (m_picWidth + m_picHeight +
         m_frameCropLeftOffset + m_frameCropRightOffset +
         m_frameCropTopOffset + m_frameCropBottomOffset) == m_resolutionCheck)
        return true;
    m_resolutionCheck = (m_picWidth + m_picHeight +
                         m_frameCropLeftOffset + m_frameCropRightOffset +
                         m_frameCropTopOffset + m_frameCropBottomOffset);
#endif

    sps->log2_max_pic_order_cnt_lsb = br.get_ue_golomb() + 4; // ue(v);
    if (sps->log2_max_pic_order_cnt_lsb > 16)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("SPS log2_max_pic_order_cnt_lsb %1 > 16")
            .arg(sps->log2_max_pic_order_cnt_lsb));
        sps->log2_max_pic_order_cnt_lsb = 16;
    }
    // MaxPicOrderCntLsb = 2 ^ ( log2_max_pic_order_cnt_lsb_minus4 + 4 )

    sps->sub_layer_ordering_info_present_flag = br.get_bits(1); // u(1)
    for (i = (sps->sub_layer_ordering_info_present_flag ? 0 :
              max_sub_layers_minus1); i <= max_sub_layers_minus1; ++i)
    {
        max_dec_pic_buffering_minus1[i] = br.get_ue_golomb(); // ue(v);
        if (max_dec_pic_buffering_minus1[i] > 16)
        {
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("max_dec_pic_bufering_minus1[%1] %2 > 16")
                .arg(i)
                .arg(max_dec_pic_buffering_minus1[i]));
        }
        br.get_ue_golomb(); // sps_max_num_reorder_pics[i] ue(v);
        br.get_ue_golomb(); // sps_max_latency_increase_plus1[i] ue(v);
    }

#if 0 // Unneeded
    /* setting default values if
     * sps->sub_layer_ordering_info_present_flag is zero */
    if (!sps_sub_layer_ordering_info_present_flag && sps_max_sub_layers_minus1)
    {
        for (i = 0; i <= (sps_max_sub_layers_minus1 - 1); ++i)
        {
            max_dec_pic_buffering_minus1[i] =
                max_dec_pic_buffering_minus1[sps_max_sub_layers_minus1];
            max_num_reorder_pics[i] =
                max_num_reorder_pics[sps_max_sub_layers_minus1];
            max_latency_increase_plus1[i] =
                max_latency_increase_plus1[sps_max_sub_layers_minus1];
        }
    }
#endif

    sps->log2_min_luma_coding_block_size = br.get_ue_golomb() + 3; // _minus3 ue(v);
    sps->log2_diff_max_min_luma_coding_block_size = br.get_ue_golomb(); //  ue(v);
    br.get_ue_golomb(); // log2_min_luma_transform_block_size_minus2 ue(v);
    br.get_ue_golomb(); // log2_diff_max_min_luma_transform_block_size ue(v);
    br.get_ue_golomb(); // max_transform_hierarchy_depth_inter ue(v);
    br.get_ue_golomb(); // max_transform_hierarchy_depth_intra ue(v);

    if (br.get_bits(1)) // scaling_list_enabled_flag // u(1)
    {
        ScalingList scaling_list;

        /* When not present, the value of
         * sps_infer_scaling_list_flag is inferred to be 0 */
        bool sps_infer_scaling_list_flag = MultiLayerExtSpsFlag ?
                                           br.get_bits(1) : false; // u(1)
        if (sps_infer_scaling_list_flag)
        {
            br.get_bits(6); // sps_scaling_list_ref_layer_id; u(6)
        }
        else
        {
            if (br.get_bits(1)) // sps_scaling_list_data_present_flag;
                scalingListData(br, scaling_list, false);
        }
    }

    br.get_bits(1);     // amp_enabled_flag u(1)
    br.get_bits(1);     // sample_adaptive_offset_enabled_flag u(1)
    if (br.get_bits(1)) // pcm_enabled_flag u(1)
    {
        br.get_bits(4);     // pcm_sample_bit_depth_luma_minus1 u(4);
        br.get_bits(4);     // pcm_sample_bit_depth_chroma_minus1 u(4);
        br.get_ue_golomb(); // log2_min_pcm_luma_coding_block_size_minus3 ue(v);
        br.get_ue_golomb(); // log2_diff_max_min_pcm_luma_coding_block_size ue(v);
        br.get_bits(1);     // pcm_loop_filter_disabled_flag u(1)
    }

    uint num_short_term_ref_pic_sets = br.get_ue_golomb(); //  ue(v);
    if (num_short_term_ref_pic_sets > short_term_ref_pic_set.size() - 1 )
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("num_short_term_ref_pic_sets %1 > 64")
            .arg(num_short_term_ref_pic_sets));
        num_short_term_ref_pic_sets = short_term_ref_pic_set.size() - 1;
    }
    for(i = 0; i < num_short_term_ref_pic_sets; ++i)
    {
        if (!shortTermRefPicSet(br, i, num_short_term_ref_pic_sets,
                                short_term_ref_pic_set,
                        max_dec_pic_buffering_minus1[max_sub_layers_minus1]))
        {
            return false;
        }
    }

    if (br.get_bits(1)) // long_term_ref_pics_present_flag u(1)
    {
        uint num_long_term_ref_pics_sps = br.get_ue_golomb(); // ue(v);
        for (i = 0; i < num_long_term_ref_pics_sps; ++i)
        {
            /*
              lt_ref_pic_poc_lsb_sps[i] specifies the picture order
              count modulo MaxPicOrderCntLsb of the i-th candidate
              long-term reference picture specified in the SPS. The
              number of bits used to represent lt_ref_pic_poc_lsb_sps[
              i ] is equal to log2_max_pic_order_cnt_lsb_minus4 + 4.
            */
            m_poc[i] = br.get_bits(sps->log2_max_pic_order_cnt_lsb); // u(v)
            LOG(VB_RECORD, LOG_WARNING, LOC +
                QString("POC[%1] %2").arg(i).arg(m_poc[i]));
            br.get_bits(1); // used_by_curr_pic_lt_sps_flag[i] u(1)
        }
    }

    br.get_bits(1); //sps_temporal_mvp_enabled_flag u(1)
    br.get_bits(1); //strong_intra_smoothing_enabled_flag u(1)

    /*
      vui_parameters_present_flag equal to 1 specifies that the
      vui_parameters() syntax structure as specified in Annex E is
      present. vui_parameters_present_flag equal to 0 specifies that
      the vui_parameters() syntax structure as specified in Annex E
      is not present.
     */
    if (br.get_bits(1)) // vui_parameters_present_flag
        vui_parameters(br, true);

    return true;
}


/*
  F.7.3.2.1 Video parameter set RBSP
*/
bool HEVCParser::parseVPS(BitReader& br)
{
    int i = 0;

    uint8_t vps_id = br.get_bits(4);  // vps_video_parameter_set_id u(4)
    br.get_bits(1);    // vps_base_layer_internal_flag u(1)
    br.get_bits(1);    // vps_base_layer_available_flag u(1)
    br.get_bits(6);    // vps_max_layers_minus1 u(6)
    uint8_t max_sub_layers_minus1 = br.get_bits(3); // u(3)
    br.get_bits(1);    // vps_temporal_id_nesting_flag u(1)

    /* uint16_t check = */ br.get_bits(16); //  vps_reserved_0xffff_16bits u(16)

    m_vps[vps_id].max_sub_layers = max_sub_layers_minus1 + 1;
    if (!profileTierLevel(br, true, max_sub_layers_minus1))
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("Failed to parse VPS profile tier level."));
        return false;
    }

    bool vps_sub_layer_ordering_info_present_flag = br.get_bits(1); // u(1)
    for (i = (vps_sub_layer_ordering_info_present_flag ? 0 :
              max_sub_layers_minus1);
         i <= max_sub_layers_minus1; ++i)
    {
        br.get_ue_golomb(); // vps_max_dec_pic_buffering_minus1[i]; ue(v)
        br.get_ue_golomb(); // vps_max_num_reorder_pics[i]; ue(v)
        br.get_ue_golomb(); // vps_max_latency_increase_plus1[i]; ue(v)
    }

#if 0 // Unneeded
    /* setting default values if
     * vps->sub_layer_ordering_info_present_flag is zero */
    if (!vps_sub_layer_ordering_info_present_flag &&
        max_sub_layers_minus1)
    {
        for (i = 0; i <= (max_sub_layers_minus1 - 1); ++i)
        {
            max_dec_pic_buffering_minus1[i] =
                max_dec_pic_buffering_minus1[max_sub_layers_minus1];
            max_num_reorder_pics[i] =
                max_num_reorder_pics[max_sub_layers_minus1];
            max_latency_increase_plus1[i] =
                max_latency_increase_plus1[max_sub_layers_minus1];
        }
    }
#endif

    uint8_t vps_max_layer_id = br.get_bits(6); // u(6)
    int vps_num_layer_sets_minus1 = br.get_ue_golomb(); // ue(v)
    for (i = 1; i <= vps_num_layer_sets_minus1; ++i)
    {
        for (int j = 0; j <= vps_max_layer_id; ++j)
        {
            br.get_bits(1); // layer_id_included_flag[i][j] u(1)
        }
    }
    if (vps_num_layer_sets_minus1 < 0)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("vps_num_layer_sets_minus1 %1 < 0")
                .arg(vps_num_layer_sets_minus1));
    }

    if (br.get_bits(1)) // vps_timing_info_present_flag u(1)
    {
        /*
          vps_num_units_in_tick is the number of time units of a clock
          operating at the frequency vps_time_scale Hz that
          corresponds to one increment (called a clock tick) of a
          clock tick counter. The value of vps_num_units_in_tick shall
          be greater than 0. A clock tick, in units of seconds, is
          equal to the quotient of vps_num_units_in_tick divided by
          vps_time_scale. For example, when the picture rate of a
          video signal is 25 Hz, vps_time_scale may be equal to 27 000
          000 and vps_num_units_in_tick may be equal to 1 080 000, and
          consequently a clock tick may be 0.04 seconds.
        */
        m_unitsInTick = br.get_bits(32); // vps_num_units_in_tick

        /*
          vps_time_scale is the number of time units that pass in one
          second. For example, a time coordinate system that measures
          time using a 27 MHz clock has a vps_time_scale of 27 000
          000. The value of vps_time_scale shall be greater than 0.
        */
        m_timeScale = br.get_bits(32); // vps_time_scale

        if (br.get_bits(1)) // vps_poc_proportional_to_timing_flag) u(1)
            br.get_ue_golomb_long(); // vps_num_ticks_poc_diff_one_minus1 ue(v)

        LOG(VB_RECORD, LOG_DEBUG,
            QString("VUI unitsInTick %1 timeScale %2 fixedRate %3")
            .arg(m_unitsInTick)
            .arg(m_timeScale)
            .arg(m_fixedRate));

#if 0  // We don't need the rest.
        uint vps_num_hrd_parameters = br.get_ue_golomb(); // ue(v)
        for (i = 0; i < vps_num_hrd_parameters; ++i)
        {
            br.get_ue_golomb(); // hrd_layer_set_idx[i]  ue(v)
            if (i > 0)
                cprms_present_flag[i] = br.get_bits(1); // u(1)
            hrd_parameters(cprms_present_flag[i], max_sub_layers_minus1);
        }
#endif
    }

#if 0 // We don't need the rest.
    bool vps_extension_flag = br.get_bits(1); // u(1)
    if (vps_extension_flag)
    {
        while (!byte_aligned())
        {
            br.get_bits(1); // vps_extension_alignment_bit_equal_to_one u(1)
        }
        vps_extension();
        vps_extension2_flag = br.get_bits(1); // u(1)
        if (vps_extension2_flag)
        {
            while (more_rbsp_data())
                br.get_bits(1); // vps_extension_data_flag u(1)
        }
    }
    rbsp_trailing_bits();
#endif

    return true;
}

/* 7.3.2.3.1 General picture parameter set RBSP syntax */
bool HEVCParser::parsePPS(BitReader& br)
{
    uint pps_id = br.get_ue_golomb(); // pps_pic_parameter_set_id ue(v)
    PPS* pps = &m_pps[pps_id];

    pps->sps_id = br.get_ue_golomb(); // pps_seq_parameter_set_id; ue(v)
    pps->dependent_slice_segments_enabled_flag = br.get_bits(1); // u(1)

    pps->output_flag_present_flag = br.get_bits(1); // u(1)
    pps->num_extra_slice_header_bits = br.get_bits(3); // u(3)

#if 0 // Rest not needed
    sign_data_hiding_enabled_flag;
    cabac_init_present_flag;
    num_ref_idx_l0_default_active_minus1;
    num_ref_idx_l1_default_active_minus1;
    init_qp_minus26;
    constrained_intra_pred_flag;
    transform_skip_enabled_flag;
    cu_qp_delta_enabled_flag;
    if( cu_qp_delta_enabled_flag )
        diff_cu_qp_delta_depth;
    pps_cb_qp_offset;
    pps_cr_qp_offset;
    pps_slice_chroma_qp_offsets_present_flag;
    weighted_pred_flag;
    weighted_bipred_flag;
    transquant_bypass_enabled_flag;
    tiles_enabled_flag;
    entropy_coding_sync_enabled_flag;
    if( tiles_enabled_flag ) {
        num_tile_columns_minus1;
        num_tile_rows_minus1;
        uniform_spacing_flag;
        if( !uniform_spacing_flag ) {
            for( i = 0; i < num_tile_columns_minus1; i++ )
                column_width_minus1[ i ];
            for( i = 0; i < num_tile_rows_minus1; i++ )
                row_height_minus1[ i ];
        }
        loop_filter_across_tiles_enabled_flag;
        pps_loop_filter_across_slices_enabled_flag;
        deblocking_filter_control_present_flag;
        if( deblocking_filter_control_present_flag ) {
        }
        deblocking_filter_override_enabled_flag;
        pps_deblocking_filter_disabled_flag;
        if( !pps_deblocking_filter_disabled_flag ) {
            pps_beta_offset_div2;
            pps_tc_offset_div2;
        }
    }
    pps_scaling_list_data_present_flag;
    if( pps_scaling_list_data_present_flag )
        scaling_list_data( );
    lists_modification_present_flag;
    log2_parallel_merge_level_minus2;
    slice_segment_header_extension_present_flag;
    pps_extension_present_flag;
    if( pps_extension_present_flag ) {
        pps_range_extension_flag;
        pps_multilayer_extension_flag;
        pps_3d_extension_flag;
        pps_scc_extension_flag;
        pps_extension_4bits;
    }
    if( pps_range_extension_flag )
        pps_range_extension( );
    if( pps_multilayer_extension_flag )
        pps_multilayer_extension( ); /* specified in Annex F */
    if( pps_3d_extension_flag )
        pps_3d_extension( ); /* specified in Annex I */
    if( pps_scc_extension_flag )
        pps_scc_extension( );
    if( pps_extension_4bits )
        while( more_rbsp_data( ) )
            pps_extension_data_flag;
    rbsp_trailing_bits( )
#endif

    return true;
}

// Following the lead of AVCParser, ignore the left cropping.
uint HEVCParser::pictureWidthCropped(void) const
{
    static const std::array<const uint8_t,5> subwc {1, 2, 2, 1, 1};
    const uint8_t crop_unit_x = subwc[m_chromaFormatIdc];
    // uint crop_rect_x = m_frameCropLeftOffset * crop_unit_x;

    return m_picWidth - ((/* m_frameCropLeftOffset + */
                          m_frameCropRightOffset) * crop_unit_x);
}

// Following the lead of AVCParser, ignore the top cropping.
uint HEVCParser::pictureHeightCropped(void) const
{
    static const std::array<const uint8_t,5> subhc {1, 2, 1, 1, 1};
    const uint8_t crop_unit_y = subhc[m_chromaFormatIdc];
    // uint crop_rect_y = m_frameCropTopOffset * crop_unit_y;

    return m_picHeight - ((/* m_frameCropTopOffset + */
                           m_frameCropBottomOffset) * crop_unit_y);
}

MythAVRational HEVCParser::getFrameRate() const
{
    return (m_unitsInTick == 0) ? MythAVRational(0) :
             MythAVRational(m_timeScale, m_unitsInTick);
}
