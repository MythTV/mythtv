// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson

#include <limits.h>

#include "sctedescriptors.h"
#include "atscdescriptors.h"
#include "dvbdescriptors.h"
#include "mythmiscutil.h" // for xml_indent

QMutex                RegistrationDescriptor::description_map_lock;
bool                  RegistrationDescriptor::description_map_initialized = false;
QMap<QString,QString> RegistrationDescriptor::description_map;

desc_list_t MPEGDescriptor::Parse(
    const unsigned char *data, uint len)
{
    desc_list_t tmp;
    uint off = 0;
    while (off < len)
    {
        tmp.push_back(data+off);
        MPEGDescriptor desc(data+off, len-off);
        if (!desc.IsValid())
        {
            tmp.pop_back();
            break;
        }
        off += desc.size();
    }
    return tmp;
}

desc_list_t MPEGDescriptor::ParseAndExclude(
    const unsigned char *data, uint len, int excluded_descid)
{
    desc_list_t tmp;
    uint off = 0;
    while (off < len)
    {
        if ((data+off)[0] != excluded_descid)
            tmp.push_back(data+off);
        MPEGDescriptor desc(data+off, len-off);
        if (!desc.IsValid())
        {
            if ((data+off)[0] != excluded_descid)
                tmp.pop_back();
            break;
        }
        off += desc.size();
    }
    return tmp;
}

#include "mythlogging.h"

desc_list_t MPEGDescriptor::ParseOnlyInclude(
    const unsigned char *data, uint len, int excluded_descid)
{
    desc_list_t tmp;
    uint off = 0;
    while (off < len)
    {
        if ((data+off)[0] == excluded_descid)
            tmp.push_back(data+off);
        MPEGDescriptor desc(data+off, len-off);
        if (!desc.IsValid())
        {
            if ((data+off)[0] == excluded_descid)
                tmp.pop_back();
            break;
        }
        off += desc.size();
    }
    return tmp;
}

const unsigned char *MPEGDescriptor::Find(const desc_list_t &parsed,
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

const unsigned char *MPEGDescriptor::FindBestMatch(
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

#define EMPTY_STR_16 "","","","", "","","","", "","","","", "","","","",

const char *descriptor_tag_strings[256] =
{
    /* 0x00 */ "",                      /* 0x01 */ "",
    /* 0x02 */ "Video",                 /* 0x03 */ "Audio",
    /* 0x04 */ "Hierarchy",             /* 0x05 */ "Registration",
    /* 0x06 */ "Data Stream Alignment", /* 0x07 */ "Target Background Grid",
    /* 0x08 */ "Video Window",          /* 0x09 */ "Conditional Access",
    /* 0x0A */ "ISO-639 Language",      /* 0x0B */ "System Clock",
    /* 0x0C */ "Multiplex Buffer Utilization", /* 0x0D */ "Copyright",
    /* 0x0E */ "Maximum Bitrate",       /* 0x0F */ "Private Data Indicator",

    /* 0x10 */ "Smooting Buffer",       /* 0x11 */ "STD",
    /* 0x12 */ "IBP",                   /* 0x13 */ "DSM-CC Carousel Identifier",
    /* 0x14 */ "DSM-CC Association Tag",
    /* 0x15 */ "DSM-CC Deferred Association Tag",
    /* 0x16 */ "Reserved(0x16)",        /* 0x17 */ "DSM-CC NPT Reference",
    /* 0x18 */ "DSM-CC NPT Endpoint",   /* 0x19 */ "DSM-CC Stream Mode",
    /* 0x1A */ "DSM-CC Stream Event",   /* 0x1B */ "MPEG-4 Video",
    /* 0x1C */ "MPEG-4 Audio",          /* 0x1D */ "IOD",
    /* 0x1E */ "SL",                    /* 0x1F */ "FMC",

    /* 0x20 */ "External ES ID",        /* 0x21 */ "Multimpex Code",
    /* 0x22 */ "FMX buffer Size",       /* 0x23 */ "Multiplex Buffer",
    /* 0x24 */ "Content Labeling",      /* 0x25 */ "Metadata Pointer",
    /* 0x26 */ "Metadata",              /* 0x27 */ "Metadata Std",
    /* 0x28 */ "AVC Video",             /* 0x29 */ "IPMP DRM",
    /* 0x2A */ "AVC Timing & HRD",      /* 0x2B */ "AAC Audio",
    /* 0x2C */ "Flex Mux Timing",       /* 0x2D */ "",
    /* 0x2E */ "",                      /* 0x2F */ "",

    /* 0x30-0x3F */ EMPTY_STR_16

    /* 0x40 */ "Network Name",          /* 0x41 */ "Service List",
    /* 0x42 */ "DVB Stuffing",          /* 0x43 */ "Satellite Delivery System",
    /* 0x44 */ "Cable Delivery System", /* 0x45 */ "VBI Data",
    /* 0x46 */ "VBI Teletext",          /* 0x47 */ "Bouquet Name",
    /* 0x48 */ "Service",               /* 0x49 */ "Country Availability",
    /* 0x4A */ "Linkage",               /* 0x4B */ "NVOD Reference",
    /* 0x4C */ "DVB Time-shifted Service",/* 0x4D */ "Short Event",
    /* 0x4E */ "Extended Event",        /* 0x4F */ "Time-shifted Event",

    /* 0x50 */ "Component",             /* 0x51 */ "Mosaic",
    /* 0x52 */ "Stream Identifier",
    /* 0x53 */ "Conditional Access Identifier",
    /* 0x54 */ "Content",               /* 0x55 */ "Parental Rating",
    /* 0x56 */ "Teletext",              /* 0x57 */ "Telephone",
    /* 0x58 */ "Local Time Offset",     /* 0x59 */ "Subtitling",
    /* 0x5A */ "Terrestrial Delivery System",
    /* 0x5B */ "Multilingual Network Name",
    /* 0x5C */ "Multilingual Bouquet Name",
    /* 0x5D */ "Multilingual Service Name",
    /* 0x5E */ "Multilingual Component",
    /* 0x5F */ "Private Data Specifier",

    /* 0x60 */ "Service Move",          /* 0x61 */ "Short Smoothing Buffer",
    /* 0x62 */ "Frequency List",        /* 0x63 */ "Partial Transport Stream",
    /* 0x64 */ "Data Broadcast",        /* 0x65 */ "Scrambling",
    /* 0x66 */ "Data Broadcast ID",     /* 0x67 */ "Transport Stream",
    /* 0x68 */ "DSNG",                  /* 0x69 */ "PDC",
    /* 0x6A */ "AC-3",                  /* 0x6B */ "Ancillary Data",
    /* 0x6C */ "Cell List",             /* 0x6D */ "Cell Frequency Link",
    /* 0x6E */ "Announcement Support",  /* 0x6F */ "Application Signalling",

    /* 0x70 */ "Adaptation Field Data", /* 0x71 */ "Service Identifier",
    /* 0x72 */ "Service Availability",  /* 0x73 */ "Default Authority",
    /* 0x74 */ "Related Content",       /* 0x75 */ "TVA ID",
    /* 0x76 */ "DVB Content Identifier",/* 0x77 */ "Time Slice FEC Identifier",
    /* 0x78 */ "ECM Repetition Rate",   /* 0x79 */ "DVB-S2 Delivery Identifier",
    /* 0x7A */ "E-AC-3",                /* 0x7B */ "DTS",
    /* 0x7C */ "AAC",                   /* 0x7D */ "",
    /* 0x7E */ "",                      /* 0x7F */ "",

    /* 0x80 */ "ATSC Stuffing",         /* 0x81 */ "AC-3 Audio",
    /* 0x82 */ "SCTE Frame Rate",       /* 0x83 */ "SCTE Extended Video",
    /* 0x84 */ "SCTE Component Name",   /* 0x85 */ "ATSC Program Identifier",
    /* 0x86 */ "Caption Service",       /* 0x87 */ "Content Advisory",
    /* 0x88 */ "ATSC CA Descriptor",    /* 0x89 */ "ATSC Descriptor Tag",
    /* 0x8A */ "SCTE CUE Identifier",   /* 0x8B */ "",
    /* 0x8C */ "TimeStamp",             /* 0x8D */ "",
    /* 0x8E */ "",                      /* 0x8F */ "",

    /* 0x90 */ "SCTE Frequency Spec",   /* 0x91 */ "SCTE Modulation Params",
    /* 0x92 */ "SCTE TSID",             /* 0x93 */ "SCTE Revision Detection",
    /* 0x94 */ "SCTE Two part channel", /* 0x95 */ "SCTE Channel Properties",
    /* 0x96 */ "SCTE Daylight Savings", /* 0x97 */ "SCTE AFD",
    /* 0x98 */ "", /* 0x99 */ "",
    /* 0x9A */ "", /* 0x9B */ "",
    /* 0x9C */ "", /* 0x9D */ "",
    /* 0x9E */ "", /* 0x9F */ "",

    /* 0xA0 */ "Extended Channel Name", /* 0xA1 */ "Service Location",
    /* 0xA2 */ "ATSC Time-shifted Service",/*0xA3*/"Component Name",
    /* 0xA4 */ "ATSC Data Service",     /* 0xA5 */ "ATSC PID Count",
    /* 0xA6 */ "ATSC Download",
    /* 0xA7 */ "ATSC Multiprotocol Encapsulation",
    /* 0xA8 */ "DCC Departing Request", /* 0xA9 */ "DCC Arriving Request",
    /* 0xAA */ "ATSC Restrictions Control",/*0xAB*/"ATSC Genre",
    /* 0xAC */ "SCTE MAC Address List", /* 0xAD */ "ATSC Private Information",
    /* 0xAE */ "ATSC Compatibility Wrap",/* 0xAF */"ATSC Broadcaster Policy",

    /* 0xB0 */ "", /* 0xB1 */ "",
    /* 0xB2 */ "", /* 0xB3 */ "",
    /* 0xB4 */ "", /* 0xB5 */ "",
    /* 0xB6 */ "ATSC Content ID", /* 0xB7 */ "",
    /* 0xB8 */ "", /* 0xB9 */ "",
    /* 0xBA */ "", /* 0xBB */ "",
    /* 0xBC */ "", /* 0xBD */ "",
    /* 0xBE */ "", /* 0xBF */ "",

    /* 0xC0-0xCF */ EMPTY_STR_16
    /* 0xD0-0xDF */ EMPTY_STR_16
    /* 0xE0-0xEF */ EMPTY_STR_16
    /* 0xF0-0xFF */ EMPTY_STR_16
};

static void comma_list_append(QString &str, QString extra)
{
    if (str.isEmpty())
        str = extra;
    else
        str = str + ", " + extra;
}

QString MPEGDescriptor::DescriptorTagString(void) const
{
    QString str = descriptor_tag_strings[DescriptorTag()];

    switch (DescriptorTag())
    {
        case PrivateDescriptorID::dvb_uk_channel_list: /* 0x83 */
            comma_list_append(str, "Possibly DVB UK Channel List");
            break;
        case PrivateDescriptorID::dish_event_rights: /* 0x87 */
            comma_list_append(str, "Possibly Dishnet Rights");
            break;
        case PrivateDescriptorID::dish_event_mpaa:   /*0x89 */
            comma_list_append(str, "Possibly Dishnet MPAA");
            break;
        case PrivateDescriptorID::dish_event_name:   /*0x91 */
            comma_list_append(str, "Possibly Dishnet EIT Name");
            break;
        case PrivateDescriptorID::dish_event_description: /* 0x92 */
            comma_list_append(str, "Possibly Dishnet EIT Description");
            break;
        case PrivateDescriptorID::dish_event_properties: /* 0x94 */
            comma_list_append(str, "Possibly Dishnet Properties");
            break;
        case PrivateDescriptorID::dish_event_vchip: /* 0x95 */
            comma_list_append(str, "Possibly Dishnet V-Chip");
            break;
        case PrivateDescriptorID::dish_event_tags: /* 0x96 */
            comma_list_append(str, "Possibly Dishnet Tag");
            break;
        case PrivateDescriptorID::premiere_content_order: /* 0xF0 */
            comma_list_append(str, "Possibly Premiere DE Content Order");
            break;
        case PrivateDescriptorID::premiere_parental_information: /* 0xF1 */
            comma_list_append(str, "Possibly Premiere DE Parental Information");
            break;
        case PrivateDescriptorID::premiere_content_transmission: /* 0xF2 */
            comma_list_append(str, "Possibly Premiere DE Content Transmission");
            break;
    }

    if (str.isEmpty())
        str = QString("Unknown(%1)").arg(DescriptorTag());

    return str;
}

#define SET_STRING(DESC_NAME) do { \
    if (IsValid()) { DESC_NAME d(_data, DescriptorLength()+2); \
    if (d.IsValid()) str = d.toString(); } } while (0)

QString MPEGDescriptor::toString() const
{
    QString str;

    if (DescriptorID::registration == DescriptorTag())
        SET_STRING(RegistrationDescriptor);
    else if (DescriptorID::iso_639_language == DescriptorTag())
        SET_STRING(ISO639LanguageDescriptor);
    else if (DescriptorID::avc_video == DescriptorTag())
        SET_STRING(AVCVideoDescriptor);
    else if (DescriptorID::ac3_audio_stream == DescriptorTag())
        SET_STRING(AudioStreamDescriptor);
    else if (DescriptorID::caption_service == DescriptorTag())
        SET_STRING(CaptionServiceDescriptor);
    else if (DescriptorID::extended_channel_name == DescriptorTag())
        SET_STRING(ExtendedChannelNameDescriptor);
    else if (DescriptorID::component_name == DescriptorTag())
        SET_STRING(ComponentNameDescriptor);
    else if (DescriptorID::conditional_access == DescriptorTag())
        SET_STRING(ConditionalAccessDescriptor);
    else if (DescriptorID::network_name == DescriptorTag())
        SET_STRING(NetworkNameDescriptor);
    //else if (DescriptorID::linkage == DescriptorTag())
    //    SET_STRING(LinkageDescriptor);
    else if (DescriptorID::adaptation_field_data == DescriptorTag())
        SET_STRING(AdaptationFieldDataDescriptor);
    //else if (DescriptorID::ancillary_data == DescriptorTag())
    //    SET_STRING(AncillaryDataDescriptor);
    else if (DescriptorID::cable_delivery_system == DescriptorTag())
        SET_STRING(CableDeliverySystemDescriptor);
    else if (DescriptorID::satellite_delivery_system == DescriptorTag())
        SET_STRING(SatelliteDeliverySystemDescriptor);
    else if (DescriptorID::terrestrial_delivery_system == DescriptorTag())
        SET_STRING(TerrestrialDeliverySystemDescriptor);
    else if (DescriptorID::frequency_list == DescriptorTag())
        SET_STRING(FrequencyListDescriptor);
    else if (DescriptorID::service == DescriptorTag())
        SET_STRING(ServiceDescriptor);
    else if (DescriptorID::stream_identifier == DescriptorTag())
        SET_STRING(StreamIdentifierDescriptor);
    else if (DescriptorID::default_authority == DescriptorTag())
        SET_STRING(DefaultAuthorityDescriptor);
    else if (DescriptorID::bouquet_name == DescriptorTag())
        SET_STRING(BouquetNameDescriptor);
    else if (DescriptorID::country_availability == DescriptorTag())
        SET_STRING(CountryAvailabilityDescriptor);
    else if (DescriptorID::service_list == DescriptorTag())
        SET_STRING(ServiceListDescriptor);
    else if (DescriptorID::scte_cue_identifier == DescriptorTag())
        SET_STRING(CueIdentifierDescriptor);
    else if (DescriptorID::scte_revision_detection == DescriptorTag())
        SET_STRING(RevisionDetectionDescriptor);
    else if (DescriptorID::teletext == DescriptorTag())
        SET_STRING(TeletextDescriptor);
    /// POSSIBLY UNSAFE ! -- begin
    else if (PrivateDescriptorID::dvb_uk_channel_list == DescriptorTag())
        SET_STRING(UKChannelListDescriptor);
    /// POSSIBLY UNSAFE ! -- end
    else if (IsValid())
    {
        str = QString("%1 Descriptor (0x%2) length(%3). Dumping\n")
            .arg(DescriptorTagString())
            .arg(DescriptorTag(),2,16,QChar('0'))
            .arg(DescriptorLength());
        //for (uint i=0; i<DescriptorLength(); i++)
        //    str.append(QString(" 0x%1").arg(int(_data[i+2]), 0, 16));
        str.append(hexdump());
    }
    else
    {
        str = "Invalid Descriptor";
    }
    return str;
}

/// Returns XML representation of string the TS Reader XML format.
/// When possible matching http://www.tsreader.com/tsreader/text-export.html
QString MPEGDescriptor::toStringXML(uint level) const
{
    QString indent_0 = xml_indent(level);
    QString indent_1 = xml_indent(level+1);
    QString str;

    str += indent_0 + "<Descriptor>\n";
    str += indent_1 + QString("<Tag>0x%1</Tag>\n")
        .arg(DescriptorTag(),2,16,QChar('0'));
    str += indent_1 + QString("<Description>%1</Description>\n")
        .arg(DescriptorTagString(),0,16);

    str += indent_1 + "<Data>";
    for (uint i = 0; i < DescriptorLength(); i++)
    {
        if (((i%8) == 0) && i)
            str += "\n" + indent_1 + "      ";
        str += QString("0x%1 ").arg(_data[i+2],2,16,QChar('0'));
    }

    str += "\n" + indent_1 + "</Data>\n";

    str += indent_1 + "<Decoded>" + toString() + "</Decoded>\n";

    str += indent_0 + "</Descriptor>";

    return str;
}

// Dump the descriptor in the same format as hexdump -C
QString MPEGDescriptor::hexdump(void) const
{
    QString str, hex, prt;
    uint i, ch;
    for (i=0; i<DescriptorLength(); i++)
    {
        ch = _data[i+2];
        hex.append(QString(" %1").arg(ch, 2, 16, QChar('0')));
        prt.append(QString("%1").arg(isalnum(ch) ? QChar(ch) : '.'));
        if (((i+1) % 8) == 0)
            hex.append(" ");
        if (((i+1) % 16) == 0)
        {
            str.append(QString("      %1 %2 |%3|\n")
                .arg(i - (i % 16),3,16,QChar('0'))
                .arg(hex).arg(prt));
            hex.clear();
            prt.clear();
        }
    }
    str.append(QString("      %1 %2 |%3|")
                .arg(i - (i % 16),3,16,QChar('0'))
                .arg(hex,-50,' ').arg(prt));
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
                   "compat(%6) still(%7) 24hr(%8) FramePacking(%9)")
        .arg(ProfileIDC()).arg(LevelIDC())
        .arg(ConstaintSet0()).arg(ConstaintSet1()).arg(ConstaintSet2())
        .arg(AVCCompatible()).arg(AVCStill()).arg(AVC24HourPicture())
        .arg(FramePackingSEINotPresentFlag());
}
