#include "dtvchannel.h"

// Qt headers
#include <qmap.h>
#include <qstring.h>
#include <qsqldatabase.h>

// MythTV headers
#include "mythcontext.h"
#include "mythdbcon.h"

#define LOC QString("DTVChan(%1): ").arg(GetDevice())
#define LOC_WARN QString("DTVChan(%1) Warning: ").arg(GetDevice())
#define LOC_ERR QString("DTVChan(%1) Error: ").arg(GetDevice())

DTVChannel::DTVChannel(TVRec *parent)
    : ChannelBase(parent),
      sistandard("mpeg"),         currentProgramNum(-1),
      currentATSCMajorChannel(0), currentATSCMinorChannel(0),
      currentTransportID(0),      currentOriginalNetworkID(0)
{
}

/** \fn DTVChannel::GetCachedPids(int, pid_cache_t&)
 *  \brief Returns cached MPEG PIDs when given a Channel ID.
 *
 *  \param chanid   Channel ID to fetch cached pids for.
 *  \param pid_cache List of PIDs with their TableID
 *                   types is returned in pid_cache.
 */
void DTVChannel::GetCachedPids(int chanid, pid_cache_t &pid_cache)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString("SELECT pid, tableid FROM pidcache "
                               "WHERE chanid='%1'").arg(chanid);
    query.prepare(thequery);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetCachedPids: fetching pids", query);
        return;
    }
    
    while (query.next())
    {
        int pid = query.value(0).toInt(), tid = query.value(1).toInt();
        if ((pid >= 0) && (tid >= 0))
            pid_cache.push_back(pid_cache_item_t(pid, tid));
    }
}

/** \fn DTVChannel::SaveCachedPids(int, const pid_cache_t&)
 *  \brief Saves PIDs for PSIP tables to database.
 *
 *  \param chanid    Channel ID to fetch cached pids for.
 *  \param pid_cache List of PIDs with their TableID types to be saved.
 */
void DTVChannel::SaveCachedPids(int chanid, const pid_cache_t &pid_cache)
{
    MSqlQuery query(MSqlQuery::InitCon());

    /// delete
    QString thequery =
        QString("DELETE FROM pidcache WHERE chanid='%1'").arg(chanid);
    query.prepare(thequery);
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("GetCachedPids -- delete", query);
        return;
    }

    /// insert
    pid_cache_t::const_iterator it = pid_cache.begin();
    for (; it != pid_cache.end(); ++it)
    {
        thequery = QString("INSERT INTO pidcache "
                           "SET chanid='%1', pid='%2', tableid='%3'")
            .arg(chanid).arg(it->first).arg(it->second);

        query.prepare(thequery);

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("GetCachedPids -- insert", query);
            return;
        }
    }
}

void DTVChannel::SetCachedATSCInfo(const QString &chan)
{
    int progsep = chan.find("-");
    int chansep = chan.find("_");

    currentProgramNum        = -1;
    currentOriginalNetworkID = 0;
    currentTransportID       = 0;
    currentATSCMajorChannel  = 0;
    currentATSCMinorChannel  = 0;

    if (progsep >= 0)
    {
        currentProgramNum = chan.right(chan.length() - progsep - 1).toInt();
        currentATSCMajorChannel = chan.left(progsep).toInt();
    }
    else if (chansep >= 0)
    {
        currentATSCMinorChannel =
            chan.right(chan.length() - chansep - 1).toInt();
        currentATSCMajorChannel = chan.left(chansep).toInt();
    }
    else
    {
        bool ok;
        int chanNum = chan.toInt(&ok);
        if (ok && chanNum >= 10)
        {
            currentATSCMinorChannel = chanNum % 10;
            currentATSCMajorChannel = chanNum / 10;
        }
    }

    if (currentATSCMinorChannel > 0)
    {
        VERBOSE(VB_CHANNEL, LOC +
                QString("SetCachedATSCInfo(%2): %3_%4").arg(chan)
                .arg(currentATSCMajorChannel).arg(currentATSCMinorChannel));
    }
    else if ((0 == currentATSCMajorChannel) && (0 == currentProgramNum))
    {
        VERBOSE(VB_CHANNEL, LOC +
                QString("SetCachedATSCInfo(%2): RESET").arg(chan));
    }
    else
    {
        VERBOSE(VB_CHANNEL, LOC +
                QString("SetCachedATSCInfo(%2): %3-%4").arg(chan)
                .arg(currentATSCMajorChannel).arg(currentProgramNum));
    }
}

QString DTVChannel::GetSIStandard(void) const
{
    QMutexLocker locker(&dtvinfo_lock);
    return QDeepCopy<QString>(sistandard);
}

void DTVChannel::SetSIStandard(const QString &si_std)
{
    QMutexLocker locker(&dtvinfo_lock);
    sistandard = QDeepCopy<QString>(si_std);
}
