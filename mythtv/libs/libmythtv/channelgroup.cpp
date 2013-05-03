// c++
#include <algorithm>

// mythtv
#include "mythlogging.h"
#include "mythdb.h"
#include "channelgroup.h"

#define LOC QString("Channel Group: ")

ChannelGroupItem& ChannelGroupItem::operator=(const ChannelGroupItem &other)
{
    grpid     = other.grpid;
    name      = (other.name);

    return *this;
}

inline bool lt_group(const ChannelGroupItem &a, const ChannelGroupItem &b)
{
    return QString::localeAwareCompare(a.name, b.name) < 0;
}

bool ChannelGroup::ToggleChannel(uint chanid, int changrpid, int delete_chan)
{
    // Check if it already exists for that chanid...
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT channelgroup.id "
        "FROM channelgroup "
        "WHERE channelgroup.chanid = :CHANID AND "
        "channelgroup.grpid = :GRPID "
        "LIMIT 1");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":GRPID", changrpid);

    if (!query.exec())
    {
        MythDB::DBError("ChannelGroup::ToggleChannel", query);
        return false;
    }
    else if (query.next() && delete_chan)
    {
        // We have a record...Remove it to toggle...
        QString id = query.value(0).toString();
        query.prepare("DELETE FROM channelgroup WHERE id = :CHANID");
        query.bindValue(":CHANID", id);
        if (!query.exec())
            MythDB::DBError("ChannelGroup::ToggleChannel -- delete", query);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Removing channel with id=%1.").arg(id));
    }
    else if (query.size() == 0)
    {
        // We have no record...Add one to toggle...
        query.prepare("INSERT INTO channelgroup (chanid,grpid) "
                      "VALUES (:CHANID, :GRPID)");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":GRPID", changrpid);
        if (!query.exec())
            MythDB::DBError("ChannelGroup::ToggleChannel -- insert", query);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Adding channel %1 to group %2.")
                 .arg(chanid).arg(changrpid));
    }

    return true;
}

bool ChannelGroup::AddChannel(uint chanid, int changrpid)
{
    // Check if it already exists for that chanid...
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT channelgroup.id "
        "FROM channelgroup "
        "WHERE channelgroup.chanid = :CHANID AND "
        "channelgroup.grpid = :GRPID "
        "LIMIT 1");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":GRPID", changrpid);

    if (!query.exec())
    {
        MythDB::DBError("ChannelGroup::AddChannel", query);
        return false;
    }
    else if (query.size() == 0)
    {
        // We have no record...Add one to toggle...
        query.prepare("INSERT INTO channelgroup (chanid,grpid) "
                      "VALUES (:CHANID, :GRPID)");
        query.bindValue(":CHANID", chanid);
        query.bindValue(":GRPID", changrpid);
        if (!query.exec())
            MythDB::DBError("ChannelGroup::AddChannel -- insert", query);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Adding channel %1 to group %2.")
                 .arg(chanid).arg(changrpid));
    }

    return true;
}

bool ChannelGroup::DeleteChannel(uint chanid, int changrpid)
{
    // Check if it already exists for that chanid...
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT channelgroup.id "
        "FROM channelgroup "
        "WHERE channelgroup.chanid = :CHANID AND "
        "channelgroup.grpid = :GRPID "
        "LIMIT 1");
    query.bindValue(":CHANID", chanid);
    query.bindValue(":GRPID", changrpid);

    if (!query.exec())
    {
        MythDB::DBError("ChannelGroup::DeleteChannel", query);
        return false;
    }
    else if (query.next())
    {
        // We have a record...Remove it to toggle...
        QString id = query.value(0).toString();
        query.prepare("DELETE FROM channelgroup WHERE id = :CHANID");
        query.bindValue(":CHANID", id);
        if (!query.exec())
            MythDB::DBError("ChannelGroup::DeleteChannel -- delete", query);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Removing channel with id=%1.").arg(id));
    }

    return true;
}

ChannelGroupList ChannelGroup::GetChannelGroups(bool includeEmpty)
{
    ChannelGroupList list;

    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr;

    if (includeEmpty)
        qstr = "SELECT grpid, name FROM channelgroupnames ORDER BY name";
    else
        qstr = "SELECT DISTINCT t1.grpid, name FROM channelgroupnames t1,channelgroup t2 "
               "WHERE t1.grpid = t2.grpid ORDER BY name";

    query.prepare(qstr);

    if (!query.exec())
        MythDB::DBError("ChannelGroup::GetChannelGroups", query);
    else
    {
        while (query.next())
        {
           ChannelGroupItem group(query.value(0).toUInt(),
                              query.value(1).toString());
           list.push_back(group);
        }
    }

    return list;
}

// Cycle through the available groups, then all channels
// Will cycle through to end then return -1
// To signify all channels.
int ChannelGroup::GetNextChannelGroup(const ChannelGroupList &sorted, int grpid)
{
    // If no groups return -1 for all channels
    if (sorted.empty())
      return -1;

    // If grpid is all channels (-1), then return the first grpid
    if (grpid == -1)
      return sorted[0].grpid;

    ChannelGroupList::const_iterator it = find(sorted.begin(), sorted.end(), grpid);

    // If grpid is not in the list, return -1 for all channels
    if (it == sorted.end())
        return -1;

    ++it;

    // If we reached the end, the next option is all channels (-1)
    if (it == sorted.end())
       return -1;

    return it->grpid;
}

QString ChannelGroup::GetChannelGroupName(int grpid)
{
    // All Channels
    if (grpid == -1)
        return QObject::tr("All Channels");

    // No group
    if (grpid == 0)
        return "";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT name FROM channelgroupnames WHERE grpid = :GROUPID");
    query.bindValue(":GROUPID", grpid);

    if (!query.exec())
        MythDB::DBError("ChannelGroup::GetChannelGroups", query);
    else if (query.next())
        return query.value(0).toString();

    return "";
}

int ChannelGroup::GetChannelGroupId(QString changroupname)
{
    // All Channels
    if (changroupname == "All Channels")
      return -1;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT grpid FROM channelgroupnames "
                  "WHERE name = :GROUPNAME");
    query.bindValue(":GROUPNAME", changroupname);

    if (!query.exec())
        MythDB::DBError("ChannelGroup::GetChannelGroups", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}
