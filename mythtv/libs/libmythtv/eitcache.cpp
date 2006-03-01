// -*- Mode: c++ -*-
/*
 * Copyright 2006 (C) Stuart Auchterlonie <stuarta at squashedfrog.net>
 * License: GPL v2
 */

#include "eitcache.h"
#include <stdio.h>

// Highest version number. version is 5bits
const uint EITCache::kVersionMax = 31;

EITCache::EITCache()
    : accessCnt(0), hitCnt(0), tblChgCnt(0), verChgCnt(0)
{
}

void EITCache::ResetStatistics(void)
{
    accessCnt = 0;
    hitCnt    = 0;
    tblChgCnt = 0;
    verChgCnt = 0;
}

QString EITCache::GetStatistics(void) const
{
    return QString(
        "EITCache::statistics: Accesses: %1, Hits: %2, "
        "Table Upgrades %3, New Versions: %4")
        .arg(accessCnt).arg(hitCnt).arg(tblChgCnt).arg(verChgCnt);
}

static uint64_t construct_key(uint tsid, uint eventid, uint serviceid)
{
    return (((uint64_t) tsid      << 48) | ((uint64_t) eventid   << 32) |
            ((uint64_t) serviceid << 16));
}

static uint64_t construct_sig(uint tableid, uint version, uint chksum)
{
    return (((uint64_t) tableid   << 40) | ((uint64_t) version   << 32) |
            ((uint64_t) chksum));
}

static uint extract_table_id(uint64_t sig)
{
    return (sig >> 40) & 0xff;
}

static uint extract_version(uint64_t sig)
{
    return (sig >> 32) & 0x1f;
}

bool EITCache::IsNewEIT(const uint tsid,      const uint eventid,
                        const uint serviceid, const uint tableid,
                        const uint version,
                        const unsigned char * const /*eitdata*/,
                        const uint /*eitlength*/)
{
    accessCnt++;

    uint64_t key = construct_key(tsid, eventid, serviceid);
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

    eventMap[key] = construct_sig(tableid, version, 0 /*chksum*/);

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
