// -*- Mode: c++ -*-
/*
 * Copyright 2006 (C) Stuart Auchterlonie <stuarta at squashedfrog.net>
 * Copyright 2006 (C) Janne Grunau <janne-mythtv at grunau.be>
 * License: GPL v2
 */

#include <QDateTime>

#include "eitcache.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "mythdate.h"

#define LOC QString("EITCache: ")

// Highest version number. version is 5bits
const uint EITCache::kVersionMax = 31;

EITCache::EITCache()
    : accessCnt(0), hitCnt(0),   tblChgCnt(0),   verChgCnt(0),
      entryCnt(0), pruneCnt(0), prunedHitCnt(0), wrongChannelHitCnt(0)
{
    // 24 hours ago
    lastPruneTime = MythDate::current().toUTC().toTime_t() - 86400;
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

static void replace_in_db(uint chanid, uint eventid, uint64_t sig)
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
        MythDB::DBError("Error updating eitcache", query);

    return;
}

static void delete_in_db(uint endtime)
{
    LOG(VB_EIT, LOG_INFO, LOC + "Deleting old cache entries from the database");
    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr =
        "DELETE FROM eit_cache "
        "WHERE endtime < :ENDTIME";

    query.prepare(qstr);
    query.bindValue(":ENDTIME", endtime);

    if (!query.exec())
        MythDB::DBError("Error deleting old eitcache entries.", query);

    return;
}

#define EITDATA      0
#define CHANNEL_LOCK 1
#define STATISTIC    2

static bool lock_channel(uint chanid, uint lastPruneTime)
{
    int lock = 1;
    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr = "SELECT COUNT(*) "
                   "FROM eit_cache "
                   "WHERE chanid  = :CHANID   AND "
                   "      endtime > :ENDTIME  AND "
                   "      status  = :STATUS";

    query.prepare(qstr);
    query.bindValue(":CHANID",   chanid);
    query.bindValue(":ENDTIME",  lastPruneTime);
    query.bindValue(":STATUS",   CHANNEL_LOCK);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Error checking for channel lock", query);
        return false;
    }

    if (query.next())
        lock = query.value(0).toInt();

    if (lock)
    {
        LOG(VB_EIT, LOG_INFO,
            LOC + QString("Ignoring channel %1 since it is locked.")
                .arg(chanid));
        return false;
    }
    else
    {
        uint now = MythDate::current().toTime_t();
        qstr = "INSERT INTO eit_cache "
               "       ( chanid,  endtime,  status) "
               "VALUES (:CHANID, :ENDTIME, :STATUS)";

        query.prepare(qstr);
        query.bindValue(":CHANID",   chanid);
        query.bindValue(":ENDTIME",  now);
        query.bindValue(":STATUS",   CHANNEL_LOCK);

        if (!query.exec())
        {
            MythDB::DBError("Error inserting channel lock", query);
            return false;
        }
    }

    return true;
}

static void unlock_channel(uint chanid, uint updated)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr =
        "DELETE FROM eit_cache "
        "WHERE chanid  = :CHANID   AND "
        "      status  = :STATUS";

    query.prepare(qstr);
    query.bindValue(":CHANID",  chanid);
    query.bindValue(":STATUS",  CHANNEL_LOCK);

    if (!query.exec())
        MythDB::DBError("Error deleting channel lock", query);

    // inserting statistics
    uint now = MythDate::current().toTime_t();
    qstr = "REPLACE INTO eit_cache "
           "       ( chanid,  eventid,  endtime,  status) "
           "VALUES (:CHANID, :EVENTID, :ENDTIME, :STATUS)";

    query.prepare(qstr);
    query.bindValue(":CHANID",   chanid);
    query.bindValue(":EVENTID",  updated);
    query.bindValue(":ENDTIME",  now);
    query.bindValue(":STATUS",   STATISTIC);

    if (!query.exec())
        MythDB::DBError("Error inserting eit statistics", query);
}


event_map_t * EITCache::LoadChannel(uint chanid)
{
    if (!lock_channel(chanid, lastPruneTime))
        return NULL;

    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr =
        "SELECT eventid,tableid,version,endtime "
        "FROM eit_cache "
        "WHERE chanid        = :CHANID   AND "
        "      endtime       > :ENDTIME  AND "
        "      status        = :STATUS";

    query.prepare(qstr);
    query.bindValue(":CHANID",   chanid);
    query.bindValue(":ENDTIME",  lastPruneTime);
    query.bindValue(":STATUS",   EITDATA);


    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Error loading eitcache", query);
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

    if (eventMap->size())
        LOG(VB_EIT, LOG_INFO, LOC + QString("Loaded %1 entries for channel %2")
                .arg(eventMap->size()).arg(chanid));

    entryCnt += eventMap->size();
    return eventMap;
}

void EITCache::WriteChannelToDB(uint chanid)
{
    event_map_t * eventMap = channelMap[chanid];

    if (!eventMap)
    {
        channelMap.remove(chanid);
        return;
    }

    uint size    = eventMap->size();
    uint updated = 0;

    event_map_t::iterator it = eventMap->begin();
    while (it != eventMap->end())
    {
        if (modified(*it) && extract_endtime(*it) > lastPruneTime)
        {
            replace_in_db(chanid, it.key(), *it);
            updated++;
            *it &= ~(uint64_t)0 >> 1; // mark as synced
        }
        ++it;
    }
    unlock_channel(chanid, updated);

    if (updated)
        LOG(VB_EIT, LOG_INFO, LOC + QString("Wrote %1 modified entries of %2 "
                                      "for channel %3 to database.")
                .arg(updated).arg(size).arg(chanid));
}

void EITCache::WriteToDB(void)
{
    QMutexLocker locker(&eventMapLock);

    key_map_t::iterator it = channelMap.begin();
    while (it != channelMap.end())
    {
        WriteChannelToDB(it.key());
        ++it;
    }
}



bool EITCache::IsNewEIT(uint chanid,  uint tableid,   uint version,
                        uint eventid, uint endtime)
{
    accessCnt++;

    if (accessCnt % 500000 == 50000)
    {
        LOG(VB_EIT, LOG_INFO, GetStatistics());
        WriteToDB();
    }

    // don't readd pruned entries
    if (endtime < lastPruneTime)
    {
        prunedHitCnt++;
        return false;
    }
    // validity check, reject events with endtime over 7 weeks in the future
    if (endtime > lastPruneTime + 50 * 86400)
        return false;

    QMutexLocker locker(&eventMapLock);
    if (!channelMap.contains(chanid))
    {
        channelMap[chanid] = LoadChannel(chanid);
    }

    if (!channelMap[chanid])
    {
        wrongChannelHitCnt++;
        return false;
    }

    event_map_t * eventMap = channelMap[chanid];
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
    if (VERBOSE_LEVEL_CHECK(VB_EIT, LOG_INFO))
    {
        QDateTime tmptime = MythDate::fromTime_t(timestamp);
        LOG(VB_EIT, LOG_INFO,
            LOC + "Pruning all entries that ended before UTC " +
            tmptime.toString(Qt::ISODate));
    }

    lastPruneTime  = timestamp;

    // Write all modified entries to DB and start with a clean cache
    WriteToDB();

    // Prune old entries in the DB
    delete_in_db(timestamp);

    return 0;
}


/** \fn EITCache::ClearChannelLocks(void)
 *  \brief removes old channel locks, use it only at master b<ackend start
 */
void EITCache::ClearChannelLocks(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr =
        "DELETE FROM eit_cache "
        "WHERE status  = :STATUS";

    query.prepare(qstr);
    query.bindValue(":STATUS",  CHANNEL_LOCK);

    if (!query.exec())
        MythDB::DBError("Error clearing channel locks", query);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
