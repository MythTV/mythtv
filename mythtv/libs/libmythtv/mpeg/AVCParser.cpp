// MythTV headers
#include "AVCParser.h"
#include <iostream>

#include "libmythbase/mythlogging.h"
#include "recorders/dtvrecorder.h" // for FrameRate

#include <cmath>
#include <strings.h>

#include "bitreader.h"
#include "bytereader.h"

/*
  Most of the comments below were cut&paste from ITU-T Rec. H.264
  as found here:  http://www.itu.int/rec/T-REC-H.264/e
 */

/*
  Useful definitions:

  * access unit: A set of NAL units always containing exactly one
  primary coded picture. In addition to the primary coded picture, an
  access unit may also contain one or more redundant coded pictures
  or other NAL units not containing slices or slice data partitions
  of a coded picture. The decoding of an access unit always results
  in a decoded picture.

  * instantaneous decoding refresh (IDR) access unit: An access unit in
  which the primary coded picture is an IDR picture.

  * instantaneous decoding refresh (IDR) picture: A coded picture
  containing only slices with I or SI slice types that causes the
  decoding process to mark all reference pictures as "unused for
  reference" immediately after decoding the IDR picture. After the
  decoding of an IDR picture all following coded pictures in decoding
  order can be decoded without inter prediction from any picture
  decoded prior to the IDR picture. The first picture of each coded
  video sequence is an IDR picture.

  * NAL unit: A syntax structure containing an indication of the type
  of data to follow and bytes containing that data in the form of an
  RBSP interspersed as necessary with emulation prevention bytes.

  * raw byte sequence payload (RBSP): A syntax structure containing an
  integer number of bytes that is encapsulated in a NAL unit. An RBSP
  is either empty or has the form of a string of data bits containing
  syntax elements followed by an RBSP stop bit and followed by zero
  or more subsequent bits equal to 0.

  * raw byte sequence payload (RBSP) stop bit: A bit equal to 1 present
  within a raw byte sequence payload (RBSP) after a string of data
  bits. The location of the end of the string of data bits within an
  RBSP can be identified by searching from the end of the RBSP for
  the RBSP stop bit, which is the last non-zero bit in the RBSP.

  * parity: The parity of a field can be top or bottom.

  * picture: A collective term for a field or a frame.

  * picture parameter set: A syntax structure containing syntax
  elements that apply to zero or more entire coded pictures as
  determined by the m_picParameterSetId syntax element found in each
  slice header.

  * primary coded picture: The coded representation of a picture to be
  used by the decoding process for a bitstream conforming to this
  Recommendation | International Standard. The primary coded picture
  contains all macroblocks of the picture. The only pictures that
  have a normative effect on the decoding process are primary coded
  pictures. See also redundant coded picture.

  * VCL: Video Coding Layer

  - The VCL is specified to efficiently represent the content of the
  video data. The NAL is specified to format that data and provide
  header information in a manner appropriate for conveyance on a
  variety of communication channels or storage media. All data are
  contained in NAL units, each of which contains an integer number of
  bytes. A NAL unit specifies a generic format for use in both
  packet-oriented and bitstream systems. The format of NAL units for
  both packet-oriented transport and byte stream is identical except
  that each NAL unit can be preceded by a start code prefix and extra
  padding bytes in the byte stream format.

*/

void AVCParser::Reset(void)
{
    H2645Parser::Reset();

    m_bottomFieldFlag     = -1;
    m_deltaPicOrderAlwaysZeroFlag = 0;
    m_deltaPicOrderCntBottom     = 0;
    m_deltaPicOrderCnt.fill(0);
    m_fieldPicFlag     = -1;
    m_frameMbsOnlyFlag = -1;
    m_frameNum = -1;
    // The value of m_idrPicId shall be in the range of 0 to 65535, inclusive.
    m_idrPicId = 65536;
    m_log2MaxFrameNum = 0;
    m_log2MaxPicOrderCntLsb = 0;
    m_nalRefIdc     = 111;  //  != [0|1|2|3]
    m_nalUnitType     = UNKNOWN;
    m_numRefFrames    = 0;
    m_picOrderCntLsb     = 0;
    m_picOrderCntType     = 0;
    m_picOrderPresentFlag = -1;
    m_picParameterSetId = -1;
    m_prevBottomFieldFlag = -1;
    m_prevDeltaPicOrderCntBottom = 0;
    m_prevDeltaPicOrderCnt.fill(0);
    m_prevFieldPicFlag = -1;
    m_prevFrameNum = -1;
    m_prevIdrPicId = 65536;
    m_prevNALRefIdc = 111;  //  != [0|1|2|3]
    m_prevNalUnitType = UNKNOWN;
    m_prevPicOrderCntLsb = 0;
    m_prevPicOrderCntType = 0;
    m_prevPicParameterSetId = -1;
    m_redundantPicCnt = 0;
    m_redundantPicCntPresentFlag = 0;
    m_seqParameterSetId = 0;
    m_sliceType = SLICE_UNDEF;

    resetRBSP();
}


QString AVCParser::NAL_type_str(int8_t type)
{
    switch (type)
    {
      case UNKNOWN:
        return "UNKNOWN";
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
    }
    return "OTHER";
}

bool AVCParser::new_AU(void)
{
    /*
      An access unit consists of one primary coded picture, zero or more
      corresponding redundant coded pictures, and zero or more non-VCL NAL
      units. The association of VCL NAL units to primary or redundant coded
      pictures is described in subclause 7.4.1.2.5.

      The first access unit in the bitstream starts with the first NAL unit
      of the bitstream.

      The first of any of the following NAL units after the last VCL NAL
      unit of a primary coded picture specifies the start of a new access
      unit.

      –    access unit delimiter NAL unit (when present)
      –    sequence parameter set NAL unit (when present)
      –    picture parameter set NAL unit (when present)
      –    SEI NAL unit (when present)
      –    NAL units with m_nalUnitType in the range of 14 to 18, inclusive
      –    first VCL NAL unit of a primary coded picture (always present)
    */

    /*
      7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded
      picture This subclause specifies constraints on VCL NAL unit syntax
      that are sufficient to enable the detection of the first VCL NAL unit
      of each primary coded picture.

      Any coded slice NAL unit or coded slice data partition A NAL unit of
      the primary coded picture of the current access unit shall be
      different from any coded slice NAL unit or coded slice data partition
      A NAL unit of the primary coded picture of the previous access unit in
      one or more of the following ways.

      - frame_num differs in value. The value of frame_num used to
        test this condition is the value of frame_num that appears in
        the syntax of the slice header, regardless of whether that value
        is inferred to have been equal to 0 for subsequent use in the
        decoding process due to the presence of
        memory_management_control_operation equal to 5.
          Note: If the current picture is an IDR picture FrameNum and
          PrevRefFrameNum are set equal to 0.
      - m_picParameterSetId differs in value.
      - m_fieldPicFlag differs in value.
      - bottom_field_flag is present in both and differs in value.
      - nal_ref_idc differs in value with one of the nal_ref_idc
        values being equal to 0.
      - m_picOrderCntType is equal to 0 for both and either
        m_picOrderCntLsb differs in value, or m_deltaPicOrderCntBottom
        differs in value.
      - m_picOrderCntType is equal to 1 for both and either
        m_deltaPicOrderCnt[0] differs in value, or
        m_deltaPicOrderCnt[1] differs in value.
      - m_nalUnitType differs in value with one of the m_nalUnitType values
        being equal to 5.
      - m_nalUnitType is equal to 5 for both and m_idrPicId differs in
        value.

      NOTE – Some of the VCL NAL units in redundant coded pictures or some
      non-VCL NAL units (e.g. an access unit delimiter NAL unit) may also
      be used for the detection of the boundary between access units, and
      may therefore aid in the detection of the start of a new primary
      coded picture.

    */

    bool       result = false;

    if (m_prevFrameNum != -1)
    {
        // Need previous slice information for comparison

        if (m_nalUnitType != SLICE_IDR && m_frameNum != m_prevFrameNum)
            result = true; // NOLINT(bugprone-branch-clone)
        else if (m_prevPicParameterSetId != -1 &&
                 m_picParameterSetId != m_prevPicParameterSetId)
            result = true;
        else if (m_fieldPicFlag != m_prevFieldPicFlag)
            result = true;
        else if ((m_bottomFieldFlag != -1 && m_prevBottomFieldFlag != -1) &&
                 m_bottomFieldFlag != m_prevBottomFieldFlag)
            result = true;
        else if ((m_nalRefIdc == 0 || m_prevNALRefIdc == 0) &&
                 m_nalRefIdc != m_prevNALRefIdc)
            result = true;
        else if ((m_picOrderCntType == 0 && m_prevPicOrderCntType == 0) &&
                 (m_picOrderCntLsb != m_prevPicOrderCntLsb ||
                  m_deltaPicOrderCntBottom != m_prevDeltaPicOrderCntBottom))
            result = true;
        else if ((m_picOrderCntType == 1 && m_prevPicOrderCntType == 1) &&
                 (m_deltaPicOrderCnt[0] != m_prevDeltaPicOrderCnt[0] ||
                  m_deltaPicOrderCnt[1] != m_prevDeltaPicOrderCnt[1]))
            result = true;
        else if ((m_nalUnitType == SLICE_IDR ||
                  m_prevNalUnitType == SLICE_IDR) &&
                 m_nalUnitType != m_prevNalUnitType)
            result = true;
        else if ((m_nalUnitType == SLICE_IDR &&
                  m_prevNalUnitType == SLICE_IDR) &&
                 m_idrPicId != m_prevIdrPicId)
            result = true;
    }

    m_prevFrameNum = m_frameNum;
    m_prevPicParameterSetId = m_picParameterSetId;
    m_prevFieldPicFlag = m_fieldPicFlag;
    m_prevBottomFieldFlag = m_bottomFieldFlag;
    m_prevNALRefIdc = m_nalRefIdc;
    m_prevPicOrderCntLsb = m_picOrderCntLsb;
    m_prevDeltaPicOrderCntBottom = m_deltaPicOrderCntBottom;
    m_prevDeltaPicOrderCnt[0] = m_deltaPicOrderCnt[0];
    m_prevDeltaPicOrderCnt[1] = m_deltaPicOrderCnt[1];
    m_prevNalUnitType = m_nalUnitType;
    m_prevIdrPicId = m_idrPicId;

    return result;
}

uint32_t AVCParser::addBytes(const uint8_t  *bytes,
                              const uint32_t  byte_count,
                              const uint64_t  stream_offset)
{
    const uint8_t *startP = bytes;

    m_stateChanged = false;
    m_onFrame      = false;
    m_onKeyFrame   = false;

    while (startP < bytes + byte_count && !m_onFrame)
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
            processRBSP(found_start_code); /* Call may set have_uinfinished_NAL
                                            * to false */
        }

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
                    "AVCParser::addBytes: Found new start "
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

/*
  m_nalUnitType specifies the type of RBSP data structure contained in
  the NAL unit as specified in Table 7-1. VCL NAL units
  are specified as those NAL units having m_nalUnitType
  equal to 1 to 5, inclusive. All remaining NAL units
  are called non-VCL NAL units:

  0  Unspecified
  1  Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
  2  Coded slice data partition A slice_data_partition_a_layer_rbsp( )
  3  Coded slice data partition B slice_data_partition_b_layer_rbsp( )
  4  Coded slice data partition C slice_data_partition_c_layer_rbsp( )
  5  Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
  6  Supplemental enhancement information (SEI) 5 sei_rbsp( )
  7  Sequence parameter set (SPS) seq_parameter_set_rbsp( )
  8  Picture parameter set pic_parameter_set_rbsp( )
  9  Access unit delimiter access_unit_delimiter_rbsp( )
  10 End of sequence end_of_seq_rbsp( )
  11 End of stream end_of_stream_rbsp( )
*/
            m_nalUnitType = m_syncAccumulator & 0x1f;
            m_nalRefIdc = (m_syncAccumulator >> 5) & 0x3;

            bool good_nal_unit = true;
            if (m_nalRefIdc)
            {
                /* nal_ref_idc shall be equal to 0 for all NAL units having
                 * m_nalUnitType equal to 6, 9, 10, 11, or 12.
                 */
                if (m_nalUnitType == SEI ||
                    (m_nalUnitType >= AU_DELIMITER &&
                     m_nalUnitType <= FILLER_DATA))
                    good_nal_unit = false;
            }
            else
            {
                /* nal_ref_idc shall not be equal to 0 for NAL units with
                 * m_nalUnitType equal to 5
                 */
                if (m_nalUnitType == SLICE_IDR)
                    good_nal_unit = false;
            }

            if (good_nal_unit)
            {
                if (m_nalUnitType == SPS || m_nalUnitType == PPS ||
                    m_nalUnitType == SEI || NALisSlice(m_nalUnitType))
                {
                    /* This is a NAL we need to parse. We may have the body
                     * of it in the part of the stream past to us this call,
                     * or we may get the rest in subsequent calls to addBytes.
                     * Either way, we set m_haveUnfinishedNAL, so that we
                     * start filling the rbsp buffer
                     */
                      m_haveUnfinishedNAL = true;
                }
                else if (m_nalUnitType == AU_DELIMITER)
                {
                    set_AU_pending();
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "AVCParser::addbytes: malformed NAL units");
            }
        } //found start code
    }

    return startP - bytes;
}


void AVCParser::processRBSP(bool rbsp_complete)
{
    auto br = BitReader(m_rbspBuffer, m_rbspIndex);

    if (m_nalUnitType == SEI)
    {
        /* SEI cannot be parsed without knowing its size. If
         * we haven't got the whole rbsp, return and wait for
         * the rest
         */
        if (!rbsp_complete)
            return;

        set_AU_pending();

        decode_SEI(br);
    }
    else if (m_nalUnitType == SPS)
    {
        /* Best wait until we have the whole thing */
        if (!rbsp_complete)
            return;

        set_AU_pending();

        if (!m_seenSPS)
            m_spsOffset = m_pktOffset;

        decode_SPS(br);
    }
    else if (m_nalUnitType == PPS)
    {
        /* Best wait until we have the whole thing */
        if (!rbsp_complete)
            return;

        set_AU_pending();

        decode_PPS(br);
    }
    else
    {
        /* Need only parse the header. So return only
         * if we have insufficient bytes */
        if (!rbsp_complete && m_rbspIndex < kMaxSliceHeaderSize)
            return;

        decode_Header(br);

        if (new_AU())
            set_AU_pending();
    }

    /* If we got this far, we managed to parse a sufficient
     * prefix of the current NAL. We can go onto the next. */
    m_haveUnfinishedNAL = false;

    if (m_auPending && NALisSlice(m_nalUnitType))
    {
        /* Once we know the slice type of a new AU, we can
         * determine if it is a keyframe or just a frame
         */
        m_auPending = false;
        m_stateChanged = m_seenSPS;

        m_onFrame = true;
        m_frameStartOffset = m_auOffset;

        if (m_isKeyframe || m_auContainsKeyframeMessage)
        {
            m_onKeyFrame = true;
            m_keyframeStartOffset = m_auOffset;
        }
    }
}

/*
  7.4.3 Slice header semantics
*/
bool AVCParser::decode_Header(BitReader& br)
{
    m_isKeyframe = false;

    if (m_log2MaxFrameNum == 0)
    {
        /* SPS has not been parsed yet */
        return false;
    }

    /*
      m_firstMbInSlice specifies the address of the first macroblock
      in the slice. When arbitrary slice order is not allowed as
      specified in Annex A, the value of first_mb_in_slice is
      constrained as follows.

      – If m_separateColourPlaneFlag is equal to 0, the value of
      first_mb_in_slice shall not be less than the value of
      first_mb_in_slice for any other slice of the current picture
      that precedes the current slice in decoding order.

      – Otherwise (m_separateColourPlaneFlag is equal to 1), the value of
      first_mb_in_slice shall not be less than the value of
      first_mb_in_slice for any other slice of the current picture
      that precedes the current slice in decoding order and has the
      same value of colour_plane_id.
     */
    /* uint first_mb_in_slice = */ br.get_ue_golomb();

    /*
      slice_type specifies the coding type of the slice according to
      Table 7-6.   e.g. P, B, I, SP, SI

      When m_nalUnitType is equal to 5 (IDR picture), slice_type shall
      be equal to 2, 4, 7, or 9 (I or SI)
     */
    m_sliceType = br.get_ue_golomb_31();

    /* s->pict_type = golomb_to_pict_type[slice_type % 5];
     */

    /*
      m_picParameterSetId specifies the picture parameter set in
      use. The value of m_picParameterSetId shall be in the range of
      0 to 255, inclusive.
     */
    m_picParameterSetId = br.get_ue_golomb();

    /*
      m_separateColourPlaneFlag equal to 1 specifies that the three
      colour components of the 4:4:4 chroma format are coded
      separately. m_separateColourPlaneFlag equal to 0 specifies that
      the colour components are not coded separately.  When
      m_separateColourPlaneFlag is not present, it shall be inferred
      to be equal to 0. When m_separateColourPlaneFlag is equal to 1,
      the primary coded picture consists of three separate components,
      each of which consists of coded samples of one colour plane (Y,
      Cb or Cr) that each use the monochrome coding syntax. In this
      case, each colour plane is associated with a specific
      colour_plane_id value.
     */
    if (m_separateColourPlaneFlag)
        br.get_bits(2);  // colour_plane_id

    /*
      frame_num is used as an identifier for pictures and shall be
      represented by m_log2MaxFrameNum_minus4 + 4 bits in the
      bitstream....

      If the current picture is an IDR picture, frame_num shall be equal to 0.

      When m_maxNumRefFrames is equal to 0, slice_type shall be equal
          to 2, 4, 7, or 9.
    */
    m_frameNum = br.get_bits(m_log2MaxFrameNum);

    /*
      m_fieldPicFlag equal to 1 specifies that the slice is a slice of a
      coded field. m_fieldPicFlag equal to 0 specifies that the slice is a
      slice of a coded frame. When m_fieldPicFlag is not present it shall be
      inferred to be equal to 0.

      bottom_field_flag equal to 1 specifies that the slice is part of a
      coded bottom field. bottom_field_flag equal to 0 specifies that the
      picture is a coded top field. When this syntax element is not present
      for the current slice, it shall be inferred to be equal to 0.
    */
    if (!m_frameMbsOnlyFlag)
    {
        m_fieldPicFlag = br.get_bits(1);
        m_bottomFieldFlag = m_fieldPicFlag ? br.get_bits(1) : 0;
    }
    else
    {
        m_fieldPicFlag = 0;
        m_bottomFieldFlag = -1;
    }

    /*
      m_idrPicId identifies an IDR picture. The values of m_idrPicId
      in all the slices of an IDR picture shall remain unchanged. When
      two consecutive access units in decoding order are both IDR
      access units, the value of m_idrPicId in the slices of the first
      such IDR access unit shall differ from the m_idrPicId in the
      second such IDR access unit. The value of m_idrPicId shall be in
      the range of 0 to 65535, inclusive.
     */
    if (m_nalUnitType == SLICE_IDR)
    {
        m_idrPicId = br.get_ue_golomb();
        m_isKeyframe = true;
    }
    else
    {
        m_isKeyframe = (m_iIsKeyframe && isKeySlice(m_sliceType));
    }

    /*
      m_picOrderCntLsb specifies the picture order count modulo
      MaxPicOrderCntLsb for the top field of a coded frame or for a coded
      field. The size of the m_picOrderCntLsb syntax element is
      m_log2MaxPicOrderCntLsbMinus4 + 4 bits. The value of the
      pic_order_cnt_lsb shall be in the range of 0 to MaxPicOrderCntLsb – 1,
      inclusive.

      m_deltaPicOrderCntBottom specifies the picture order count
      difference between the bottom field and the top field of a coded
      frame.
    */
    if (m_picOrderCntType == 0)
    {
        m_picOrderCntLsb = br.get_bits(m_log2MaxPicOrderCntLsb);

        if ((m_picOrderPresentFlag == 1) && !m_fieldPicFlag)
            m_deltaPicOrderCntBottom = br.get_se_golomb();
        else
            m_deltaPicOrderCntBottom = 0;
    }
    else
    {
        m_deltaPicOrderCntBottom = 0;
    }

    /*
      m_deltaPicOrderAlwaysZeroFlag equal to 1 specifies that
      m_deltaPicOrderCnt[ 0 ] and m_deltaPicOrderCnt[ 1 ] are not
      present in the slice headers of the sequence and shall be
      inferred to be equal to 0. m_deltaPicOrderAlwaysZeroFlag
      equal to 0 specifies that m_deltaPicOrderCnt[ 0 ] is present in
      the slice headers of the sequence and m_deltaPicOrderCnt[ 1 ]
      may be present in the slice headers of the sequence.
    */
    if (m_deltaPicOrderAlwaysZeroFlag)
    {
        m_deltaPicOrderCnt[1] = m_deltaPicOrderCnt[0] = 0;
    }
    else if (m_picOrderCntType == 1)
    {
        /*
          m_deltaPicOrderCnt[ 0 ] specifies the picture order count
          difference from the expected picture order count for the top
          field of a coded frame or for a coded field as specified in
          subclause 8.2.1. The value of m_deltaPicOrderCnt[ 0 ] shall
          be in the range of -2^31 to 2^31 - 1, inclusive. When this
          syntax element is not present in the bitstream for the
          current slice, it shall be inferred to be equal to 0.

          m_deltaPicOrderCnt[ 1 ] specifies the picture order count
          difference from the expected picture order count for the
          bottom field of a coded frame specified in subclause
          8.2.1. The value of m_deltaPicOrderCnt[ 1 ] shall be in the
          range of -2^31 to 2^31 - 1, inclusive. When this syntax
          element is not present in the bitstream for the current
          slice, it shall be inferred to be equal to 0.
        */
        m_deltaPicOrderCnt[0] = br.get_se_golomb();

        if ((m_picOrderPresentFlag == 1) && !m_fieldPicFlag)
            m_deltaPicOrderCnt[1] = br.get_se_golomb();
        else
            m_deltaPicOrderCnt[1] = 0;
     }

    /*
      m_redundantPicCnt shall be equal to 0 for slices and slice data
      partitions belonging to the primary coded picture. The
      m_redundantPicCnt shall be greater than 0 for coded slices and
      coded slice data partitions in redundant coded pictures.  When
      m_redundantPicCnt is not present, its value shall be inferred to
      be equal to 0. The value of m_redundantPicCnt shall be in the
      range of 0 to 127, inclusive.
    */
    m_redundantPicCnt = m_redundantPicCntPresentFlag ? br.get_ue_golomb() : 0;

    return true;
}

/*
 * libavcodec used for example
 */
void AVCParser::decode_SPS(BitReader& br)
{
    m_seenSPS = true;

    int profile_idc = br.get_bits(8);
    br.get_bits(1);     // constraint_set0_flag
    br.get_bits(1);     // constraint_set1_flag
    br.get_bits(1);     // constraint_set2_flag
    br.get_bits(1);     // constraint_set3_flag
    br.get_bits(4);     // reserved
    br.get_bits(8);     // level_idc
    br.get_ue_golomb(); // sps_id

    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
        profile_idc == 244 || profile_idc == 44  || profile_idc == 83  ||
        profile_idc == 86  || profile_idc == 118 || profile_idc == 128 )
    { // high profile
        m_chromaFormatIdc = br.get_ue_golomb();
        if (m_chromaFormatIdc == 3)
            m_separateColourPlaneFlag = (br.get_bits(1) == 1);

        br.get_ue_golomb();     // bit_depth_luma_minus8
        br.get_ue_golomb();     // bit_depth_chroma_minus8
        br.get_bits(1);         // qpprime_y_zero_transform_bypass_flag

        if (br.get_bits(1))     // seq_scaling_matrix_present_flag
        {
            for (int idx = 0; idx < ((m_chromaFormatIdc != 3) ? 8 : 12); ++idx)
            {
                if (br.get_bits(1)) // Scaling list present
                {
                    int lastScale = 8;
                    int nextScale = 8;
                    int sl_n = ((idx < 6) ? 16 : 64);
                    for(int sl_i = 0; sl_i < sl_n; ++sl_i)
                    {
                        if (nextScale != 0)
                        {
                            int deltaScale = br.get_se_golomb();
                            nextScale = (lastScale + deltaScale + 256) % 256;
                        }
                        lastScale = (nextScale == 0) ? lastScale : nextScale;
                    }
                }
            }
        }
    }

    /*
      m_log2MaxFrameNum_minus4 specifies the value of the variable
      MaxFrameNum that is used in frame_num related derivations as
      follows:

       MaxFrameNum = 2( m_log2MaxFrameNum_minus4 + 4 )
     */
    m_log2MaxFrameNum = br.get_ue_golomb() + 4;

    /*
      m_picOrderCntType specifies the method to decode picture order
      count (as specified in subclause 8.2.1). The value of
      m_picOrderCntType shall be in the range of 0 to 2, inclusive.
     */
    m_picOrderCntType = br.get_ue_golomb();
    if (m_picOrderCntType == 0)
    {
        /*
          m_log2MaxPicOrderCntLsbMinus4 specifies the value of the
          variable MaxPicOrderCntLsb that is used in the decoding
          process for picture order count as specified in subclause
          8.2.1 as follows:

          MaxPicOrderCntLsb = 2( m_log2MaxPicOrderCntLsbMinus4 + 4 )

          The value of m_log2MaxPicOrderCntLsbMinus4 shall be in
          the range of 0 to 12, inclusive.
         */
        m_log2MaxPicOrderCntLsb = br.get_ue_golomb() + 4;
    }
    else if (m_picOrderCntType == 1)
    {
        /*
          m_deltaPicOrderAlwaysZeroFlag equal to 1 specifies that
          m_deltaPicOrderCnt[ 0 ] and m_deltaPicOrderCnt[ 1 ] are
          not present in the slice headers of the sequence and shall
          be inferred to be equal to
          0. m_deltaPicOrderAlwaysZeroFlag
         */
        m_deltaPicOrderAlwaysZeroFlag = br.get_bits(1);

        /*
          offset_for_non_ref_pic is used to calculate the picture
          order count of a non-reference picture as specified in
          8.2.1. The value of offset_for_non_ref_pic shall be in the
          range of -231 to 231 - 1, inclusive.
         */
        [[maybe_unused]] int offset_for_non_ref_pic = br.get_se_golomb();

        /*
          offset_for_top_to_bottom_field is used to calculate the
          picture order count of a bottom field as specified in
          subclause 8.2.1. The value of offset_for_top_to_bottom_field
          shall be in the range of -231 to 231 - 1, inclusive.
         */
        [[maybe_unused]] int offset_for_top_to_bottom_field = br.get_se_golomb();

        /*
          offset_for_ref_frame[ i ] is an element of a list of
          m_numRefFrames_in_pic_order_cnt_cycle values used in the
          decoding process for picture order count as specified in
          subclause 8.2.1. The value of offset_for_ref_frame[ i ]
          shall be in the range of -231 to 231 - 1, inclusive.
         */
        uint tmp = br.get_ue_golomb();
        for (uint idx = 0; idx < tmp; ++idx)
            br.get_se_golomb();  // offset_for_ref_frame[i]
    }

    /*
      m_numRefFrames specifies the maximum number of short-term and
      long-term reference frames, complementary reference field pairs,
      and non-paired reference fields that may be used by the decoding
      process for inter prediction of any picture in the
      sequence. m_numRefFrames also determines the size of the sliding
      window operation as specified in subclause 8.2.5.3. The value of
      m_numRefFrames shall be in the range of 0 to MaxDpbSize (as
      specified in subclause A.3.1 or A.3.2), inclusive.
     */
    m_numRefFrames = br.get_ue_golomb();
    /*
      gaps_in_frame_num_value_allowed_flag specifies the allowed
      values of frame_num as specified in subclause 7.4.3 and the
      decoding process in case of an inferred gap between values of
      frame_num as specified in subclause 8.2.5.2.
     */
    /* bool gaps_in_frame_num_allowed_flag = */ br.get_bits(1);

    /*
      pic_width_in_mbs_minus1 plus 1 specifies the width of each
      decoded picture in units of macroblocks.  16 macroblocks in a row
     */
    m_picWidth = (br.get_ue_golomb() + 1) * 16;
    /*
      pic_height_in_map_units_minus1 plus 1 specifies the height in
      slice group map units of a decoded frame or field.  16
      macroblocks in each column.
     */
    m_picHeight = (br.get_ue_golomb() + 1) * 16;

    /*
      m_frameMbsOnlyFlag equal to 0 specifies that coded pictures of
      the coded video sequence may either be coded fields or coded
      frames. m_frameMbsOnlyFlag equal to 1 specifies that every
      coded picture of the coded video sequence is a coded frame
      containing only frame macroblocks.
     */
    m_frameMbsOnlyFlag = br.get_bits(1);
    if (!m_frameMbsOnlyFlag)
    {
        m_picHeight *= 2;

        /*
          mb_adaptive_frame_field_flag equal to 0 specifies no
          switching between frame and field macroblocks within a
          picture. mb_adaptive_frame_field_flag equal to 1 specifies
          the possible use of switching between frame and field
          macroblocks within frames. When mb_adaptive_frame_field_flag
          is not present, it shall be inferred to be equal to 0.
         */
        br.get_bits(1); // mb_adaptive_frame_field_flag
    }

    br.get_bits(1);     // direct_8x8_inference_flag

    /*
      frame_cropping_flag equal to 1 specifies that the frame cropping
      offset parameters follow next in the sequence parameter
      set. frame_cropping_flag equal to 0 specifies that the frame
      cropping offset parameters are not present.
     */
    if (br.get_bits(1)) // frame_cropping_flag
    {
        m_frameCropLeftOffset   = br.get_ue_golomb();
        m_frameCropRightOffset  = br.get_ue_golomb();
        m_frameCropTopOffset    = br.get_ue_golomb();
        m_frameCropBottomOffset = br.get_ue_golomb();
    }

    /*
      vui_parameters_present_flag equal to 1 specifies that the
      vui_parameters( ) syntax structure as specified in Annex E is
      present. vui_parameters_present_flag equal to 0 specifies that
      the vui_parameters( ) syntax structure as specified in Annex E
      is not present.
     */
    if (br.get_bits(1)) // vui_parameters_present_flag
        vui_parameters(br, false);
}

void AVCParser::parse_SPS(uint8_t *sps, uint32_t sps_size,
                           bool& interlaced, int32_t& max_ref_frames)
{
    auto br = BitReader(sps, sps_size);
    decode_SPS(br);
    interlaced = (m_frameMbsOnlyFlag == 0);
    max_ref_frames = m_numRefFrames;
}

void AVCParser::decode_PPS(BitReader& br)
{
    /*
      m_picParameterSetId identifies the picture parameter set that
      is referred to in the slice header. The value of
      m_picParameterSetId shall be in the range of 0 to 255,
      inclusive.
     */
    m_picParameterSetId = br.get_ue_golomb();

    /*
      m_seqParameterSetId refers to the active sequence parameter
      set. The value of m_seqParameterSetId shall be in the range of
      0 to 31, inclusive.
     */
    m_seqParameterSetId = br.get_ue_golomb();
    br.get_bits(1); // entropy_coding_mode_flag;

    /*
      m_picOrderPresentFlag equal to 1 specifies that the picture
      order count related syntax elements are present in the slice
      headers as specified in subclause 7.3.3. m_picOrderPresentFlag
      equal to 0 specifies that the picture order count related syntax
      elements are not present in the slice headers.
     */
    m_picOrderPresentFlag = br.get_bits(1);

#if 0 // Rest not currently needed, and requires <math.h>
    uint num_slice_groups = br.get_ue_golomb() + 1;
    if (num_slice_groups > 1) // num_slice_groups (minus 1)
    {
        uint idx;

        switch (br.get_ue_golomb()) // slice_group_map_type
        {
          case 0:
            for (idx = 0; idx < num_slice_groups; ++idx)
                br.get_ue_golomb(); // run_length_minus1[idx]
            break;
          case 1:
            for (idx = 0; idx < num_slice_groups; ++idx)
            {
                br.get_ue_golomb(); // top_left[idx]
                br.get_ue_golomb(); // bottom_right[idx]
            }
            break;
          case 3:
          case 4:
          case 5:
            br.get_bits(1);     // slice_group_change_direction_flag
            br.get_ue_golomb(); // slice_group_change_rate_minus1
            break;
          case 6:
            uint pic_size_in_map_units = br.get_ue_golomb() + 1;
            // TODO bit hacks: combine https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
            // with https://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
            // to replace floating point below
            uint num_bits = (int)ceil(log2(num_slice_groups));
            for (idx = 0; idx < pic_size_in_map_units; ++idx)
            {
                br.get_bits(num_bits); //slice_group_id[idx]
            }
        }
    }

    br.get_ue_golomb(); // num_ref_idx_10_active_minus1
    br.get_ue_golomb(); // num_ref_idx_11_active_minus1
    br.get_bits(1);     // weighted_pred_flag;
    br.get_bits(2);     // weighted_bipred_idc
    br.get_se_golomb(); // pic_init_qp_minus26
    br.get_se_golomb(); // pic_init_qs_minus26
    br.get_se_golomb(); // chroma_qp_index_offset
    br.get_bits(1);     // deblocking_filter_control_present_flag
    br.get_bits(1);     // constrained_intra_pref_flag
    m_redundantPicCntPresentFlag = br.get_bits(1);
#endif
}

void AVCParser::decode_SEI(BitReader& br)
{
    int   recovery_frame_cnt = -1;
#if 0
    bool  exact_match_flag = false;
    bool  broken_link_flag = false;
    int   changing_group_slice_idc = -1;
#endif

    int type = 0;
    int size = 0;

    /* A message requires at least 2 bytes, and then
     * there's the stop bit plus alignment, so there
     * can be no message in less than 24 bits */
    while (br.get_bits_left() >= 24)
    {
        type += br.show_bits(8);
        while (br.get_bits(8) == 0xFF)
            type += br.show_bits(8);

        size += br.show_bits(8);
        while (br.get_bits(8) == 0xFF)
            size += br.show_bits(8);

        switch (type)
        {
          case SEI_TYPE_RECOVERY_POINT:
            recovery_frame_cnt = br.get_ue_golomb();
#if 0
            exact_match_flag = (br.get_bits(1) != 0U);
            broken_link_flag = (br.get_bits(1) != 0U);
            changing_group_slice_idc = br.get_bits(2);
#endif
            m_auContainsKeyframeMessage = (recovery_frame_cnt == 0);
            if ((size - 12) > 0)
                br.skip_bits((size - 12) * 8);
            return;

          default:
            br.skip_bits(size * 8);
            break;
        }
    }
}

// Following the lead of libavcodec, ignore the left cropping.
uint AVCParser::pictureWidthCropped(void) const
{
    uint ChromaArrayType = m_separateColourPlaneFlag ? 0 : m_chromaFormatIdc;
    uint CropUnitX = 1;
    uint SubWidthC = m_chromaFormatIdc == 3 ? 1 : 2;
    if (ChromaArrayType != 0)
        CropUnitX = SubWidthC;
    uint crop = CropUnitX * m_frameCropRightOffset;
    return m_picWidth - crop;
}

// Following the lead of libavcodec, ignore the top cropping.
uint AVCParser::pictureHeightCropped(void) const
{
    uint ChromaArrayType = m_separateColourPlaneFlag ? 0 : m_chromaFormatIdc;
    uint CropUnitY = 2 - m_frameMbsOnlyFlag;
    uint SubHeightC = m_chromaFormatIdc <= 1 ? 2 : 1;
    if (ChromaArrayType != 0)
        CropUnitY *= SubHeightC;
    uint crop = CropUnitY * m_frameCropBottomOffset;
    return m_picHeight - crop;
}

H2645Parser::field_type AVCParser::getFieldType(void) const
{
    if (m_bottomFieldFlag == -1)
        return FRAME;
    return m_bottomFieldFlag ? FIELD_BOTTOM : FIELD_TOP;
}

/* H.264

   num_units_in_tick is the number of time units of a clock operating
   at the frequency time_scale Hz that corresponds to one increment
   (called a clock tick) of a clock tick counter. num_units_in_tick
   shall be greater than 0. A clock tick is the minimum interval of
   time that can be represented in the coded data. For example, when
   the frame rate of a video signal is 30 000 ÷ 1001 Hz, time_scale
   may be equal to 60 000 and num_units_in_tick may be equal to
   1001. See Equation C-1.
*/

double AVCParser::frameRate(void) const
{
    uint64_t num   = 500 * (uint64_t)m_timeScale; /* 1000 * 0.5 */
    double   fps   = ( m_unitsInTick != 0 ? num / (double)m_unitsInTick : 0 ) / 1000;

    return fps;
}

MythAVRational AVCParser::getFrameRate() const
{
    if (m_unitsInTick == 0)
        return MythAVRational(0);
    if (m_timeScale & 0x1)
        return MythAVRational(m_timeScale, m_unitsInTick * 2);
    return MythAVRational(m_timeScale / 2, m_unitsInTick);
}
