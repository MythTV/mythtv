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

// MythTV headers
#include "mythtvexp.h" 

// TODO: Refactor DBChannel, PixmapChannel and ChannelInfo into a single class

class MTV_PUBLIC DBChannel
{
  public:
    DBChannel(const DBChannel&);
    DBChannel(const QString &_channum, const QString &_callsign,
              uint _chanid, uint _major_chan, uint _minor_chan,
              uint _mplexid, bool _visible,
              const QString &_name, const QString &_icon,
              uint _sourceid, uint _cardid, uint _grpid);
    DBChannel& operator=(const DBChannel&);

    bool operator == (uint _chanid) const
        { return chanid == _chanid; }

  public:
    QString channum;
    QString callsign;
    QString name;
    QString icon;
    uint    chanid;
    uint    major_chan;
    uint    minor_chan;
    uint    mplexid;
    uint    sourceid;
    uint    cardid;
    uint    grpid;
    bool    visible;
};
typedef vector<DBChannel> DBChanList;

class MTV_PUBLIC ChannelInfo
{
 public:
    ChannelInfo() : chanid(-1), sourceid(-1), favid(-1) {}
    QString GetFormatted(const QString &format) const;

    QString callsign;
    QString iconpath;
    QString chanstr;
    QString channame;
    int chanid;
    int sourceid;
    QString sourcename;
    int favid;
    QString recpriority;
};

Q_DECLARE_METATYPE(ChannelInfo*)

class MTV_PUBLIC PixmapChannel : public DBChannel
{
  public:
    PixmapChannel(const PixmapChannel &other) :
        DBChannel(other) { }
    PixmapChannel(const DBChannel &other) :
        DBChannel(other) { }

    bool CacheChannelIcon(void);
    QString GetFormatted(const QString &format) const;

  public:
    QString m_localIcon;
};

class MTV_PUBLIC ChannelInsertInfo
{
  public:
    ChannelInsertInfo(void) :
        db_mplexid(0), source_id(0), channel_id(0),
        callsign(QString::null), service_name(QString::null),
        chan_num(QString::null), service_id(0),
        atsc_major_channel(0), atsc_minor_channel(0),
        use_on_air_guide(false),
        hidden(false), hidden_in_guide(false),
        freqid(QString::null), icon(QString::null),
        format(QString::null), xmltvid(QString::null),
        default_authority(QString::null),
        pat_tsid(0), vct_tsid(0), vct_chan_tsid(0), sdt_tsid(0),
        orig_netid(0), netid(0),
        si_standard(QString::null),
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

    bool IsSameChannel(const ChannelInsertInfo&) const;

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

#endif // DB_CHANNEL_INFO_H_
