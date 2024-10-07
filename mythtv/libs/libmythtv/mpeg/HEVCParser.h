// -*- Mode: c++ -*-
/*******************************************************************
 * HEVCParser
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

#ifndef HEVCPARSER_H
#define HEVCPARSER_H

#include <array>
#include <map>

#include "mpeg/H2645Parser.h"

class HEVCParser : public H2645Parser
{
  public:

    enum NAL_unit_type : std::uint8_t {
        TAIL_N = 0, // Coded slice segment of a non-TSA, non-STSA trailing picture
        TAIL_R = 1, // slice_segment_layer_rbsp() VCL

        TSA_N  = 2, // Coded slice segment of a TSA picture
        TSA_R  = 3, // slice_segment_layer_rbsp() VCL

        STSA_N = 4, // Coded slice segment of an STSA picture
        STSA_R = 5, // slice_segment_layer_rbsp() VCL

        RADL_N = 6, // Coded slice segment of a RADL picture
        RADL_R = 7, // slice_segment_layer_rbsp() VCL

        // random access skipped leading (RASL) pictures
        RASL_N = 8, // Coded slice segment of a RASL picture
        RASL_R = 9, // slice_segment_layer_rbsp() VCL

        RSV_VCL_N10 = 10, //
        RSV_VCL_N12 = 12, // Reserved non-IRAP SLNR VCL NAL unit types VCL
        RSV_VCL_N14 = 14, //

        RSV_VCL_R11 = 11, //
        RSV_VCL_R13 = 13, // Reserved non-IRAP sub-layer reference VCL NAL unit
        RSV_VCL_R15 = 15, // types VCL

        // RADL = random access decodable leading picture

        /*
          A broken link access (BLA) picture having
          nal_unit_type equal to BLA_W_LP may have associated
          RASL or RADL pictures present in the bitstream. A
          BLA picture having nal_unit_type equal to
          BLA_W_RADL does not have associated RASL pictures
          present in the bitstream, but may have associated
          RADL pictures in the bitstream. A BLA picture
          having nal_unit_type equal to BLA_N_LP does not
          have associated leading pictures present in the
          bitstream.
        */
        BLA_W_LP    = 16, //
        BLA_W_RADL  = 17, // Coded slice segment of a BLA picture
        BLA_N_LP    = 18, // slice_segment_layer_rbsp() VCL

        /*
          An instantaneous decoding refresh (IDR) picture
          having nal_unit_type equal to IDR_N_LP does not
          have associated leading pictures present in the
          bitstream. An IDR picture having nal_unit_type
          equal to IDR_W_RADL does not have associated RASL
          pictures present in the bitstream, but may have
          associated RADL pictures in the bitstream.
        */
        IDR_W_RADL  = 19, // Coded slice segment of an IDR picture
        IDR_N_LP    = 20, // slice_segment_layer_rbsp() VCL

        /* A clean random access (CRA) picture may have
           associated random access skipped leading (RASL) or
           random access decodable leading (RADL) pictures
           present in the bitstream.
        */
        CRA_NUT     = 21, // Coded slice segment of a CRA picture
        // slice_segment_layer_rbsp() VCL

        RSV_IRAP_VCL22 = 22, //
        RSV_IRAP_VCL23 = 23, // Reserved IRAP VCL NAL unit types VCL

        RSV_VCL24 = 24, // Reserved non-IRAP VCL NAL unit types
        RSV_VCL25 = 25,
        RSV_VCL26 = 26,
        RSV_VCL27 = 27,
        RSV_VCL28 = 28,
        RSV_VCL29 = 29,
        RSV_VCL30 = 30,
        RSV_VCL31 = 31,

        VPS_NUT   = 32, // Video parameter set video_parameter_set_rbsp() non-VCL
        SPS_NUT   = 33, // Sequence parameter set seq_parameter_set_rbsp() non-VCL
        PPS_NUT   = 34, // Picture parameter set pic_parameter_set_rbsp() non-VCL
        AUD_NUT   = 35, // Access unit delimiter access_unit_delimiter_rbsp() non-VCL
        EOS_NUT   = 36, // End of sequence end_of_seq_rbsp() non-VCL
        EOB_NUT   = 37, // End of bitstream end_of_bitstream_rbsp() non-VCL
        FD_NUT    = 38, // Filler data filler_data_rbsp() non-VCL

        PREFIX_SEI_NUT = 39, // Supplemental enhancement information
        SUFFIX_SEI_NUT = 40, // sei_rbsp() non-VCL

        RSV_NVCL41 = 41, // Reserved non-VCL
        RSV_NVCL42 = 42,
        RSV_NVCL43 = 43,
        RSV_NVCL44 = 44,
        RSV_NVCL45 = 45,
        RSV_NVCL46 = 46,
        RSV_NVCL47 = 47,

        UNSPEC48   = 48, // Unspecified non-VCL
        UNSPEC49   = 49,
        UNSPEC50   = 50,
        UNSPEC51   = 51,
        UNSPEC52   = 52,
        UNSPEC53   = 53,
        UNSPEC54   = 54,
        UNSPEC55   = 55,
        UNSPEC56   = 56,
        UNSPEC57   = 57,
        UNSPEC58   = 58,
        UNSPEC59   = 59,
        UNSPEC60   = 60,
        UNSPEC61   = 61,
        UNSPEC62   = 62,
        UNSPEC63   = 63
    };

    struct SPS {
        uint8_t log2_min_luma_coding_block_size;
        uint8_t log2_diff_max_min_luma_coding_block_size;
        uint8_t log2_max_pic_order_cnt_lsb;
        bool    separate_colour_plane_flag;
        bool    sub_layer_ordering_info_present_flag;

    };

    struct VPS {
        uint8_t max_sub_layers; // range 0 to 3
    };

    struct PPS {
        bool    dependent_slice_segments_enabled_flag;
        bool    output_flag_present_flag;
        uint8_t num_extra_slice_header_bits;
        int     sps_id;
    };

    struct ShortTermRefPicSet {
        /* calculated values */
        std::array<int32_t,16> DeltaPocS0;
        std::array<int32_t,16> DeltaPocS1;
        std::array<uint8_t,16> UsedByCurrPicS0;
        std::array<uint8_t,16> UsedByCurrPicS1;
        uint8_t NumDeltaPocs;
        uint8_t NumNegativePics;
        uint8_t NumPositivePics;

        // Parsed values
        bool     inter_ref_pic_set_prediction_flag;
        uint16_t abs_delta_rps_minus1;
        uint8_t  delta_idx_minus1;
        uint8_t  delta_rps_sign;
    };


    /*
      scaling_list_dc_coef_minus8_16x16: this plus 8 specifies the DC
          Coefficient values for 16x16 scaling list
      scaling_list_dc_coef_minus8_32x32: this plus 8 specifies the
          DC Coefficient values for 32x32 scaling list
       scaling_lists_4x4: 4x4 scaling list
       scaling_lists_8x8: 8x8 scaling list
       scaling_lists_16x16: 16x16 scaling list
       scaling_lists_32x32: 32x32 scaling list
     */
    struct ScalingList {
        std::vector<int16_t> scaling_list_dc_coef_minus8_16x16 {6,0};
        std::vector<int16_t> scaling_list_dc_coef_minus8_32x32 {2,0};

        std::array<std::array<uint8_t,16>,6> scaling_lists_4x4   {};
        std::array<std::array<uint8_t,64>,6> scaling_lists_8x8   {};
        std::array<std::array<uint8_t,64>,6> scaling_lists_16x16 {};
        std::array<std::array<uint8_t,64>,2> scaling_lists_32x32 {};
    };

    enum QuantMatrixSize : std::uint8_t
    {
        QUANT_MATIX_4X4   = 0,
        QUANT_MATIX_8X8   = 1,
        QUANT_MATIX_16X16 = 2,
        QUANT_MATIX_32X32 = 3
    };


    HEVCParser(void) { ; }
    HEVCParser(const HEVCParser& rhs);
    ~HEVCParser(void) override { ; }

    /* random access point (RAP) are NAL type of:
     * * stantaneous decoding refresh (IDR)
     * * broken link access (BLA)
     * * clean random access (CRA)
     */
    static bool NALisRAP(uint type)
    {
        return (BLA_W_LP <= type && type <= CRA_NUT);
    }

    /*
      3.73 intra random access point (IRAP) picture: A coded picture
      for which each VCL NAL unit has nal_unit_type in the range of
      BLA_W_LP to RSV_IRAP_VCL23, inclusive.
    */
    static bool NALisIRAP(uint type)
    {
        return (BLA_W_LP <= type && type <= RSV_IRAP_VCL23);
    }

    static bool NALisVCL(uint type)
    {
        return (TAIL_N <= type && type <= RSV_VCL31);
    }

    uint32_t addBytes(const uint8_t  *bytes,
                      uint32_t  byte_count,
                      uint64_t  stream_offset) override;

    void Reset(void) override;

    QString NAL_type_str(int8_t type) override;

    uint pictureWidthCropped(void)  const override;
    uint pictureHeightCropped(void) const override;

    field_type getFieldType(void) const override { return FRAME; }
    MythAVRational getFrameRate() const override;

  protected:
    bool newAU(void);
    void processRBSP(bool rbsp_complete);
    bool profileTierLevel(BitReader& br,
                          bool profilePresentFlag,
                          int maxNumSubLayersMinus1);
    bool parseSliceSegmentLayer(BitReader& br);
    bool parseSliceSegmentHeader(BitReader& br);
    bool parseSPS(BitReader& br);
    bool parseVPS(BitReader& br);
    bool parsePPS(BitReader& br);

  private:
    uint32_t m_maxPicOrderCntLsb          {0};
    uint32_t m_picOrderCntMsb             {0};
    uint32_t m_picOrderCntVal             {0};
    uint32_t m_prevPicOrderCntLsb         {0};
    uint32_t m_prevPicOrderCntMsb         {0};
    uint32_t m_resolutionCheck            {0};

    uint     m_nalUnitType                {UNSPEC63};

    uint8_t  m_nalTemperalId              {0};
    uint8_t  m_nuhLayerId                 {0};

    bool     m_firstSliceSegmentInPicFlag {false};
    bool     m_nextNALisAU                {false};
    bool     m_noRaslOutputFlag           {false};
    bool     m_seenEOS                    {true};

    std::map<uint, SPS>  m_sps;
    std::map<uint, PPS>  m_pps;
    std::map<uint, VPS>  m_vps;
    std::map<uint, uint> m_poc;
};

#endif /* HEVCPARSER_H */
