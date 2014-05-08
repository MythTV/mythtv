// -*- Mode: c++ -*-
#ifndef CHANUTIL_H
#define CHANUTIL_H

// POSIX headers
#include <stdint.h>

// C++ headers
#include <vector>
#include <deque>
using namespace std;

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

class pid_cache_item_t
{
  public:
    pid_cache_item_t() : pid(0), sid_tid(0) {}
    pid_cache_item_t(uint _pid, uint _sid_tid) :
        pid(_pid), sid_tid(_sid_tid) {}
    uint GetPID(void) const { return pid; }
    uint GetStreamID(void) const
        { return (sid_tid&0x100) ? GetID() : 0; }
    uint GetTableID(void) const
        { return (sid_tid&0x100) ? 0 : GetID(); }
    uint GetID(void) const { return sid_tid & 0xff; }
    bool IsPCRPID(void) const { return sid_tid&0x200; }
    bool IsPermanent(void) const { return sid_tid&0x10000; }
    uint GetComposite(void) const { return sid_tid; }
  private:
    uint pid;
    uint sid_tid;
};
typedef vector<pid_cache_item_t> pid_cache_t;

/** \class ChannelUtil
 *  \brief Collection of helper utilities for channel DB use
 */
class MTV_PUBLIC ChannelUtil
{
    Q_DECLARE_TR_FUNCTIONS(ChannelUtil)

  public:
    // Multiplex Stuff

    static uint    CreateMultiplex(
        int  sourceid,          QString sistandard,
        uint64_t frequency,     QString modulation,
        int  transport_id = -1, int     network_id = -1);

    static uint    CreateMultiplex(
        int         sourceid,     QString     sistandard,
        uint64_t    frequency,    QString     modulation,
        // DVB specific
        int         transport_id, int         network_id,
        int         symbol_rate,  signed char bandwidth,
        signed char polarity,     signed char inversion,
        signed char trans_mode,
        QString     inner_FEC,    QString     constellation,
        signed char hierarchy,    QString     hp_code_rate,
        QString     lp_code_rate, QString     guard_interval,
        QString     mod_sys,      QString     rolloff);

    static uint    CreateMultiplex(uint sourceid, const DTVMultiplex&,
                                   int transport_id, int network_id);

    static vector<uint> CreateMultiplexes(
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
        { uint m1, m2; GetATSCChannel(sourceid, channum, m1,m2); return m2; }

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
                                 bool hidden,
                                 bool hidden_in_guide,
                                 const QString &freqid,
                                 QString icon    = QString::null,
                                 QString format  = "Default",
                                 QString xmltvid = QString::null,
                                 QString default_authority = QString::null);

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
                                 bool hidden,
                                 bool hidden_in_guide,
                                 QString freqid  = QString::null,
                                 QString icon    = QString::null,
                                 QString format  = QString::null,
                                 QString xmltvid = QString::null,
                                 QString default_authority = QString::null);

    static bool    CreateIPTVTuningData(
        uint channel_id, const IPTVTuningData &tuning)
    {
        return UpdateIPTVTuningData(channel_id, tuning);
    }

    static bool    UpdateIPTVTuningData(
        uint channel_id, const IPTVTuningData &tuning);

    static void    UpdateInsertInfoFromDB(ChannelInsertInfo &chan);

    static bool    DeleteChannel(uint channel_id);

    static bool    SetVisible(uint channel_id, bool hidden);

    static bool    SetServiceVersion(int mplexid, int version);

    static int     GetChanID(int db_mplexid,    int service_transport_id,
                             int major_channel, int minor_channel,
                             int program_number);

    static int     GetServiceVersion(int mplexid);

    // Misc gets
    static QString GetDefaultAuthority(uint chanid);
    static QString GetIcon(uint chanid);
    static vector<uint> GetCardIDs(uint chanid);
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

    static ChannelInfoList GetChannels(
        uint sourceid, bool visible_only, 
        const QString &group_by = QString(), uint channel_groupid = 0)
    {
        return GetChannelsInternal(sourceid, visible_only, false,
                                   group_by, channel_groupid);
    }
    /// Returns channels that are not connected to a capture card
    /// and channels that are not marked as visible.
    static ChannelInfoList GetAllChannels(uint sourceid)
    {
        return GetChannelsInternal(sourceid, false, true, QString(), 0);
    }
    static vector<uint> GetChanIDs(int sourceid = -1);
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
                                  bool skip_same_channum_and_callsign = false);

    static QString GetChannelValueStr(const QString &channel_field,
                                      uint           sourceid,
                                      const QString &channum);

    static int     GetChannelValueInt(const QString &channel_field,
                                      uint           sourceid,
                                      const QString &channum);

    static bool    IsOnSameMultiplex(uint sourceid,
                                     const QString &new_channum,
                                     const QString &old_channum);

    static QStringList GetValidRecorderList(uint            chanid,
                                            const QString &channum);

    static bool    IsConflicting(const QString &channum,
                                 uint sourceid = 0, uint excluded_chanid = 0)
    {
        vector<uint> chanids = GetConflicting(channum, sourceid);
        return (chanids.size() > 1) ||
            ((1 == chanids.size()) && (chanids[0] != excluded_chanid));
    }

    static vector<uint> GetConflicting(const QString &channum,
                                       uint sourceid = 0);

    /**
     * \brief Returns the channel-number string of the given channel.
     * \param chanid primary key for channel record
     */
    static QString GetChanNum(int chanid);

    /**
     * \brief Returns the listings time offset in minutes for given channel.
     * \param chanid primary key for channel record
     */
    static int     GetTimeOffset(int chanid);
    static int     GetSourceID(int mplexid);
    static uint    GetSourceIDForChannel(uint chanid);
    static int     GetInputID(int sourceid, int cardid);

    static QStringList GetCardTypes(uint chandid);

    static bool    GetCachedPids(uint chanid, pid_cache_t &pid_cache);

    // Misc sets
    static bool    SetChannelValue(const QString &field_name,
                                   QString        value,
                                   uint           sourceid,
                                   const QString &channum);

    static bool    SetChannelValue(const QString &field_name,
                                   QString        value,
                                   int            chanid);

    static bool    SaveCachedPids(uint chanid,
                                  const pid_cache_t &pid_cache,
                                  bool delete_all = false);

    static const QString kATSCSeparators;

  private:
    static ChannelInfoList GetChannelsInternal(
        uint sourceid, bool visible_only, bool include_disconnected,
        const QString &group_by, uint channel_groupid);
    static QString GetChannelStringField(int chanid, const QString &field);
    static QString GetChannelValueStr(const QString &channel_field,
                                      uint           cardid,
                                      const QString &input,
                                      const QString &channum);

    static int     GetChannelValueInt(const QString &channel_field,
                                      uint           cardid,
                                      const QString &input,
                                      const QString &channum);

};

#endif // CHANUTIL_H
