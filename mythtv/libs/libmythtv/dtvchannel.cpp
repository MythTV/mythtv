#include "dtvchannel.h"

// MythTV headers
#include "mythdb.h"
#include "cardutil.h"

#define LOC QString("DTVChan(%1): ").arg(GetDevice())
#define LOC_WARN QString("DTVChan(%1) Warning: ").arg(GetDevice())
#define LOC_ERR QString("DTVChan(%1) Error: ").arg(GetDevice())

QMutex                    DTVChannel::master_map_lock;
QMap<QString,DTVChannel*> DTVChannel::master_map;

DTVChannel::DTVChannel(TVRec *parent)
    : ChannelBase(parent),
      tunerType(DTVTunerType::kTunerTypeUnknown),
      sistandard("mpeg"),         tuningMode(QString::null),
      currentProgramNum(-1),
      currentATSCMajorChannel(0), currentATSCMinorChannel(0),
      currentTransportID(0),      currentOriginalNetworkID(0)
{
}

DTVChannel::~DTVChannel()
{
    QMutexLocker locker(&master_map_lock);
    QMap<QString,DTVChannel*>::iterator it = master_map.begin();
    for (; it != master_map.end(); ++it)
    {
        if (*it == this)
        {
            master_map.erase(it);
            break;
        }
    }
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
        MythDB::DBError("GetCachedPids: fetching pids", query);
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
        MythDB::DBError("GetCachedPids -- delete", query);
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
            MythDB::DBError("GetCachedPids -- insert", query);
            return;
        }
    }
}

void DTVChannel::SetDTVInfo(uint atsc_major, uint atsc_minor,
                            uint dvb_orig_netid,
                            uint mpeg_tsid, int mpeg_pnum)
{
    QMutexLocker locker(&dtvinfo_lock);
    currentProgramNum        = mpeg_pnum;
    currentATSCMajorChannel  = atsc_major;
    currentATSCMinorChannel  = atsc_minor;
    currentTransportID       = mpeg_tsid;
    currentOriginalNetworkID = dvb_orig_netid;
}

QString DTVChannel::GetSIStandard(void) const
{
    QMutexLocker locker(&dtvinfo_lock);
    QString tmp = sistandard; tmp.detach();
    return tmp;
}

void DTVChannel::SetSIStandard(const QString &si_std)
{
    QMutexLocker locker(&dtvinfo_lock);
    sistandard = si_std.toLower();
    sistandard.detach();
}

QString DTVChannel::GetSuggestedTuningMode(bool is_live_tv) const
{
    uint cardid = GetCardID();
    QString input = GetCurrentInput();

    uint quickTuning = 0;
    if (cardid && !input.isEmpty())
        quickTuning = CardUtil::GetQuickTuning(cardid, input);

    bool useQuickTuning = (quickTuning && is_live_tv) || (quickTuning > 1);

    QMutexLocker locker(&dtvinfo_lock);
    if (!useQuickTuning && ((sistandard == "atsc") || (sistandard == "dvb")))
    {
        QString tmp = sistandard; tmp.detach();
        return tmp;
    }

    return "mpeg";
}

QString DTVChannel::GetTuningMode(void) const
{
    QMutexLocker locker(&dtvinfo_lock);
    QString tmp = tuningMode; tmp.detach();
    return tmp;
}

vector<DTVTunerType> DTVChannel::GetTunerTypes(void) const
{
    vector<DTVTunerType> tts;
    if (tunerType != DTVTunerType::kTunerTypeUnknown)
        tts.push_back(tunerType);
    return tts;
}

void DTVChannel::SetTuningMode(const QString &tuning_mode)
{
    QMutexLocker locker(&dtvinfo_lock);
    tuningMode = tuning_mode.toLower();
    tuningMode.detach();
}

DTVChannel *DTVChannel::GetMaster(const QString &videodevice)
{
    QMutexLocker locker(&master_map_lock);

    QMap<QString,DTVChannel*>::iterator it = master_map.find(videodevice);
    if (it != master_map.end())
        return *it;

    QString tmp = videodevice; tmp.detach();
    master_map[tmp] = this;

    return this;
}

const DTVChannel *DTVChannel::GetMaster(const QString &videodevice) const
{
    QMutexLocker locker(&master_map_lock);

    QMap<QString,DTVChannel*>::iterator it = master_map.find(videodevice);
    if (it != master_map.end())
        return *it;

    QString tmp = videodevice; tmp.detach();
    master_map[tmp] = (DTVChannel*) this;

    return this;
}
