#ifndef CHANUTIL_H
#define CHANUTIL_H

#include <vector>
using namespace std;

#include <stdint.h>
#include <qstring.h>

class NetworkInformationTable;

class DBChannel
{
  public:
    DBChannel(const DBChannel&);
    DBChannel(const QString &_channum, const QString &_callsign,
              uint _chanid, uint _major_chan, uint _minor_chan,
              uint _favorite, bool _visible,
              const QString &_name, const QString &_icon) :
        channum(_channum), callsign(_callsign), chanid(_chanid),
        major_chan(_major_chan), minor_chan(_minor_chan),
        favorite(_favorite), visible(_visible),
        name(_name), icon(_icon) {}

    bool operator == (uint _chanid) const
        { return chanid == _chanid; }

    DBChannel& operator=(const DBChannel&);

  public:
    QString channum;
    QString callsign;
    uint    chanid;
    uint    major_chan;
    uint    minor_chan;
    uint    favorite;
    bool    visible;
    QString name;
    QString icon;
};
typedef vector<DBChannel> DBChanList;


/** \class ChannelUtil
 *  \brief Collection of helper utilities for channel DB use
 */
class ChannelUtil
{
  public:
    // Multiplex Stuff
    static uint    CreateMultiplex(
        int  sourceid,          QString sistandard,
        uint freq,              QString modulation,
        int  transport_id = -1, int     network_id = -1);

    static uint    CreateMultiplex(
        int         sourceid,     QString     sistandard,
        uint        freq,         QString     modulation,
        // DVB specific
        int         transport_id, int         network_id,
        int         symbol_rate,  signed char bandwidth,
        signed char polarity,     signed char inversion,
        signed char trans_mode,
        QString     inner_FEC,    QString     constellation,
        signed char hierarchy,    QString     hp_code_rate,
        QString     lp_code_rate, QString     guard_interval);

    static vector<uint> CreateMultiplexes(
        int sourceid, const NetworkInformationTable *nit);

    static uint    GetMplexID(uint sourceid, const QString &channum);
    static int     GetMplexID(uint sourceid,     uint frequency);
    static int     GetMplexID(uint sourceid,     uint frequency,
                              uint transport_id, uint network_id);
    static int     GetMplexID(uint sourceid,
                              uint transport_id, uint network_id);
    static int     GetBetterMplexID(int current_mplexid,
                                    int transport_id, int network_id);

    static bool    GetTuningParams(uint mplexid,
                                   QString  &modulation,
                                   uint64_t &frequency,
                                   uint     &dvb_transportid,
                                   uint     &dvb_networkid);

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
                                 int  freqid,
                                 QString icon    = "",
                                 QString format  = "Default",
                                 QString xmltvid = "");

    static bool    UpdateChannel(uint db_mplexid,
                                 uint source_id,
                                 uint channel_id,
                                 const QString &callsign,
                                 const QString &service_name,
                                 const QString &chan_num,
                                 uint service_id,
                                 uint atsc_major_channel,
                                 uint atsc_minor_channel,
                                 int freqid);

    static bool    SetServiceVersion(int mplexid, int version);

    static int     GetChanID(int db_mplexid,    int service_transport_id,
                             int major_channel, int minor_channel,
                             int program_number);

    static int     GetServiceVersion(int mplexid);

    // Misc gets

    static int     GetChanID(uint sourceid, const QString &channum)
        { return GetChannelValueInt("chanid", sourceid, channum); }
    static bool    GetChannelData(
        uint    sourceid,         const QString &channum,
        QString &tvformat,        QString       &modulation,
        QString &freqtable,       QString       &freqid,
        int     &finetune,        uint64_t      &frequency,
        int     &mpeg_prog_num,
        uint    &atsc_major,      uint          &atsc_minor,
        uint    &dvb_transportid, uint          &dvb_networkid,
        uint    &mplexid,         bool          &commfree);
    static uint    GetSourceID(uint cardid, const QString &inputname);
    static int     GetProgramNumber(uint sourceid, const QString &channum)
        { return GetChannelValueInt("serviceid", sourceid, channum); }
    static QString GetVideoFilters(uint sourceid, const QString &channum)
        { return GetChannelValueStr("videofilters", sourceid, channum); }

    static DBChanList GetChannels(uint srcid, bool vis_only, QString grp="");
    static void    SortChannels(DBChanList &list, const QString &order,
                                bool eliminate_duplicates = false);
    static void    EliminateDuplicateChanNum(DBChanList &list);

    static uint    GetNextChannel(const DBChanList &sorted,
                                  uint old_chanid, int direction);

    static QString GetChannelValueStr(const QString &channel_field,
                                      uint           sourceid,
                                      const QString &channum);

    static int     GetChannelValueInt(const QString &channel_field,
                                      uint           sourceid,
                                      const QString &channum);

    static bool    IsOnSameMultiplex(uint sourceid,
                                     const QString &new_channum,
                                     const QString &old_channum);

    /**
     * \brief Returns the channel-number string of the given channel.
     * \param chanid primary key for channel record
     */
    static QString GetChanNum(int chanid);
    /**
     * \brief Returns the callsign of the given channel.
     * \param chanid primary key for channel record
     */
    static QString GetCallsign(int chanid);
    /**
     * \brief Returns the service name of the given channel.
     * \param chanid primary key for channel record
     */
    static QString GetServiceName(int chanid);
    static int     GetSourceID(int mplexid);
    static QString GetInputName(int sourceid);
    static int     GetInputID(int sourceid, int cardid);
    static QString GetDTVPrivateType(uint networkid, const QString &key,
                                     const QString sitype = "dvb");

    // Misc sets
    static bool    SetChannelValue(const QString &field_name,
                                   QString        value,
                                   uint           sourceid,
                                   const QString &channum);

  private:
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
