// -*- Mode: c++ -*-
/**
 *   SCTE System information tables.
 *   Copyright (c) 2011, Digital Nirvana, Inc.
 *   Author: Daniel Kristjansson
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

#include "sctetables.h"
#include "mythdate.h" // for xml_indent

QString CarrierDefinitionSubtable::toString(void) const
{
    return QString("CarrierDefinitionSubtable number_of_carriers(%1)")
        .arg(NumberOfCarriers()) +
        QString(" descriptors(%1)").arg(DescriptorsCount());
}

QString CarrierDefinitionSubtable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);
    QString str = indent_0 +
        QString("<CarrierDefinition descriptors_count=\"%2\" ")
        .arg(DescriptorsCount());
    str += QString("number_of_carriers=\"%1\"").arg(NumberOfCarriers());
    str += "\n" + indent_1;
    str += QString("spacing_unit=\"%1\" spacing_unit_hz=\"%2\"")
        .arg(SpacingUnit()).arg(SpacingUnitHz());
    str += "\n" + indent_1;
    str += QString("frequency_spacing=\"%1\" frequency_spacing_hz=\"%2\"")
        .arg(FrequencySpacing()).arg(FrequencySpacingHz());
    str += "\n" + indent_1;
    str += QString("frequency_unit=\"%1\" frequency_unit_hz=\"%2\"")
        .arg(FrequencyUnit()).arg(FrequencyUnitHz());
    str += "\n" + indent_1;
    str += QString("first_carrier_frequency=\"%1\" "
                   "first_carrier_frequency_hz=\"%2\">\n")
        .arg(FirstCarrierFrequency())
        .arg(FirstCarrierFrequencyHz());

    vector<const unsigned char*> desc =
        MPEGDescriptor::Parse(Descriptors(), DescriptorsLength());
    for (uint i = 0; i < desc.size(); i++)
    {
        str += MPEGDescriptor(desc[i], 300)
            .toStringXML(indent_level + 1) + "\n";
    }
    return str + indent_0 + "</CarrierDefinition>";
}

QString ModulationModeSubtable::TransmissionSystemString(void) const
{
    switch (TransmissionSystem())
    {
        case kTSUnknown:    return "Unknown";
        case kTSITUAnnexA:  return "Annex A (8 Mhz Global QAM)";
        case kTSITUAnnexB:  return "Annex B (6 Mhz US QAM)";
        case kTSITUQPSK:    return "ITU 1211 (QPSK Satelite)";
        case kTSATSC:       return "ATSC";
        case kTSDigiCipher: return "DigiCipher II";
        default: return QString("Reserved(%1)").arg(TransmissionSystem());
    }
}

QString ModulationModeSubtable::InnerCodingModeString(void) const
{
    switch (InnerCodingMode())
    {
        case kRate5_11Coding: return "5/11";
        case kRate1_2Coding:  return "1/2";
        case kRate3_5Coding:  return "3/5";
        case kRate2_3Coding:  return "2/3";
        case kRate3_4Coding:  return "3/4";
        case kRate4_5Coding:  return "4/5";
        case kRate5_6Coding:  return "5/6";
        case kRate7_8Coding:  return "7/8";
        case kNone:           return "None";
        default: return QString("Reserved(%1)").arg(InnerCodingMode());
    }
}

QString ModulationModeSubtable::ModulationFormatString(void) const
{
    switch (ModulationFormat())
    {
        case kUnknown: return "Unknown";
        case kQPSK:    return "QPSK";
        case kBPSK:    return "BPSK";
        case kOQPSK:   return "OQPSK";
        case kVSB8:    return "8-VSB";
        case kVSB16:   return "16-VSB";
        case kQAM16:   return "QAM-16";
        case kQAM32:   return "QAM-32";
        case kQAM64:   return "QAM-64";
        case kQAM80:   return "QAM-80";
        case kQAM96:   return "QAM-96";
        case kQAM112:  return "QAM-112";
        case kQAM128:  return "QAM-128";
        case kQAM160:  return "QAM-160";
        case kQAM192:  return "QAM-192";
        case kQAM224:  return "QAM-224";
        case kQAM256:  return "QAM-256";
        case kQAM320:  return "QAM-320";
        case kQAM384:  return "QAM-384";
        case kQAM448:  return "QAM-448";
        case kQAM512:  return "QAM-512";
        case kQAM640:  return "QAM-640";
        case kQAM768:  return "QAM-768";
        case kQAM896:  return "QAM-896";
        case kQAM1024: return "QAM-1024";
        default: return QString("Reserved(%1)").arg(ModulationFormat());
    }
}

QString ModulationModeSubtable::toString(void) const
{
    return "ModulationMode";
}

QString ModulationModeSubtable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);
    QString str = indent_0 +
        QString("<ModulationMode descriptors_count=\"%2\"")
        .arg(DescriptorsCount());
    str += "\n" + indent_1;
    str += QString("transmission_system=\"%1\" transmission_system_desc=\"%2\"")
        .arg(TransmissionSystem()).arg(TransmissionSystemString());
    str += "\n" + indent_1;
    str += QString("inner_coding_mode=\"%1\" inner_coding_mode_desc=\"%2\"")
        .arg(InnerCodingMode()).arg(InnerCodingModeString());
    str += "\n" + indent_1;
    str += QString("split_bitstream_mode=\"%1\" ")
        .arg(xml_bool_to_string(SplitBitstreamMode()));
    str += QString("symbol_rate=\"%1\"").arg(SymbolRate());

    vector<const unsigned char*> desc =
        MPEGDescriptor::Parse(Descriptors(), DescriptorsLength());

    if (desc.empty())
        return str + " />";

    str += ">\n";
    for (uint i = 0; i < desc.size(); i++)
    {
        str += MPEGDescriptor(desc[i], 300)
            .toStringXML(indent_level + 1) + "\n";
    }

    return str + indent_0 + "</ModulationMode>";
} 

bool SCTENetworkInformationTable::Parse(void)
{
    _ptrs.clear();

    if ((kCarrierDefinitionSubtable == TableSubtype()) ||
        (kModulationModeSubtable == TableSubtype()))
    {
        uint offset = (kCarrierDefinitionSubtable == TableSubtype()) ? 6 : 7;
        const unsigned char *next = pesdata() + 7;
        for (uint i = 0; i < NumberOfRecords(); i++)
        {
            _ptrs.push_back(next);
            uint desc_count = next[offset-1];
            next += offset;
            for (uint j = 0; j < desc_count; j++)
            {
                MPEGDescriptor desc(next);
                if (!desc.IsValid())
                {
                    _ptrs.clear();
                    return false;
                }
                next += desc.size();
            }
        }
        _ptrs.push_back(next);
        return true;
    }

    return false;
}

QString SCTENetworkInformationTable::toString(void) const
{
    QString str = QString("Network Information Section (SCTE) crc(0x%1)\n")
        .arg(CRC(),8,16,QChar('0'));
    str += QString("first_index(%1) number_of_records(%2) table_subtype(%3)\n")
        .arg(FirstIndex()).arg(NumberOfRecords()).arg(TableSubtype());
    if (kCarrierDefinitionSubtable == TableSubtype())
    {
        for (uint i = 0;  i < NumberOfRecords(); i++)
            str += CarrierDefinition(i).toString() + "\n";
    }
    else if (kModulationModeSubtable == TableSubtype())
    {
        for (uint i = 0;  i < NumberOfRecords(); i++)
            str += ModulationMode(i).toString() + "\n";
    }

    return str;
}

QString SCTENetworkInformationTable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);

    QString str = indent_0 + "<SCTENetworkInformationSection psip=\"scte\" ";
    str += QString("transmission_medium=\"%1\" ").arg(TransmissionMedium());
    str += QString("first_index=\"%1\" ").arg(FirstIndex());
    str += "\n" + indent_1;
    str += QString("number_of_records=\"%1\" ").arg(NumberOfRecords());
    str += QString("table_subtype=\"%1\"").arg(TableSubtype());
    str += PSIPTable::XMLValues(indent_level + 1) + ">\n";

    if (kCarrierDefinitionSubtable == TableSubtype())
    {
        for (uint i = 0;  i < NumberOfRecords(); i++)
            str += CarrierDefinition(i).toStringXML(indent_level + 1) + "\n";
    }
    else if (kModulationModeSubtable == TableSubtype())
    {
        for (uint i = 0;  i < NumberOfRecords(); i++)
            str += ModulationMode(i).toStringXML(indent_level + 1) + "\n";
    }

    vector<const unsigned char*> desc =
        MPEGDescriptor::Parse(Descriptors(), DescriptorsLength());
    for (uint i = 0; i < desc.size(); i++)
    {
        str += MPEGDescriptor(desc[i], 300)
            .toStringXML(indent_level + 1) + "\n";
    }

    return str + indent_0 + "</SCTENetworkInformationSection>";
}

//////////////////////////////////////////////////////////////////////

void NetworkTextTable::Parse(void) const
{
    // TODO
}

QString NetworkTextTable::toString(void) const
{
    // TODO
    return QString("Network Text Section crc(0x%1)\n")
        .arg(CRC(),8,16,QChar('0'));
}

QString NetworkTextTable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);

    QString str = indent_0 + "<NetworkTextSection ";
    str += QString("iso_639_language_code=\"%1\" ").arg(LanguageString());
    str += QString("transmission_medium=\"%1\" ").arg(TransmissionMedium());
    str += QString("table_subtype=\"%1\"").arg(TableSubtype());
    return str + " />";

/*
    str += ">\n";
    vector<const unsigned char*> desc =
        MPEGDescriptor::Parse(Descriptors(), DescriptorsLength());
    for (uint i = 0; i < desc.size(); i++)
    {
        str += MPEGDescriptor(desc[i], 300)
            .toStringXML(indent_level + 1) + "\n";
    }

    return str + indent_0 + "</NetworkTextSection>";
*/
}

//////////////////////////////////////////////////////////////////////

bool ShortVirtualChannelTable::Parse(void)
{
    _ptrs.clear();

    if (kDefinedChannelsMap == TableSubtype())
    {
        _ptrs.push_back(pesdata() + DefinedChannelsMap().Size() + 7);
    }
    else if (kVirtualChannelMap == TableSubtype())
    {
        bool descriptors_included = pesdata()[7] & 0x20;
        uint number_of_vc_records = pesdata()[13];
        const unsigned char *next = pesdata() + 14;
        if (!descriptors_included)
        {
            for (uint i = 0; i < number_of_vc_records; i++)
            {
                _ptrs.push_back(next);
                next += 9;
            }
            _ptrs.push_back(next);
        }
        else
        {
            for (uint i = 0; i < number_of_vc_records; i++)
            {
                _ptrs.push_back(next);
                uint desc_count = next[10];
                next += 10;
                for (uint j = 0; j < desc_count; j++)
                {
                    MPEGDescriptor desc(next);
                    if (!desc.IsValid())
                    {
                        _ptrs.clear();
                        return false;
                    }
                    next += desc.size();
                }
            }
        }
        _ptrs.push_back(next);
    }
    else if (kInverseChannelMap == TableSubtype())
    {
        _ptrs.push_back(pesdata() + InverseChannelMap().Size() + 7);
    }
    else
    {
        return false;
    }

    return true;
}

QString DefinedChannelsMapSubtable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);
    QString str = indent_0 + "<DefinedChannelsMap ";
    str += QString("first_virtual_channel=\"%1\" ")
        .arg(FirstVirtualChannel());
    str += QString("dcm_data_length=\"%1\">\n")
        .arg(DCMDataLength());

    for (uint i = 0; i < DCMDataLength(); i++)
    {
        str += indent_1 + QString("<Range range_defined=\"%1\"%2 "
                                  "channels_count=\"%3\" />\n")
            .arg(xml_bool_to_string(RangeDefined(i)))
            .arg(RangeDefined(i) ? " " : "")
            .arg(ChannelsCount(i));
    }

    return str + indent_0 + "</DefinedChannelsMap>";
}

QString VirtualChannelMapSubtable::VideoStandardString(uint i) const
{
    switch (VideoStandard(i))
    {
        case kNTSC:   return "NTSC";
        case kPAL625: return "PAL-625";
        case kPAL525: return "PAL-525";
        case kSECAM:  return "SECAM";
        case kMAC:    return "MAC";
        default:      return QString("Reserved(%1)").arg(VideoStandard(i));
    }
}

QString VirtualChannelMapSubtable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);
    QString indent_2 = xml_indent(indent_level + 2);
    QString str = indent_0 + "<DefinedChannelsMap ";
    str += QString("number_of_vc_records=\"%1\"")
        .arg(NumberOfVCRecords());
    str += "\n" + indent_1;
    str += QString("descriptors_included=\"%1\" ")
        .arg(xml_bool_to_string(DescriptorsIncluded()));
    str += QString("splice=\"%1\" ")
        .arg(xml_bool_to_string(Splice()));
    str += "\n" + indent_1;
    str += QString("activation_time=\"%1\" actication_time_desc=\"%2\"")
        .arg(ActivationTimeRaw())
        .arg(ActivationTimeUTC().toString(Qt::ISODate));
    str += ">\n";

    for (uint i = 0; i < NumberOfVCRecords(); i++)
    {
        str += indent_1 + "<Channel ";
        str += QString("virtual_channel_number=\"%1\" ")
            .arg(VirtualChannelNumber(i));
        str += "\n" + indent_2;
        str += QString("application_virtual_channel=\"%1\" ")
            .arg(xml_bool_to_string(ApplicationVirtualChannel(i)));
        str += QString("path_select=\"%1\" ").arg(PathSelect(i));
        str += "\n" + indent_2;
        str += QString("transport_type=\"%1\" transport_type_desc=\"%2\" ")
            .arg(TransportType(i)).arg(TransportTypeString(i));
        str += "\n" + indent_2;
        str += QString("channel_type=\"%1\" channel_type_desc=\"%2\" ")
            .arg(ChannelType(i)).arg(ChannelTypeString(i));
        if (ApplicationVirtualChannel(i))
            str += QString("application_id=\"%1\" ").arg(ApplicationID(i));
        else
            str += QString("source_id=\"%1\" ").arg(SourceID(i));
        str += "\n" + indent_2;
        if (kMPEG2Transport == TransportType(i))
        {
            str += QString("cds_reference=\"%1\" ").arg(CDSReference(i));
            str += QString("program_number=\"%1\" ").arg(ProgramNumber(i));
            str += QString("mms_reference=\"%1\" ").arg(MMSReference(i));
        }
        else
        {
            str += QString("cds_reference=\"%1\" ").arg(CDSReference(i));
            str += QString("scrampled=\"%1\" ").arg(Scrambled(i));
            str += "\n" + indent_2;
            str += QString("video_standard=\"%1\" video_standard_desc=\"%2\" ")
                .arg(VideoStandard(i)).arg(VideoStandardString(i));
        }
        if (!DescriptorsIncluded())
        {
            str += "/>\n";
            continue;
        }

        str += "\n" + indent_2;
        str += QString("descriptors_count=\"%1\"").arg(DescriptorsCount(i));
        str += ">\n";
        
        vector<const unsigned char*> desc =
            MPEGDescriptor::Parse(Descriptors(i), DescriptorsLength(i));
        for (uint i = 0; i < desc.size(); i++)
        {
            str += MPEGDescriptor(desc[i], 300)
                .toStringXML(indent_level + 2) + "\n";
        }
        str += indent_1 + "</Channel>";
    }

    return str + indent_0 + "</DefinedChannelsMap>";
}

QString InverseChannelMapSubtable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);
    QString str = indent_0 + "<InverseChannelMap ";
    str += QString("first_map_index=\"%1\" ")
        .arg(FirstMapIndex());
    str += QString("record_count=\"%1\">\n")
        .arg(RecordCount());

    for (uint i = 0; i < RecordCount(); i++)
    {
        str += indent_1 + QString("<Map source_id=\"%1\" "
                                  "virtual_channel_number=\"%2\" />\n")
            .arg(SourceID(i)).arg(VirtualChannelNumber(i));
    }

    return str + indent_0 + "</InverseChannelMap>";
}

QString ShortVirtualChannelTable::TableSubtypeString(void) const
{
    switch (TableSubtype())
    {
        case kVirtualChannelMap:  return "Virtual Channel Map";
        case kDefinedChannelsMap: return "Defined Channels Map";
        case kInverseChannelMap:  return "Inverse Channel Map";
        default: return QString("Reserved(%1)").arg(TableSubtype());
    }
}

QString ShortVirtualChannelTable::toString(void) const
{
    // TODO
    return QString("Short Virtual Channel Section ID(%1) crc(0x%2)\n")
        .arg(ID()).arg(CRC(),8,16,QChar('0'));
}

QString ShortVirtualChannelTable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);
    QString str = indent_0 +
        QString("<ShortVirtualChannelSection vct_id=\"%1\" ").arg(ID());
    str += QString("transmission_medium=\"%1\" ").arg(TransmissionMedium());
    str += "\n" + indent_1;
    str += QString("table_subtype=\"%1\" table_subtype_desc=\"%2\"")
        .arg(TableSubtype()).arg(TableSubtypeString());
    str += "\n" + indent_1 + PSIPTable::XMLValues(indent_level + 1) + ">\n";

    if (kDefinedChannelsMap == TableSubtype())
        str += DefinedChannelsMap().toStringXML(indent_level + 1) + "\n";
    else if (kVirtualChannelMap == TableSubtype())
        str += VirtualChannelMap().toStringXML(indent_level + 1) + "\n";
    else if (kInverseChannelMap == TableSubtype())
        str += InverseChannelMap().toStringXML(indent_level + 1) + "\n";

    vector<const unsigned char*> desc =
        MPEGDescriptor::Parse(Descriptors(), DescriptorsLength());
    for (uint i = 0; i < desc.size(); i++)
    {
        str += MPEGDescriptor(desc[i], 300)
            .toStringXML(indent_level + 1) + "\n";
    }

    return str + indent_0 + "</ShortVirtualChannelSection>";
}

//////////////////////////////////////////////////////////////////////

QString SCTESystemTimeTable::toString(void) const
{
    QString str =
        QString("SystemTimeSection (SCTE) raw(%1) GPS_UTC_Offset(%2) utc(%3_")
        .arg(SystemTimeRaw()).arg(GPSUTCOffset())
        .arg(SystemTimeUTC().toString(Qt::ISODate));
    return str;
}

QString SCTESystemTimeTable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);
    QString str = indent_0 +
        QString("<SCTESystemTimeSection system_time=\"%1\" "
                "gps_utc_offset=\"%2\"\n%3utc_time_desc=\"%4\" psip=\"scte\"")
        .arg(SystemTimeRaw()).arg(GPSUTCOffset())
        .arg(indent_1)
        .arg(SystemTimeUTC().toString(Qt::ISODate));

    if (!DescriptorsLength())
        return str + " />";

    str += ">\n";

    vector<const unsigned char*> desc =
        MPEGDescriptor::Parse(Descriptors(), DescriptorsLength());
    for (uint i = 0; i < desc.size(); i++)
    {
        str += MPEGDescriptor(desc[i], 300)
            .toStringXML(indent_level + 1) + "\n";
    }

    return str + indent_0 + "</SCTESystemTimeSection>";
}

//////////////////////////////////////////////////////////////////////

QString AggregateDataEventTable::toString(void) const
{
    // TODO
    return "Aggregate Data Event Section\n";
}

QString AggregateDataEventTable::toStringXML(uint indent_level) const
{
    // TODO
    return "<AggregateDataEventSection />";
}
