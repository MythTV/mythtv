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

ChannelInfo::ChannelInfo()
{
    Init();
}

ChannelInfo::ChannelInfo(const ChannelInfo &other)
{
    Init();

    // Channel table
    chanid        = other.chanid;
    channum       = other.channum;
    freqid        = other.freqid;
    sourceid      = other.sourceid;
    callsign      = other.callsign;
    name          = other.name;
    icon          = other.icon;
    finetune      = other.finetune;
    videofilters  = other.videofilters;
    xmltvid       = other.xmltvid;
    recpriority   = other.recpriority;
    contrast      = other.contrast;
    brightness    = other.brightness;
    colour        = other.colour;
    hue           = other.hue;
    tvformat      = other.tvformat;
    visible       = other.visible;
    outputfilters = other.outputfilters;
    useonairguide = other.useonairguide;
    mplexid       = (other.mplexid == 32767) ? 0 : other.mplexid;
    serviceid     = other.serviceid;
    atsc_major_chan = other.atsc_major_chan;
    atsc_minor_chan = other.atsc_minor_chan;
    last_record   = other.last_record;
    default_authority = other.default_authority;
    commmethod    = other.commmethod;
    tmoffset      = other.tmoffset;
    iptvid        = other.iptvid;

    // Not in channel table
    m_groupIdList = other.m_groupIdList;
    m_cardIdList  = other.m_cardIdList;
    old_xmltvid   = other.old_xmltvid;
    m_sourcename  = other.m_sourcename;
}

ChannelInfo::ChannelInfo(
    const QString &_channum, const QString &_callsign,
    uint _chanid, uint _major_chan, uint _minor_chan,
    uint _mplexid, bool _visible,
    const QString &_name, const QString &_icon,
    uint _sourceid)
{
    Init();

    channum = _channum;
    callsign = _callsign;
    name = _name;
    icon = _icon;
    chanid = _chanid;
    atsc_major_chan = _major_chan;
    atsc_minor_chan = _minor_chan;
    mplexid = (_mplexid == 32767) ? 0 : _mplexid;
    sourceid = _sourceid;
    visible = _visible;
}

ChannelInfo &ChannelInfo::operator=(const ChannelInfo &other)
{
    if (this == &other)
        return *this;

    // Channel table
    chanid        = other.chanid;
    channum       = other.channum;
    freqid        = other.freqid;
    sourceid      = other.sourceid;
    callsign      = other.callsign;
    name          = other.name;
    icon          = other.icon;
    finetune      = other.finetune;
    videofilters  = other.videofilters;
    xmltvid       = other.xmltvid;
    recpriority   = other.recpriority;
    contrast      = other.contrast;
    brightness    = other.brightness;
    colour        = other.colour;
    hue           = other.hue;
    tvformat      = other.tvformat;
    visible       = other.visible;
    outputfilters = other.outputfilters;
    useonairguide = other.useonairguide;
    mplexid       = (other.mplexid == 32767) ? 0 : other.mplexid;
    serviceid     = other.serviceid;
    atsc_major_chan = other.atsc_major_chan;
    atsc_minor_chan = other.atsc_minor_chan;
    last_record   = other.last_record;
    default_authority = other.default_authority;
    commmethod    = other.commmethod;
    tmoffset      = other.tmoffset;
    iptvid        = other.iptvid;

    // Not in channel table
    m_groupIdList = other.m_groupIdList;
    m_cardIdList  = other.m_cardIdList;
    old_xmltvid   = other.old_xmltvid;
    m_sourcename  = other.m_sourcename;

    return *this;
}

void ChannelInfo::Init()
{
    chanid = 0;
//  channum = QString();
//  freqid = QString(); May be overloaded to a non-frequency identifier
    sourceid = 0;

//  callsign = QString();
//  name = QString();
//  icon = QString();

    finetune = 0;
//  videofilters = QString();
//  xmltvid = QString();
    recpriority = 0;

    contrast = 32768;
    brightness = 32768;
    colour = 32768;
    hue = 32768;

//  tvformat = QString();
    visible = true;
//  outputfilters = QString();
    useonairguide = false;

    mplexid = 0;
    serviceid = 0;
    atsc_major_chan = 0;
    atsc_minor_chan = 0;

    last_record = QDateTime();

//  default_authority = QString();
    commmethod = -1;
    tmoffset = 0;
    iptvid = 0;

    m_cardIdList.clear();
    m_groupIdList.clear();
    m_sourcename.clear();
}

bool ChannelInfo::Load(uint lchanid)
{
    if (lchanid <= 0 && chanid <= 0)
        return false;

    if (lchanid <= 0)
        lchanid = chanid;

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

    chanid        = lchanid;
    channum       = query.value(0).toString();
    freqid        = query.value(1).toString();
    sourceid      = query.value(2).toUInt();
    callsign      = query.value(3).toString();
    name          = query.value(4).toString();
    icon          = query.value(5).toString();
    finetune      = query.value(6).toInt();
    videofilters  = query.value(7).toString();
    xmltvid       = query.value(8).toString();
    recpriority   = query.value(9).toInt();
    contrast      = query.value(10).toUInt();
    brightness    = query.value(11).toUInt();
    colour        = query.value(12).toUInt();
    hue           = query.value(13).toUInt();
    tvformat      = query.value(14).toString();
    visible       = query.value(15).toBool();
    outputfilters = query.value(16).toString();
    useonairguide = query.value(17).toBool();
    mplexid       = query.value(18).toUInt();
    serviceid     = query.value(19).toUInt();
    atsc_major_chan = query.value(20).toUInt();
    atsc_minor_chan = query.value(21).toUInt();
    last_record   = query.value(22).toDateTime();
    default_authority = query.value(23).toString();
    commmethod    = query.value(24).toUInt();
    tmoffset      = query.value(25).toUInt();
    iptvid        = query.value(26).toUInt();

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

    tmp.replace("<num>",  channum);
    tmp.replace("<sign>", callsign);
    tmp.replace("<name>", name);

    return tmp;
}

QString ChannelInfo::GetSourceName()
{
    if (sourceid > 0 && m_sourcename.isNull())
        m_sourcename = SourceUtil::GetSourceName(sourceid);
    
    return m_sourcename;
}

void ChannelInfo::ToMap(InfoMap& infoMap)
{
    infoMap["callsign"] = callsign;
    infoMap["channeliconpath"] = icon;
    //infoMap["chanstr"] = chanstr;
    infoMap["channelname"] = name;
    infoMap["channelid"] = QString().setNum(chanid);
    infoMap["channelsourcename"] = GetSourceName();
    infoMap["channelrecpriority"] = QString().setNum(recpriority);
    
    infoMap["channelnumber"] = channum;
    
    infoMap["majorchan"] = QString().setNum(atsc_major_chan);
    infoMap["minorchan"] = QString().setNum(atsc_minor_chan);
    infoMap["mplexid"] = QString().setNum(mplexid);
    infoMap["channelvisible"] = visible ? QObject::tr("Yes") : QObject::tr("No");

    if (!GetGroupIds().isEmpty())
        infoMap["channelgroupname"] = ChannelGroup::GetChannelGroupName(GetGroupIds().first());
}

void ChannelInfo::LoadCardIds()
{
    if (chanid && m_cardIdList.isEmpty())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT capturecard.cardid FROM channel "
            "JOIN cardinput   ON cardinput.sourceid = channel.sourceid "
            "JOIN capturecard ON cardinput.cardid = capturecard.cardid "
            "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", chanid);
        
        if (!query.exec())
            MythDB::DBError("ChannelInfo::GetCardIds()", query);
        else
        {
            while(query.next())
            {
                AddCardId(query.value(0).toUInt());
            }
        }
    }
}

void ChannelInfo::LoadGroupIds()
{
    if (chanid && m_groupIdList.isEmpty())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT grpid FROM channelgroup "
                      "WHERE chanid = :CHANID");
        query.bindValue(":CHANID", chanid);

        if (!query.exec())
            MythDB::DBError("ChannelInfo::GetCardIds()", query);
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
        "   :IS_DATA_SERVICE,   :IS_AUDIO_SERVICE,  :IS_OPEBCABLE,       "
        "   :COULD_BE_OPENCABLE,:DECRYPTION_STATUS, :DEFAULT_AUTHORITY   "
        " );");

    query.bindValue(":SCANID", scanid);
    query.bindValue(":TRANSPORTID", transportid);
    query.bindValue(":MPLEX_ID", db_mplexid);
    query.bindValue(":SOURCE_ID", source_id);
    query.bindValue(":CHANNEL_ID", channel_id);
    query.bindValue(":CALLSIGN", callsign);
    query.bindValue(":SERVICE_NAME", service_name);
    query.bindValue(":CHAN_NUM", chan_num);
    query.bindValue(":SERVICE_ID", service_id);
    query.bindValue(":ATSC_MAJOR_CHANNEL", atsc_major_channel);
    query.bindValue(":ATSC_MINOR_CHANNEL", atsc_minor_channel);
    query.bindValue(":USE_ON_AIR_GUIDE", use_on_air_guide);
    query.bindValue(":HIDDEN", hidden);
    query.bindValue(":HIDDEN_IN_GUIDE", hidden_in_guide);
    query.bindValue(":FREQID", freqid);
    query.bindValue(":ICON", icon);
    query.bindValue(":TVFORMAT", format);
    query.bindValue(":XMLTVID", xmltvid);
    query.bindValue(":PAT_TSID", pat_tsid);
    query.bindValue(":VCT_TSID", vct_tsid);
    query.bindValue(":VCT_CHAN_TSID", vct_chan_tsid);
    query.bindValue(":SDT_TSID", sdt_tsid);
    query.bindValue(":ORIG_NETID",  orig_netid);
    query.bindValue(":NETID", netid);
    query.bindValue(":SI_STANDARD", si_standard);
    query.bindValue(":IN_CHANNELS_CONF", in_channels_conf);
    query.bindValue(":IN_PAT", in_pat);
    query.bindValue(":IN_PMT", in_pmt);
    query.bindValue(":IN_VCT", in_vct);
    query.bindValue(":IN_NIT", in_nit);
    query.bindValue(":IN_SDT", in_sdt);
    query.bindValue(":IS_ENCRYPTED", is_encrypted);
    query.bindValue(":IS_DATA_SERVICE", is_data_service);
    query.bindValue(":IS_AUDIO_SERVICE", is_audio_service);
    query.bindValue(":IS_OPEBCABLE", is_opencable);
    query.bindValue(":COULD_BE_OPENCABLE", could_be_opencable);
    query.bindValue(":DECRYPTION_STATUS", decryption_status);
    query.bindValue(":DEFAULT_AUTHORITY", default_authority);

    if (!query.exec())
    {
        MythDB::DBError("ChannelInsertInfo SaveScan 1", query);
        return false;
    }

    return true;
}

ChannelInsertInfo::ChannelInsertInfo(
    uint    _db_mplexid,         uint    _source_id,
    uint    _channel_id,         QString _callsign,
    QString _service_name,       QString _chan_num,
    uint    _service_id,
    uint    _atsc_major_channel, uint    _atsc_minor_channel,
    bool    _use_on_air_guide,   bool    _hidden,
    bool    _hidden_in_guide,
    QString _freqid,             QString _icon,
    QString _format,             QString _xmltvid,
    uint    _pat_tsid,           uint    _vct_tsid,
    uint    _vct_chan_tsid,      uint    _sdt_tsid,
    uint    _orig_netid,         uint    _netid,
    QString _si_standard,
    bool    _in_channels_conf,   bool    _in_pat,
    bool    _in_pmt,             bool    _in_vct,
    bool    _in_nit,             bool    _in_sdt,
    bool    _is_encrypted,       bool    _is_data_service,
    bool    _is_audio_service,   bool    _is_opencable,
    bool    _could_be_opencable, int     _decryption_status,
    QString _default_authority) :
    db_mplexid(_db_mplexid),
    source_id(_source_id),
    channel_id(_channel_id),
    callsign(_callsign),
    service_name(_service_name),
    chan_num(_chan_num),
    service_id(_service_id),
    atsc_major_channel(_atsc_major_channel),
    atsc_minor_channel(_atsc_minor_channel),
    use_on_air_guide(_use_on_air_guide),
    hidden(_hidden),
    hidden_in_guide(_hidden_in_guide),
    freqid(_freqid),
    icon(_icon),
    format(_format),
    xmltvid(_xmltvid),
    default_authority(_default_authority),
    pat_tsid(_pat_tsid),
    vct_tsid(_vct_tsid),
    vct_chan_tsid(_vct_chan_tsid),
    sdt_tsid(_sdt_tsid),
    orig_netid(_orig_netid),
    netid(_netid),
    si_standard(_si_standard),
    in_channels_conf(_in_channels_conf),
    in_pat(_in_pat),
    in_pmt(_in_pmt),
    in_vct(_in_vct),
    in_nit(_in_nit),
    in_sdt(_in_sdt),
    is_encrypted(_is_encrypted),
    is_data_service(_is_data_service),
    is_audio_service(_is_audio_service),
    is_opencable(_is_opencable),
    could_be_opencable(_could_be_opencable),
    decryption_status(_decryption_status)
{
    callsign.detach();
    service_name.detach();
    chan_num.detach();
    freqid.detach();
    icon.detach();
    format.detach();
    xmltvid.detach();
    default_authority.detach();
    si_standard.detach();
}

ChannelInsertInfo &ChannelInsertInfo::operator=(
    const ChannelInsertInfo &other)
{
    db_mplexid         = other.db_mplexid;
    source_id          = other.source_id;
    channel_id         = other.channel_id;
    callsign           = other.callsign;     callsign.detach();
    service_name       = other.service_name; service_name.detach();
    chan_num           = other.chan_num;     chan_num.detach();
    service_id         = other.service_id;
    atsc_major_channel = other.atsc_major_channel;
    atsc_minor_channel = other.atsc_minor_channel;
    use_on_air_guide   = other.use_on_air_guide;
    hidden             = other.hidden;
    hidden_in_guide    = other.hidden_in_guide;
    freqid             = other.freqid;      freqid.detach();
    icon               = other.icon;        icon.detach();
    format             = other.format;      format.detach();
    xmltvid            = other.xmltvid;     xmltvid.detach();
    default_authority  = other.default_authority; default_authority.detach();

    // non-DB info
    pat_tsid           = other.pat_tsid;
    vct_tsid           = other.vct_tsid;
    vct_chan_tsid      = other.vct_chan_tsid;
    sdt_tsid           = other.sdt_tsid;
    orig_netid         = other.orig_netid;
    netid              = other.netid;
    si_standard        = other.si_standard; si_standard.detach();
    in_channels_conf   = other.in_channels_conf;
    in_pat             = other.in_pat;
    in_pmt             = other.in_pmt;
    in_vct             = other.in_vct;
    in_nit             = other.in_nit;
    in_sdt             = other.in_sdt;
    is_encrypted       = other.is_encrypted;
    is_data_service    = other.is_data_service;
    is_audio_service   = other.is_audio_service;
    is_opencable       = other.is_opencable;
    could_be_opencable = other.could_be_opencable;
    decryption_status  = other.decryption_status;

    return *this;
}

void ChannelInsertInfo::ImportExtraInfo(const ChannelInsertInfo &other)
{
    if (other.db_mplexid && !db_mplexid)
        db_mplexid         = other.db_mplexid;
    if (other.source_id && !source_id)
        source_id          = other.source_id;
    if (other.channel_id && !channel_id)
        channel_id         = other.channel_id;
    if (!other.callsign.isEmpty() && callsign.isEmpty())
    {
        callsign           = other.callsign;     callsign.detach();
    }
    if (!other.service_name.isEmpty() && service_name.isEmpty())
    {
        service_name       = other.service_name; service_name.detach();
    }
    if (!other.chan_num.isEmpty() &&
        ((chan_num.isEmpty() || chan_num == "0")))
    {
        chan_num           = other.chan_num;     chan_num.detach();
    }
    if (other.service_id && !service_id)
        service_id         = other.service_id;
    if (other.atsc_major_channel && !atsc_major_channel)
        atsc_major_channel = other.atsc_major_channel;
    if (other.atsc_minor_channel && !atsc_minor_channel)
        atsc_minor_channel = other.atsc_minor_channel;
    //use_on_air_guide   = other.use_on_air_guide;
    //hidden             = other.hidden;
    //hidden_in_guide    = other.hidden_in_guide;
    if (!other.freqid.isEmpty() && freqid.isEmpty())
    {
        freqid             = other.freqid;      freqid.detach();
    }
    if (!other.icon.isEmpty() && icon.isEmpty())
    {
        icon               = other.icon;        icon.detach();
    }
    if (!other.format.isEmpty() && format.isEmpty())
    {
        format             = other.format;      format.detach();
    }
    if (!other.xmltvid.isEmpty() && xmltvid.isEmpty())
    {
        xmltvid            = other.xmltvid;     xmltvid.detach();
    }
    if (!other.default_authority.isEmpty() && default_authority.isEmpty())
    {
        default_authority  = other.default_authority; default_authority.detach();
    }
    // non-DB info
    if (other.pat_tsid && !pat_tsid)
        pat_tsid           = other.pat_tsid;
    if (other.vct_tsid && !vct_tsid)
        vct_tsid           = other.vct_tsid;
    if (other.vct_chan_tsid && !vct_chan_tsid)
        vct_chan_tsid      = other.vct_chan_tsid;
    if (other.sdt_tsid && !sdt_tsid)
        sdt_tsid           = other.sdt_tsid;
    if (other.orig_netid && !orig_netid)
        orig_netid         = other.orig_netid;
    if (other.netid && !netid)
        netid              = other.netid;
    if (!other.si_standard.isEmpty() &&
        (si_standard.isEmpty() || ("mpeg" == si_standard)))
    {
        si_standard        = other.si_standard; si_standard.detach();
    }
    if (other.in_channels_conf && !in_channels_conf)
        in_channels_conf   = other.in_channels_conf;
    if (other.in_pat && !in_pat)
        in_pat             = other.in_pat;
    if (other.in_pmt && !in_pmt)
        in_pmt             = other.in_pmt;
    if (other.in_vct && !in_vct)
        in_vct             = other.in_vct;
    if (other.in_nit && !in_nit)
        in_nit             = other.in_nit;
    if (other.in_sdt && !in_sdt)
        in_sdt             = other.in_sdt;
    if (other.in_pat && !in_pat)
        is_encrypted       = other.is_encrypted;
    if (other.is_data_service && !is_data_service)
        is_data_service    = other.is_data_service;
    if (other.is_audio_service && !is_audio_service)
        is_audio_service   = other.is_audio_service;
    if (other.is_opencable && !is_opencable)
        is_opencable       = other.is_opencable;
    if (other.could_be_opencable && !could_be_opencable)
        could_be_opencable = other.could_be_opencable;
    if (kEncUnknown == decryption_status)
        decryption_status  = other.decryption_status;
}

bool ChannelInsertInfo::IsSameChannel(
    const ChannelInsertInfo &other, bool relaxed) const
{
    if (atsc_major_channel &&
        (atsc_major_channel == other.atsc_major_channel) &&
        (atsc_minor_channel == other.atsc_minor_channel))
    {
        return true;
    }

    if ((orig_netid == other.orig_netid) &&
        (sdt_tsid == other.sdt_tsid)     &&
        (service_id == other.service_id))
        return true;

    if (!orig_netid && !other.orig_netid &&
        (pat_tsid == other.pat_tsid) && (service_id == other.service_id))
        return true;

    if (relaxed)
    {
        if (("mpeg" == si_standard || "mpeg" == other.si_standard ||
             "dvb" == si_standard || "dvb" == other.si_standard ||
             si_standard.isEmpty() || other.si_standard.isEmpty()) &&
            (service_id == other.service_id))
        {
            return true;
        }
    }

    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
