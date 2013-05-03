#ifndef DB_CHANNEL_INFO_H_
#define DB_CHANNEL_INFO_H_

// POSIX headers
#include <stdint.h>

// C++ headers
#include <vector>

using namespace std;

// Qt headers
#include <QString>
#include <QImage>
#include <QVariant>
#include <QDateTime>

// MythTV headers
#include "mythtvexp.h"
#include "programtypes.h"

class MTV_PUBLIC ChannelInfo
{
 public:
    ChannelInfo();
    ChannelInfo(const ChannelInfo&);
    ChannelInfo(const QString &_channum, const QString &_callsign,
              uint _chanid, uint _major_chan, uint _minor_chan,
              uint _mplexid, bool _visible,
              const QString &_name, const QString &_icon,
              uint _sourceid);
    
    ChannelInfo& operator=(const ChannelInfo&);

    bool operator == (uint _chanid) const
        { return chanid == _chanid; }
        
    enum ChannelFormat { kChannelShort, kChannelLong };
    QString GetFormatted(const ChannelFormat &format) const;
    void ToMap(InfoMap &infoMap);

    QString GetSourceName();
    void SetSourceName(const QString lname) { m_sourcename = lname; }

    
    const QList<uint> GetGroupIds() const { return m_groupIdList; }
    void LoadGroupIds();
    void AddGroupId(uint lgroupid)
    {
        if (!m_groupIdList.contains(lgroupid))
            m_groupIdList.push_back(lgroupid);
    }
    void RemoveGroupId(uint lgroupid) { m_groupIdList.removeOne(lgroupid); }

    
    const QList<uint> GetCardIds() const { return m_cardIdList; }
    void LoadCardIds();
    // Since cardids must only appear once in a list, protect access
    // to it
    void AddCardId(uint lcardid)
    {
        if (!m_cardIdList.contains(lcardid))
            m_cardIdList.push_back(lcardid);
    }
    void RemoveCardId(uint lcardid) { m_cardIdList.removeOne(lcardid); }
    
    
  private:
    void Init();

  public:
      
    // Ordered to match channel table
    uint    chanid;
    QString channum;
    QString freqid;
    uint    sourceid;
    
    QString callsign;
    QString name;
    QString icon;
    
    int     finetune;
    QString videofilters;
    QString xmltvid;
    int     recpriority;

    uint    contrast;
    uint    brightness;
    uint    colour;
    uint    hue;

    QString tvformat;
    bool    visible;
    QString outputfilters;
    bool    useonairguide;
    
    uint    mplexid;
    uint    serviceid;
    uint    atsc_major_chan;
    uint    atsc_minor_chan;

    QDateTime last_record;

    QString default_authority;
    int     commmethod;
    int     tmoffset;
    uint    iptvid;

    QString old_xmltvid; // Used by mythfilldatabase when updating the xmltvid

  private:
    QString m_sourcename; // Cache here rather than looking up each time
    // Following not in database - Cached
    QList<uint>  m_groupIdList;
    QList<uint>  m_cardIdList;
};
typedef vector<ChannelInfo> ChannelInfoList;

class MTV_PUBLIC ChannelInsertInfo
{
  public:
    ChannelInsertInfo(void) :
        db_mplexid(0), source_id(0), channel_id(0),
        callsign(""), service_name(""),
        chan_num(""), service_id(0),
        atsc_major_channel(0), atsc_minor_channel(0),
        use_on_air_guide(false),
        hidden(false), hidden_in_guide(false),
        freqid(""), icon(""),
        format(""), xmltvid(""),
        default_authority(""),
        pat_tsid(0), vct_tsid(0), vct_chan_tsid(0), sdt_tsid(0),
        orig_netid(0), netid(0),
        si_standard(""),
        in_channels_conf(false),
        in_pat(false), in_pmt(false),
        in_vct(false),
        in_nit(false), in_sdt(false),
        is_encrypted(false),
        is_data_service(false), is_audio_service(false),
        is_opencable(false), could_be_opencable(false),
        decryption_status(0) { }

    ChannelInsertInfo(
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
        QString _default_authority);

    ChannelInsertInfo(const ChannelInsertInfo &other) { (*this = other); }
    ChannelInsertInfo &operator=(const ChannelInsertInfo &other);

    bool IsSameChannel(const ChannelInsertInfo&, bool relaxed = false) const;

    bool SaveScan(uint scanid, uint transportid) const;

    void ImportExtraInfo(const ChannelInsertInfo &other);

  public:
    uint    db_mplexid;
    uint    source_id;
    uint    channel_id;
    QString callsign;
    QString service_name;
    QString chan_num;
    uint    service_id;
    uint    atsc_major_channel;
    uint    atsc_minor_channel;
    bool    use_on_air_guide;
    bool    hidden;
    bool    hidden_in_guide;
    QString freqid;
    QString icon;
    QString format;
    QString xmltvid;
    QString default_authority;

    // non-DB info
    uint    pat_tsid;
    uint    vct_tsid;
    uint    vct_chan_tsid;
    uint    sdt_tsid;
    uint    orig_netid;
    uint    netid;
    QString si_standard;
    bool    in_channels_conf;
    bool    in_pat;
    bool    in_pmt;
    bool    in_vct;
    bool    in_nit;
    bool    in_sdt;
    bool    is_encrypted;
    bool    is_data_service;
    bool    is_audio_service;
    bool    is_opencable;
    bool    could_be_opencable;
    int     decryption_status;
};
typedef vector<ChannelInsertInfo> ChannelInsertInfoList;

Q_DECLARE_METATYPE(ChannelInfo*)

#endif // DB_CHANNEL_INFO_H_
