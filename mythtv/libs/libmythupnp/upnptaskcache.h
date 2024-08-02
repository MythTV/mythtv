//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskcache.h
// Created     : Jan. 9, 2007
//
// Purpose     : UPnp Task to keep SSDPCache free of stale records
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPNPTASKCACHE_H
#define UPNPTASKCACHE_H

#include <QString>

#include "libmythbase/mythlogging.h"

#include "libmythupnp/upnp.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPCacheTask Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SSDPCacheTask : public Task
{
    protected:

        std::chrono::milliseconds m_nInterval     {30s}; // Number of ms between executing.
        int                       m_nExecuteCount {0};   // Used for debugging.

        // Destructor protected to force use of Release Method

        ~SSDPCacheTask() override = default;

    public:

        SSDPCacheTask() : Task("SSDPCacheTask")
        {
            m_nInterval     = 30s;
// TODO: Rework when separating upnp/ssdp stack
//               XmlConfiguration().GetDuration<std::chrono::seconds>("UPnP/SSDP/CacheInterval", 30s);
        }

        QString Name() override // Task
        { 
            return( "SSDPCache" );
        }

        void Execute( TaskQueue *pQueue ) override // Task
        {
            m_nExecuteCount++;

            int nCount = SSDPCache::Instance()->RemoveStale();

            if (nCount > 0)
            {
                LOG(VB_UPNP, LOG_INFO,
                    QString("SSDPCacheTask - Removed %1 stale entries.")
                        .arg(nCount));
            }

            if ((m_nExecuteCount % 60) == 0)
                SSDPCache::Instance()->Dump();
                                    
            pQueue->AddTask( m_nInterval, (Task *)this  );
        }

};

#endif // UPNPTASKCACHE_H
