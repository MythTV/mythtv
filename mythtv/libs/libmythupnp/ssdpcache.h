//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdpcache.h
// Created     : Jan. 8, 2007
//
// Purpose     : SSDP Cache Declaration
//
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __SSDPCLIENT_H__
#define __SSDPCLIENT_H__

// Qt headers
#include <QObject>
#include <QMutex>
#include <QMap>

// MythTV headers
#include "referencecounter.h"
#include "mythobservable.h"
#include "upnpdevice.h"
#include "upnpexp.h"

/// Key == Unique Service Name (USN)
typedef QMap< QString, DeviceLocation * > EntryMap;

/////////////////////////////////////////////////////////////////////////////
// QDict Implementation that uses RefCounted pointers
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC SSDPCacheEntries : public ReferenceCounter
{
  protected:
    /// Destructor protected to enforce Release method usage
    virtual ~SSDPCacheEntries();

  public:
    SSDPCacheEntries();

    void Clear(void);
    uint Count(void) const
        { QMutexLocker locker(&m_mutex); return m_mapEntries.size(); }
    void Insert(const QString &sUSN, DeviceLocation *pEntry);
    void Remove(const QString &sUSN);
    uint RemoveStale(const TaskTime &ttNow);

    DeviceLocation *Find(const QString &sUSN);

    DeviceLocation *GetFirst(void);

    void GetEntryMap(EntryMap&);

    QTextStream &OutputXML(QTextStream &os, uint *pnEntryCount = NULL) const;
    void Dump(uint &nEntryCount) const;

    static QString GetNormalizedUSN(const QString &sUSN);

  public:
    static int      g_nAllocated;       // Debugging only

  protected:
    mutable QMutex  m_mutex;
    EntryMap        m_mapEntries;
};

/// Key == Service Type URI
typedef QMap< QString, SSDPCacheEntries * > SSDPCacheEntriesMap;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPCache Class Definition - (Singleton)
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC SSDPCache : public QObject,
                  public MythObservable
{
    Q_OBJECT

    private:
        // Singleton instance used by all.
        static SSDPCache*       g_pSSDPCache;  

    protected:

        mutable QMutex          m_mutex;
        SSDPCacheEntriesMap     m_cache;

        void NotifyAdd   ( const QString &sURI,
                           const QString &sUSN,
                           const QString &sLocation );
        void NotifyRemove( const QString &sURI, const QString &sUSN );

    private:

        // ------------------------------------------------------------------
        // Private so the singleton pattern can be inforced.
        // ------------------------------------------------------------------

        SSDPCache();

    public:

        static SSDPCache* Instance();

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

        void Dump       (void);

        QTextStream &OutputXML(QTextStream &os,
                               uint        *pnDevCount   = NULL,
                               uint        *pnEntryCount = NULL) const;

        SSDPCacheEntries *Find( const QString &sURI );
        DeviceLocation   *Find( const QString &sURI, const QString &sUSN );
};

#endif
