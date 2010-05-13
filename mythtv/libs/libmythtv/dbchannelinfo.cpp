// -*- Mode: c++ -*-

// Qt headers
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QUrl>

// MythTV headers
#include "dbchannelinfo.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "mpegstreamdata.h" // for CryptStatus
#include "remotefile.h"

DBChannel::DBChannel(const DBChannel &other)
{
    (*this) = other;
}

DBChannel::DBChannel(
    const QString &_channum, const QString &_callsign,
    uint _chanid, uint _major_chan, uint _minor_chan,
    uint _mplexid, bool _visible,
    const QString &_name, const QString &_icon) :
    channum(_channum),
    callsign(_callsign), chanid(_chanid),
    major_chan(_major_chan), minor_chan(_minor_chan),
    mplexid(_mplexid), visible(_visible),
    name(_name), icon(_icon)
{
    channum.detach();
    callsign.detach();
    name.detach();
    icon.detach();
    mplexid = (mplexid == 32767) ? 0 : mplexid;
    icon = (icon == "none") ? QString::null : icon;
}

DBChannel &DBChannel::operator=(const DBChannel &other)
{
    channum    = other.channum;  channum.detach();
    callsign   = other.callsign; callsign.detach();
    chanid     = other.chanid;
    major_chan = other.major_chan;
    minor_chan = other.minor_chan;
    mplexid    = (other.mplexid == 32767) ? 0 : other.mplexid;
    visible    = other.visible;
    name       = other.name; name.detach();
    icon       = other.icon; icon.detach();

    return *this;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

QString ChannelInfo::GetFormatted(const QString &format) const
{
    QString tmp = format;

    if (tmp.isEmpty())
        return "";

    tmp.replace("<num>",  chanstr);
    tmp.replace("<sign>", callsign);
    tmp.replace("<name>", channame);

    return tmp;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool PixmapChannel::CacheChannelIcon(void)
{
    if (icon.isEmpty())
        return false;

    m_localIcon = icon;

    // Is icon local?
    if (QFile(icon).exists())
        return true;

    QString localDirStr = QString("%1/channels").arg(GetConfDir());
    QDir localDir(localDirStr);

    if (!localDir.exists() && !localDir.mkdir(localDirStr))
    {
        VERBOSE(VB_IMPORTANT, QString("Icons directory is missing and could "
                                      "not be created: %1").arg(localDirStr));
        icon.clear();
        return false;
    }

    // Has it been saved to the local cache?
    m_localIcon = QString("%1/%2").arg(localDirStr)
                                 .arg(QFileInfo(icon).fileName());
    if (QFile(m_localIcon).exists())
        return true;

    // Get address of master backed
    QString url = gCoreContext->GetMasterHostPrefix();
    if (url.length() < 1)
    {
        icon.clear();
        return false;
    }

    url.append(icon);

    QUrl qurl = url;
    if (qurl.host().isEmpty())
    {
        icon.clear();
        return false;
    }

    RemoteFile *rf = new RemoteFile(url, false, false, 0);

    QByteArray data;
    bool ret = rf->SaveAs(data);

    delete rf;

    if (ret && data.size())
    {
        QImage image;

        image.loadFromData(data);

        //if (image.loadFromData(data) && image.width() > 0

        if (image.save(m_localIcon))
        {
            VERBOSE(VB_GENERAL, QString("Caching channel icon %1").arg(m_localIcon));
            return true;
        }
        else
            VERBOSE(VB_GENERAL, QString("Failed to save to %1").arg(m_localIcon));
    }

    // if we get here then the icon is set in the db but couldn't be found
    // anywhere so maybe we should remove it from the DB?
    icon.clear();

    return false;
}

QString PixmapChannel::GetFormatted(const QString &format) const
{
    QString tmp = format;

    if (tmp.isEmpty())
        return "";

    tmp.replace("<num>",  channum);
    tmp.replace("<sign>", callsign);
    tmp.replace("<name>", name);

    return tmp;
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
    if (!other.si_standard.isEmpty() && si_standard.isEmpty())
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

bool ChannelInsertInfo::IsSameChannel(const ChannelInsertInfo &other) const
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

    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
