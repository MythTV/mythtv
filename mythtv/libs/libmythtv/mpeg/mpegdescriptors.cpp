// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#include <limits.h>

#include "atscdescriptors.h"
#include "dvbdescriptors.h"

QMutex                RegistrationDescriptor::description_map_lock;
bool                  RegistrationDescriptor::description_map_initialized = false;
QMap<QString,QString> RegistrationDescriptor::description_map;

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

desc_list_t MPEGDescriptor::ParseAndExclude(
    const unsigned char* data, uint len, int excluded_descid)
{
    desc_list_t tmp;
    uint off = 0;
    while (off < len)
    {
        if ((data+off)[0] != excluded_descid)
            tmp.push_back(data+off);
        MPEGDescriptor desc(data+off);
        off += desc.DescriptorLength() + 2;
    }
    return tmp;
}

desc_list_t MPEGDescriptor::ParseOnlyInclude(
    const unsigned char* data, uint len, int excluded_descid)
{
    desc_list_t tmp;
    uint off = 0;
    while (off < len)
    {
        if ((data+off)[0] == excluded_descid)
            tmp.push_back(data+off);
        MPEGDescriptor desc(data+off);
        off += desc.DescriptorLength() + 2;
    }
    return tmp;
}

const unsigned char* MPEGDescriptor::Find(const desc_list_t &parsed,
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

desc_list_t MPEGDescriptor::FindAll(const desc_list_t &parsed, uint desc_tag)
{
    desc_list_t tmp;
    desc_list_t::const_iterator it = parsed.begin();
    for (; it != parsed.end(); ++it)
    {
        if ((*it)[0] == desc_tag)
            tmp.push_back(*it);
    }
    return tmp;
}

static uint maxPriority(const QMap<uint,uint> &langPrefs)
{
    uint max_pri = 0;
    QMap<uint,uint>::const_iterator it = langPrefs.begin();
    for (; it != langPrefs.end(); ++it)
        max_pri = max(max_pri, *it);
    return max_pri;
}

const unsigned char* MPEGDescriptor::FindBestMatch(
    const desc_list_t &parsed, uint desc_tag, QMap<uint,uint> &langPrefs)
{
    uint match_idx = 0;
    uint match_pri = UINT_MAX;
    int  unmatched_idx = -1;

    uint i = (desc_tag == DescriptorID::short_event) ? 0 : parsed.size();
    for (; i < parsed.size(); i++)
    {
        if (DescriptorID::short_event == parsed[i][0])
        {
            ShortEventDescriptor sed(parsed[i]);
            QMap<uint,uint>::const_iterator it =
                langPrefs.find(sed.CanonicalLanguageKey());

            if ((it != langPrefs.end()) && (*it < match_pri))
            {
                match_idx = i;
                match_pri = *it;
            }

            if (unmatched_idx < 0)
                unmatched_idx = i;
        }
    }

    if (match_pri != UINT_MAX)
        return parsed[match_idx];

    if ((desc_tag == DescriptorID::short_event) && (unmatched_idx >= 0))
    {
        ShortEventDescriptor sed(parsed[unmatched_idx]);
        langPrefs[sed.CanonicalLanguageKey()] = maxPriority(langPrefs) + 1;
        return parsed[unmatched_idx];
    }

    return NULL;
}

desc_list_t MPEGDescriptor::FindBestMatches(
    const desc_list_t &parsed, uint desc_tag, QMap<uint,uint> &langPrefs)
{
    uint match_pri = UINT_MAX;
    int  match_key = 0;
    int  unmatched_idx = -1;

    uint i = (desc_tag == DescriptorID::extended_event) ? 0 : parsed.size();
    for (; i < parsed.size(); i++)
    {
        if (DescriptorID::extended_event == parsed[i][0])
        {
            ExtendedEventDescriptor eed(parsed[i]);
            QMap<uint,uint>::const_iterator it =
                langPrefs.find(eed.CanonicalLanguageKey());

            if ((it != langPrefs.end()) && (*it < match_pri))
            {
                match_key = eed.LanguageKey();
                match_pri = *it;
            }

            if (unmatched_idx < 0)
                unmatched_idx = i;
        }
    }

    if ((desc_tag == DescriptorID::extended_event) &&
        (match_key == 0) && (unmatched_idx >= 0))
    {
        ExtendedEventDescriptor eed(parsed[unmatched_idx]);
        langPrefs[eed.CanonicalLanguageKey()] = maxPriority(langPrefs) + 1;
        match_key = eed.LanguageKey();
    }

    desc_list_t tmp;
    if (match_pri == UINT_MAX)
        return tmp;

    for (uint i = 0; i < parsed.size(); i++)
    {
        if ((DescriptorID::extended_event == desc_tag) &&
            (DescriptorID::extended_event == parsed[i][0]))
        {
            ExtendedEventDescriptor eed(parsed[i]);
            if (eed.LanguageKey() == match_key)
                tmp.push_back(parsed[i]);
        }
    }

    return tmp;
}


QString MPEGDescriptor::DescriptorTagString() const
{
    switch (DescriptorTag())
    {
        // MPEG
        case DescriptorID::video:
            return QString("Video");
        case DescriptorID::audio:
            return QString("Audio");
        case DescriptorID::hierarchy:
            return QString("Hierarchy");
        case DescriptorID::registration:
            return QString("Registration");
        case DescriptorID::conditional_access:
            return QString("Conditional Access");
        case DescriptorID::ISO_639_language:
            return QString("ISO-639 Language");
        case DescriptorID::system_clock:
            return QString("System Clock");
        case DescriptorID::multiplex_buffer_utilization:
            return QString("Multiplex Buffer Utilization");
        case DescriptorID::copyright:
            return QString("Copyright");
        case DescriptorID::maximum_bitrate:
            return QString("Maximum Bitrate");
        case DescriptorID::private_data_indicator:
            return QString("Private Data Indicator");
        case DescriptorID::smoothing_buffer:
            return QString("Smoothing Buffer");
        case DescriptorID::STD:
            return QString("STD");
        case DescriptorID::IBP:
            return QString("IBP");
        case DescriptorID::carousel_identifier:
            return QString("DSM-CC Carousel Identifier");
        case DescriptorID::association_tag:
            return QString("DSM-CC Association Tag");
        case DescriptorID::deferred_association_tag:
            return QString("DSM-CC Deferred Association Tag");
        case DescriptorID::npt_reference:
            return QString("DSM-CC NPT Reference");
        case DescriptorID::npt_endpoint:
            return QString("DSM-CC NPT Endpoint");
        case DescriptorID::stream_mode:
            return QString("DSM-CC Stream Mode");
        case DescriptorID::stream_event:
            return QString("DSM-CC Stream Event");
        case DescriptorID::mpeg4_video:
            return QString("MPEG-4 Video");
        case DescriptorID::mpeg4_audio:
            return QString("MPEG-4 Audio");
        case DescriptorID::IOD:
            return QString("IOD");
        case DescriptorID::SL:
            return QString("SL");
        case DescriptorID::FMC:
            return QString("FMC");
        case DescriptorID::external_es_id:
            return QString("External ES ID");
        case DescriptorID::mux_code:
            return QString("Multimpex Code");
        case DescriptorID::fmx_buffer_size:
            return QString("FMX buffer Size");
        case DescriptorID::multiplex_buffer:
            return QString("Multiplex Buffer");
        case DescriptorID::content_labeling:
            return QString("Content Labeling");
        case DescriptorID::metadata_pointer:
            return QString("Metadata Pointer");
        case DescriptorID::metadata:
            return QString("Metadata");
        case DescriptorID::metadata_std:
            return QString("Metadata Std");
        case DescriptorID::avc_video:
            return QString("AVC Video");
        case DescriptorID::ipmp:
            return QString("IPMP Digital Restrictions Management");
        case DescriptorID::avc_timing__hrd:
            return QString("AVC Timing & HRD");
        case DescriptorID::MPEG2_AAC_audio:
            return QString("AAC Audio");
        case DescriptorID::FlexMuxTiming:
            return QString("Flex Mux Timing");

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
        case DescriptorID::S2_delivery_system:
            return QString("DVB-S2 Delivery Identifier");
        case DescriptorID::EAC3:
            return QString("E-AC-3");
        case DescriptorID::DTS:
            return QString("DTS");
        case DescriptorID::AAC:
            return QString("AAC");

        // private
        case DescriptorID::dvb_uk_channel_list:
            return QString("Possibly a DVB UK Channel List");

        /// ATSC
        case DescriptorID::atsc_stuffing:
            return QString("ATSC Stuffing");
        case DescriptorID::ac3_audio_stream:
            return QString("AC-3 Audio");
        case DescriptorID::caption_service:
            return QString("Caption Service");
        case DescriptorID::content_advisory:
            return QString("Content Advisory");
        case DescriptorID::dish_event_name:
            return QString("Dishnet EIT Name");
        case DescriptorID::dish_event_description:
            return QString("Dishnet EIT Description");
        case DescriptorID::extended_channel_name:
            return QString("Extended Channel Name");
        case DescriptorID::service_location:
            return QString("Service Location");
        case DescriptorID::atsc_time_shifted_service:
            return QString("ATSC Time-shifted Service");
        case DescriptorID::component_name:
            return QString("Component Name");
        case DescriptorID::atsc_data_service:
            return QString("ATSC Data Service");
        case DescriptorID::atsc_pid_count:
            return QString("ATSC PID Count");
        case DescriptorID::atsc_download:
            return QString("ATSC Download");
        case DescriptorID::multiprotocol_encapsulation:
            return QString("ATSC Multiprotocol Encapsulation");
        case DescriptorID::DCC_departing_request:
            return QString("DCC Departing Request");
        case DescriptorID::DCC_arriving_request:
            return QString("DCC Arriving Request");
        case DescriptorID::DRM_control:
            return QString("Consumer Restrictions Control");
        case DescriptorID::atsc_genre:
            return QString("ATSC Genre");
        case DescriptorID::atsc_private_information:
            return QString("ATSC Private Information");
        case DescriptorID::atsc_content_identifier:
            return QString("Content Identifier");

        default:
            return QString("Unknown(%1)").arg(DescriptorTag());
    }
}

QString MPEGDescriptor::toString() const
{
    QString str;

    if (DescriptorID::registration == DescriptorTag())
        str = RegistrationDescriptor(_data).toString();
    else if (DescriptorID::ISO_639_language == DescriptorTag())
        str = ISO639LanguageDescriptor(_data).toString();
    else if (DescriptorID::avc_video == DescriptorTag())
        str = AVCVideoDescriptor(_data).toString();
    else if (DescriptorID::ac3_audio_stream == DescriptorTag())
        str = AudioStreamDescriptor(_data).toString();
    else if (DescriptorID::caption_service == DescriptorTag())
        str = CaptionServiceDescriptor(_data).toString();
    else if (DescriptorID::extended_channel_name == DescriptorTag())
        str = ExtendedChannelNameDescriptor(_data).toString();
    else if (DescriptorID::component_name == DescriptorTag())
        str = ComponentNameDescriptor(_data).toString();
    else if (DescriptorID::conditional_access == DescriptorTag())
        str = ConditionalAccessDescriptor(_data).toString();
    else if (DescriptorID::network_name == DescriptorTag())
        str = NetworkNameDescriptor(_data).toString();
    else if (DescriptorID::linkage == DescriptorTag())
        str = LinkageDescriptor(_data).toString();
    else if (DescriptorID::adaptation_field_data == DescriptorTag())
        str = AdaptationFieldDataDescriptor(_data).toString();
    else if (DescriptorID::ancillary_data == DescriptorTag())
        str = AncillaryDataDescriptor(_data).toString();
    else if (DescriptorID::cable_delivery_system == DescriptorTag())
        str = CableDeliverySystemDescriptor(_data).toString();
    else if (DescriptorID::satellite_delivery_system == DescriptorTag())
        str = SatelliteDeliverySystemDescriptor(_data).toString();
    else if (DescriptorID::terrestrial_delivery_system == DescriptorTag())
        str = TerrestrialDeliverySystemDescriptor(_data).toString();
    else if (DescriptorID::frequency_list == DescriptorTag())
        str = FrequencyListDescriptor(_data).toString();
    else if (DescriptorID::dvb_uk_channel_list == DescriptorTag())
        str = UKChannelListDescriptor(_data).toString();
    else if (DescriptorID::service == DescriptorTag())
        str = ServiceDescriptor(_data).toString();
    else if (DescriptorID::stream_identifier == DescriptorTag())
        str = StreamIdentifierDescriptor(_data).toString();
    else if (DescriptorID::default_authority == DescriptorTag())
        str = DefaultAuthorityDescriptor(_data).toString();
    else if (DescriptorID::bouquet_name == DescriptorTag())
        str = BouquetNameDescriptor(_data).toString();
    else if (DescriptorID::country_availability == DescriptorTag())
        str = CountryAvailabilityDescriptor(_data).toString();
    else if (DescriptorID::service_list == DescriptorTag())
        str = ServiceListDescriptor(_data).toString();
    else
    {
        str.append(QString("%1 Descriptor (0x%2)")
                   .arg(DescriptorTagString())
                   .arg(int(DescriptorTag()), 0, 16));
        str.append(QString(" length(%1)").arg(int(DescriptorLength())));
        //for (uint i=0; i<DescriptorLength(); i++)
        //    str.append(QString(" 0x%1").arg(int(_data[i+2]), 0, 16));
    }
    return str;
}

void RegistrationDescriptor::InitializeDescriptionMap(void)
{
    QMutexLocker locker(&description_map_lock);
    if (description_map_initialized)
        return;

    description_map["AC-3"] = "ATSC audio stream A/52";
    description_map["AVSV"] = "China A/V Working Group";
    description_map["BDC0"] = "Broadcast Data Corporation Software Data Service";
    description_map["BSSD"] = "SMPTE 302M-1998 Audio data as specified in (AES3)";
    description_map["CAPO"] = "SMPTE 315M-1999 Camera Positioning Information";
    description_map["CUEI"] = "SCTE 35 2003, Cable Digital Program Insertion Cueing Message";
    description_map["DDED"] = "LGEUS Digital Delivery with encryption and decryption";
    description_map["DISH"] = "EchoStar MPEG streams";
    description_map["DRA1"] = "Chinese EIS SJ/T11368-2006 DRA digital audio";
    description_map["DTS1"] = "DTS Frame size of 512 bytes";
    description_map["DTS2"] = "DTS Frame size of 1024 bytes";
    description_map["DTS3"] = "DTS Frame size of 2048";
    description_map["DVDF"] = "DVD compatible MPEG2-TS";
    description_map["ETV1"] = "CableLabs ETV info is present";
    description_map["GA94"] = "ATSC program ID A/53";
    description_map["GWKS"] = "GuideWorks EPG info";
    description_map["HDMV"] = "Blu-Ray A/V for read-only media (H.264 TS)";
    description_map["HDMX"] = "Matsushita-TS";
    description_map["KLVA"] = "SMPTE RP 217-2001 MXF KLV packets present";
    description_map["LU-A"] = "SMPTE RDD-11 HDSDI HD serial/video data";
    description_map["MTRM"] = "D-VHS compatible MPEG2-TS";
    description_map["NMR1"] = "Nielsen content identifier";
    description_map["PAUX"] = "Philips ES containing low-speed data";
    description_map["PMSF"] = "MPEG-TS stream modified by STB";
    description_map["PRMC"] = "Philips ES containing remote control data";
    description_map["SCTE"] = "SCTE 54 2003 Digital Video Service Multiplex and TS for Cable";
    description_map["SEN1"] = "ATSC Private Information identifies source of stream";
    description_map["SESF"] = "Blu-Ray A/V for ReWritable media (H.264 TS)";
    description_map["SPLC"] = "SMPTE Proposed 312M Splice Point compatible TS";
    description_map["SVMD"] = "SMPTE Proposed Video Metatdata Dictionary for MPEG2-TS";
    description_map["SZMI"] = "ATSC Private Info from Building B";
    description_map["TRIV"] = "ATSC Private Info from Triveni Digital";
    description_map["TSBV"] = "Toshiba self-encoded H.264 TS";
    description_map["TSHV"] = "Sony self-encoded MPEG-TS and private data";
    description_map["TSMV"] = "Sony self-encoded MPEG-TS and private data";
    description_map["TVG1"] = "TV Guide EPG Data";
    description_map["TVG2"] = "TV Guide EPG Data";
    description_map["TVG3"] = "TV Guide EPG Data";
    description_map["ULE1"] = "IETF RFC4326 compatible MPEG2-TS";
    description_map["VC-1"] = "SMPTE Draft RP 227 VC-1 Bitstream Transport Encodings";

    for (uint i = 0; i <= 99; i++)
    {
        description_map[QString("US%1").arg(i, 2, QLatin1Char('0'))] =
            "NIMA, Unspecified military application";
    }

    description_map_initialized = true;
}

QString RegistrationDescriptor::GetDescription(const QString &fmt)
{
    InitializeDescriptionMap();

    QString ret = QString::null;
    {
        QMutexLocker locker(&description_map_lock);
        QMap<QString,QString>::const_iterator it = description_map.find(fmt);
        if (it != description_map.end())
            ret = *it;
    }

    if (!ret.isNull())
        ret.detach();

    return ret;
}

QString RegistrationDescriptor::toString() const
{
    QString fmt = FormatIdentifierString();
    QString msg = QString("Registration Descriptor: '%1' ").arg(fmt);

    QString msg2 = GetDescription(fmt);
    if (msg2.isEmpty())
        msg2 = "Unknown, see http://www.smpte-ra.org/mpegreg/mpegreg.html";

    return msg + msg2;
}

QString ConditionalAccessDescriptor::toString() const
{
    return QString("Conditional Access: sid(0x%1) pid(0x%2) data_size(%3)")
        .arg(SystemID(),0,16).arg(PID(),0,16).arg(DataSize());
}

QString ISO639LanguageDescriptor::toString() const
{
    return QString("ISO-639 Language: code(%1) canonical(%2) eng(%3)")
        .arg(LanguageString()).arg(CanonicalLanguageString())
        .arg(iso639_key_toName(CanonicalLanguageKey()));
}

QString AVCVideoDescriptor::toString() const
{
    return QString("AVC Video: IDC prof(%1) IDC level(%2) sets(%3%4%5) "
                   "compat(%6) still(%7) 24hr(%8)")
        .arg(ProfileIDC()).arg(LevelIDC())
        .arg(ConstaintSet0()).arg(ConstaintSet1()).arg(ConstaintSet2())
        .arg(AVCCompatible()).arg(AVCStill()).arg(AVC24HourPicture());
}
