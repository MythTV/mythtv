// -*- Mode: c++ -*-
/*
 * Copyright 2006 (C) Stuart Auchterlonie <stuarta at squashedfrog.net>
 * Copyright 2006 (C) Janne Grunau <janne-mythtv at grunau.be>
 * License: GPL v2
 */


#include <qdatetime.h>

#include "eitcache.h"
#include "mythcontext.h"
#include "mythdbcon.h"

#define LOC QString("EITCache: ")

// Highest version number. version is 5bits
const uint EITCache::kVersionMax = 31;

EITCache::EITCache()
    : accessCnt(0), hitCnt(0),   tblChgCnt(0),   verChgCnt(0),
      entryCnt(0), pruneCnt(0), prunedHitCnt(0), wrongChannelHitCnt(0)
{
    // 24 hours ago
    lastPruneTime = QDateTime::currentDateTime(Qt::UTC).toTime_t() - 86400;
}

EITCache::~EITCache()
{
    WriteToDB();
}

void EITCache::ResetStatistics(void)
{
    accessCnt = 0;
    hitCnt    = 0;
    tblChgCnt = 0;
    verChgCnt = 0;
    entryCnt  = 0;
    pruneCnt  = 0;
    prunedHitCnt = 0;
    wrongChannelHitCnt = 0;
}

QString EITCache::GetStatistics(void) const
{
    QMutexLocker locker(&eventMapLock);
    return QString(
        "EITCache::statistics: Accesses: %1, Hits: %2, "
        "Table Upgrades %3, New Versions: %4, Entries: %5 "
        "Pruned entries: %6, pruned Hits: %7 Discard channel Hit %8 "
        "Hit Ratio %9.")
        .arg(accessCnt).arg(hitCnt).arg(tblChgCnt).arg(verChgCnt)
        .arg(entryCnt).arg(pruneCnt).arg(prunedHitCnt)
        .arg(wrongChannelHitCnt)
        .arg((hitCnt+prunedHitCnt+wrongChannelHitCnt)/(double)accessCnt);
}

static inline uint64_t construct_channel(uint networkid,
                                         uint tsid, uint serviceid)
{
    return (((uint64_t) networkid << 32) | ((uint64_t) tsid    << 16) |
            ((uint64_t) serviceid));
}

static inline uint extract_onid(uint64_t channel)
{
    return (channel >> 32) & 0xffff;
}

static inline uint extract_tsid(uint64_t channel)
{
    return (channel >> 16) & 0xffff;
}

static inline uint extrac_sid(uint64_t channel)
{
    return channel & 0xffff;
}

static inline uint64_t construct_sig(uint tableid, uint version,
                                     uint endtime, bool modified)
{
    return (((uint64_t) modified  << 63) | ((uint64_t) tableid   << 40) |
            ((uint64_t) version   << 32) | ((uint64_t) endtime));
}

static inline uint extract_table_id(uint64_t sig)
{
    return (sig >> 40) & 0xff;
}

static inline uint extract_version(uint64_t sig)
{
    return (sig >> 32) & 0x1f;
}

static inline uint extract_endtime(uint64_t sig)
{
    return sig & 0xffffffff;
}

static inline bool modified(uint64_t sig)
{
    return sig >> 63;
}

static int get_chanid_from_db(uint networkid, uint tsid, uint serviceid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // DVB Link to chanid
    QString qstr =
        "SELECT chanid, useonairguide "
        "FROM channel, dtv_multiplex "
        "WHERE serviceid        = :SERVICEID   AND "
        "      networkid        = :NETWORKID   AND "
        "      transportid      = :TRANSPORTID AND "
        "      channel.mplexid  = dtv_multiplex.mplexid";

    query.prepare(qstr);
    query.bindValue(":SERVICEID",   serviceid);
    query.bindValue(":NETWORKID",   networkid);
    query.bindValue(":TRANSPORTID", tsid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Looking up chanID", query);
    else if (query.next())
    {
        bool useOnAirGuide = query.value(1).toBool();
        return (useOnAirGuide) ? query.value(0).toInt() : -1;
    }
    return -1;
}

static void replace_in_db(int chanid, uint eventid, uint64_t sig)
{

    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr =
        "REPLACE INTO eit_cache "
        "       ( chanid,  eventid,  tableid,  version,  endtime) "
        "VALUES (:CHANID, :EVENTID, :TABLEID, :VERSION, :ENDTIME)";

    query.prepare(qstr);
    query.bindValue(":CHANID",   chanid);
    query.bindValue(":EVENTID",  eventid);
    query.bindValue(":TABLEID",  extract_table_id(sig));
    query.bindValue(":VERSION",  extract_version(sig));
    query.bindValue(":ENDTIME",  extract_endtime(sig));

    if (!query.exec())
        MythContext::DBError("Error updating eitcache", query);

    return;
}

static void delete_in_db(uint endtime)
{
    VERBOSE(VB_EIT, LOC + "Deleting old cache entries from the database");
    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr =
        "DELETE FROM eit_cache "
        "WHERE endtime < :ENDTIME";

    query.prepare(qstr);
    query.bindValue(":ENDTIME", endtime);

    if (!query.exec())
        MythContext::DBError("Error deleting old eitcache entries.", query);

    return;
}


event_map_t * EITCache::LoadChannel(uint networkid, uint tsid, uint serviceid)
{
    int chanid = get_chanid_from_db(networkid, tsid, serviceid);

    if (chanid < 1)
        return NULL;

    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr =
        "SELECT eventid,tableid,version,endtime "
        "FROM eit_cache "
        "WHERE chanid        = :CHANID   AND "
        "      endtime       > :ENDTIME";

    query.prepare(qstr);
    query.bindValue(":CHANID",   chanid);
    query.bindValue(":ENDTIME",  lastPruneTime);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Error loading eitcache", query);
        return NULL;
    }

    event_map_t * eventMap = new event_map_t();

    while (query.next())
    {
        uint eventid = query.value(0).toUInt();
        uint tableid = query.value(1).toUInt();
        uint version = query.value(2).toUInt();
        uint endtime = query.value(3).toUInt();

        (*eventMap)[eventid] = construct_sig(tableid, version, endtime, false);
    }

    VERBOSE(VB_EIT, LOC + QString("Loaded %1 entries for channel %2")
            .arg(eventMap->size()).arg(chanid));

    entryCnt += eventMap->size();
    return eventMap;
}

void EITCache::DropChannel(uint64_t channel)
{
    int chanid = get_chanid_from_db(extract_onid(channel),
                                    extract_tsid(channel),
                                    extrac_sid(channel));

    if (chanid < 1)
        return;

    event_map_t * eventMap = channelMap[channel];

    uint size    = eventMap->size();
    uint written = 0;

    event_map_t::iterator it = eventMap->begin();
    while (it != eventMap->end())
    {
        if (modified(*it) && extract_endtime(*it) > lastPruneTime)
        {
            replace_in_db(chanid, it.key(), *it);
            written++;
        }

        event_map_t::iterator tmp = it;
        ++tmp;
        eventMap->erase(it);
        it = tmp;
    }
    delete eventMap;
    channelMap.erase(channel);

    VERBOSE(VB_EIT, LOC + QString("Wrote %1 modified entries of %2 "
                                  "for channel %3 to database.")
            .arg(written).arg(size).arg(chanid));
    entryCnt -= size;
}

void EITCache::WriteToDB(void)
{
    QMutexLocker locker(&eventMapLock);

    key_map_t::iterator it = channelMap.begin();
    while (it != channelMap.end())
    {
        key_map_t::iterator tmp = it;
        ++tmp;
        DropChannel(it.key());
        it = tmp;
    }
    entryCnt = 0;
}



bool EITCache::IsNewEIT(uint networkid, uint tsid,    uint serviceid,
                        uint tableid,   uint version,
                        uint eventid,   uint endtime)
{
    accessCnt++;

    if (accessCnt % 500000 == 50000)
    {
        VERBOSE(VB_GENERAL, endl << GetStatistics());
        WriteToDB();
    }

    // don't readd pruned entries
    if (endtime < lastPruneTime)
    {
        prunedHitCnt++;
        return false;
    }

    uint64_t channel = construct_channel(networkid, tsid, serviceid);

    QMutexLocker locker(&eventMapLock);
    if (!channelMap.contains(channel))
    {
        channelMap[channel] = LoadChannel(networkid, tsid, serviceid);
    }

    if (!channelMap[channel])
    {
        wrongChannelHitCnt++;
        return false;
    }

    event_map_t * eventMap = channelMap[channel];
    event_map_t::iterator it = eventMap->find(eventid);
    if (it != eventMap->end())
    {
        if (extract_table_id(*it) > tableid)
        {
            // EIT from lower (ie. better) table number
            tblChgCnt++;
        }
        else if ((extract_table_id(*it) == tableid) &&
                 ((extract_version(*it) < version) ||
                  ((extract_version(*it) == kVersionMax) &&
                   version < kVersionMax)))
        {
            // EIT updated version on current table
            verChgCnt++;
        }
        else
        {
            // EIT data previously seen
            hitCnt++;
            return false;
        }
    }

    eventMap->insert(eventid, construct_sig(tableid, version, endtime, true));
    entryCnt++;

    return true;
}

/** \fn EITCache::PruneOldEntries(uint timestamp)
 *  \brief Prunes entries that describe events ending before timestamp time.
 *  \return number of entries pruned
 */
uint EITCache::PruneOldEntries(uint timestamp)
{
    if (print_verbose_messages & VB_EIT)
    {
        QDateTime tmptime;
        tmptime.setTime_t(timestamp);
        VERBOSE(VB_EIT, LOC + "Pruning all entries that ended before UTC " +
                tmptime.toString(Qt::ISODate));
    }

    lastPruneTime  = timestamp;

    // Write all modified entries to DB and start with a clean cache
    WriteToDB();

    // Prune old entries in the DB
    delete_in_db(timestamp);

    return 0;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
