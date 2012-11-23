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
        m_interactive(false),         m_nonUSUpdating(false),
        m_channelPreset(false),       m_channelUpdates(false),
        m_removeNewChannels(false),   m_filterNewChannels(true) {}

    bool insert_chan(uint sourceid);
    void handleChannels(int id, ChannelInfoList *chanlist);
    unsigned int promptForChannelUpdates(ChannelInfoList::iterator chaninfo,
                                         unsigned int chanid);

    ChannelInfo FindMatchingChannel(const ChannelInfo &chanInfo,
                            QHash<QString, ChannelInfo> existingChannels) const;
    QHash<QString, ChannelInfo> channelList(int sourceId);
    QString normalizeChannelKey(const QString &chanName) const;

  public:
    bool    m_interactive;
    bool    m_nonUSUpdating;
    bool    m_channelPreset;
    bool    m_channelUpdates;
    bool    m_removeNewChannels;
    bool    m_filterNewChannels;
    QString m_cardType;
};

#endif // _CHANNELDATA_H_
