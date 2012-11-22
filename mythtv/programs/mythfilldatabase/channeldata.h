#ifndef _CHANNELDATA_H_
#define _CHANNELDATA_H_

// Qt headers
#include <QString>

// libmythtv
#include "channelinfo.h"

class ChannelData
{
  public:
    ChannelData() :
        interactive(false),         non_us_updating(false),
        channel_preset(false),      channel_updates(false),
        remove_new_channels(false), filter_new_channels(true) {}

    bool insert_chan(uint sourceid);
    void handleChannels(int id, ChannelInfoList *chanlist);
    unsigned int promptForChannelUpdates(
        ChannelInfoList::iterator chaninfo, unsigned int chanid);

  public:
    bool    interactive;
    bool    non_us_updating;
    bool    channel_preset;
    bool    channel_updates;
    bool    remove_new_channels;
    bool    filter_new_channels;
    QString cardtype;
};

#endif // _CHANNELDATA_H_
