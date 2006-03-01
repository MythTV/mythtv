/*
 * Copyright 2006 (C) Stuart Auchterlonie <stuarta at squashedfrog.net>
 * License: GPL v2
 */

#ifndef _EIT_CACHE_H
#define _EIT_CACHE_H

#include <stdint.h>

// Qt headers
#include <qmap.h>
#include <qstring.h>

typedef QMap<uint64_t, uint64_t> key_map_t;

class EITCache
{
  public:
    EITCache();
   ~EITCache() {};

    bool IsNewEIT(const uint tsid,       const uint eventid,
                  const uint serviceid,  const uint tableid,
                  const uint version,
                  const unsigned char * const eitdata,
                  const uint eitlength);

    void ResetStatistics(void);
    QString GetStatistics(void) const;

  private:
    // event key cache
    key_map_t   eventMap;

    // statistics
    uint        accessCnt;
    uint        hitCnt;
    uint        tblChgCnt;
    uint        verChgCnt;

    static const uint kVersionMax;
};

#endif // _EIT_CACHE_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
