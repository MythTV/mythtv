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
        grpid(other.grpid), name(other.name) {}
    ChannelGroupItem(const uint _grpid, const QString &_name) :
        grpid(_grpid), name(_name) {}

    bool operator == (uint _grpid) const
        { return grpid == _grpid; }

    ChannelGroupItem& operator=(const ChannelGroupItem&);

  public:
    uint    grpid;
    QString name;
};
typedef vector<ChannelGroupItem> ChannelGroupList;

/** \class ChannelGroup
*/
class MTV_PUBLIC ChannelGroup
{
    Q_DECLARE_TR_FUNCTIONS(ChannelGroup)

  public:
    // ChannelGroup 
    static ChannelGroupList  GetChannelGroups(bool includeEmpty = true);
    static bool              ToggleChannel(uint chanid, int changrpid, int delete_chan);
    static bool              AddChannel(uint chanid, int changrpid);
    static bool              DeleteChannel(uint chanid, int changrpid);
    static int               GetNextChannelGroup(const ChannelGroupList &sorted, int grpid);
    static QString           GetChannelGroupName(int grpid);
    static int               GetChannelGroupId(QString changroupname);

  private:

};

#endif
