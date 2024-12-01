// -*- Mode: c++ -*-
#ifndef CHANUTIL_H
#define CHANUTIL_H

// C++ headers
#include <cstdint>
#include <deque>
#include <vector>

// Qt headers
#include <QString>
#include <QCoreApplication>

// MythTV headers
#include "mythtvexp.h"
#include "dtvmultiplex.h"
#include "channelinfo.h"
#include "iptvtuningdata.h"
#include "tv.h" // for CHANNEL_DIRECTION

class NetworkInformationTable;
class TestEITFixups;

class pid_cache_item_t
{
  public:
    pid_cache_item_t() = default;
    pid_cache_item_t(uint pid, uint sid_tid) :
        m_pid(pid), m_sidTid(sid_tid) {}
    uint GetPID(void) const { return m_pid; }
    uint GetStreamID(void) const
        { return (m_sidTid&0x100) ? GetID() : 0; }
    uint GetTableID(void) const
        { return (m_sidTid&0x100) ? 0 : GetID(); }
    uint GetID(void) const { return m_sidTid & 0xff; }
    bool IsPCRPID(void) const { return ( m_sidTid&0x200 ) != 0; }
    bool IsPermanent(void) const { return ( m_sidTid&0x10000 ) != 0; }
    uint GetComposite(void) const { return m_sidTid; }
  private:
    uint m_pid     {0};
    uint m_sidTid  {0};
};
using pid_cache_t = std::vector<pid_cache_item_t>;

/** \class ChannelUtil
 *  \brief Collection of helper utilities for channel DB use
 */
class MTV_PUBLIC ChannelUtil
{
    friend class TestEITFixups;

    Q_DECLARE_TR_FUNCTIONS(ChannelUtil);

  public:
    // Multiplex Stuff

    static uint    CreateMultiplex(
        int  sourceid,          const QString& sistandard,
        uint64_t frequency,     const QString& modulation,
        int  transport_id = -1, int     network_id = -1);

    static uint    CreateMultiplex(
        int            sourceid,     const QString& sistandard,
        uint64_t       frequency,    const QString& modulation,
        // DVB specific
        int            transport_id, int            network_id,
        int            symbol_rate,  signed char    bandwidth,
        signed char    polarity,     signed char    inversion,
        signed char    trans_mode,
        const QString& inner_FEC,    const QString& constellation,
        signed char    hierarchy,    const QString& hp_code_rate,
        const QString& lp_code_rate, const QString& guard_interval,
        const QString& mod_sys,      const QString& rolloff);

    static uint    CreateMultiplex(uint sourceid, const DTVMultiplex &mux,
                                   int transport_id, int network_id);

    static std::vector<uint> CreateMultiplexes(
        int sourceid, const NetworkInformationTable *nit);

    static uint    GetMplexID(uint sourceid, const QString &channum);
    static int     GetMplexID(uint sourceid,     uint64_t frequency);
    static int     GetMplexID(uint sourceid,     uint64_t frequency,
                              uint transport_id, uint network_id);
    static int     GetMplexID(uint sourceid,
                              uint transport_id, uint network_id);
    static uint    GetMplexID(uint chanid);
    static int     GetBetterMplexID(int current_mplexid,
                                    int transport_id, int network_id);

    static bool    GetTuningParams(uint mplexid,
                                   QString  &modulation,
                                   uint64_t &frequency,
                                   uint     &dvb_transportid,
                                   uint     &dvb_networkid,
                                   QString  &si_std);

    static bool    GetATSCChannel(uint sourceid, const QString &channum,
                                  uint &major,   uint          &minor);
    static bool    IsATSCChannel(uint sourceid, const QString &channum)
        {
            uint m1 = 0;
            uint m2 = 0;
            GetATSCChannel(sourceid, channum, m1,m2);
            return m2 != 0U;
        }

    // Channel/Service Stuff
    static int     CreateChanID(uint sourceid, const QString &chan_num);

    static bool    CreateChannel(uint db_mplexid,
                                 uint db_sourceid,
                                 uint new_channel_id,
                                 const QString &callsign,
                                 const QString &service_name,
                                 const QString &chan_num,
                                 uint service_id,
                                 uint atsc_major_channel,
                                 uint atsc_minor_channel,
                                 bool use_on_air_guide,
                                 ChannelVisibleType visible,
                                 const QString &freqid,
                                 const QString& icon    = QString(),
                                 QString format  = "Default",
                                 const QString& xmltvid = QString(),
                                 const QString& default_authority = QString(),
                                 uint service_type = 0,
                                 int  recpriority = 0,
                                 int  tmOffset = 0,
                                 int commMethod = -1);

    static bool    UpdateChannel(uint db_mplexid,
                                 uint source_id,
                                 uint channel_id,
                                 const QString &callsign,
                                 const QString &service_name,
                                 const QString &chan_num,
                                 uint service_id,
                                 uint atsc_major_channel,
                                 uint atsc_minor_channel,
                                 bool use_on_air_guide,
                                 ChannelVisibleType visible,
                                 const QString& freqid  = QString(),
                                 const QString& icon    = QString(),
                                 QString format  = QString(),
                                 const QString& xmltvid = QString(),
                                 const QString& default_authority = QString(),
                                 uint service_type = 0,
                                 // note INT_MIN is invalid for these fields
                                 // to indicate that they are not to be updated
                                 int  recpriority = INT_MIN,
                                 int  tmOffset = INT_MIN,
                                 int commMethod = INT_MIN);

    static bool    CreateIPTVTuningData(
        uint channel_id, const IPTVTuningData &tuning)
    {
        return UpdateIPTVTuningData(channel_id, tuning);
    }

    static bool    UpdateIPTVTuningData(
        uint channel_id, const IPTVTuningData &tuning);

    static void    UpdateInsertInfoFromDB(ChannelInsertInfo &chan);
    static void    UpdateChannelNumberFromDB(ChannelInsertInfo &chan);

    static bool    DeleteChannel(uint channel_id);

    static bool    SetVisible(uint channel_id, ChannelVisibleType visible);

    static bool    SetServiceVersion(int mplexid, int version);

    static int     GetChanID(int db_mplexid,    int service_transport_id,
                             int major_channel, int minor_channel,
                             int program_number);

    static int     GetServiceVersion(int mplexid);

    // Misc gets
    static QString GetDefaultAuthority(uint chanid);
    static QString GetIcon(uint chanid);
    static std::vector<uint> GetInputIDs(uint chanid);
    static QString GetUnknownCallsign(void);
    static uint    FindChannel(uint sourceid, const QString &freqid);
    static int     GetChanID(uint sourceid, const QString &channum)
        { return GetChannelValueInt("chanid", sourceid, channum); }
    static bool    GetChannelData(
        uint    sourceid,
        uint    &chanid,          const QString &channum,
        QString &tvformat,        QString       &modulation,
        QString &freqtable,       QString       &freqid,
        int     &finetune,        uint64_t      &frequency,
        QString &dtv_si_std,      int     &mpeg_prog_num,
        uint    &atsc_major,      uint          &atsc_minor,
        uint    &dvb_transportid, uint          &dvb_networkid,
        uint    &mplexid,         bool          &commfree);
    static int     GetProgramNumber(uint sourceid, const QString &channum)
        { return GetChannelValueInt("serviceid", sourceid, channum); }
    static QString GetVideoFilters(uint sourceid, const QString &channum)
        { return GetChannelValueStr("videofilters", sourceid, channum); }
    static IPTVTuningData GetIPTVTuningData(uint chanid);

    // Are there any other possibly useful sort orders?
    // e.g. Sorting by sourceid would probably be better done by two distinct
    // calls to LoadChannels specifying the sourceid
    enum OrderBy : std::uint8_t
    {
        kChanOrderByChanNum,
        kChanOrderByName,
        kChanOrderByLiveTV,
    };

    enum GroupBy : std::uint8_t
    {
        kChanGroupByCallsign,
        kChanGroupByCallsignAndChannum,
        kChanGroupByChanid // Because of the nature of the query we always need to group
    };

    /**
     *  \brief Load channels from database into a list of ChannelInfo objects
     *
     *  \note This replaces all previous methods e.g. GetChannels() and
     *        GetAllChannels() in channelutil.h
     *
     *  \note There is no grouping option by design and here's
     * why. Grouping with a ChannelInfo object is largely
     * self-defeating, 99% of the information in the remaining result
     * won't apply to the other 'grouped' channels. Therefore GroupBy
     * should be used when you only care about Name, Number and
     * Callsign. Loading a complete ChannelInfo object, including
     * input info is therefore an actual waste of processor cycles and
     * memory, so we may yet introduce a parent class for ChannelInfo
     * called BasicChannelInfo or similar with it's own Load function
    */
    static ChannelInfoList LoadChannels(uint startIndex, uint count,
                                        uint &totalAvailable,
                                        bool ignoreHidden = true,
                                        OrderBy orderBy = kChanOrderByChanNum,
                                        GroupBy groupBy = kChanGroupByChanid,
                                        uint sourceID = 0,
                                        uint channelGroupID = 0,
                                        bool liveTVOnly = false,
                                        const QString& callsign = "",
                                        const QString& channum = "",
                                        bool ignoreUntunable = true);

    /**
     * \deprecated Use LoadChannels instead
     */
    static ChannelInfoList GetChannels(
        uint sourceid, bool visible_only, 
        const QString &group_by = QString(), uint channel_groupid = 0)
    {
        return GetChannelsInternal(sourceid, visible_only, false,
                                   group_by, channel_groupid);
    }

    /// Returns channels that are not connected to an input and
    /// channels that are not marked as visible.
    /**
     * \deprecated
     */
    static ChannelInfoList GetAllChannels(uint sourceid)
    {
        return GetChannelsInternal(sourceid, false, true, QString(), 0);
    }
    static std::vector<uint> GetChanIDs(int sourceid = -1, bool onlyVisible = false);
    static uint    GetChannelCount(int sourceid = -1);
    static void    SortChannels(ChannelInfoList &list, const QString &order,
                                bool eliminate_duplicates = false);
    static int     GetNearestChannel(const ChannelInfoList &list,
                                     const QString &channum);

    static uint    GetNextChannel(const ChannelInfoList &sorted,
                                  uint old_chanid,
                                  uint mplexid_restriction,
                                  uint chanid_restriction,
                                  ChannelChangeDirection direction,
                                  bool skip_non_visible = true,
                                  bool skip_same_channum_and_callsign = false,
                                  bool skip_other_sources = false);

    static QString GetChannelValueStr(const QString &channel_field,
                                      uint           sourceid,
                                      const QString &channum);

    static int     GetChannelValueInt(const QString &channel_field,
                                      uint           sourceid,
                                      const QString &channum);

    static QString GetChannelNumber(uint           sourceid,
                                    const QString &channel_name);

    static bool    IsOnSameMultiplex(uint srcid,
                                     const QString &new_channum,
                                     const QString &old_channum);

    static QStringList GetValidRecorderList(uint            chanid,
                                            const QString &channum);

    static bool    IsConflicting(const QString &channum,
                                 uint sourceid = 0, uint excluded_chanid = 0)
    {
        std::vector<uint> chanids = GetConflicting(channum, sourceid);
        return (chanids.size() > 1) ||
            ((1 == chanids.size()) && (chanids[0] != excluded_chanid));
    }

    static std::vector<uint> GetConflicting(const QString &channum,
                                       uint sourceid = 0);

    /**
     * \brief Returns the channel-number string of the given channel.
     * \param chanid primary key for channel record
     */
    static QString GetChanNum(int chan_id);

    /**
     * \brief Returns the listings time offset in minutes for given channel.
     * \param chanid primary key for channel record
     */
    static std::chrono::minutes GetTimeOffset(int chan_id);
    static int     GetSourceID(int mplexid);
    static uint    GetSourceIDForChannel(uint chanid);

    static QStringList GetInputTypes(uint chanid);

    static bool    GetCachedPids(uint chanid, pid_cache_t &pid_cache);

    // Misc sets
    static bool    SetChannelValue(const QString &field_name,
                                   const QString& value,
                                   uint           sourceid,
                                   const QString &channum);

    static bool    SetChannelValue(const QString &field_name,
                                   const QString& value,
                                   int            chanid);

    static bool    SaveCachedPids(uint chanid,
                                  const pid_cache_t &_pid_cache,
                                  bool delete_all = false);

    static const QString kATSCSeparators;

  private:
    /**
     * \deprecated Use LoadChannels instead
     */
    static ChannelInfoList GetChannelsInternal(
        uint sourceid, bool visible_only, bool include_disconnected,
        const QString &group_by, uint channel_groupid);
    static QString GetChannelStringField(int chan_id, const QString &field);

    static QReadWriteLock s_channelDefaultAuthorityMapLock;
    static QMap<uint,QString> s_channelDefaultAuthorityMap;
    static bool s_channelDefaultAuthority_runInit;
};

#endif // CHANUTIL_H
