/* -*- Mode: c++ -*-
 * Copyright 2006 (C) Stuart Auchterlonie <stuarta at squashedfrog.net>
 * License: GPL v2
 */

#ifndef EIT_CACHE_H
#define EIT_CACHE_H

#include <cstdint>

// Qt headers
#include <QString>
#include <QMutex>
#include <QMap>

// MythTV headers
#include "mythtvexp.h"

using event_map_t = QMap<uint, uint64_t>;
using key_map_t = QMap<uint, event_map_t*>;

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
    bool WriteChannelToDB(QStringList &value_clauses, uint chanid);

    // Event key cache
    key_map_t      m_channelMap;

    mutable QMutex m_eventMapLock;
    uint           m_lastPruneTime;

    // Cache persistency in database table eit_cache
    bool           m_persistent         {true};

    // Statistics
    uint           m_accessCnt          {0};
    uint           m_hitCnt             {0};
    uint           m_tblChgCnt          {0};
    uint           m_verChgCnt          {0};
    uint           m_endChgCnt          {0};
    uint           m_entryCnt           {0};
    uint           m_pruneCnt           {0};
    uint           m_prunedHitCnt       {0};
    uint           m_futureHitCnt       {0};
    uint           m_wrongChannelHitCnt {0};

    static const uint kVersionMax;

  public:
    static MTV_PUBLIC void ClearChannelLocks(void);
    void SetPersistent(bool persistent) { m_persistent = persistent; }
};

#endif // EIT_CACHE_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
