// -*- Mode: c++ -*-
/*
 * Copyright 2006 (C) Stuart Auchterlonie <stuarta at squashedfrog.net>
 * Copyright 2006 (C) Janne Grunau <janne-mythtv at grunau.be>
 * License: GPL v2
 */

#include <QDateTime>

#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"

#include "eitcache.h"

#define LOC QString("EITCache: ")

// Highest version number. version is 5bits
const uint EITCache::kVersionMax = 31;

EITCache::EITCache()
{
    // 24 hours ago
    m_lastPruneTime = MythDate::current().toUTC().toSecsSinceEpoch() - 86400;
}

EITCache::~EITCache()
{
    WriteToDB();
}

void EITCache::ResetStatistics(void)
{
    m_accessCnt = 0;
    m_hitCnt    = 0;
    m_tblChgCnt = 0;
    m_verChgCnt = 0;
    m_endChgCnt = 0;
    m_entryCnt  = 0;
    m_pruneCnt  = 0;
    m_prunedHitCnt = 0;
    m_futureHitCnt = 0;
    m_wrongChannelHitCnt = 0;
}

QString EITCache::GetStatistics(void) const
{
    QMutexLocker locker(&m_eventMapLock);
    return
        QString("Access:%1 ").arg(m_accessCnt) +
        QString("HitRatio:%1 ").arg((m_hitCnt+m_prunedHitCnt+m_futureHitCnt+m_wrongChannelHitCnt)/(double)m_accessCnt) +
        QString("Hits:%1 ").arg(m_hitCnt) +
        QString("Table:%1 ").arg(m_tblChgCnt) +
        QString("Version:%1 ").arg(m_verChgCnt) +
        QString("Endtime:%1 ").arg(m_endChgCnt) +
        QString("New:%1 ").arg(m_entryCnt) +
        QString("Pruned:%1 ").arg(m_pruneCnt) +
        QString("PrunedHits:%1 ").arg(m_prunedHitCnt) +
        QString("Future:%1 ").arg(m_futureHitCnt) +
        QString("WrongChannel:%1").arg(m_wrongChannelHitCnt);
}

/*
 * FIXME: This code has a builtin assumption that all timestamps will
 * fit into a 32bit integer.  Qt5.8 has switched to using a 64bit
 * integer for timestamps.
 */
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
    return (sig >> 63) != 0U;
}

static void replace_in_db(QStringList &value_clauses,
                          uint chanid, uint eventid, uint64_t sig)
{
    value_clauses << QString("(%1,%2,%3,%4,%5)")
        .arg(chanid).arg(eventid).arg(extract_table_id(sig))
        .arg(extract_version(sig)).arg(extract_endtime(sig));
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
}

enum channel_status : std::uint8_t
{
    EITDATA      = 0,
    CHANNEL_LOCK = 1,
    STATISTIC    = 2
};

static bool lock_channel(uint chanid, uint endtime)
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
    query.bindValue(":ENDTIME",  endtime);
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
    uint now = MythDate::current().toSecsSinceEpoch();
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
    uint now = MythDate::current().toSecsSinceEpoch();
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
    // Event map is empty when we do not backup the cache in the database
    if (!m_persistent)
    {
        return new event_map_t();
    }

    if (!lock_channel(chanid, m_lastPruneTime))
        return nullptr;

    MSqlQuery query(MSqlQuery::InitCon());

    QString qstr =
        "SELECT eventid,tableid,version,endtime "
        "FROM eit_cache "
        "WHERE chanid        = :CHANID   AND "
        "      endtime       > :ENDTIME  AND "
        "      status        = :STATUS";

    query.prepare(qstr);
    query.bindValue(":CHANID",   chanid);
    query.bindValue(":ENDTIME",  m_lastPruneTime);
    query.bindValue(":STATUS",   EITDATA);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Error loading eitcache", query);
        return nullptr;
    }

    auto *eventMap = new event_map_t();

    while (query.next())
    {
        uint eventid = query.value(0).toUInt();
        uint tableid = query.value(1).toUInt();
        uint version = query.value(2).toUInt();
        uint endtime = query.value(3).toUInt();

        (*eventMap)[eventid] = construct_sig(tableid, version, endtime, false);
    }

    if (!eventMap->empty())
        LOG(VB_EIT, LOG_DEBUG, LOC + QString("Loaded %1 entries for chanid %2")
                .arg(eventMap->size()).arg(chanid));

    m_entryCnt += eventMap->size();
    return eventMap;
}

bool EITCache::WriteChannelToDB(QStringList &value_clauses, uint chanid)
{
    event_map_t * eventMap = m_channelMap[chanid];

    if (!eventMap)
        return false;

    uint size    = eventMap->size();
    uint updated = 0;
    uint removed = 0;

    event_map_t::iterator it = eventMap->begin();
    while (it != eventMap->end())
    {
        if (extract_endtime(*it) > m_lastPruneTime)
        {
            if (modified(*it))
            {
                if (m_persistent)
                {
                    replace_in_db(value_clauses, chanid, it.key(), *it);
                }

                updated++;
                *it &= ~(uint64_t)0 >> 1; // Mark as synced
            }
            ++it;
        }
        else
        {
            // Event is too old; remove from eit cache in memory
            it = eventMap->erase(it);
            removed++;
        }
    }

    if (m_persistent)
    {
        unlock_channel(chanid, updated);
    }

    if (updated)
    {
        if (m_persistent)
        {
            LOG(VB_EIT, LOG_DEBUG, LOC +
                QString("Writing %1 modified entries of %2 for chanid %3 to database.")
                    .arg(updated).arg(size).arg(chanid));
        }
        else
        {
            LOG(VB_EIT, LOG_DEBUG, LOC +
                QString("Updated %1 modified entries of %2 for chanid %3 in cache.")
                    .arg(updated).arg(size).arg(chanid));
        }
    }
    if (removed)
    {
        LOG(VB_EIT, LOG_DEBUG, LOC + QString("Removed %1 old entries of %2 "
                                      "for chanid %3 from cache.")
                .arg(removed).arg(size).arg(chanid));
    }
    m_pruneCnt += removed;

    return true;
}

void EITCache::WriteToDB(void)
{
    QMutexLocker locker(&m_eventMapLock);

    QStringList value_clauses;
    key_map_t::iterator it = m_channelMap.begin();
    while (it != m_channelMap.end())
    {
        if (!WriteChannelToDB(value_clauses, it.key()))
            it = m_channelMap.erase(it);
        else
            ++it;
    }

    if (m_persistent)
    {
        if(value_clauses.isEmpty())
        {
            return;
        }

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(QString("REPLACE INTO eit_cache "
                            "(chanid, eventid, tableid, version, endtime) "
                            "VALUES %1").arg(value_clauses.join(",")));
        if (!query.exec())
        {
            MythDB::DBError("Error updating eitcache", query);
        }
    }
}

bool EITCache::IsNewEIT(uint chanid,  uint tableid,   uint version,
                        uint eventid, uint endtime)
{
    m_accessCnt++;

    if (m_accessCnt %  10000 == 0)
    {
        LOG(VB_EIT, LOG_INFO, LOC + GetStatistics());
        ResetStatistics();
    }

    // Don't re-add pruned entries
    if (endtime < m_lastPruneTime)
    {
        m_prunedHitCnt++;
        return false;
    }

    // Validity check, reject events with endtime over 7 weeks in the future
    if (endtime > m_lastPruneTime + (50 * 86400))
    {
        m_futureHitCnt++;
        return false;
    }

    QMutexLocker locker(&m_eventMapLock);
    if (!m_channelMap.contains(chanid))
    {
        m_channelMap[chanid] = LoadChannel(chanid);
    }

    if (!m_channelMap[chanid])
    {
        m_wrongChannelHitCnt++;
        return false;
    }

    event_map_t * eventMap = m_channelMap[chanid];
    event_map_t::iterator it = eventMap->find(eventid);
    if (it != eventMap->end())
    {
        if (extract_table_id(*it) > tableid)
        {
            // EIT from lower (ie. better) table number
            m_tblChgCnt++;
        }
        else if ((extract_table_id(*it) == tableid) &&
                 (extract_version(*it) != version))
        {
            // EIT updated version on current table
            m_verChgCnt++;
        }
        else if (extract_endtime(*it) != endtime)
        {
            // Endtime (starttime + duration) changed
            m_endChgCnt++;
        }
        else
        {
            // EIT data previously seen
            m_hitCnt++;
            return false;
        }
    }

    eventMap->insert(eventid, construct_sig(tableid, version, endtime, true));
    m_entryCnt++;

    return true;
}

/** \fn EITCache::PruneOldEntries(uint timestamp)
 *  \brief Prunes entries that describe events ending before timestamp time.
 *  \return Number of entries pruned
 */
uint EITCache::PruneOldEntries(uint timestamp)
{
    if (VERBOSE_LEVEL_CHECK(VB_EIT, LOG_DEBUG))
    {
        QDateTime tmptime = MythDate::fromSecsSinceEpoch(timestamp);
        LOG(VB_EIT, LOG_INFO,
            LOC + "Pruning all entries that ended before UTC " +
            tmptime.toString(Qt::ISODate));
    }

    m_lastPruneTime  = timestamp;

    // Write all modified entries to DB and start with a clean cache
    WriteToDB();

    // Prune old entries in the DB
    if (m_persistent)
    {
        delete_in_db(timestamp);
    }

    return 0;
}


/** \fn EITCache::ClearChannelLocks(void)
 *  \brief Removes old channel locks, use it only at master backend start
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
