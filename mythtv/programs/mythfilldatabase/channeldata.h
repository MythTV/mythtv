#ifndef _CHANNELDATA_H_
#define _CHANNELDATA_H_

// Qt headers
#include <qstring.h>
#include <qvaluelist.h>

class ChanInfo
{
  public:
    ChanInfo() { }
    ChanInfo(const ChanInfo &other) { callsign = other.callsign; 
                                      iconpath = other.iconpath;
                                      chanstr = other.chanstr;
                                      xmltvid = other.xmltvid;
                                      old_xmltvid = other.old_xmltvid;
                                      name = other.name;
                                      freqid = other.freqid;
                                      finetune = other.finetune;
                                      tvformat = other.tvformat;
                                    }

    QString callsign;
    QString iconpath;
    QString chanstr;
    QString xmltvid;
    QString old_xmltvid;
    QString name;
    QString freqid;
    QString finetune;
    QString tvformat;
};

class ChannelData
{
  public:
    ChannelData() :
        quiet(false),               interactive(false),
        non_us_updating(false),     channel_preset(false),
        channel_updates(false),     remove_new_channels(false),
        cardtype(QString::null) {}

    bool insert_chan(uint sourceid);
    void handleChannels(int id, QValueList<ChanInfo> *chanlist);
    unsigned int promptForChannelUpdates(
        QValueList<ChanInfo>::iterator chaninfo, unsigned int chanid);

  public:
    bool    quiet;
    bool    interactive;
    bool    non_us_updating;
    bool    channel_preset;
    bool    channel_updates;
    bool    remove_new_channels;
    QString cardtype;
};

#endif // _CHANNELDATA_H_
