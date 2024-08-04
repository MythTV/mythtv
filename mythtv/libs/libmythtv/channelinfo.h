#ifndef DB_CHANNEL_INFO_H_
#define DB_CHANNEL_INFO_H_

// C++ headers
#include <cstdint>
#include <utility>
#include <vector>

// Qt headers
#include <QString>
#include <QImage>
#include <QVariant>
#include <QDateTime>

// MythTV headers
#include "libmythbase/mythtypes.h"
#include "libmythbase/programtypes.h"
#include "libmythtv/mythtvexp.h"

enum ChannelVisibleType : std::int8_t
{
    kChannelAlwaysVisible = 2,
    kChannelVisible       = 1,
    kChannelNotVisible    = 0,
    kChannelNeverVisible  = -1
};
MTV_PUBLIC QString toString(ChannelVisibleType /*type*/);
MTV_PUBLIC QString toRawString(ChannelVisibleType /*type*/);
MTV_PUBLIC ChannelVisibleType channelVisibleTypeFromString(const QString& /*type*/);

class MTV_PUBLIC ChannelInfo
{
 public:
    ChannelInfo() = default;
    ChannelInfo(const ChannelInfo &other);
    ChannelInfo(QString _channum, QString _callsign,
              uint _chanid, uint _major_chan, uint _minor_chan,
              uint _mplexid, ChannelVisibleType _visible,
              QString _name, QString _icon,
              uint _sourceid);
    
    ChannelInfo& operator=(const ChannelInfo &other);

    bool operator == (uint chanid) const
        { return m_chanId == chanid; }
        
    bool Load(uint lchanid = -1);

    enum ChannelFormat : std::uint8_t { kChannelShort, kChannelLong };
    QString GetFormatted(ChannelFormat format) const;
    void ToMap(InfoMap &infoMap);

    QString GetSourceName();
    void SetSourceName(const QString &lname) { m_sourceName = lname; }

    
    QList<uint> GetGroupIds() const { return m_groupIdList; }
    void LoadGroupIds();
    void AddGroupId(uint lgroupid)
    {
        if (!m_groupIdList.contains(lgroupid))
            m_groupIdList.push_back(lgroupid);
    }
    void RemoveGroupId(uint lgroupid) { m_groupIdList.removeOne(lgroupid); }

    
    QList<uint> GetInputIds() const { return m_inputIdList; }
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
    uint         m_chanId            {0};
    QString      m_chanNum;
    QString      m_freqId;           // May be overloaded to a
                                     // non-frequency identifier
    uint         m_sourceId          {0};
    
    QString      m_callSign;
    QString      m_name;
    QString      m_icon;
    
    int          m_fineTune          {0};
    QString      m_videoFilters;
    QString      m_xmltvId;
    int          m_recPriority       {0};

    uint         m_contrast          {32768};
    uint         m_brightness        {32768};
    uint         m_colour            {32768};
    uint         m_hue               {32768};

    QString      m_tvFormat;
    ChannelVisibleType m_visible     {kChannelVisible};
    QString      m_outputFilters;
    bool         m_useOnAirGuide     {false};
    
    uint         m_mplexId           {0};
    uint         m_serviceId         {0};
    uint         m_serviceType       {0};
    uint         m_atscMajorChan     {0};
    uint         m_atscMinorChan     {0};

    QDateTime    m_lastRecord;

    QString      m_defaultAuthority;
    int          m_commMethod        {-1};
    int          m_tmOffset          {0};
    uint         m_iptvId            {0};

    QString      m_oldXmltvId; // Used by mythfilldatabase when updating the xmltvid

  private:
    QString      m_sourceName; // Cache here rather than looking up each time
    // Following not in database - Cached
    QList<uint>  m_groupIdList;
    QList<uint>  m_inputIdList;
};
using ChannelInfoList = std::vector<ChannelInfo>;

class MTV_PUBLIC ChannelInsertInfo
{
  public:
    ChannelInsertInfo(void) = default;
    ChannelInsertInfo(
        uint           _db_mplexid,         uint           _source_id,
        uint           _channel_id,         QString        _callsign,
        QString        _service_name,       QString        _chan_num,
        uint           _service_id,

        uint           _atsc_major_channel, uint           _atsc_minor_channel,
        bool           _use_on_air_guide,   bool           _hidden,
        bool           _hidden_in_guide,

        QString        _freqid,             QString        _icon,
        QString        _format,             QString        _xmltvid,

        uint           _pat_tsid,           uint           _vct_tsid,
        uint           _vct_chan_tsid,      uint           _sdt_tsid,

        uint           _orig_netid,         uint           _netid,

        QString        _si_standard,

        bool           _in_channels_conf,   bool           _in_pat,
        bool           _in_pmt,             bool           _in_vct,
        bool           _in_nit,             bool           _in_sdt,

        bool           _is_encrypted,       bool           _is_data_service,
        bool           _is_audio_service,   bool           _is_opencable,
        bool           _could_be_opencable, int            _decryption_status,
        QString        _default_authority,  uint           _service_type,
        uint           _logical_channel,    uint           _simulcast_channel ) :
    m_dbMplexId(_db_mplexid),
    m_sourceId(_source_id),
    m_channelId(_channel_id),
    m_callSign(std::move(_callsign)),
    m_serviceName(std::move(_service_name)),
    m_chanNum(std::move(_chan_num)),
    m_serviceId(_service_id),
    m_serviceType(_service_type),
    m_atscMajorChannel(_atsc_major_channel),
    m_atscMinorChannel(_atsc_minor_channel),
    m_useOnAirGuide(_use_on_air_guide),
    m_hidden(_hidden),
    m_hiddenInGuide(_hidden_in_guide),
    m_freqId(std::move(_freqid)),
    m_icon(std::move(_icon)),
    m_format(std::move(_format)),
    m_xmltvId(std::move(_xmltvid)),
    m_defaultAuthority(std::move(_default_authority)),
    m_patTsId(_pat_tsid),
    m_vctTsId(_vct_tsid),
    m_vctChanTsId(_vct_chan_tsid),
    m_sdtTsId(_sdt_tsid),
    m_origNetId(_orig_netid),
    m_netId(_netid),
    m_siStandard(std::move(_si_standard)),
    m_inChannelsConf(_in_channels_conf),
    m_inPat(_in_pat),
    m_inPmt(_in_pmt),
    m_inVct(_in_vct),
    m_inNit(_in_nit),
    m_inSdt(_in_sdt),
    m_isEncrypted(_is_encrypted),
    m_isDataService(_is_data_service),
    m_isAudioService(_is_audio_service),
    m_isOpencable(_is_opencable),
    m_couldBeOpencable(_could_be_opencable),
    m_decryptionStatus(_decryption_status),
    m_logicalChannel(_logical_channel),
    m_simulcastChannel(_simulcast_channel) {}

    ChannelInsertInfo(const ChannelInsertInfo &other) { (*this = other); }
    ChannelInsertInfo &operator=(const ChannelInsertInfo&) = default;

    bool IsSameChannel(const ChannelInsertInfo &other, int relaxed = 0) const;

    bool SaveScan(uint scanid, uint transportid) const;

    void ImportExtraInfo(const ChannelInsertInfo &other);

  public:
    uint    m_dbMplexId          {0};
    uint    m_sourceId           {0};
    uint    m_channelId          {0};
    QString m_callSign;
    QString m_serviceName;
    QString m_chanNum;
    uint    m_serviceId          {0};
    uint    m_serviceType        {0};
    uint    m_atscMajorChannel   {0};
    uint    m_atscMinorChannel   {0};
    bool    m_useOnAirGuide      {false};
    bool    m_hidden             {false};
    bool    m_hiddenInGuide      {false};
    ChannelVisibleType m_visible {kChannelVisible};
    QString m_freqId;
    QString m_icon;
    QString m_format;
    QString m_xmltvId;
    QString m_defaultAuthority;

    // non-DB info
    uint    m_patTsId            {0};
    uint    m_vctTsId            {0};
    uint    m_vctChanTsId        {0};
    uint    m_sdtTsId            {0};
    uint    m_origNetId          {0};
    uint    m_netId              {0};
    QString m_siStandard;
    bool    m_inChannelsConf     {false};
    bool    m_inPat              {false};
    bool    m_inPmt              {false};
    bool    m_inVct              {false};
    bool    m_inNit              {false};
    bool    m_inSdt              {false};
    bool    m_isEncrypted        {false};
    bool    m_isDataService      {false};
    bool    m_isAudioService     {false};
    bool    m_isOpencable        {false};
    bool    m_couldBeOpencable   {false};
    int     m_decryptionStatus   {0};
    uint    m_logicalChannel     {0};
    uint    m_simulcastChannel   {0};

    // Service relocated descriptor
    uint    m_oldOrigNetId       {0};
    uint    m_oldTsId            {0};
    uint    m_oldServiceId       {0};
};
using ChannelInsertInfoList = std::vector<ChannelInsertInfo>;

Q_DECLARE_METATYPE(ChannelInfo*)

#endif // DB_CHANNEL_INFO_H_
