// C headers
#include <unistd.h>

// Qt headers
#include <QTextCodec>
#include <QCoreApplication>

// MythTV headers
#include "dvbdescriptors.h"
#include "iso6937tables.h"
#include "freesat_huffman.h"
#include "mythlogging.h"
#include "programinfo.h"


static QString decode_iso6937(const unsigned char *buf, uint length)
{
    // ISO/IEC 6937 to unicode (UCS2) convertor...
    // This is a composed encoding - accent first then plain character
    QString result = "";
    ushort ch = 0x20;
    for (uint i = 0; (i < length) && buf[i]; i++)
    {
        if (ch == 0xFFFF)
        {
            // Process second byte of two byte character
            ch = iso6937table_secondary[buf[i-1]][buf[i]];
            if (ch == 0xFFFF)
            {
                // If no valid code found in secondary table,
                // reprocess this second byte as first byte.
                ch = iso6937table_base[buf[i]];
                if (ch == 0xFFFF)
                    continue; // process second byte
            }
        }
        else
        {
            // Process first character of two possible characters.
            // double byte characters have a sentinel (0xffff) in this table.
            ch = iso6937table_base[buf[i]];
            if (ch == 0xFFFF)
                continue; // process second byte

        }
        result += QChar(ch);
    }
    return result;
}

static QString decode_text(const unsigned char *buf, uint length);

// Decode a text string according to ETSI EN 300 468 Annex A
QString dvb_decode_text(const unsigned char *src, uint raw_length,
                        const unsigned char *encoding_override,
                        uint encoding_override_length)
{
    if (!raw_length)
        return "";

    if (src[0] == 0x1f)
        return freesat_huffman_to_string(src, raw_length);

    if (((0x10 < src[0]) && (src[0] < 0x15)) ||
        ((0x15 < src[0]) && (src[0] < 0x20)))
    {
        // TODO: Handle multi-byte encodings
        LOG(VB_SIPARSER, LOG_ERR, 
            "dvb_decode_text: Multi-byte coded text is not yet supported.");
        return "";
    }

    // if a override encoding is specified and the default ISO 6937 encoding
    // would be used copy the override encoding in front of the text
    unsigned char *dst =
        new unsigned char[raw_length + encoding_override_length];

    uint length = 0;
    if (encoding_override && src[0] >= 0x20) {
        memcpy(dst, encoding_override, encoding_override_length);
        length = encoding_override_length;
    }

    // Strip formatting characters
    for (uint i = 0; i < raw_length; i++)
    {
        if ((src[i] < 0x80) || (src[i] > 0x9F))
            dst[length++] = src[i];
        // replace CR/LF with a space
        else if (src[i] == 0x8A)
            dst[length++] = 0x20;
    }

    // Exit on empty string, sans formatting.

    QString sStr = (!length) ? "" : decode_text(dst, length);

    delete [] dst;

    return sStr;
}

static QString decode_text(const unsigned char *buf, uint length)
{
    // Only some of the QTextCodec calls are reentrant.
    // If you use this please verify that you are using a reentrant call.
    static const QTextCodec *iso8859_codecs[16] =
    {
        QTextCodec::codecForName("Latin1"),
        QTextCodec::codecForName("ISO8859-1"),  // Western
        QTextCodec::codecForName("ISO8859-2"),  // Central European
        QTextCodec::codecForName("ISO8859-3"),  // Central European
        QTextCodec::codecForName("ISO8859-4"),  // Baltic
        QTextCodec::codecForName("ISO8859-5"),  // Cyrillic
        QTextCodec::codecForName("ISO8859-6"),  // Arabic
        QTextCodec::codecForName("ISO8859-7"),  // Greek
        QTextCodec::codecForName("ISO8859-8"),  // Hebrew, visually ordered
        QTextCodec::codecForName("ISO8859-9"),  // Turkish
        QTextCodec::codecForName("ISO8859-10"),
        QTextCodec::codecForName("ISO8859-11"),
        QTextCodec::codecForName("ISO8859-12"),
        QTextCodec::codecForName("ISO8859-13"),
        QTextCodec::codecForName("ISO8859-14"),
        QTextCodec::codecForName("ISO8859-15"), // Western
    };

    // Decode using the correct text codec
    if (buf[0] >= 0x20)
    {
        return decode_iso6937(buf, length);
    }
    else if ((buf[0] >= 0x01) && (buf[0] <= 0x0B))
    {
        return iso8859_codecs[4 + buf[0]]->toUnicode((char*)(buf + 1), length - 1);
    }
    else if (buf[0] == 0x10)
    {
        // If the first byte of the text field has a value "0x10"
        // then the following two bytes carry a 16-bit value (uimsbf) N
        // to indicate that the remaining data of the text field is
        // coded using the character code table specified by
        // ISO Standard 8859, parts 1 to 9

        uint code = buf[1] << 8 | buf[2];
        if (code <= 15)
            return iso8859_codecs[code]->toUnicode((char*)(buf + 3), length - 3);
        else
            return QString::fromLocal8Bit((char*)(buf + 3), length - 3);
    }
    else if (buf[0] == 0x15) // Already Unicode
    {
        return QString::fromUtf8((char*)(buf + 1), length - 1);
    }
    else
    {
        // Unknown/invalid encoding - assume local8Bit
        return QString::fromLocal8Bit((char*)(buf + 1), length - 1);
    }
}


QString dvb_decode_short_name(const unsigned char *src, uint raw_length)
{
    if (raw_length > 50)
    {
        LOG(VB_SIPARSER, LOG_WARNING,
            QString("dvb_decode_short_name: name is %1 chars "
                    "long. Unlikely to be a short name.")
                .arg(raw_length));
        return "";
    }

    if (((0x10 < src[0]) && (src[0] < 0x15)) ||
        ((0x15 < src[0]) && (src[0] < 0x20)))
    {
        // TODO: Handle multi-byte encodings
        LOG(VB_SIPARSER, LOG_ERR, "dvb_decode_short_name: "
                         "Multi-byte coded text is not yet supported.");
        return "";
    }

    unsigned char *dst = new unsigned char[raw_length];
    uint length = 0;

    // check for emphasis control codes
    for (uint i = 0; i < raw_length; i++)
        if (src[i] == 0x86)
            while ((++i < raw_length) && (src[i] != 0x87))
            {
                if ((src[i] < 0x80) || (src[i] > 0x9F))
                    dst[length++] = src[i];
                // replace CR/LF with a space
                else if (src[i] == 0x8A)
                    dst[length++] = 0x20;
            }

    QString sStr = (!length) ? dvb_decode_text(src, raw_length) 
                             : decode_text(dst, length);
                                     
    delete [] dst;

    return sStr;
}

QMutex             ContentDescriptor::categoryLock;
QMap<uint,QString> ContentDescriptor::categoryDesc;
volatile bool      ContentDescriptor::categoryDescExists = false;

ProgramInfo::CategoryType ContentDescriptor::GetMythCategory(uint i) const
{
    if (0x1 == Nibble1(i))
        return ProgramInfo::kCategoryMovie;
    if (0x4 == Nibble1(i))
        return ProgramInfo::kCategorySports;
    return ProgramInfo::kCategoryTVShow;
}

const char *linkage_types[] =
{
    "Reserved(0x00)",
    "Information Service",
    "EPG Service",
    "CA Replacement Service",
    "TS Containing Complete Network/Bouquet SI",
    "Service Replacement Service",
    "Data Broadcast Service",
    "RCS Map",
    "Mobile Hand-Over",
    "System Software Update Service",
    "TS Containing SSU, BAT or NIT",
    "IP/MAC Notification Service",
    "TS Containing INT, BAT or NIT",
    "Event Linkage",
};


QString LinkageDescriptor::LinkageTypeString(void) const
{
    if (LinkageType() < (sizeof(linkage_types) / sizeof(const char*)))
        return QString(linkage_types[LinkageType()]);
    if ((LinkageType() <= 0x7f) || (LinkageType() == 0x7f))
        return QString("Reserved(0x%1)").arg(LinkageType(),2,16,QChar('0'));
    return QString("User Defined(0x%1)").arg(LinkageType(),2,16,QChar('0'));
}

QString LinkageDescriptor::MobileHandOverTypeString(void) const
{
    if (kHandOverIdentical == MobileHandOverType())
        return "Hand-Over to an Identical Service";
    if (kHandOverLocalVariation == MobileHandOverType())
        return "Hand-Over to a Local Variation";
    if (kHandOverAssociatedService == MobileHandOverType())
        return "Hand-over to an Associated Service";
    return "Reserved";
}

QString ContentDescriptor::GetDescription(uint i) const
{
    if (!categoryDescExists)
        Init();

    QMutexLocker locker(&categoryLock);

    // Try to get detailed description
    QMap<uint,QString>::const_iterator it = categoryDesc.find(Nibble(i));
    if (it != categoryDesc.end())
        return *it;

    // Fall back to category description
    it = categoryDesc.find(Nibble1(i)<<4);
    if (it != categoryDesc.end())
        return *it;

    // Found nothing? Just return empty string.
    return "";
}

QString ContentDescriptor::toString() const
{
    QString tmp("ContentDescriptor: ");
    for (uint i = 0; i < Count(); i++)
        tmp += myth_category_type_to_string(GetMythCategory(i)) + " : " + GetDescription(i) + ", ";
    return tmp;
}

void ContentDescriptor::Init(void)
{
    QMutexLocker locker(&categoryLock);

    if (categoryDescExists)
        return;

    //: %1 is the main category, %2 is the subcategory
    QString subCatStr = QCoreApplication::translate("(Categories)", 
        "%1 - %2", "Category with subcategory display");

    categoryDesc[0x10] = QCoreApplication::translate("(Categories)", "Movie");
    categoryDesc[0x11] = subCatStr
        .arg(QCoreApplication::translate("(Categories)", "Movie"))
        .arg(QCoreApplication::translate("(Categories)", "Detective/Thriller"));
    categoryDesc[0x12] = subCatStr
        .arg(QCoreApplication::translate("(Categories)", "Movie"))
        .arg(QCoreApplication::translate("(Categories)", 
                                         "Adventure/Western/War"));
    categoryDesc[0x13] = subCatStr
        .arg(QCoreApplication::translate("(Categories)", "Movie"))
        .arg(QCoreApplication::translate("(Categories)", 
                                         "Science Fiction/Fantasy/Horror"));
    categoryDesc[0x14] = subCatStr
        .arg(QCoreApplication::translate("(Categories)", "Movie"))
        .arg(QCoreApplication::translate("(Categories)", "Comedy"));
    categoryDesc[0x15] = subCatStr
        .arg(QCoreApplication::translate("(Categories)", "Movie"))
        .arg(QCoreApplication::translate("(Categories)", 
                                         "Soap/melodrama/folkloric"));
    categoryDesc[0x16] = subCatStr
        .arg(QCoreApplication::translate("(Categories)", "Movie"))
        .arg(QCoreApplication::translate("(Categories)", "Romance"));
    categoryDesc[0x17] = subCatStr
        .arg(QCoreApplication::translate("(Categories)","Movie"))
        .arg(QCoreApplication::translate("(Categories)", 
            "Serious/Classical/Religious/Historical Movie/Drama"));
    categoryDesc[0x18] = subCatStr
        .arg(QCoreApplication::translate("(Categories)","Movie"))
        .arg(QCoreApplication::translate("(Categories)", "Adult", 
                                         "Adult Movie"));

    categoryDesc[0x20] = QCoreApplication::translate("(Categories)", "News");
    categoryDesc[0x21] = QCoreApplication::translate("(Categories)",
                                                     "News/weather report");
    categoryDesc[0x22] = QCoreApplication::translate("(Categories)",
                                                     "News magazine");
    categoryDesc[0x23] = QCoreApplication::translate("(Categories)",
                                                     "Documentary");
    categoryDesc[0x24] = QCoreApplication::translate("(Categories)",
                                                     "Intelligent Programs");

    categoryDesc[0x30] = QCoreApplication::translate("(Categories)",
                                                     "Entertainment");
    categoryDesc[0x31] = QCoreApplication::translate("(Categories)",
                                                     "Game Show");
    categoryDesc[0x32] = QCoreApplication::translate("(Categories)",
                                                     "Variety Show");
    categoryDesc[0x33] = QCoreApplication::translate("(Categories)",
                                                     "Talk Show");

    categoryDesc[0x40] = QCoreApplication::translate("(Categories)",
                                                     "Sports");
    categoryDesc[0x41] = QCoreApplication::translate("(Categories)",
                             "Special Events (World Cup, World Series, etc)");
    categoryDesc[0x42] = QCoreApplication::translate("(Categories)",
                                                     "Sports Magazines");
    categoryDesc[0x43] = QCoreApplication::translate("(Categories)",
                                                     "Football (Soccer)");
    categoryDesc[0x44] = QCoreApplication::translate("(Categories)",
                                                     "Tennis/Squash");
    categoryDesc[0x45] = QCoreApplication::translate("(Categories)",
                                                     "Misc. Team Sports"); 
                                                     // not football/soccer
    categoryDesc[0x46] = QCoreApplication::translate("(Categories)",
                                                     "Athletics");
    categoryDesc[0x47] = QCoreApplication::translate("(Categories)",
                                                     "Motor Sport");
    categoryDesc[0x48] = QCoreApplication::translate("(Categories)",
                                                     "Water Sport");
    categoryDesc[0x49] = QCoreApplication::translate("(Categories)",
                                                     "Winter Sports");
    categoryDesc[0x4A] = QCoreApplication::translate("(Categories)",
                                                     "Equestrian");
    categoryDesc[0x4B] = QCoreApplication::translate("(Categories)",
                                                     "Martial Sports");

    categoryDesc[0x50] = QCoreApplication::translate("(Categories)", "Kids");
    categoryDesc[0x51] = QCoreApplication::translate("(Categories)",
                             "Pre-School Children's Programs");
    categoryDesc[0x52] = QCoreApplication::translate("(Categories)",
                             "Entertainment Programs for 6 to 14");
    categoryDesc[0x53] = QCoreApplication::translate("(Categories)",
                             "Entertainment Programs for 10 to 16");
    categoryDesc[0x54] = QCoreApplication::translate("(Categories)",
                             "Informational/Educational");
    categoryDesc[0x55] = QCoreApplication::translate("(Categories)",
                                                     "Cartoons/Puppets");

    categoryDesc[0x60] = QCoreApplication::translate("(Categories)",
                                                     "Music/Ballet/Dance");
    categoryDesc[0x61] = QCoreApplication::translate("(Categories)",
                                                     "Rock/Pop");
    categoryDesc[0x62] = QCoreApplication::translate("(Categories)",
                                                     "Classical Music");
    categoryDesc[0x63] = QCoreApplication::translate("(Categories)",
                                                     "Folk Music");
    categoryDesc[0x64] = QCoreApplication::translate("(Categories)",
                                                     "Jazz");
    categoryDesc[0x65] = QCoreApplication::translate("(Categories)",
                                                     "Musical/Opera");
    categoryDesc[0x66] = QCoreApplication::translate("(Categories)",
                                                     "Ballet");

    categoryDesc[0x70] = QCoreApplication::translate("(Categories)",
                                                     "Arts/Culture");
    categoryDesc[0x71] = QCoreApplication::translate("(Categories)",
                                                     "Performing Arts");
    categoryDesc[0x72] = QCoreApplication::translate("(Categories)",
                                                     "Fine Arts");
    categoryDesc[0x73] = QCoreApplication::translate("(Categories)",
                                                     "Religion");
    categoryDesc[0x74] = QCoreApplication::translate("(Categories)",
                                                     "Popular Culture/Traditional Arts");
    categoryDesc[0x75] = QCoreApplication::translate("(Categories)",
                                                     "Literature");
    categoryDesc[0x76] = QCoreApplication::translate("(Categories)",
                                                     "Film/Cinema");
    categoryDesc[0x77] = QCoreApplication::translate("(Categories)",
                                                     "Experimental Film/Video");
    categoryDesc[0x78] = QCoreApplication::translate("(Categories)",
                                                     "Broadcasting/Press");
    categoryDesc[0x79] = QCoreApplication::translate("(Categories)",
                                                     "New Media");
    categoryDesc[0x7A] = QCoreApplication::translate("(Categories)",
                                                     "Arts/Culture Magazines");
    categoryDesc[0x7B] = QCoreApplication::translate("(Categories)", "Fashion");

    categoryDesc[0x80] = QCoreApplication::translate("(Categories)",
                             "Social/Policical/Economics");
    categoryDesc[0x81] = QCoreApplication::translate("(Categories)",
                             "Magazines/Reports/Documentary");
    categoryDesc[0x82] = QCoreApplication::translate("(Categories)",
                             "Economics/Social Advisory");
    categoryDesc[0x83] = QCoreApplication::translate("(Categories)",
                                                     "Remarkable People");

    categoryDesc[0x90] = QCoreApplication::translate("(Categories)",
                             "Education/Science/Factual");
    categoryDesc[0x91] = QCoreApplication::translate("(Categories)",
                             "Nature/animals/Environment");
    categoryDesc[0x92] = QCoreApplication::translate("(Categories)",
                             "Technology/Natural Sciences");
    categoryDesc[0x93] = QCoreApplication::translate("(Categories)",
                             "Medicine/Physiology/Psychology");
    categoryDesc[0x94] = QCoreApplication::translate("(Categories)",
                             "Foreign Countries/Expeditions");
    categoryDesc[0x95] = QCoreApplication::translate("(Categories)",
                             "Social/Spiritual Sciences");
    categoryDesc[0x96] = QCoreApplication::translate("(Categories)",
                                                     "Further Education");
    categoryDesc[0x97] = QCoreApplication::translate("(Categories)",
                                                     "Languages");

    categoryDesc[0xA0] = QCoreApplication::translate("(Categories)",
                                                     "Leisure/Hobbies");
    categoryDesc[0xA1] = QCoreApplication::translate("(Categories)",
                                                     "Tourism/Travel");
    categoryDesc[0xA2] = QCoreApplication::translate("(Categories)",
                                                     "Handicraft");
    categoryDesc[0xA3] = QCoreApplication::translate("(Categories)",
                                                     "Motoring");
    categoryDesc[0xA4] = QCoreApplication::translate("(Categories)",
                                                     "Fitness & Health");
    categoryDesc[0xA5] = QCoreApplication::translate("(Categories)", "Cooking");
    categoryDesc[0xA6] = QCoreApplication::translate("(Categories)",
                                                     "Advertizement/Shopping");
    categoryDesc[0xA7] = QCoreApplication::translate("(Categories)",
                                                     "Gardening");
    // Special
    categoryDesc[0xB0] = QCoreApplication::translate("(Categories)",
                                                     "Original Language");
    categoryDesc[0xB1] = QCoreApplication::translate("(Categories)",
                                                     "Black & White");
    categoryDesc[0xB2] = QCoreApplication::translate("(Categories)",
                             "\"Unpublished\" Programs");
    categoryDesc[0xB3] = QCoreApplication::translate("(Categories)",
                                                     "Live Broadcast");
    // UK Freeview custom id
    categoryDesc[0xF0] = QCoreApplication::translate("(Categories)",
                                                     "Drama");

    categoryDescExists = true;
}

QString FrequencyListDescriptor::toString() const
{
    QString str = "FrequencyListDescriptor: frequencies: ";

    for (uint i = 0; i < FrequencyCount(); i++)
        str.append(QString(" %1").arg(FrequencyHz(i)));

    return str;
}

QString ServiceDescriptorMapping::toString() const
{
    QString str = "";

    if (IsDTV())
        str.append(" (TV)");
    else if (IsDigitalAudio())
        str.append(" (Radio)");
    else if (IsHDTV())
        str.append(" (HDTV)");
    else if (IsTeletext())
        str.append(" (Teletext)");
    else
        str.append(QString(" (Unknown %1)").arg(ServiceType(),2,16));

    return str;
}

QString TeletextDescriptor::toString(void) const
{
    QString str = QString("Teletext Descriptor: %1 pages")
        .arg(StreamCount());

    for (uint i = 0; i < StreamCount(); i++)
    {
        if (1 != StreamCount())
            str.append("\n ");

        str.append(QString(" type(%1) mag(%2) page(%3) lang(%4)")
                   .arg(TeletextType(i))
                   .arg(TeletextMagazineNum(i), 0, 16)
                   .arg(TeletextPageNum(i), 2, 16, QChar('0'))
                   .arg(LanguageString(i)));
    }

    return str;
}

QString CableDeliverySystemDescriptor::toString() const
{
    QString str = QString("CableDeliverySystemDescriptor: ");

    str.append(QString("Frequency: %1\n").arg(FrequencyHz()));
    str.append(QString("      Mod=%1, SymbR=%2, FECInner=%3, FECOuter=%4")
        .arg(ModulationString())
        .arg(SymbolRateHz())
        .arg(FECInnerString())
        .arg(FECOuterString()));

    return str;
}

QString SatelliteDeliverySystemDescriptor::toString() const
{
    QString str = QString("SatelliteDeliverySystemDescriptor: ");

    str.append(QString("Frequency: %1\n").arg(FrequencyHz()));
    str.append(QString("      Mod=%1, SymbR=%2, FECInner=%3, Orbit=%4, Pol=%5")
        .arg(ModulationString())
        .arg(SymbolRateHz())
        .arg(FECInnerString())
        .arg(OrbitalPositionString())
        .arg(PolarizationString()));

    return str;
}

QString TerrestrialDeliverySystemDescriptor::toString() const
{
    QString str = QString("TerrestrialDeliverySystemDescriptor: ");

    str.append(QString("Frequency: %1\n").arg(FrequencyHz()));
    str.append(QString("      BW=%1k, C=%2, HP=%3, LP=%4, GI=%5, TransMode=%6k")
        .arg(BandwidthString())
        .arg(ConstellationString())
        .arg(CodeRateHPString())
        .arg(CodeRateLPString())
        .arg(GuardIntervalString())
        .arg(TransmissionModeString()));

    return str;
}

QString UKChannelListDescriptor::toString() const
{
    QString ret = "UKChannelListDescriptor sid->chan_num: ";
    for (uint i = 0; i < ChannelCount(); i++)
    {
        ret += QString("%1->%2").arg(ServiceID(i)).arg(ChannelNumber(i));
        ret += (i+1<ChannelCount()) ? ", " : "";
    }
    return ret;
}

QString CAIdentifierDescriptor::toString(void) const
{
    QString ret = QString("CAIdentifierDescriptor ");
    for (uint i = 0; i < CASystemCount(); ++i)
    {
        ret += QString("ca_system_id(0x%1) ")
            .arg(CASystemId(i), 0, 16);
    }
    return ret;
}

QString DataBroadcastDescriptor::toString(void) const
{
    QString ret = QString("DataBroadcastDescriptor: "
                                "data_broadcast_id(%1) "
                                "component_tag(%1) ")
            .arg(DataBroadcastId(), 0, 10)
            .arg(DataComponentTag(), 0, 10);

    ret += QString("selector(0x ");
    for (uint i = 0; i < SelectorLength(); i++)
        ret += QString("%1 ").arg(Selector()[i], 0, 16);
    ret += ") ";
    
    ret += QString("ISO_639_language_code(%1) ")
        .arg(LanguageString());

    ret += QString("text(%1) ") + QString(Text());

    return ret;
}

QString LocalTimeOffsetDescriptor::toString(void) const
{
    QString ret = QString("LocalTimeOffsetDescriptor ");
    uint count = Count();
    for (uint i = 0; i < count; ++i)
    {
        ret += QString("country_code(%1) country_region_id(0x%2) "
                       "local_time_offset_with_polarity(%3) "
                       "time_of_change(TODO)")
            .arg(CountryCodeString(i))
            .arg(CountryRegionId(i), 0, 16)
            .arg(LocalTimeOffsetWithPolarity(i));
        // TODO add time of change
    }
    return ret;
}

QString NVODReferenceDescriptor::toString(void) const
{
    QString ret = QString("NVODReferenceDescriptor ");
    for (uint i = 0; i < Count(); ++i)
    {
        ret += QString("transport_stream_id(0x%1) original_network_id(0x%2) "
                       "service_id(0x%3) ")
            .arg(TransportStreamId(i), 0, 16)
            .arg(OriginalNetworkId(i), 0, 16)
            .arg(ServiceId(i), 0, 16);
    }
    return ret;
}

QString PartialTransportStreamDescriptor::toString(void) const
{
    return QString("PartialTransportStreamDescriptor peak_rate(%1) "
                   "min_overall_smooth_rate(%2) max_overall_smooth_buf(3)")
        .arg(PeakRate()).arg(SmoothRate()).arg(SmoothBuf());
}

QString AC3Descriptor::toString(void) const
{
    QString ret = QString("AC3DescriptorDescriptor ");
    if (HasComponentType())
        ret += QString("component_type(%1) ")
        .arg(ComponentType(), 0, 10);
    if (HasBSID())
        ret += QString("bsid(0x%1) ").arg(BSID(),0,16);
    if (HasMainID())
        ret += QString("mainid(0x%1) ").arg(MainID(),0,16);
    if (HasASVC())
        ret += QString("asvc(%1) ").arg(ASVC());
    return ret;
}


