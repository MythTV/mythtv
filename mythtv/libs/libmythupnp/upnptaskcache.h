//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskcache.h
//                                                                            
// Purpose - UPnp Task to keep SSDPCache free of stale records
//                                                                            
// Created By  : David Blain                    Created On : Jan. 9, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPTASKCACHE_H__
#define __UPNPTASKCACHE_H__

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
            m_nInterval     = UPnp::g_pConfig->GetValue( "UPnP/SSDP/CacheInterval", 30 ) * 1000;

        }

        virtual QString Name   ()
        { 
            return( "SSDPCache" );
        }

        virtual void    Execute( TaskQueue *pQueue )
        {
            m_nExecuteCount++;

            int nCount = UPnp::g_SSDPCache.RemoveStale();

            if (nCount > 0)
                VERBOSE( VB_UPNP, QString( "SSDPCacheTask - Removed %1 stale entries." ).arg( nCount ));

            if ((m_nExecuteCount % 60) == 0)
                UPnp::g_SSDPCache.Dump();
                                    
            pQueue->AddTask( m_nInterval, (Task *)this  );
        }

};

#endif
