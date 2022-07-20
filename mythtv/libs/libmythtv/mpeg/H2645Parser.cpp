// MythTV headers
#include "H2645Parser.h"
#include <iostream>

#include "libmythbase/mythlogging.h"
#include "recorders/dtvrecorder.h" // for FrameRate and ScanType
#include "bitreader.h"

#include <strings.h>

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

H2645Parser::H2645Parser(void)
  : m_rbspBuffer(new uint8_t[m_rbspBufferSize])
{
    if (m_rbspBuffer == nullptr)
        m_rbspBufferSize = 0;
}

void H2645Parser::Reset(void)
{
    m_stateChanged = false;
    m_seenSPS = false;
    m_isKeyframe = false;
    m_spsOffset = 0;

    m_syncAccumulator = 0xffffffff;
    m_auPending = false;

    m_picWidth = 0;
    m_picHeight = 0;
    m_frameCropLeftOffset = 0;
    m_frameCropRightOffset = 0;
    m_frameCropTopOffset = 0;
    m_frameCropBottomOffset = 0;
    m_aspectRatioIdc = 0;
    m_sarWidth = 0;
    m_sarHeight = 0;
    m_unitsInTick = 0;
    m_timeScale = 0;
    m_fixedRate = false;

    m_pktOffset = 0;
    m_auOffset = 0;
    m_frameStartOffset = 0;
    m_keyframeStartOffset = 0;
    m_onFrame = false;
    m_onKeyFrame = false;

    m_chromaFormatIdc = 1;
    m_separateColourPlaneFlag = false;
}

void H2645Parser::resetRBSP(void)
{
    m_rbspIndex = 0;
    m_consecutiveZeros = 0;
    m_haveUnfinishedNAL = false;
}

bool H2645Parser::fillRBSP(const uint8_t *byteP, uint32_t byte_count,
                          bool found_start_code)
{
    uint32_t required_size = m_rbspIndex + byte_count;
    if (m_rbspBufferSize < required_size)
    {
        // Round up to packet size
        required_size = ((required_size / 188) + 1) * 188;

        /* Need a bigger buffer */
        auto *new_buffer = new uint8_t[required_size];

        if (new_buffer == nullptr)
        {
            /* Allocation failed. Discard the new bytes */
            LOG(VB_GENERAL, LOG_ERR,
                "H2645Parser::fillRBSP: FAILED to allocate RBSP buffer!");
            return false;
        }

        /* Copy across bytes from old buffer */
        memcpy(new_buffer, m_rbspBuffer, m_rbspIndex);
        delete [] m_rbspBuffer;
        m_rbspBuffer = new_buffer;
        m_rbspBufferSize = required_size;
    }

/*
    NumBytesInNalUnit is set equal to the number of bytes starting
    with the byte at the current position in the byte stream up to and
    including the last byte that precedes the location of one or more
    of the following conditions:
    – A subsequent byte-aligned three-byte sequence equal to 0x000000,
    – A subsequent byte-aligned three-byte sequence equal to 0x000001,
*/


    /* Fill rbsp while we have data */
    while (byte_count)
    {
        /* Copy the byte into the rbsp, unless it
         * is the 0x03 in a 0x000003 */
        if (m_consecutiveZeros < 2 || *byteP != 0x03)
            m_rbspBuffer[m_rbspIndex++] = *byteP;

        if (*byteP == 0)
            ++m_consecutiveZeros;
        else
            m_consecutiveZeros = 0;

        ++byteP;
        --byte_count;
    }

    /* If we've found the next start code then that, plus the first byte of
     * the next NAL, plus the preceding zero bytes will all be in the rbsp
     * buffer. Move rbsp_index back to the end of the actual rbsp data. We
     * need to know the correct size of the rbsp to decode some NALs.
     */
    if (found_start_code)
    {
        if (m_rbspIndex >= 4)
        {
            m_rbspIndex -= 4;
            while (m_rbspIndex > 0 && m_rbspBuffer[m_rbspIndex - 1] == 0)
                --m_rbspIndex;
        }
        else
        {
            /* This should never happen. */
            LOG(VB_GENERAL, LOG_ERR,
                QString("H2645Parser::fillRBSP: Found start code, rbsp_index "
                        "is %1 but it should be >4")
                    .arg(m_rbspIndex));
        }
    }

    return true;
}

/* Video Usability Information */
void H2645Parser::vui_parameters(BitReader& br, bool hevc)
{
    /*
      aspect_ratio_info_present_flag equal to 1 specifies that
      m_aspectRatioIdc is present. aspect_ratio_info_present_flag
      equal to 0 specifies that m_aspectRatioIdc is not present.
     */
    if (br.next_bit()) //aspect_ratio_info_present_flag
    {
        /*
          m_aspectRatioIdc specifies the value of the sample aspect
          ratio of the luma samples. Table E-1 shows the meaning of
          the code. When m_aspectRatioIdc indicates Extended_SAR, the
          sample aspect ratio is represented by m_sarWidth and
          m_sarHeight. When the m_aspectRatioIdc syntax element is not
          present, m_aspectRatioIdc value shall be inferred to be
          equal to 0.
         */
        m_aspectRatioIdc = br.get_bits(8);

        switch (m_aspectRatioIdc)
        {
          case 0: // NOLINT(bugprone-branch-clone)
            // Unspecified
            break;
          case 1:
            // 1:1
            /*
              1280x720 16:9 frame without overscan
              1920x1080 16:9 frame without overscan (cropped from 1920x1088)
              640x480 4:3 frame without overscan
             */
            break;
          case 2:
            // 12:11
            /*
              720x576 4:3 frame with horizontal overscan
              352x288 4:3 frame without overscan
             */
            break;
          case 3:
            // 10:11
            /*
              720x480 4:3 frame with horizontal overscan
              352x240 4:3 frame without overscan
             */
            break;
          case 4:
            // 16:11
            /*
              720x576 16:9 frame with horizontal overscan
              540x576 4:3 frame with horizontal overscan
             */
            break;
          case 5:
            // 40:33
            /*
              720x480 16:9 frame with horizontal overscan
              540x480 4:3 frame with horizontal overscan
             */
            break;
          case 6:
            // 24:11
            /*
              352x576 4:3 frame without overscan
              540x576 16:9 frame with horizontal overscan
             */
            break;
          case 7:
            // 20:11
            /*
              352x480 4:3 frame without overscan
              480x480 16:9 frame with horizontal overscan
             */
            break;
          case 8:
            // 32:11
            /*
              352x576 16:9 frame without overscan
             */
            break;
          case 9:
            // 80:33
            /*
              352x480 16:9 frame without overscan
             */
            break;
          case 10:
            // 18:11
            /*
              480x576 4:3 frame with horizontal overscan
             */
            break;
          case 11:
            // 15:11
            /*
              480x480 4:3 frame with horizontal overscan
             */
            break;
          case 12:
            // 64:33
            /*
              540x576 16:9 frame with horizontal overscan
             */
            break;
          case 13:
            // 160:99
            /*
              540x576 16:9 frame with horizontal overscan
             */
            break;
          case kExtendedSar:
            m_sarWidth  = br.get_bits(16);
            m_sarHeight = br.get_bits(16);
            LOG(VB_RECORD, LOG_DEBUG,
                QString("sarWidth %1 sarHeight %2")
                .arg(m_sarWidth).arg(m_sarHeight));
            break;
        }
    }
    else
    {
        m_sarWidth = m_sarHeight = 0;
    }

    if (br.next_bit()) //overscan_info_present_flag
        br.next_bit(); //overscan_appropriate_flag

    if (br.next_bit()) //video_signal_type_present_flag
    {
        br.get_bits(3); //video_format
        br.next_bit();  //video_full_range_flag
        if (br.next_bit()) // colour_description_present_flag
        {
            br.get_bits(8); // colour_primaries
            br.get_bits(8); // transfer_characteristics
            br.get_bits(8); // matrix_coefficients
        }
    }

    if (br.next_bit()) //chroma_loc_info_present_flag
    {
        br.get_ue_golomb(); //chroma_sample_loc_type_top_field ue(v)
        br.get_ue_golomb(); //chroma_sample_loc_type_bottom_field ue(v)
    }

    if (hevc)
    {
        br.next_bit();           // get_neutral_chroma_indication_flag u(1)
        br.next_bit();           // field_seq_flag u(1);
        br.next_bit();           // frame_field_info_present_flag u(1);
        if (br.next_bit()) {     // default_display_window_flag u(1);
            br.get_ue_golomb();  // def_disp_win_left_offset ue(v);
            br.get_ue_golomb();  // def_disp_win_right_offset ue(v);
            br.get_ue_golomb();  // def_disp_win_top_offset ue(v);
            br.get_ue_golomb();  // def_disp_win_bottom_offset ue(v);
        }
    }

    if (br.next_bit()) //timing_info_present_flag
    {
        m_unitsInTick = br.get_bits(32); // num_units_in_tick
        m_timeScale = br.get_bits(32);   // time_scale
        m_fixedRate = (br.get_bits(1) != 0U);

        LOG(VB_RECORD, LOG_DEBUG,
            QString("VUI unitsInTick %1 timeScale %2 fixedRate %3")
            .arg(m_unitsInTick)
            .arg(m_timeScale)
            .arg(m_fixedRate));
    }
}

uint H2645Parser::aspectRatio(void) const
{

    MythAVRational aspect {0};

    if (m_picHeight)
        aspect = MythAVRational(pictureWidthCropped(), pictureHeightCropped());

    switch (m_aspectRatioIdc)
    {
        case 0: // Unspecified
        case 1: // 1:1
            break;
        case 2:
            // 12:11
            aspect *= MythAVRational(12, 11);
            break;
        case 3:
            // 10:11
            aspect *= MythAVRational(10, 11);
            break;
        case 4:
            // 16:11
            aspect *= MythAVRational(16, 11);
            break;
        case 5:
            // 40:33
            aspect *= MythAVRational(40, 33);
            break;
        case 6:
            // 24:11
            aspect *= MythAVRational(24, 11);
            break;
        case 7:
            // 20:11
            aspect *= MythAVRational(20, 11);
            break;
        case 8:
            // 32:11
            aspect *= MythAVRational(32, 11);
            break;
        case 9:
            // 80:33
            aspect *= MythAVRational(80, 33);
            break;
        case 10:
            // 18:11
            aspect *= MythAVRational(18, 11);
            break;
        case 11:
            // 15:11
            aspect *= MythAVRational(15, 11);
            break;
        case 12:
            // 64:33
            aspect *= MythAVRational(64, 33);
            break;
        case 13:
            // 160:99
            aspect *= MythAVRational(160, 99);
            break;
        case 14:
            // 4:3
            aspect *= MythAVRational(4, 3);
            break;
        case 15:
            // 3:2
            aspect *= MythAVRational(3, 2);
            break;
        case 16:
            // 2:1
            aspect *= MythAVRational(2, 1);
            break;
        case kExtendedSar:
            if (m_sarHeight)
                aspect *= MythAVRational(m_sarWidth, m_sarHeight);
            else
                aspect = MythAVRational(0);
            break;
    }

    if (aspect == MythAVRational(0))
        return 0;
    if (aspect == MythAVRational(4, 3))  // 1.3333333333333333
        return 2;
    if (aspect == MythAVRational(16, 9)) // 1.7777777777777777
        return 3;
    if (aspect == MythAVRational(221, 100)) // 2.21
        return 4;

    return aspect.toFixed(1000000);
}
