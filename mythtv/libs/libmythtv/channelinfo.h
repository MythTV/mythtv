#ifndef DB_CHANNEL_INFO_H_
#define DB_CHANNEL_INFO_H_

// C++ headers
#include <cstdint>
#include <vector>

using namespace std;

// Qt headers
#include <QString>
#include <QImage>
#include <QVariant>
#include <QDateTime>

// MythTV headers
#include "mythtvexp.h"
#include "mythtypes.h"
#include "programtypes.h"

class MTV_PUBLIC ChannelInfo
{
 public:
    ChannelInfo() = default;
    ChannelInfo(const ChannelInfo&);
    ChannelInfo(const QString &_channum, const QString &_callsign,
              uint _chanid, uint _major_chan, uint _minor_chan,
              uint _mplexid, bool _visible,
              const QString &_name, const QString &_icon,
              uint _sourceid);
    
    ChannelInfo& operator=(const ChannelInfo&);

    bool operator == (uint chanid) const
        { return m_chanid == chanid; }
        
    bool Load(uint lchanid = -1);

    enum ChannelFormat { kChannelShort, kChannelLong };
    QString GetFormatted(const ChannelFormat &format) const;
    void ToMap(InfoMap &infoMap);

    QString GetSourceName();
    void SetSourceName(const QString &lname) { m_sourcename = lname; }

    
    const QList<uint> GetGroupIds() const { return m_groupIdList; }
    void LoadGroupIds();
    void AddGroupId(uint lgroupid)
    {
        if (!m_groupIdList.contains(lgroupid))
            m_groupIdList.push_back(lgroupid);
    }
    void RemoveGroupId(uint lgroupid) { m_groupIdList.removeOne(lgroupid); }

    
    const QList<uint> GetInputIds() const { return m_inputIdList; }
    void LoadInputIds();
    // Since inputids must only appear once in a list, protect access
    // to it
    void AddInputId(uint linputid)
    {
        if (!m_inputIdList.contains(linputid))
            m_inputIdList.push_back(linputid);
    }
    void RemoveInputId(uint linputid) { m_inputIdList.removeOne(linputid); }
    
    
  private:
    void Init();

  public:
      
    // Ordered to match channel table
    uint         m_chanid            {0};
    QString      m_channum;
    QString      m_freqid;           // May be overloaded to a
                                     // non-frequency identifier
    uint         m_sourceid          {0};
    
    QString      m_callsign;
    QString      m_name;
    QString      m_icon;
    
    int          m_finetune          {0};
    QString      m_videofilters;
    QString      m_xmltvid;
    int          m_recpriority       {0};

    uint         m_contrast          {32768};
    uint         m_brightness        {32768};
    uint         m_colour            {32768};
    uint         m_hue               {32768};

    QString      m_tvformat;
    bool         m_visible           {true};
    QString      m_outputfilters;
    bool         m_useonairguide     {false};
    
    uint         m_mplexid           {0};
    uint         m_serviceid         {0};
    uint         m_service_type      {0};
    uint         m_atsc_major_chan   {0};
    uint         m_atsc_minor_chan   {0};

    QDateTime    m_last_record;

    QString      m_default_authority;
    int          m_commmethod        {-1};
    int          m_tmoffset          {0};
    uint         m_iptvid            {0};

    QString      m_old_xmltvid; // Used by mythfilldatabase when updating the xmltvid

  private:
    QString      m_sourcename; // Cache here rather than looking up each time
    // Following not in database - Cached
    QList<uint>  m_groupIdList;
    QList<uint>  m_inputIdList;
};
typedef vector<ChannelInfo> ChannelInfoList;

class MTV_PUBLIC ChannelInsertInfo
{
  public:
    ChannelInsertInfo(void) = default;
    ChannelInsertInfo(
        uint           _db_mplexid,         uint           _source_id,
        uint           _channel_id,         const QString& _callsign,
        const QString& _service_name,       const QString& _chan_num,
        uint           _service_id,

        uint           _atsc_major_channel, uint           _atsc_minor_channel,
        bool           _use_on_air_guide,   bool           _hidden,
        bool           _hidden_in_guide,

        const QString& _freqid,             const QString& _icon,
        const QString& _format,             const QString& _xmltvid,

        uint           _pat_tsid,           uint           _vct_tsid,
        uint           _vct_chan_tsid,      uint           _sdt_tsid,

        uint           _orig_netid,         uint           _netid,

        const QString& _si_standard,

        bool           _in_channels_conf,   bool           _in_pat,
        bool           _in_pmt,             bool           _in_vct,
        bool           _in_nit,             bool           _in_sdt,

        bool           _is_encrypted,       bool           _is_data_service,
        bool           _is_audio_service,   bool           _is_opencable,
        bool           _could_be_opencable, int            _decryption_status,
        const QString& _default_authority,  uint           _service_type) :
    m_db_mplexid(_db_mplexid),
    m_source_id(_source_id),
    m_channel_id(_channel_id),
    m_callsign(_callsign),
    m_service_name(_service_name),
    m_chan_num(_chan_num),
    m_service_id(_service_id),
    m_service_type(_service_type),
    m_atsc_major_channel(_atsc_major_channel),
    m_atsc_minor_channel(_atsc_minor_channel),
    m_use_on_air_guide(_use_on_air_guide),
    m_hidden(_hidden),
    m_hidden_in_guide(_hidden_in_guide),
    m_freqid(_freqid),
    m_icon(_icon),
    m_format(_format),
    m_xmltvid(_xmltvid),
    m_default_authority(_default_authority),
    m_pat_tsid(_pat_tsid),
    m_vct_tsid(_vct_tsid),
    m_vct_chan_tsid(_vct_chan_tsid),
    m_sdt_tsid(_sdt_tsid),
    m_orig_netid(_orig_netid),
    m_netid(_netid),
    m_si_standard(_si_standard),
    m_in_channels_conf(_in_channels_conf),
    m_in_pat(_in_pat),
    m_in_pmt(_in_pmt),
    m_in_vct(_in_vct),
    m_in_nit(_in_nit),
    m_in_sdt(_in_sdt),
    m_is_encrypted(_is_encrypted),
    m_is_data_service(_is_data_service),
    m_is_audio_service(_is_audio_service),
    m_is_opencable(_is_opencable),
    m_could_be_opencable(_could_be_opencable),
    m_decryption_status(_decryption_status) {}

    ChannelInsertInfo(const ChannelInsertInfo &other) { (*this = other); }
    ChannelInsertInfo &operator=(const ChannelInsertInfo &other) = default;

    bool IsSameChannel(const ChannelInsertInfo&, int relaxed = 0) const;

    bool SaveScan(uint scanid, uint transportid) const;

    void ImportExtraInfo(const ChannelInsertInfo &other);

  public:
    uint    m_db_mplexid         {0};
    uint    m_source_id          {0};
    uint    m_channel_id         {0};
    QString m_callsign;
    QString m_service_name;
    QString m_chan_num;
    uint    m_service_id         {0};
    uint    m_service_type       {0};
    uint    m_atsc_major_channel {0};
    uint    m_atsc_minor_channel {0};
    bool    m_use_on_air_guide   {false};
    bool    m_hidden             {false};
    bool    m_hidden_in_guide    {false};
    QString m_freqid;
    QString m_icon;
    QString m_format;
    QString m_xmltvid;
    QString m_default_authority;

    // non-DB info
    uint    m_pat_tsid           {0};
    uint    m_vct_tsid           {0};
    uint    m_vct_chan_tsid      {0};
    uint    m_sdt_tsid           {0};
    uint    m_orig_netid         {0};
    uint    m_netid              {0};
    QString m_si_standard;
    bool    m_in_channels_conf   {false};
    bool    m_in_pat             {false};
    bool    m_in_pmt             {false};
    bool    m_in_vct             {false};
    bool    m_in_nit             {false};
    bool    m_in_sdt             {false};
    bool    m_is_encrypted       {false};
    bool    m_is_data_service    {false};
    bool    m_is_audio_service   {false};
    bool    m_is_opencable       {false};
    bool    m_could_be_opencable {false};
    int     m_decryption_status  {0};
};
typedef vector<ChannelInsertInfo> ChannelInsertInfoList;

Q_DECLARE_METATYPE(ChannelInfo*)

#endif // DB_CHANNEL_INFO_H_
