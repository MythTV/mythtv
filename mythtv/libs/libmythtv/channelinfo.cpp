// -*- Mode: c++ -*-

// Qt headers
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QUrl>

// MythTV headers
#include "channelinfo.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "mpegstreamdata.h" // for CryptStatus
#include "remotefile.h"
#include "channelgroup.h"
#include "sourceutil.h"

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

ChannelInfo::ChannelInfo(const ChannelInfo &other)
{
    // Channel table
    m_chanid            = other.m_chanid;
    m_channum           = other.m_channum;
    m_freqid            = other.m_freqid;
    m_sourceid          = other.m_sourceid;
    m_callsign          = other.m_callsign;
    m_name              = other.m_name;
    m_icon              = other.m_icon;
    m_finetune          = other.m_finetune;
    m_videofilters      = other.m_videofilters;
    m_xmltvid           = other.m_xmltvid;
    m_recpriority       = other.m_recpriority;
    m_contrast          = other.m_contrast;
    m_brightness        = other.m_brightness;
    m_colour            = other.m_colour;
    m_hue               = other.m_hue;
    m_tvformat          = other.m_tvformat;
    m_visible           = other.m_visible;
    m_outputfilters     = other.m_outputfilters;
    m_useonairguide     = other.m_useonairguide;
    m_mplexid           = (other.m_mplexid == 32767) ? 0 : other.m_mplexid;
    m_serviceid         = other.m_serviceid;
    m_service_type      = other.m_service_type;
    m_atsc_major_chan   = other.m_atsc_major_chan;
    m_atsc_minor_chan   = other.m_atsc_minor_chan;
    m_last_record       = other.m_last_record;
    m_default_authority = other.m_default_authority;
    m_commmethod        = other.m_commmethod;
    m_tmoffset          = other.m_tmoffset;
    m_iptvid            = other.m_iptvid;

    // Not in channel table
    m_groupIdList       = other.m_groupIdList;
    m_inputIdList       = other.m_inputIdList;
    m_old_xmltvid       = other.m_old_xmltvid;
    m_sourcename        = other.m_sourcename;
}

ChannelInfo::ChannelInfo(
    const QString &_channum, const QString &_callsign,
    uint _chanid, uint _major_chan, uint _minor_chan,
    uint _mplexid, bool _visible,
    const QString &_name, const QString &_icon,
    uint _sourceid)
{
    m_channum = _channum;
    m_callsign = _callsign;
    m_name = _name;
    m_icon = _icon;
    m_chanid = _chanid;
    m_atsc_major_chan = _major_chan;
    m_atsc_minor_chan = _minor_chan;
    m_mplexid = (_mplexid == 32767) ? 0 : _mplexid;
    m_sourceid = _sourceid;
    m_visible = _visible;
}

ChannelInfo &ChannelInfo::operator=(const ChannelInfo &other)
{
    if (this == &other)
        return *this;

    // Channel table
    m_chanid            = other.m_chanid;
    m_channum           = other.m_channum;
    m_freqid            = other.m_freqid;
    m_sourceid          = other.m_sourceid;
    m_callsign          = other.m_callsign;
    m_name              = other.m_name;
    m_icon              = other.m_icon;
    m_finetune          = other.m_finetune;
    m_videofilters      = other.m_videofilters;
    m_xmltvid           = other.m_xmltvid;
    m_recpriority       = other.m_recpriority;
    m_contrast          = other.m_contrast;
    m_brightness        = other.m_brightness;
    m_colour            = other.m_colour;
    m_hue               = other.m_hue;
    m_tvformat          = other.m_tvformat;
    m_visible           = other.m_visible;
    m_outputfilters     = other.m_outputfilters;
    m_useonairguide     = other.m_useonairguide;
    m_mplexid           = (other.m_mplexid == 32767) ? 0 : other.m_mplexid;
    m_serviceid         = other.m_serviceid;
    m_atsc_major_chan   = other.m_atsc_major_chan;
    m_atsc_minor_chan   = other.m_atsc_minor_chan;
    m_last_record       = other.m_last_record;
    m_default_authority = other.m_default_authority;
    m_commmethod        = other.m_commmethod;
    m_tmoffset          = other.m_tmoffset;
    m_iptvid            = other.m_iptvid;

    // Not in channel table
    m_groupIdList       = other.m_groupIdList;
    m_inputIdList       = other.m_inputIdList;
    m_old_xmltvid       = other.m_old_xmltvid;
    m_sourcename        = other.m_sourcename;

    return *this;
}

bool ChannelInfo::Load(uint lchanid)
{
    if (lchanid == 0 && m_chanid == 0)
        return false;

    if (lchanid == 0)
        lchanid = m_chanid;

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

    m_chanid            = lchanid;
    m_channum           = query.value(0).toString();
    m_freqid            = query.value(1).toString();
    m_sourceid          = query.value(2).toUInt();
    m_callsign          = query.value(3).toString();
    m_name              = query.value(4).toString();
    m_icon              = query.value(5).toString();
    m_finetune          = query.value(6).toInt();
    m_videofilters      = query.value(7).toString();
    m_xmltvid           = query.value(8).toString();
    m_recpriority       = query.value(9).toInt();
    m_contrast          = query.value(10).toUInt();
    m_brightness        = query.value(11).toUInt();
    m_colour            = query.value(12).toUInt();
    m_hue               = query.value(13).toUInt();
    m_tvformat          = query.value(14).toString();
    m_visible           = query.value(15).toBool();
    m_outputfilters     = query.value(16).toString();
    m_useonairguide     = query.value(17).toBool();
    m_mplexid           = query.value(18).toUInt();
    m_serviceid         = query.value(19).toUInt();
    m_atsc_major_chan   = query.value(20).toUInt();
    m_atsc_minor_chan   = query.value(21).toUInt();
    m_last_record       = query.value(22).toDateTime();
    m_default_authority = query.value(23).toString();
    m_commmethod        = query.value(24).toUInt();
    m_tmoffset          = query.value(25).toUInt();
    m_iptvid            = query.value(26).toUInt();

    return true;
}

QString ChannelInfo::GetFormatted(const ChannelFormat &format) const
{
    QString tmp;

    if (format & kChannelLong)
        tmp = gCoreContext->GetSetting("LongChannelFormat", "<num> <name>");
    else // kChannelShort
        tmp = gCoreContext->GetSetting("ChannelFormat", "<num> <sign>");


    if (tmp.isEmpty())
        return QString();

    tmp.replace("<num>",  m_channum);
    tmp.replace("<sign>", m_callsign);
    tmp.replace("<name>", m_name);

    return tmp;
}

QString ChannelInfo::GetSourceName()
{
    if (m_sourceid > 0 && m_sourcename.isNull())
        m_sourcename = SourceUtil::GetSourceName(m_sourceid);

    return m_sourcename;
}

void ChannelInfo::ToMap(InfoMap& infoMap)
{
    infoMap["callsign"] = m_callsign;
    infoMap["channeliconpath"] = m_icon;
    //infoMap["chanstr"] = chanstr;
    infoMap["channelname"] = m_name;
    infoMap["channelid"] = QString().setNum(m_chanid);
    infoMap["channelsourcename"] = GetSourceName();
    infoMap["channelrecpriority"] = QString().setNum(m_recpriority);

    infoMap["channelnumber"] = m_channum;

    infoMap["majorchan"] = QString().setNum(m_atsc_major_chan);
    infoMap["minorchan"] = QString().setNum(m_atsc_minor_chan);
    infoMap["mplexid"] = QString().setNum(m_mplexid);
    infoMap["channelvisible"] = m_visible ? QObject::tr("Yes") : QObject::tr("No");

    if (!GetGroupIds().isEmpty())
        infoMap["channelgroupname"] = ChannelGroup::GetChannelGroupName(GetGroupIds().first());
}

void ChannelInfo::LoadInputIds()
{
    if (m_chanid && m_inputIdList.isEmpty())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT capturecard.cardid FROM channel "
            "JOIN capturecard ON capturecard.sourceid = channel.sourceid "
            "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", m_chanid);

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
    if (m_chanid && m_groupIdList.isEmpty())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT grpid FROM channelgroup "
                      "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", m_chanid);

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
        "    could_be_opencable, decryption_status,  default_authority   "
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
        "   :COULD_BE_OPENCABLE,:DECRYPTION_STATUS, :DEFAULT_AUTHORITY   "
        " );");

    query.bindValue(":SCANID", scanid);
    query.bindValue(":TRANSPORTID", transportid);
    query.bindValue(":MPLEX_ID", m_db_mplexid);
    query.bindValue(":SOURCE_ID", m_source_id);
    query.bindValue(":CHANNEL_ID", m_channel_id);
    query.bindValueNoNull(":CALLSIGN", m_callsign);
    query.bindValueNoNull(":SERVICE_NAME", m_service_name);
    query.bindValueNoNull(":CHAN_NUM", m_chan_num);
    query.bindValue(":SERVICE_ID", m_service_id);
    query.bindValue(":ATSC_MAJOR_CHANNEL", m_atsc_major_channel);
    query.bindValue(":ATSC_MINOR_CHANNEL", m_atsc_minor_channel);
    query.bindValue(":USE_ON_AIR_GUIDE", m_use_on_air_guide);
    query.bindValue(":HIDDEN", m_hidden);
    query.bindValue(":HIDDEN_IN_GUIDE", m_hidden_in_guide);
    query.bindValueNoNull(":FREQID", m_freqid);
    query.bindValueNoNull(":ICON", m_icon);
    query.bindValueNoNull(":TVFORMAT", m_format);
    query.bindValueNoNull(":XMLTVID", m_xmltvid);
    query.bindValue(":PAT_TSID", m_pat_tsid);
    query.bindValue(":VCT_TSID", m_vct_tsid);
    query.bindValue(":VCT_CHAN_TSID", m_vct_chan_tsid);
    query.bindValue(":SDT_TSID", m_sdt_tsid);
    query.bindValue(":ORIG_NETID",  m_orig_netid);
    query.bindValue(":NETID", m_netid);
    query.bindValueNoNull(":SI_STANDARD", m_si_standard);
    query.bindValue(":IN_CHANNELS_CONF", m_in_channels_conf);
    query.bindValue(":IN_PAT", m_in_pat);
    query.bindValue(":IN_PMT", m_in_pmt);
    query.bindValue(":IN_VCT", m_in_vct);
    query.bindValue(":IN_NIT", m_in_nit);
    query.bindValue(":IN_SDT", m_in_sdt);
    query.bindValue(":IS_ENCRYPTED", m_is_encrypted);
    query.bindValue(":IS_DATA_SERVICE", m_is_data_service);
    query.bindValue(":IS_AUDIO_SERVICE", m_is_audio_service);
    query.bindValue(":IS_OPENCABLE", m_is_opencable);
    query.bindValue(":COULD_BE_OPENCABLE", m_could_be_opencable);
    query.bindValue(":DECRYPTION_STATUS", m_decryption_status);
    query.bindValueNoNull(":DEFAULT_AUTHORITY", m_default_authority);

    if (!query.exec())
    {
        MythDB::DBError("ChannelInsertInfo::SaveScan()", query);
        return false;
    }

    return true;
}

void ChannelInsertInfo::ImportExtraInfo(const ChannelInsertInfo &other)
{
    if (other.m_db_mplexid && !m_db_mplexid)
        m_db_mplexid         = other.m_db_mplexid;
    if (other.m_source_id && !m_source_id)
        m_source_id          = other.m_source_id;
    if (other.m_channel_id && !m_channel_id)
        m_channel_id         = other.m_channel_id;
    if (!other.m_callsign.isEmpty() && m_callsign.isEmpty())
        m_callsign           = other.m_callsign;
    if (!other.m_service_name.isEmpty() && m_service_name.isEmpty())
        m_service_name       = other.m_service_name;
    if (!other.m_chan_num.isEmpty() &&
        ((m_chan_num.isEmpty() || m_chan_num == "0")))
        m_chan_num           = other.m_chan_num;
    if (other.m_service_id && !m_service_id)
        m_service_id         = other.m_service_id;
    if (other.m_atsc_major_channel && !m_atsc_major_channel)
        m_atsc_major_channel = other.m_atsc_major_channel;
    if (other.m_atsc_minor_channel && !m_atsc_minor_channel)
        m_atsc_minor_channel = other.m_atsc_minor_channel;
    //m_use_on_air_guide   = other.m_use_on_air_guide;
    //m_hidden             = other.m_hidden;
    //m_hidden_in_guide    = other.m_hidden_in_guide;
    if (!other.m_freqid.isEmpty() && m_freqid.isEmpty())
        m_freqid             = other.m_freqid;
    if (!other.m_icon.isEmpty() && m_icon.isEmpty())
        m_icon               = other.m_icon;
    if (!other.m_format.isEmpty() && m_format.isEmpty())
        m_format             = other.m_format;
    if (!other.m_xmltvid.isEmpty() && m_xmltvid.isEmpty())
        m_xmltvid            = other.m_xmltvid;
    if (!other.m_default_authority.isEmpty() && m_default_authority.isEmpty())
        m_default_authority  = other.m_default_authority;
    // non-DB info
    if (other.m_pat_tsid && !m_pat_tsid)
        m_pat_tsid           = other.m_pat_tsid;
    if (other.m_vct_tsid && !m_vct_tsid)
        m_vct_tsid           = other.m_vct_tsid;
    if (other.m_vct_chan_tsid && !m_vct_chan_tsid)
        m_vct_chan_tsid      = other.m_vct_chan_tsid;
    if (other.m_sdt_tsid && !m_sdt_tsid)
        m_sdt_tsid           = other.m_sdt_tsid;
    if (other.m_orig_netid && !m_orig_netid)
        m_orig_netid         = other.m_orig_netid;
    if (other.m_netid && !m_netid)
        m_netid              = other.m_netid;
    if (!other.m_si_standard.isEmpty() &&
        (m_si_standard.isEmpty() || ("mpeg" == m_si_standard)))
        m_si_standard        = other.m_si_standard;
    if (other.m_in_channels_conf && !m_in_channels_conf)
        m_in_channels_conf   = other.m_in_channels_conf;
    if (other.m_in_pat && !m_in_pat)
        m_in_pat             = other.m_in_pat;
    if (other.m_in_pmt && !m_in_pmt)
        m_in_pmt             = other.m_in_pmt;
    if (other.m_in_vct && !m_in_vct)
        m_in_vct             = other.m_in_vct;
    if (other.m_in_nit && !m_in_nit)
        m_in_nit             = other.m_in_nit;
    if (other.m_in_sdt && !m_in_sdt)
        m_in_sdt             = other.m_in_sdt;
    if (other.m_in_pat && !m_in_pat)
        m_is_encrypted       = other.m_is_encrypted;
    if (other.m_is_data_service && !m_is_data_service)
        m_is_data_service    = other.m_is_data_service;
    if (other.m_is_audio_service && !m_is_audio_service)
        m_is_audio_service   = other.m_is_audio_service;
    if (other.m_is_opencable && !m_is_opencable)
        m_is_opencable       = other.m_is_opencable;
    if (other.m_could_be_opencable && !m_could_be_opencable)
        m_could_be_opencable = other.m_could_be_opencable;
    if (kEncUnknown == m_decryption_status)
        m_decryption_status  = other.m_decryption_status;
}

// relaxed  0   compare channels to check for duplicates
//          1   compare channels across transports after rescan
//          2   compare channels in same transport
//
bool ChannelInsertInfo::IsSameChannel(
    const ChannelInsertInfo &other, int relaxed) const
{
    if (m_atsc_major_channel &&
        (m_atsc_major_channel == other.m_atsc_major_channel) &&
        (m_atsc_minor_channel == other.m_atsc_minor_channel))
    {
        return true;
    }

    if ((m_orig_netid == other.m_orig_netid) &&
        (m_sdt_tsid == other.m_sdt_tsid)     &&
        (m_service_id == other.m_service_id))
    {
        return true;
    }

    if (!m_orig_netid && !other.m_orig_netid &&
        (m_pat_tsid == other.m_pat_tsid)     &&
        (m_service_id == other.m_service_id))
    {
        return true;
    }

    if (relaxed > 0)
    {
        if ((m_orig_netid == other.m_orig_netid) &&
            (m_service_id == other.m_service_id))
        {
            return true;
        }
    }

    if (relaxed > 1)
    {
        if (("mpeg" == m_si_standard || "mpeg" == other.m_si_standard ||
             "dvb" == m_si_standard || "dvb" == other.m_si_standard ||
             m_si_standard.isEmpty() || other.m_si_standard.isEmpty()) &&
            (m_service_id == other.m_service_id))
        {
            return true;
        }
    }

    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
