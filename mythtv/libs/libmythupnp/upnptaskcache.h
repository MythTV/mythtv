//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskcache.h
// Created     : Jan. 9, 2007
//
// Purpose     : UPnp Task to keep SSDPCache free of stale records
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPTASKCACHE_H__
#define __UPNPTASKCACHE_H__

#include <QString>

#include "mythlogging.h"
#include "upnp.h"

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

        int     m_nInterval;        // Number of ms between executing.
        int     m_nExecuteCount;    // Used for debugging.

        // Destructor protected to force use of Release Method

        virtual ~SSDPCacheTask() {};

    public:

        SSDPCacheTask()
        {
            m_nExecuteCount = 0;
            m_nInterval     = 1000 *
              UPnp::GetConfiguration()->GetValue("UPnP/SSDP/CacheInterval", 30);
        }

        virtual QString Name   ()
        { 
            return( "SSDPCache" );
        }

        virtual void    Execute( TaskQueue *pQueue )
        {
            m_nExecuteCount++;

            int nCount = SSDPCache::Instance()->RemoveStale();

            if (nCount > 0)
                LOG(VB_UPNP, LOG_INFO,
                    QString("SSDPCacheTask - Removed %1 stale entries.")
                        .arg(nCount));

            if ((m_nExecuteCount % 60) == 0)
                SSDPCache::Instance()->Dump();
                                    
            pQueue->AddTask( m_nInterval, (Task *)this  );
        }

};

#endif
