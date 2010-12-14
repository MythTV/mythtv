//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdpcache.h
// Created     : Jan. 8, 2007
//
// Purpose     : SSDP Cache Declaration
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

#ifndef __SSDPCLIENT_H__
#define __SSDPCLIENT_H__

#include "upnpexp.h"
#include "mythobservable.h"

#include <QObject>
#include <QMap>

#include "upnpdevice.h"

typedef QMap< QString, DeviceLocation * > EntryMap;     // Key == Unique Service Name (USN)

/////////////////////////////////////////////////////////////////////////////
// QDict Implementation that uses RefCounted pointers
/////////////////////////////////////////////////////////////////////////////

class SSDPCacheEntries : public RefCounted
{
    public:

        static int      g_nAllocated;       // Debugging only

    protected:

        QMutex      m_mutex;
        EntryMap    m_mapEntries;

    protected:

        // Destructor protected to force use of Release Method

        virtual ~SSDPCacheEntries();

    public:

                 SSDPCacheEntries();

        void Lock       () { m_mutex.lock();   }
        void Unlock     () { m_mutex.unlock(); }

        void            Clear      ( );

        int             Count      ( ) { return m_mapEntries.size(); }

        DeviceLocation *Find       ( const QString &sUSN );
        void            Insert     ( const QString &sUSN, DeviceLocation *pEntry );
        void            Remove     ( const QString &sUSN );
        int             RemoveStale( const TaskTime &ttNow );

        // Must Lock/Unlock when using.
        EntryMap       *GetEntryMap() { return &m_mapEntries;  }
};

typedef QMap< QString, SSDPCacheEntries * > SSDPCacheEntriesMap;   // Key == Service Type URI

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC SSDPCache : public QObject,
                  public MythObservable
{
    Q_OBJECT

    protected:

        QMutex                  m_mutex;
        SSDPCacheEntriesMap     m_cache;

        void NotifyAdd   ( const QString &sURI,
                           const QString &sUSN,
                           const QString &sLocation );
        void NotifyRemove( const QString &sURI, const QString &sUSN );

    public:

                 SSDPCache();
        virtual ~SSDPCache();

        void Lock       () { m_mutex.lock();   }
        void Unlock     () { m_mutex.unlock(); }

        SSDPCacheEntriesMap::Iterator Begin() { return m_cache.begin(); }
        SSDPCacheEntriesMap::Iterator End  () { return m_cache.end();   }

        int  Count      () { return m_cache.count(); }
        void Clear      ();
        void Add        ( const QString &sURI,
                          const QString &sUSN,
                          const QString &sLocation,
                          long           sExpiresInSecs );

        void Remove     ( const QString &sURI, const QString &sUSN );
        int  RemoveStale( );

        void Dump       ( );

        SSDPCacheEntries *Find( const QString &sURI );
        DeviceLocation   *Find( const QString &sURI, const QString &sUSN );
};

#endif
