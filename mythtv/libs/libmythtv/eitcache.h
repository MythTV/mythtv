/* -*- Mode: c++ -*-
 * Copyright 2006 (C) Stuart Auchterlonie <stuarta at squashedfrog.net>
 * License: GPL v2
 */

#ifndef _EIT_CACHE_H
#define _EIT_CACHE_H

#include <stdint.h>

// Qt headers
#include <qmap.h>
#include <qmutex.h>
#include <qstring.h>

typedef QMap<uint64_t, uint64_t> key_map_t;

class EITCache
{
  public:
    EITCache();
   ~EITCache() {};

    bool IsNewEIT(uint networkid, uint tsid,    uint serviceid,
                  uint tableid,   uint version,
                  uint eventid,   uint endtime);

    uint PruneOldEntries(uint utc_timestamp);

    void ResetStatistics(void);
    QString GetStatistics(void) const;

  private:
    // event key cache
    key_map_t   eventMap;

    mutable QMutex eventMapLock;
    uint            lastPruneTime;

    // statistics
    uint        accessCnt;
    uint        hitCnt;
    uint        tblChgCnt;
    uint        verChgCnt;
    uint        pruneCnt;
    uint        prunedHitCnt;

    static const uint kVersionMax;
};

#endif // _EIT_CACHE_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
