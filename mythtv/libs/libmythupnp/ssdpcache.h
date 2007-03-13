//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdpcache.h
//                                                                            
// Purpose - SSDP Cache Declaration
//                                                                            
// Created By  : David Blain                    Created On : Jan. 8, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __SSDPCLIENT_H__
#define __SSDPCLIENT_H__

#include "mythobservable.h"

#include <qobject.h>
#include <qdict.h>
#include <qmap.h>

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

class SSDPCache : public QObject,  
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
