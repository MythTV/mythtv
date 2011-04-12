//////////////////////////////////////////////////////////////////////////////
// Program Name: upnptaskcache.h
// Created     : Jan. 9, 2007
//
// Purpose     : UPnp Task to keep SSDPCache free of stale records
//                                                                            
// Copyright (c) 2007 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
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
            m_nInterval     = UPnp::GetConfiguration()->GetValue( "UPnP/SSDP/CacheInterval", 30 ) * 1000;

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
                VERBOSE( VB_UPNP, QString( "SSDPCacheTask - Removed %1 stale entries." ).arg( nCount ));

            if ((m_nExecuteCount % 60) == 0)
                SSDPCache::Instance()->Dump();
                                    
            pQueue->AddTask( m_nInterval, (Task *)this  );
        }

};

#endif
