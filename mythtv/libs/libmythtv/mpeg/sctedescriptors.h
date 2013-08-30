// -*- Mode: c++ -*-
/**
 *   SCTE Descriptors.
 *   Copyright (c) 2011, Digital Nirvana, Inc.
 *   Authors: Daniel Kristjansson, Sergey Staroletov
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef _SCTE_TABLES_H_
#define _SCTE_TABLES_H_

// Qt headers
#include <QString>

// MythTV
#include "mpegdescriptors.h"

// SCTE 57 p 83
class FrameRateDescriptor : public MPEGDescriptor
{
  public:
    FrameRateDescriptor(const unsigned char *data, uint len) :
        MPEGDescriptor(data, len, DescriptorID::scte_frame_rate, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x82
    // descriptor_length        8   1.0
    // multiple_frame_rate_flag 1   2.0
    bool MultipleFrameRates(void) const { return _data[2] & 0x80; }
    // frame_rate_code          4   2.1
    uint FrameRateCode(void) const { return (_data[2] >> 3) & 0xF; }
    /// returns maximum frame rate in video
    double FrameRate(void) const
    {
        switch (FrameRateCode())
        {
            case 0x1 :
                return 24.0 / 1.001;
            case 0x2 :
                return 24.0;
            case 0x3 :
                return 25.0;
            case 0x4 :
                return 30.0 / 1.001;
            case 0x5:
                return 30.0;
            case 0x6:
                return 50.0;
            case 0x7:
                return 60.0 / 1.001;
            case 0x8:
                return 60.0;
            default:
                //invalid
                return 0.0;
        }
    }
    // reserved                 3   2.5

    QString toString(void) const
    {
        return QString("FrameRateDescriptor: "
                       "MultipleFrameRates(%1) MaximumFrameRate(%2)")
            .arg(MultipleFrameRates())
            .arg(FrameRate());
    }
};

// SCTE 57 p 84
class ExtendedVideoDescriptor : public MPEGDescriptor
{
  public:
    ExtendedVideoDescriptor(const unsigned char *data, uint len) :
        MPEGDescriptor(data, len, DescriptorID::scte_extended_video, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x83
    // descriptor_length        8   1.0
    // catalog_mode_flag        1   2.0
    bool CatalogModeFlag(void) const { return _data[2] & 0x80; }
    // video_includes_setup     1   2.1
    bool VideoIncludesSetup(void) const { return _data[2] & 0x40; }
    // reserved                 6   2.2

    QString toString(void) const
    {
        return QString("ExtendedVideoDescriptor: "
                       "CatalogModeFlag(%1) VideoIncludesSetup(%2)")
            .arg(CatalogModeFlag())
            .arg(VideoIncludesSetup());
    }
};

// SCTE 57 p 85
class SCTEComponentNameDescriptor : public MPEGDescriptor
{
  public:
    SCTEComponentNameDescriptor(const unsigned char *data, uint len) :
        MPEGDescriptor(data, len, DescriptorID::scte_component_name)
    {
        // TODO make sure descriptor is long enough.. set _data NULL otherwise
    }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x84
    // descriptor_length        8   1.0
    // reserved                 2   2.0
    // string_count             6   2.2
    uint StringCount(void) const { return _data[2] & 0x3F; }

    // for (i = 0; i < string_count; i++)
    // {
    // ISO_639_language_code   24  loc(i)
    int LanguageKey(uint i) const
        { return iso639_str3_to_key(&_data[loc(i)]); }
    QString LanguageString(uint i) const
        { return iso639_key_to_str3(LanguageKey(loc(i))); }
    int CanonicalLanguageKey(uint i) const
        { return iso639_key_to_canonical_key(LanguageKey(loc(i))); }
    QString CanonicalLanguageString(uint i) const
        { return iso639_key_to_str3(CanonicalLanguageKey(loc(i))); }
    // string_length            8  loc(i)+3
    uint StringLength(uint i) const
        { return _data[loc(i) + 3]; }
    // name_string              *  loc(i)+4
    QString NameString(uint i) const;
    // }

    QString toString(void) const;

  private:
    uint loc(uint number) const
    {
        uint place = 3;
        for (uint i = 0; i < number; ++i)
            place += 4 + _data[place + 3];
        return place;
    }
};

/** This descriptor is used to identify streams with SpliceInformationTable
 *  data in them.
 *
 * \note While specified by SCTE 35 this descriptor is used worldwide.
 */
class CueIdentifierDescriptor : public MPEGDescriptor
{
  public:
    CueIdentifierDescriptor(const unsigned char *data, uint len) :
        MPEGDescriptor(data, len, DescriptorID::scte_cue_identifier, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x8A
    // descriptor_length        8   1.0       0x01
    // cue_stream_type          8   2.0
    enum // Table 6.3 Cue Stream Types
    {
        kLimited        = 0x0, ///< Only splice null, insert, and schedule
        kAllCommands    = 0x1, ///< Carries all commands.
        kSegmentation   = 0x2, ///< Carries time signal w/ segmentation desc.
        kTieredSplicing = 0x3, ///< outside scope of SCTE 35
        kTieredSegmentation = 0x4, ///< outside scope of SCTE 35
        // 0x05-0x7f are reserved for future use
        // 0x80-0xff user defined range
    };
    uint CueStreamType(void) const { return _data[2]; }
    QString CueStreamTypeString(void) const;
    QString toString(void) const;
};

// See SCTE 57 p 80
class FrequencySpecificationDescriptor : public MPEGDescriptor
{
  public:
    FrequencySpecificationDescriptor(const unsigned char *data, uint len) :
        MPEGDescriptor(data, len, DescriptorID::scte_frequency_spec, 2)
    { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x90
    // descriptor_length        8   1.0
    // frequency_unit           1   2.0
    bool FrequencyUnit(void) const { return _data[2] & 0x80; }
    uint FrequencyUnitHz(void) const
        { return FrequencyUnit() ? 10000 : 125000; }
    // carrier_frequency       15   2.1
    uint CarrierFrequency(void) const
        { return ((_data[2] << 8) | _data[3]) & 0x7fff; }
    unsigned long long CarrierFrequnecyHz(void) const
    {
        return FrequencyUnitHz() * ((unsigned long long) CarrierFrequency());
    }

    QString toString(void) const
    {
        return QString("FrequencySpecificationDescriptor: %2 Hz")
            .arg(CarrierFrequnecyHz());
    }
};

// SCTE 57 p 81
class ModulationParamsDescriptor : public MPEGDescriptor
{
  public:
    ModulationParamsDescriptor(const unsigned char *data, uint len) :
        MPEGDescriptor(data, len, DescriptorID::scte_modulation_params, 6) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x91
    // descriptor_length        8   1.0
    // transmissionSystem       4   2.0
    uint TransmissionSystem(void) const { return _data[2] >> 4; }
    // inner_coding_mode        4   2.4
    uint InnerCodingMode(void) const { return _data[2] & 0x0f; }
    // split_bitstream_mode     1   3.0
    bool SplitBitstreamMode(void) const { return _data[3] >> 7; }
    //reserved                  2   3.1
    //modulation_format         5   3.3
    uint ModulationFormat(void) const { return _data[3] & 0x1F; }
    // reserved                 4   4.0
    // symbol_rate             28   4.4
    uint SymbolRate(void) const
    {
        return ((_data[4] << 24) | (_data[5] << 16) |
                (_data[6] << 8) | _data[7]) & 0x7FFF;
    }
};

// SCTE 57 p 82
class TransportStreamIdDescriptor : public MPEGDescriptor
{
  public:
    TransportStreamIdDescriptor(const unsigned char *data, uint len) :
        MPEGDescriptor(data, len, DescriptorID::scte_transport_stream_id, 2) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x92
    // descriptor_length        8   1.0
    // target_transport_stream_id 16 2.0
    uint TargetTransportStreamId(void) const
        { return (_data[2] << 8) | _data[3]; }

    QString toString(void) const
    {
        return QString("TransportStreamIdDescriptor: 0x%1")
            .arg(TargetTransportStreamId(),0,16);
    }
};

// See SCTE 65 p 55 -- optional descriptor
class RevisionDetectionDescriptor : public MPEGDescriptor
{
  public:
    RevisionDetectionDescriptor(const unsigned char *data, uint len) :
        MPEGDescriptor(data, len, DescriptorID::scte_revision_detection, 3) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x93
    // descriptor_length        8   1.0       0x01
    // reserved                 3   2.0
    // table_version_number     5   2.3
    uint TableVersionNumber(void) const { return _data[2] & 0x1f; }
    // section_number           8   3.0
    uint SectionNumber(void) const { return _data[3]; }
    // last_section_number      8   4.0
    uint LastSectionNumber(void) const { return _data[4]; }

    QString toString(void) const;
};

#endif // _SCTE_TABLES_H_
