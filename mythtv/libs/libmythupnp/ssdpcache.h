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

#include "mythcontext.h"

#include "qdict.h"
#include "qmap.h"
#include "sys/time.h"
#include "qdatetime.h"

/////////////////////////////////////////////////////////////////////////////
// SSDPCacheEntry Class Definition/Implementation
/////////////////////////////////////////////////////////////////////////////

class SSDPCacheEntry : public RefCounted 
{
    public:

        static int      g_nAllocated;       // Debugging only

    protected:

        // Destructor protected to force use of Release Method

        virtual        ~SSDPCacheEntry() 
        { 
            // Should be atomic decrement
            g_nAllocated--;
        }

    public:

        QString     m_sURI;           // Service Type URI
        QString     m_sUSN;           // Unique Service Name
        QString     m_sLocation;      // URL to Device Description
        TaskTime    m_ttExpires;

    public:

        SSDPCacheEntry( const QString &sURI,
                        const QString &sUSN,
                        const QString &sLocation,
                        TaskTime       ttExpires ) : m_sURI     ( sURI      ),
                                                     m_sUSN     ( sUSN      ),
                                                     m_sLocation( sLocation ),
                                                     m_ttExpires( ttExpires )
        {
            // Should be atomic increment
            g_nAllocated++;
        }

        int ExpiresInSecs()
        {
            TaskTime ttNow;
            gettimeofday( &ttNow, NULL );

            return m_ttExpires.tv_sec - ttNow.tv_sec;
        }


};


typedef QMap< QString, SSDPCacheEntry * > EntryMap;     // Key == Unique Service Name (USN)

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

        SSDPCacheEntry *Find       ( const QString &sUSN );
        void            Insert     ( const QString &sUSN, SSDPCacheEntry *pEntry );
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

class SSDPCache 
{
    protected:

        QMutex                  m_mutex;
        SSDPCacheEntriesMap     m_cache;
    
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
        SSDPCacheEntry   *Find( const QString &sURI, const QString &sUSN );
};

#endif
