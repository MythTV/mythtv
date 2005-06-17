// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#include "atscdescriptors.h"
#include "dvbdescriptors.h"

desc_list_t MPEGDescriptor::Parse(
    const unsigned char* data, uint len)
{
    desc_list_t tmp;
    uint off = 0;
    while (off < len)
    {
        tmp.push_back(data+off);
        MPEGDescriptor desc(data+off);
        off += desc.DescriptorLength() + 2;
    }
    return tmp;
}

const unsigned char* MPEGDescriptor::Find(const desc_list_t& parsed,
                                          uint desc_tag)
{
    desc_list_t::const_iterator it = parsed.begin();
    for (; it != parsed.end(); ++it)
    {
        if ((*it)[0] == desc_tag)
            return *it;
    }
    return NULL;
}

QString MPEGDescriptor::DescriptorTagString() const
{
    switch(DescriptorTag())
    {
        // MPEG
        case DescriptorID::registration:
            return QString("Registration");

        // DVB
        case DescriptorID::network_name:
            return QString("Network Name");
        case DescriptorID::service_list:
            return QString("Service List");
        case DescriptorID::dvb_stuffing:
            return QString("DVB Stuffing");
        case DescriptorID::satellite_delivery_system:
            return QString("Satellite Delivery System");
        case DescriptorID::cable_delivery_system:
            return QString("Cable Delivery System");
        case DescriptorID::VBI_data:
            return QString("VBI Data");
        case DescriptorID::VBI_teletext:
            return QString("VBI Teletext");
        case DescriptorID::bouquet_name:
            return QString("Bouquet Name");
        case DescriptorID::service:
            return QString("Service");
        case DescriptorID::country_availability:
            return QString("Country Availability");
        case DescriptorID::linkage:
            return QString("Linkage");
        case DescriptorID::NVOD_reference:
            return QString("NVOD Reference");
        case DescriptorID::dvb_time_shifted_service:
            return QString("DVB Time-shifted Service");
        case DescriptorID::short_event:
            return QString("Short Event");
        case DescriptorID::extended_event:
            return QString("Extended Event");
        case DescriptorID::time_shifted_event:
            return QString("Time-shifted Event");
        case DescriptorID::component:
            return QString("Component");
        case DescriptorID::mosaic:
            return QString("Mosaic");
        case DescriptorID::stream_identifier:
            return QString("Stream Identifier");
        case DescriptorID::CA_identifier:
            return QString("Conditional Access Identifier");
        case DescriptorID::content:
            return QString("Content");
        case DescriptorID::parental_rating:
            return QString("Parental Rating");
        case DescriptorID::teletext:
            return QString("Teletext");
        case DescriptorID::telephone:
            return QString("Telephone");
        case DescriptorID::local_time_offset:
            return QString("Local Time Offset");
        case DescriptorID::subtitling:
            return QString("Subtitling");
        case DescriptorID::terrestrial_delivery_system:
            return QString("Terrestrial Delivery System");
        case DescriptorID::multilingual_network_name:
            return QString("Multilingual Network Name");
        case DescriptorID::multilingual_bouquet_name:
            return QString("Multilingual Bouquet Name");
        case DescriptorID::multilingual_service_name:
            return QString("Multilingual Service Name");
        case DescriptorID::multilingual_component:
            return QString("Multilingual Component");
        case DescriptorID::private_data_specifier:
            return QString("Private Data Specifier");
        case DescriptorID::service_move:
            return QString("Service Move");
        case DescriptorID::short_smoothing_buffer:
            return QString("Short Smoothing Buffer");
        case DescriptorID::frequency_list:
            return QString("Frequency List");
        case DescriptorID::partial_transport_stream:
            return QString("Partial Transport Stream");
        case DescriptorID::data_broadcast:
            return QString("Data Broadcast");
        case DescriptorID::scrambling:
            return QString("Scrambling");
        case DescriptorID::data_broadcast_id:
            return QString("Data Broadcast Identifier");
        case DescriptorID::transport_stream:
            return QString("Transport Stream");
        case DescriptorID::DSNG:
            return QString("DSNG");
        case DescriptorID::PDC:
            return QString("PDC");
        case DescriptorID::AC3:
            return QString("AC-3");
        case DescriptorID::ancillary_data:
            return QString("Ancillary Data");
        case DescriptorID::cell_list:
            return QString("Cell List");
        case DescriptorID::cell_frequency_link:
            return QString("Cell Frequency Link");
        case DescriptorID::announcement_support:
            return QString("Announcement Support");
        case DescriptorID::application_signalling:
            return QString("Application Signalling");
        case DescriptorID::adaptation_field_data:
            return QString("Adaptation Field Data");
        case DescriptorID::service_identifier:
            return QString("Service Identifier");
        case DescriptorID::service_availability:
            return QString("Service Availability");
        case DescriptorID::default_authority:
            return QString("Default Authority");
        case DescriptorID::related_content:
            return QString("Related Content");
        case DescriptorID::TVA_id:
            return QString("TVA Id");
        case DescriptorID::dvb_content_identifier:
            return QString("DVB Content Identifier");
        case DescriptorID::time_slice_fec_identifier:
            return QString("Time Slice FEC Identifier");
        case DescriptorID::ECM_repetition_rate:
            return QString("ECM Repetition Rate");

        /// ATSC
        case DescriptorID::atsc_stuffing:
            return QString("ATSC Stuffing");
        case DescriptorID::audio_stream:
            return QString("Audio");
        case DescriptorID::caption_service:
            return QString("Caption Service");
        case DescriptorID:: content_advisory:
            return QString("Content Advisory");
        case DescriptorID::extended_channel_name:
            return QString("Extended Channel Name");
        case DescriptorID::service_location:
            return QString("Service Location");
        case DescriptorID::atsc_time_shifted_service:
            return QString("ATSC Time-shifted Service");
        case DescriptorID::component_name:
            return QString("Component Name");
        case DescriptorID::DCC_departing_request:
            return QString("DCC Departing Request");
        case DescriptorID::DCC_arriving_request:
            return QString("DCC Arriving Request");
        case DescriptorID::DRM_control:
            return QString("Consumer Restrictions Control");
        case DescriptorID::atsc_content_identifier:
            return QString("Content Identifier");

        default: return QString("Unknown");
    }
}

QString MPEGDescriptor::toString() const {
    QString str;

    if (DescriptorID::registration == DescriptorTag())
    {
        str = RegistrationDescriptor(_data).toString();
    }
    else if (DescriptorID::audio_stream == DescriptorTag())
    {
        str = AudioStreamDescriptor(_data).toString();
    }
    else if (DescriptorID::caption_service == DescriptorTag())
    {
        str = CaptionServiceDescriptor(_data).toString();
    }
    else if (DescriptorID::component_name == DescriptorTag())
    {
        str = ComponentNameDescriptor(_data).toString();
    }
    else
    {
        str.append(QString("   %1 Descriptor (0x%2)")
                   .arg(DescriptorTagString())
                   .arg(int(DescriptorTag()), 0, 16));
        str.append(QString(" length(%1)").arg(int(DescriptorLength())));
        //for (uint i=0; i<DescriptorLength(); i++)
        //    str.append(QString(" 0x%1").arg(int(_data[i+2]), 0, 16));
    }
    return str;
}

