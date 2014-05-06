/* -*- Mode: c++ -*-
 * Copyright 2006 (C) Stuart Auchterlonie <stuarta at squashedfrog.net>
 * License: GPL v2
 */

#ifndef _EIT_CACHE_H
#define _EIT_CACHE_H

#include <stdint.h>

// Qt headers
#include <QString>
#include <QMutex>
#include <QMap>

// MythTV headers
#include "mythtvexp.h"

typedef QMap<uint, uint64_t> event_map_t;
typedef QMap<uint, event_map_t*> key_map_t;

class EITCache
{
  public:
    EITCache();
   ~EITCache();

    bool IsNewEIT(uint chanid, uint tableid,   uint version,
                  uint eventid,   uint endtime);

    uint PruneOldEntries(uint utc_timestamp);
    void WriteToDB(void);

    void ResetStatistics(void);
    QString GetStatistics(void) const;

  private:
    event_map_t * LoadChannel(uint chanid);
    void WriteChannelToDB(uint chanid);

    // event key cache
    key_map_t   channelMap;

    mutable QMutex eventMapLock;
    uint            lastPruneTime;

    // statistics
    uint        accessCnt;
    uint        hitCnt;
    uint        tblChgCnt;
    uint        verChgCnt;
    uint        endChgCnt;
    uint        entryCnt;
    uint        pruneCnt;
    uint        prunedHitCnt;
    uint        futureHitCnt;
    uint        wrongChannelHitCnt;

    static const uint kVersionMax;

  public:
    static MTV_PUBLIC void ClearChannelLocks(void);
};

#endif // _EIT_CACHE_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
