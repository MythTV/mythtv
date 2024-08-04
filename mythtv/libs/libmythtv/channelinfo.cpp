// -*- Mode: c++ -*-

// C++ headers
#include <utility>

// Qt headers
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QUrl>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/remotefile.h"

#include "channelgroup.h"
#include "channelinfo.h"
#include "mpeg/mpegstreamdata.h" // for CryptStatus
#include "sourceutil.h"

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ChannelInfo::ChannelInfo(const ChannelInfo &other) :
    // Channel table
    m_chanId             (other.m_chanId),
    m_chanNum            (other.m_chanNum),
    m_freqId             (other.m_freqId),
    m_sourceId           (other.m_sourceId),
    m_callSign           (other.m_callSign),
    m_name               (other.m_name),
    m_icon               (other.m_icon),
    m_fineTune           (other.m_fineTune),
    m_videoFilters       (other.m_videoFilters),
    m_xmltvId            (other.m_xmltvId),
    m_recPriority        (other.m_recPriority),
    m_contrast           (other.m_contrast),
    m_brightness         (other.m_brightness),
    m_colour             (other.m_colour),
    m_hue                (other.m_hue),
    m_tvFormat           (other.m_tvFormat),
    m_visible            (other.m_visible),
    m_outputFilters      (other.m_outputFilters),
    m_useOnAirGuide      (other.m_useOnAirGuide),
    m_mplexId            ((other.m_mplexId == 32767) ? 0 : other.m_mplexId),
    m_serviceId          (other.m_serviceId),
    m_serviceType        (other.m_serviceType),
    m_atscMajorChan      (other.m_atscMajorChan),
    m_atscMinorChan      (other.m_atscMinorChan),
    m_lastRecord         (other.m_lastRecord),
    m_defaultAuthority   (other.m_defaultAuthority),
    m_commMethod         (other.m_commMethod),
    m_tmOffset           (other.m_tmOffset),
    m_iptvId             (other.m_iptvId),

    // Not in channel table
    m_oldXmltvId         (other.m_oldXmltvId),
    m_sourceName         (other.m_sourceName),
    m_groupIdList        (other.m_groupIdList),
    m_inputIdList        (other.m_inputIdList)
{
}

ChannelInfo::ChannelInfo(
    QString _channum, QString _callsign,
    uint _chanid, uint _major_chan, uint _minor_chan,
    uint _mplexid, ChannelVisibleType _visible,
    QString _name, QString _icon,
    uint _sourceid) :
    m_chanId(_chanid),
    m_chanNum(std::move(_channum)),
    m_sourceId(_sourceid),
    m_callSign(std::move(_callsign)),
    m_name(std::move(_name)),
    m_icon(std::move(_icon)),
    m_visible(_visible),
    m_mplexId((_mplexid == 32767) ? 0 : _mplexid),
    m_atscMajorChan(_major_chan),
    m_atscMinorChan(_minor_chan)
{
}

ChannelInfo &ChannelInfo::operator=(const ChannelInfo &other)
{
    if (this == &other)
        return *this;

    // Channel table
    m_chanId            = other.m_chanId;
    m_chanNum           = other.m_chanNum;
    m_freqId            = other.m_freqId;
    m_sourceId          = other.m_sourceId;
    m_callSign          = other.m_callSign;
    m_name              = other.m_name;
    m_icon              = other.m_icon;
    m_fineTune          = other.m_fineTune;
    m_videoFilters      = other.m_videoFilters;
    m_xmltvId           = other.m_xmltvId;
    m_recPriority       = other.m_recPriority;
    m_contrast          = other.m_contrast;
    m_brightness        = other.m_brightness;
    m_colour            = other.m_colour;
    m_hue               = other.m_hue;
    m_tvFormat          = other.m_tvFormat;
    m_visible           = other.m_visible;
    m_outputFilters     = other.m_outputFilters;
    m_useOnAirGuide     = other.m_useOnAirGuide;
    m_mplexId           = (other.m_mplexId == 32767) ? 0 : other.m_mplexId;
    m_serviceId         = other.m_serviceId;
    m_serviceType       = other.m_serviceType;
    m_atscMajorChan     = other.m_atscMajorChan;
    m_atscMinorChan     = other.m_atscMinorChan;
    m_lastRecord        = other.m_lastRecord;
    m_defaultAuthority  = other.m_defaultAuthority;
    m_commMethod        = other.m_commMethod;
    m_tmOffset          = other.m_tmOffset;
    m_iptvId            = other.m_iptvId;

    // Not in channel table
    m_groupIdList       = other.m_groupIdList;
    m_inputIdList       = other.m_inputIdList;
    m_oldXmltvId        = other.m_oldXmltvId;
    m_sourceName        = other.m_sourceName;

    return *this;
}

bool ChannelInfo::Load(uint lchanid)
{
    if (lchanid == 0 && m_chanId == 0)
        return false;

    if (lchanid == 0)
        lchanid = m_chanId;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT channum, freqid, sourceid, "
                    "callsign, name, icon, finetune, videofilters, xmltvid, "
                    "recpriority, contrast, brightness, colour, hue, tvformat, "
                    "visible, outputfilters, useonairguide, mplexid, "
                    "serviceid, atsc_major_chan, atsc_minor_chan, last_record, "
                    "default_authority, commmethod, tmoffset, iptvid "
                    "FROM channel WHERE chanid = :CHANID ;");

    query.bindValue(":CHANID", lchanid);

    if (!query.exec())
    {
        MythDB::DBError("ChannelInfo::Load()", query);
        return false;
    }

    if (!query.next())
        return false;

    m_chanId            = lchanid;
    m_chanNum           = query.value(0).toString();
    m_freqId            = query.value(1).toString();
    m_sourceId          = query.value(2).toUInt();
    m_callSign          = query.value(3).toString();
    m_name              = query.value(4).toString();
    m_icon              = query.value(5).toString();
    m_fineTune          = query.value(6).toInt();
    m_videoFilters      = query.value(7).toString();
    m_xmltvId           = query.value(8).toString();
    m_recPriority       = query.value(9).toInt();
    m_contrast          = query.value(10).toUInt();
    m_brightness        = query.value(11).toUInt();
    m_colour            = query.value(12).toUInt();
    m_hue               = query.value(13).toUInt();
    m_tvFormat          = query.value(14).toString();
    m_visible           =
        static_cast<ChannelVisibleType>(query.value(15).toInt());
    m_outputFilters     = query.value(16).toString();
    m_useOnAirGuide     = query.value(17).toBool();
    m_mplexId           = query.value(18).toUInt();
    m_serviceId         = query.value(19).toUInt();
    m_atscMajorChan     = query.value(20).toUInt();
    m_atscMinorChan     = query.value(21).toUInt();
    m_lastRecord        = query.value(22).toDateTime();
    m_defaultAuthority  = query.value(23).toString();
    m_commMethod        = query.value(24).toUInt();
    m_tmOffset          = query.value(25).toUInt();
    m_iptvId            = query.value(26).toUInt();

    return true;
}

QString ChannelInfo::GetFormatted(ChannelFormat format) const
{
    QString tmp;

    if (format & kChannelLong)
        tmp = gCoreContext->GetSetting("LongChannelFormat", "<num> <name>");
    else // kChannelShort
        tmp = gCoreContext->GetSetting("ChannelFormat", "<num> <sign>");


    if (tmp.isEmpty())
        return {};

    tmp.replace("<num>",  m_chanNum);
    tmp.replace("<sign>", m_callSign);
    tmp.replace("<name>", m_name);

    return tmp;
}

QString ChannelInfo::GetSourceName()
{
    if (m_sourceId > 0 && m_sourceName.isNull())
        m_sourceName = SourceUtil::GetSourceName(m_sourceId);

    return m_sourceName;
}

void ChannelInfo::ToMap(InfoMap& infoMap)
{
    infoMap["callsign"] = m_callSign;
    infoMap["channeliconpath"] = m_icon;
    //infoMap["chanstr"] = chanstr;
    infoMap["channelname"] = m_name;
    infoMap["channelid"] = QString().setNum(m_chanId);
    infoMap["channelsourcename"] = GetSourceName();
    infoMap["channelrecpriority"] = QString().setNum(m_recPriority);

    infoMap["channelnumber"] = m_chanNum;

    infoMap["majorchan"] = QString().setNum(m_atscMajorChan);
    infoMap["minorchan"] = QString().setNum(m_atscMinorChan);
    infoMap["mplexid"] = QString().setNum(m_mplexId);
    infoMap["channelvisible"] = m_visible ? QObject::tr("Yes") : QObject::tr("No");

    if (!GetGroupIds().isEmpty())
        infoMap["channelgroupname"] = ChannelGroup::GetChannelGroupName(GetGroupIds().constFirst());
}

void ChannelInfo::LoadInputIds()
{
    if (m_chanId && m_inputIdList.isEmpty())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT capturecard.cardid FROM channel "
            "JOIN capturecard ON capturecard.sourceid = channel.sourceid "
            "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", m_chanId);

        if (!query.exec())
            MythDB::DBError("ChannelInfo::LoadInputIds()", query);
        else
        {
            while(query.next())
            {
                AddInputId(query.value(0).toUInt());
            }
        }
    }
}

void ChannelInfo::LoadGroupIds()
{
    if (m_chanId && m_groupIdList.isEmpty())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT grpid FROM channelgroup "
                      "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", m_chanId);

        if (!query.exec())
            MythDB::DBError("ChannelInfo::LoadGroupIds()", query);
        else if (query.size() == 0)
        {
            // HACK Avoid re-running this query each time for channels
            //      which don't belong to any group
            AddGroupId(0);
        }
        else
        {
            while(query.next())
            {
                AddGroupId(query.value(0).toUInt());
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool ChannelInsertInfo::SaveScan(uint scanid, uint transportid) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "INSERT INTO channelscan_channel "
        " (  scanid,             transportid,                            "
        "    mplex_id,           source_id,          channel_id,         "
        "    callsign,           service_name,       chan_num,           "
        "    service_id,         atsc_major_channel, atsc_minor_channel, "
        "    use_on_air_guide,   hidden,             hidden_in_guide,    "
        "    freqid,             icon,               tvformat,           "
        "    xmltvid,            pat_tsid,           vct_tsid,           "
        "    vct_chan_tsid,      sdt_tsid,           orig_netid,         "
        "    netid,              si_standard,        in_channels_conf,   "
        "    in_pat,             in_pmt,             in_vct,             "
        "    in_nit,             in_sdt,             is_encrypted,       "
        "    is_data_service,    is_audio_service,   is_opencable,       "
        "    could_be_opencable, decryption_status,  default_authority,  "
        "    service_type,       logical_channel,    simulcast_channel   "
        " )  "
        "VALUES "
        " ( :SCANID,            :TRANSPORTID,                            "
        "   :MPLEX_ID,          :SOURCE_ID,         :CHANNEL_ID,         "
        "   :CALLSIGN,          :SERVICE_NAME,      :CHAN_NUM,           "
        "   :SERVICE_ID,        :ATSC_MAJOR_CHANNEL,:ATSC_MINOR_CHANNEL, "
        "   :USE_ON_AIR_GUIDE,  :HIDDEN,            :HIDDEN_IN_GUIDE,    "
        "   :FREQID,            :ICON,              :TVFORMAT,           "
        "   :XMLTVID,           :PAT_TSID,          :VCT_TSID,           "
        "   :VCT_CHAN_TSID,     :SDT_TSID,          :ORIG_NETID,         "
        "   :NETID,             :SI_STANDARD,       :IN_CHANNELS_CONF,   "
        "   :IN_PAT,            :IN_PMT,            :IN_VCT,             "
        "   :IN_NIT,            :IN_SDT,            :IS_ENCRYPTED,       "
        "   :IS_DATA_SERVICE,   :IS_AUDIO_SERVICE,  :IS_OPENCABLE,       "
        "   :COULD_BE_OPENCABLE,:DECRYPTION_STATUS, :DEFAULT_AUTHORITY,  "
        "   :SERVICE_TYPE,      :LOGICAL_CHANNEL,   :SIMULCAST_CHANNEL   "
        " );");

    query.bindValue(":SCANID", scanid);
    query.bindValue(":TRANSPORTID", transportid);
    query.bindValue(":MPLEX_ID", m_dbMplexId);
    query.bindValue(":SOURCE_ID", m_sourceId);
    query.bindValue(":CHANNEL_ID", m_channelId);
    query.bindValueNoNull(":CALLSIGN", m_callSign);
    query.bindValueNoNull(":SERVICE_NAME", m_serviceName);
    query.bindValueNoNull(":CHAN_NUM", m_chanNum);
    query.bindValue(":SERVICE_ID", m_serviceId);
    query.bindValue(":ATSC_MAJOR_CHANNEL", m_atscMajorChannel);
    query.bindValue(":ATSC_MINOR_CHANNEL", m_atscMinorChannel);
    query.bindValue(":USE_ON_AIR_GUIDE", m_useOnAirGuide);
    query.bindValue(":HIDDEN", m_hidden);
    query.bindValue(":HIDDEN_IN_GUIDE", m_hiddenInGuide);
    query.bindValueNoNull(":FREQID", m_freqId);
    query.bindValueNoNull(":ICON", m_icon);
    query.bindValueNoNull(":TVFORMAT", m_format);
    query.bindValueNoNull(":XMLTVID", m_xmltvId);
    query.bindValue(":PAT_TSID", m_patTsId);
    query.bindValue(":VCT_TSID", m_vctTsId);
    query.bindValue(":VCT_CHAN_TSID", m_vctChanTsId);
    query.bindValue(":SDT_TSID", m_sdtTsId);
    query.bindValue(":ORIG_NETID",  m_origNetId);
    query.bindValue(":NETID", m_netId);
    query.bindValueNoNull(":SI_STANDARD", m_siStandard);
    query.bindValue(":IN_CHANNELS_CONF", m_inChannelsConf);
    query.bindValue(":IN_PAT", m_inPat);
    query.bindValue(":IN_PMT", m_inPmt);
    query.bindValue(":IN_VCT", m_inVct);
    query.bindValue(":IN_NIT", m_inNit);
    query.bindValue(":IN_SDT", m_inSdt);
    query.bindValue(":IS_ENCRYPTED", m_isEncrypted);
    query.bindValue(":IS_DATA_SERVICE", m_isDataService);
    query.bindValue(":IS_AUDIO_SERVICE", m_isAudioService);
    query.bindValue(":IS_OPENCABLE", m_isOpencable);
    query.bindValue(":COULD_BE_OPENCABLE", m_couldBeOpencable);
    query.bindValue(":DECRYPTION_STATUS", m_decryptionStatus);
    query.bindValueNoNull(":DEFAULT_AUTHORITY", m_defaultAuthority);
    query.bindValue(":SERVICE_TYPE", m_serviceType);
    query.bindValue(":LOGICAL_CHANNEL", m_logicalChannel);
    query.bindValue(":SIMULCAST_CHANNEL", m_simulcastChannel);

    if (!query.exec())
    {
        MythDB::DBError("ChannelInsertInfo::SaveScan()", query);
        return false;
    }

    return true;
}

void ChannelInsertInfo::ImportExtraInfo(const ChannelInsertInfo &other)
{
    if (other.m_dbMplexId && !m_dbMplexId)
        m_dbMplexId          = other.m_dbMplexId;
    if (other.m_sourceId && !m_sourceId)
        m_sourceId           = other.m_sourceId;
    if (other.m_channelId && !m_channelId)
        m_channelId          = other.m_channelId;
    if (!other.m_callSign.isEmpty() && m_callSign.isEmpty())
        m_callSign           = other.m_callSign;
    if (!other.m_serviceName.isEmpty() && m_serviceName.isEmpty())
        m_serviceName        = other.m_serviceName;
    if (!other.m_chanNum.isEmpty() &&
        ((m_chanNum.isEmpty() || m_chanNum == "0")))
        m_chanNum            = other.m_chanNum;
    if (other.m_serviceId && !m_serviceId)
        m_serviceId          = other.m_serviceId;
    if (other.m_atscMajorChannel && !m_atscMajorChannel)
        m_atscMajorChannel   = other.m_atscMajorChannel;
    if (other.m_atscMinorChannel && !m_atscMinorChannel)
        m_atscMinorChannel   = other.m_atscMinorChannel;
    //m_useOnAirGuide        = other.m_useOnAirGuide;
    //m_hidden               = other.m_hidden;
    //m_hiddenInGuide        = other.m_hiddenInGuide;
    if (!other.m_freqId.isEmpty() && m_freqId.isEmpty())
        m_freqId             = other.m_freqId;
    if (!other.m_icon.isEmpty() && m_icon.isEmpty())
        m_icon               = other.m_icon;
    if (!other.m_format.isEmpty() && m_format.isEmpty())
        m_format             = other.m_format;
    if (!other.m_xmltvId.isEmpty() && m_xmltvId.isEmpty())
        m_xmltvId            = other.m_xmltvId;
    if (!other.m_defaultAuthority.isEmpty() && m_defaultAuthority.isEmpty())
        m_defaultAuthority   = other.m_defaultAuthority;
    // non-DB info
    if (other.m_patTsId && !m_patTsId)
        m_patTsId            = other.m_patTsId;
    if (other.m_vctTsId && !m_vctTsId)
        m_vctTsId            = other.m_vctTsId;
    if (other.m_vctChanTsId && !m_vctChanTsId)
        m_vctChanTsId        = other.m_vctChanTsId;
    if (other.m_sdtTsId && !m_sdtTsId)
        m_sdtTsId            = other.m_sdtTsId;
    if (other.m_origNetId && !m_origNetId)
        m_origNetId          = other.m_origNetId;
    if (other.m_netId && !m_netId)
        m_netId              = other.m_netId;
    if (!other.m_siStandard.isEmpty() &&
        (m_siStandard.isEmpty() || ("mpeg" == m_siStandard)))
        m_siStandard         = other.m_siStandard;
    if (other.m_inChannelsConf && !m_inChannelsConf)
        m_inChannelsConf     = other.m_inChannelsConf;
    if (other.m_inPat && !m_inPat)
        m_inPat              = other.m_inPat;
    if (other.m_inPmt && !m_inPmt)
        m_inPmt              = other.m_inPmt;
    if (other.m_inVct && !m_inVct)
        m_inVct              = other.m_inVct;
    if (other.m_inNit && !m_inNit)
        m_inNit              = other.m_inNit;
    if (other.m_inSdt && !m_inSdt)
        m_inSdt              = other.m_inSdt;
    if (other.m_inPat && !m_inPat)
        m_isEncrypted        = other.m_isEncrypted;
    if (other.m_isDataService && !m_isDataService)
        m_isDataService      = other.m_isDataService;
    if (other.m_isAudioService && !m_isAudioService)
        m_isAudioService     = other.m_isAudioService;
    if (other.m_isOpencable && !m_isOpencable)
        m_isOpencable        = other.m_isOpencable;
    if (other.m_couldBeOpencable && !m_couldBeOpencable)
        m_couldBeOpencable   = other.m_couldBeOpencable;
    if (kEncUnknown == m_decryptionStatus)
        m_decryptionStatus   = other.m_decryptionStatus;
}

// relaxed  0   compare channels to check for duplicates
//          1   compare channels across transports after rescan
//          2   compare channels in same transport
//
bool ChannelInsertInfo::IsSameChannel(
    const ChannelInsertInfo &other, int relaxed) const
{
    if (m_atscMajorChannel)
    {
        return ((m_atscMajorChannel == other.m_atscMajorChannel) &&
                (m_atscMinorChannel == other.m_atscMinorChannel));
    }

    if ((m_origNetId == other.m_origNetId) &&
        (m_sdtTsId   == other.m_sdtTsId)   &&
        (m_serviceId == other.m_serviceId))
    {
        return true;
    }

    if (!m_origNetId && !other.m_origNetId &&
        (m_patTsId   == other.m_patTsId)   &&
        (m_serviceId == other.m_serviceId))
    {
        return true;
    }

    if (relaxed > 0)
    {
        if ((m_origNetId == other.m_origNetId) &&
            (m_serviceId == other.m_serviceId))
        {
            return true;
        }
    }

    if (relaxed > 1)
    {
        if (("mpeg" == m_siStandard || "mpeg" == other.m_siStandard ||
             "dvb"  == m_siStandard || "dvb"  == other.m_siStandard ||
             m_siStandard.isEmpty() || other.m_siStandard.isEmpty()) &&
            (m_serviceId == other.m_serviceId))
        {
            return true;
        }
    }

    return false;
}

QString toString(ChannelVisibleType type)
{
    switch (type)
    {
        case kChannelAlwaysVisible:
            return QObject::tr("Always Visible");
        case kChannelVisible:
            return QObject::tr("Visible");
        case kChannelNotVisible:
            return QObject::tr("Not Visible");
        case kChannelNeverVisible:
            return QObject::tr("Never Visible");
        default:
            return QObject::tr("Unknown");
    }
}

QString toRawString(ChannelVisibleType type)
{
    switch (type)
    {
        case kChannelAlwaysVisible:
            return {"Always Visible"};
        case kChannelVisible:
            return {"Visible"};
        case kChannelNotVisible:
            return {"Not Visible"};
        case kChannelNeverVisible:
            return {"Never Visible"};
        default:
            return {"Unknown"};
    }
}

ChannelVisibleType channelVisibleTypeFromString(const QString& type)
{
    if (type.toLower() == "always visible" ||
        type.toLower() == "always")
        return kChannelAlwaysVisible;
    if (type.toLower() == "visible" ||
        type.toLower() == "yes")
        return kChannelVisible;
    if (type.toLower() == "not visible" ||
        type.toLower() == "not" ||
        type.toLower() == "no")
        return kChannelNotVisible;
    if (type.toLower() == "never visible" ||
        type.toLower() == "never")
        return kChannelNeverVisible;
    return kChannelVisible;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
