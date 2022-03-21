#ifndef CHANNELDATA_H
#define CHANNELDATA_H

// Qt headers
#include <QString>

// MythTV
#include "libmythtv/channelinfo.h"

using ChannelList = QMultiHash<QString, ChannelInfo>;

class ChannelData
{
  public:
    ChannelData() = default;

    bool insert_chan(uint sourceid) const;
    void handleChannels(int id, ChannelInfoList *chanlist) const;
    unsigned int promptForChannelUpdates(ChannelInfoList::iterator chaninfo,
                                         unsigned int chanid) const;

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

#endif // CHANNELDATA_H
