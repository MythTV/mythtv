
#include "upnphelpers.h"

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

namespace UPnPDateTime
{

QString DurationFormat(std::chrono::milliseconds msec)
{
    // Appendix D. Date&Time Syntax - UPnP ContentDirectory Service 2008, 2013
    // duration ::= 'P' [n 'D'] time
    // time ::= HH ':' MM ':' SS
    //
    // e.g.
    //"<upnp:scheduledDuration>P1D02:30:23</upnp:scheduledDuration>"
    // 1 day 2 Hours 30 Minutes 23 Seconds

    QString durationStr = "P%1%2";
    QString dayStr;
    if ( msec > 24h )
    {
        dayStr = QString("D%1").arg((msec % 24h).count());
    }
    QString timeStr = UPnPDateTime::TimeFormat(msec);

    return durationStr.arg(dayStr, timeStr);
}

QString TimeFormat(const QTime time)
{
    QString timeStr = time.toString("HH:mm:ss");
    return timeStr;
}

    QString TimeFormat(std::chrono::milliseconds msec)
{
    QTime time = QTime::fromMSecsSinceStartOfDay(msec.count());
    return time.toString("HH:mm:ss");
}

QString DateTimeFormat(const QDateTime &dateTime)
{
    QString dateTimeStr = dateTime.toString(Qt::ISODate);
    return dateTimeStr;
}

QString NamedDayFormat(const QDateTime &dateTime)
{
    return UPnPDateTime::NamedDayFormat(dateTime.date());
}

QString NamedDayFormat(const QDate date)
{
    // Note QDate::toString() and QDate::shortDayName() return localized strings
    // which are of no use to us.
    //
    // Valid values per the spec are MON, TUE, WED, THU etc
    switch (date.dayOfWeek())
    {
        case 1:
            return "MON";
        case 2:
            return "TUE";
        case 3:
            return "WED";
        case 4:
            return "THU";
        case 5:
            return "FRI";
        case 6:
            return "SAT";
        case 7:
            return "SUN";
        default:
            return "";
    }
}

QString resDurationFormat(std::chrono::milliseconds msec)
{
    // Appendix B.2 Resource Encoding Characteristics Properties
    //          B.2.1.4 res@duration

    // H[H]:MM:SS[.FS]
    // H = Hours (1-2 digits),
    // M = Minutes (2 digits, 0 prefix)
    // S = Seconds (2 digits, 0 prefix)
    // FS = Fractional Seconds (milliseconds)
    QTime time = QTime::fromMSecsSinceStartOfDay(msec.count());
    return time.toString("H:mm:ss.zzz");
}

};

namespace DLNA
{

QString DLNAProfileName(const QString &mimeType, const QSize resolution,
                        const double /*videoFrameRate*/, const QString &container,
                        const QString &vidCodec, const QString &audioCodec)
{
    QString sProfileName;
    bool isHD = (resolution.height() >= 720) || (resolution.width() > 720);

    //QString sBroadcastStandard = "";
    // HACK This is just temporary until we start storing more video
    // information in the database for each file and can determine this
    // stuff 'properly'
    QString sCountryCode = gCoreContext->GetLocale()->GetCountryCode();
    bool isNorthAmerica = (sCountryCode == "us" || sCountryCode == "ca" ||
                            sCountryCode == "mx"); // North America (NTSC/ATSC)

    if (container == "MPEG2-PS")
    {
        if (isHD && audioCodec == "DTS")
            sProfileName = "MPEG_PS_HD_DTS";
        else if (isHD)
        {
            // Fallthough, no DLNA profiles
        }
        else if (audioCodec == "DTS")
        {
            sProfileName = "MPEG_PS_SD_DTS";
        }
        else
        {
            if (isNorthAmerica)
                sProfileName = "MPEG_PS_NTSC";
            else
                sProfileName = "MPEG_PS_PAL";
        }
    }
    else if (container == "MPEG2-TS")
    {
        if (isNorthAmerica)
        {
            if (vidCodec == "H264")
                sProfileName = "AVC_TS_NA_ISO";
            else if (isHD) // && videoCodec == "MPEG2VIDEO"
                sProfileName = "MPEG_TS_HD_NA_ISO";
            else // if (videoCodec == "MPEG2VIDEO")
                sProfileName = "MPEG_TS_SD_NA_ISO";
        }
        else // Europe standard (DVB)
        {
            if (vidCodec == "H264" || isHD) // All HD is AVC with DVB
                sProfileName = "AVC_TS_EU_ISO";
            else // if (videoCodec == "MPEG2VIDEO")
                sProfileName = "MPEG_TS_SD_EU_ISO";
        }
    }
    else if (mimeType == "video/x-matroska" || container == "MATROSKA")
    {
        // TODO: We need to know the video and audio codecs before we can serve
        // up the correct profile.
        //
        // sAdditionalInfoList << "DLNA.ORG_PN=AVC_MKV_SOMETHING";
        if (vidCodec == "H264")
        {
            // TODO We need to know the H264 profile used, for now we go with High Profile
            //      as this covers all - devices supporting High Profile also support
            //      Medium Profile
            if (audioCodec == "AAC") // No HEAAC detection atm, it's a sub-profile of AAC
            {
                sProfileName = "AVC_MKV_HP_HD_AAC_MULT5";
            }
            else if (audioCodec == "AC3")
            {
                sProfileName = "AVC_MKV_HP_HD_AC3";
            }
            else if (audioCodec == "E-AC3") // Up to 35Mbps
            {
                sProfileName = "AVC_MKV_HP_HD_EAC3";
            }
            else if (audioCodec == "MP3")
            {
                sProfileName = "AVC_MKV_HP_HD_MPEG1_L3";
            }
            else if (audioCodec == "DTS") // Up to 55Mbps // No DTSE/DTSL detection atm, sub-profiles of DTS
            {
                sProfileName = "AVC_MKV_HP_HD_DTS";
            }
            else if (audioCodec == "MLP") // Up to 45Mbps
            {
                sProfileName = "AVC_MKV_HP_HD_MLP";
            }
        }
    }
    else if (mimeType == "audio/mpeg")
    {
        sProfileName = "MP3X";
    }
    else if (mimeType == "audio/x-ms-wma")
    {
        sProfileName = "WMAFULL";
    }
    else if (mimeType == "audio/vnd.dolby.dd-raw")
    {
        sProfileName = "AC3";
    }
    else if (mimeType == "audio/mp4")
    {
        sProfileName = "AAC_ISO_320";
    }
    else if (mimeType == "image/jpeg")
    {
        if (resolution.width() <= 160 && resolution.height() <= 160)
            sProfileName = "JPEG_TN";
        else if (resolution.width() <= 640 && resolution.height() <= 480)
            sProfileName = "JPEG_SM";
        else if (resolution.width() <= 1024 && resolution.height() <= 768)
            sProfileName = "JPEG_MED";
        else if (resolution.width() <= 4096 && resolution.height() <= 4096)
            sProfileName = "JPEG_LRG";
    }
    else if (mimeType == "image/png")
    {
        if (resolution.width() <= 160 && resolution.height() <= 160)
            sProfileName = "PNG_TN";
        else if (resolution.width() <= 4096 && resolution.height() <= 4096)
            sProfileName = "PNG_LRG";
    }
    else if (mimeType == "image/gif")
    {
        if (resolution.width() <= 1600 && resolution.height() <= 1200)
            sProfileName = "GIF_LRG";
    }
    else
    {
        // Not a DLNA supported profile?
    }

    return sProfileName;
}

QString DLNAFourthField(UPNPProtocol::TransferProtocol protocol,
                        const QString &mimeType, const QSize resolution,
                        double videoFrameRate, const QString &container,
                        const QString &videoCodec, const QString &audioCodec,
                        bool isTranscoded)
{

    QStringList sAdditionalInfoList;
    //
    //  4th_field = pn-param [op-param] [ps-param] [ci-param] [flags-param] [ *(other-param)]
    //

    //
    // PN-Param (Profile Name)
    //
    QString sProfileName = DLNAProfileName(mimeType, resolution, videoFrameRate,
                                           container, videoCodec, audioCodec);
    if (!sProfileName.isEmpty())
        sAdditionalInfoList << QString("DLNA.ORG_PN=%1").arg(sProfileName);

    //
    // OP-Param (Operation Parameters)
    //
    sAdditionalInfoList << DLNA::OpParamString(protocol);

    //
    // PS-Param (Play Speed)
    //

        // Not presently used by MythTV

    //
    // CI-Param  - Optional, may be omitted if value is zero (0)
    //
    if (isTranscoded)
        sAdditionalInfoList << DLNA::ConversionIndicatorString(isTranscoded);

    //
    // Flags-Param
    //
    if (mimeType.startsWith("audio") || mimeType.startsWith("video"))
    {
        sAdditionalInfoList << DLNA::FlagsString(DLNA::ktm_s |
                                                 DLNA::ktm_b |
                                                 DLNA::kv1_5_flag);
    }
    else
    {
        sAdditionalInfoList << DLNA::FlagsString(DLNA::ktm_i |
                                                 DLNA::ktm_b |
                                                 DLNA::kv1_5_flag);
    }


    //
    // Build the complete string
    //
    QString sAdditionalInfo = "*";
    // If we have DLNA additional info and we have the mandatory PN param
    // then add it to the string. If the PN is missing then we must ignore the
    // rest
    // 7.4.1.3.13.8 - "b) The pn-param (DLNA.ORG_PN) is the only required
    //                    parameter for DLNA media format profiles."
    //
    // 7.4.1.3.17.1 -  4th_field = pn-param [op-param] [ps-param] [ci-param] [flags-param] [ *(other-param)]
    //              - "b) This syntax prohibits the use of the "*" value for
    //                    content that conforms to a DLNA media format profile.
    //                    Content that does not conform to a DLNA media format
    //                    profile can use the "*" value in the 4th field.

    if (!sAdditionalInfoList.isEmpty() &&
        sAdditionalInfoList.first().startsWith("DLNA.ORG_PN"))
        sAdditionalInfo = sAdditionalInfoList.join(";");

    return sAdditionalInfo;
}


// NOTE The order of the DLNA args is mandatory - 7.4.1.3.17 MM protocolInfo values: 4th field
QString ProtocolInfoString(UPNPProtocol::TransferProtocol protocol,
                           const QString &mimeType, const QSize resolution,
                           double videoFrameRate, const QString &container,
                           const QString &videoCodec, const QString &audioCodec,
                           bool isTranscoded)
{
    QStringList protocolInfoFields;

    //
    //  1st Field = protocol
    //

    if (protocol == UPNPProtocol::kHTTP)
        protocolInfoFields << "http-get";
    else if (protocol == UPNPProtocol::kRTP)
        protocolInfoFields << "rtsp-rtp-udp";

    //
    //  2nd Field =
    //

    // Not applicable to us, return wildcard
    protocolInfoFields << "*";

    //
    //  3rd Field = mime type
    //
    protocolInfoFields << mimeType;

    //
    //  4th Field = Additional Implementation Information (Used by DLNA)
    //
    protocolInfoFields << DLNAFourthField(protocol, mimeType, resolution,
                                          videoFrameRate, container,
                                          videoCodec, audioCodec,
                                          isTranscoded);

    if (protocolInfoFields.size() != 4)
        LOG(VB_GENERAL, LOG_CRIT, "DLNA::ProtocolInfoString() : Invalid number of fields in string");

    QString str = protocolInfoFields.join(":");

    if (str.length() > 256)
    {
        LOG(VB_GENERAL, LOG_WARNING, "DLNA::ProtocolInfoString() : ProtocolInfo string exceeds "
                                     "256 byte limit for compatibility with older UPnP devices. "
                                     "Consider omitting optional DLNA information such as ci-param");
    }
    return str;
}


QString FlagsString(uint32_t flags)
{
    QString str;

    if (flags == 0)
        return str;

    // DLNA::kv1_5_flag must be set
    if ((flags & DLNA::ktm_s) && (flags & DLNA::ktm_i))
    {
        LOG(VB_GENERAL, LOG_ERR, "Programmer Error: 'Streaming' and 'Interactive' mode flags are mutally exclusive");
        flags &= ~(DLNA::ktm_s | DLNA::ktm_i);
    }

    // DLNA::kv1_5_flag must be set otherwise other flags are ignored
    if (flags & ~DLNA::kv1_5_flag)
        flags |= DLNA::kv1_5_flag;

    return QString("DLNA.ORG_FLAGS=%1") // 8 HEX Digits, following by 24 zeros
        .arg(flags,8,10,QChar('0')) + "000000000000000000000000";
}

QString OpParamString(UPNPProtocol::TransferProtocol protocol)
{
    QString str = "DLNA.ORG_OP=%1%2";

    if (protocol == UPNPProtocol::kHTTP)
    {
        // Section 7.4.1.3.20
        str = str.arg("0",  // A-val - DLNA Time Seek header support (not currently supported)
                      "1"); // B-val - HTTP Range support
    }
    else if (protocol == UPNPProtocol::kRTP)
    {
        // Section 7.4.1.3.21
        str = str.arg("0",  // A-val - Range Header Support
                      "0"); // B-val - Must always zero
    }

    return str;
}

QString ConversionIndicatorString(bool wasConverted)
{
    QString str = "DLNA.ORG_CI=%1";

    return str.arg(wasConverted ? "1" : "0");
}

};
