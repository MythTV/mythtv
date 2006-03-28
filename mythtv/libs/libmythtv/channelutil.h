#ifndef CHANUTIL_H
#define CHANUTIL_H

#include <vector>
using namespace std;

#include <qstring.h>

class NetworkInformationTable;

/** \class ChannelUtil
 *  \brief Collection of helper utilities for channel DB use
 */
class ChannelUtil
{
  public:
    // Multiplex Stuff
    static int     CreateMultiplex(int sourceid, const QString &sistandard,
                                   uint freq,    const QString &modulation);
    static int     CreateMultiplex(int sourceid, const QString &sistandard,
                                   uint freq,    const QString &modulation,
                                   // DVB specific
                                   int transport_id,     int network_id,
                                   bool set_odfm_info,
                                   int symbol_rate,       signed char bandwidth,
                                   signed char polarity,  signed char inversion,
                                   signed char trans_mode,
                                   QString inner_FEC,     QString constellation,
                                   signed char hierarchy, QString hp_code_rate,
                                   QString lp_code_rate, QString guard_interval);
    static vector<uint>
                   CreateMultiplexes(int sourceid,
                                     const NetworkInformationTable *nit);

    static int     GetMplexID(uint sourceid,     uint frequency);
    static int     GetMplexID(uint sourceid,     uint frequency,
                              uint transport_id, uint network_id);
    static int     GetMplexID(uint sourceid,
                              uint transport_id, uint network_id);
    static int     GetBetterMplexID(int current_mplexid,
                                    int transport_id, int network_id);

    static int     GetTuningParams(int mplexid, QString &modulation);

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

    // Misc
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
    static QString GetDTVPrivateType(uint networkid, const QString &key,
                                     const QString sitype = "dvb");

  private:
      static QString GetChannelStringField(int chanid, const QString &field);
};

#endif // CHANUTIL_H
