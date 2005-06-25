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
                                   int symbol_rate,      char bandwidth,
                                   char polarity,        char inversion,
                                   char trans_mode,
                                   QString inner_FEC,    QString constellation,
                                   char hierarchy,       QString hp_code_rate,
                                   QString lp_code_rate, QString guard_interval);
    static vector<int>
                   CreateMultiplexes(int sourceid,
                                     const NetworkInformationTable *nit);

    static int     GetMplexID(int sourceid, uint freq);
    static int     GetMplexID(int sourceid, uint frequency,
                              int transport_id, int network_id);
    static int     GetBetterMplexID(int current_mplexid,
                                    int transport_id, int network_id);

    static int     GetTuningParams(int mplexid, QString &modulation);

    // Service Stuff
    static bool    CreateChannel(uint db_mplexid,
                                 uint db_sourceid,
                                 uint new_channel_id,
                                 QString service_name,
                                 QString chan_num,
                                 uint service_id,
                                 uint atsc_major_channel,
                                 uint atsc_minor_channel,
                                 bool use_on_air_guide,
                                 bool hidden,
                                 bool hidden_in_guide,
                                 int  freqid);
    static bool    UpdateChannel(uint db_mplexid,
                                 uint source_id,
                                 uint channel_id,
                                 QString service_name,
                                 QString chan_num,
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
    static QString GetChanNum(int chanid);
    static int     GetSourceID(int mplexid);
    static QString GetInputName(int sourceid);


  //private:
    static int     FindUnusedChannelID(int sourceid);
};

#endif // CHANUTIL_H
