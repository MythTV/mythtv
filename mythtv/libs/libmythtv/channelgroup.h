#ifndef CHANNELGROUP_H
#define CHANNELGROUP_H

// c/c++
#include <vector>
using namespace std;

// qt
#include <QString>
#include <QCoreApplication>

// mythtv
#include "mythtvexp.h"

class MTV_PUBLIC ChannelGroupItem
{
  public:
    ChannelGroupItem(const ChannelGroupItem &other) :
        m_grpid(other.m_grpid), m_name(other.m_name) {}
    ChannelGroupItem(const uint grpid, const QString &name) :
        m_grpid(grpid), m_name(name) {}

    bool operator == (uint grpid) const
        { return m_grpid == grpid; }

    ChannelGroupItem& operator=(const ChannelGroupItem&) = default;

  public:
    uint    m_grpid;
    QString m_name;
};
using ChannelGroupList = vector<ChannelGroupItem>;

/** \class ChannelGroup
*/
class MTV_PUBLIC ChannelGroup
{
    Q_DECLARE_TR_FUNCTIONS(ChannelGroup);

  public:
    // ChannelGroup 
    static ChannelGroupList  GetChannelGroups(bool includeEmpty = true);
    static bool              ToggleChannel(uint chanid, int changrpid, bool delete_chan);
    static bool              AddChannel(uint chanid, int changrpid);
    static bool              DeleteChannel(uint chanid, int changrpid);
    static int               GetNextChannelGroup(const ChannelGroupList &sorted, int grpid);
    static QString           GetChannelGroupName(int grpid);
    static int               GetChannelGroupId(const QString& changroupname);

  private:

};

#endif
