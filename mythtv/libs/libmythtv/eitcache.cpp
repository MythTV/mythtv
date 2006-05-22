// -*- Mode: c++ -*-
/*
 * Copyright 2006 (C) Stuart Auchterlonie <stuarta at squashedfrog.net>
 * Copyright 2006 (C) Janne Grunau <janne-mythtv at grunau.be>
 * License: GPL v2
 */


#include <qdatetime.h>

#include "eitcache.h"
#include "mythcontext.h"

#define LOC QString("EITCache: ")

// Highest version number. version is 5bits
const uint EITCache::kVersionMax = 31;

EITCache::EITCache()
    : accessCnt(0), hitCnt(0),   tblChgCnt(0),
      verChgCnt(0), pruneCnt(0), prunedHitCnt(0)
{
    // 24 hours ago
    lastPruneTime = QDateTime::currentDateTime(Qt::UTC).toTime_t() - 86400;
}

void EITCache::ResetStatistics(void)
{
    accessCnt = 0;
    hitCnt    = 0;
    tblChgCnt = 0;
    verChgCnt = 0;
    pruneCnt  = 0;
    prunedHitCnt = 0;
}

QString EITCache::GetStatistics(void) const
{
    QMutexLocker locker(&eventMapLock);
    return QString(
        "EITCache::statistics: Accesses: %1, Hits: %2, "
        "Table Upgrades %3, New Versions: %4, Entries: %5 "
        "Pruned entries: %6, pruned Hits: %7.")
        .arg(accessCnt).arg(hitCnt).arg(tblChgCnt).arg(verChgCnt)
        .arg(eventMap.size()).arg(pruneCnt).arg(prunedHitCnt);
}

static uint64_t construct_key(uint networkid, uint tsid,
                              uint serviceid, uint eventid)
{
    return (((uint64_t) networkid << 48) | ((uint64_t) tsid    << 32) |
            ((uint64_t) serviceid << 16) | ((uint64_t) eventid      ));
}

static uint64_t construct_sig(uint tableid, uint version, uint endtime)
{
    return (((uint64_t) tableid   << 40) | ((uint64_t) version   << 32) |
            ((uint64_t) endtime));
}

static uint extract_table_id(uint64_t sig)
{
    return (sig >> 40) & 0xff;
}

static uint extract_version(uint64_t sig)
{
    return (sig >> 32) & 0x1f;
}

static uint extract_endtime(uint64_t sig)
{
    return sig & 0xffffffff;
}

bool EITCache::IsNewEIT(uint networkid, uint tsid,    uint serviceid,
                        uint tableid,   uint version,
                        uint eventid,   uint endtime)
{
    accessCnt++;

    // don't readd pruned entries
    if (endtime < lastPruneTime)
    {
        prunedHitCnt++;
        return false;
    }

    uint64_t key = construct_key(networkid, tsid, serviceid, eventid);

    QMutexLocker locker(&eventMapLock);
    key_map_t::const_iterator it = eventMap.find(key);

    if (it != eventMap.end())
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

    eventMap[key] = construct_sig(tableid, version, endtime);

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

    QMutexLocker locker(&eventMapLock);
    uint orig_size = eventMap.size();

    key_map_t::iterator it = eventMap.begin();
    while (it != eventMap.end())
    {
        if (extract_endtime(*it) < timestamp)
        {
            key_map_t::iterator tmp = it;
            ++tmp;
            eventMap.erase(it);
            it = tmp;
            continue;
        }
        ++it;
    }

    uint pruned    = orig_size - eventMap.size();
    prunedHitCnt  += pruned;
    lastPruneTime  = timestamp;

    return pruned;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
