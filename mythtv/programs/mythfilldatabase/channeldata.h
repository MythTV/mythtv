#ifndef _CHANNELDATA_H_
#define _CHANNELDATA_H_

// Qt headers
#include <QString>

// libmythtv
#include "channelinfo.h"

using ChannelList = QMultiHash<QString, ChannelInfo>;

class ChannelData
{
  public:
    ChannelData() = default;

    bool insert_chan(uint sourceid);
    void handleChannels(int id, ChannelInfoList *chanlist);
    unsigned int promptForChannelUpdates(ChannelInfoList::iterator chaninfo,
                                         unsigned int chanid);

    static ChannelInfo FindMatchingChannel(const ChannelInfo &chanInfo,
                            ChannelList existingChannels);
    static ChannelList channelList(int sourceId);
    static QString normalizeChannelKey(const QString &chanName);

  public:
    bool    m_interactive       {false};
    bool    m_guideDataOnly     {false};
    bool    m_channelPreset     {false};
    bool    m_channelUpdates    {false};
    bool    m_filterNewChannels {false};
    QString m_cardType;
};

#endif // _CHANNELDATA_H_
