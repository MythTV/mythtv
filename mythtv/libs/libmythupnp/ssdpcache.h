//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdpcache.h
// Created     : Jan. 8, 2007
//
// Purpose     : SSDP Cache Declaration
//
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SSDPCLIENT_H
#define SSDPCLIENT_H

#include <chrono>

// Qt headers
#include <QObject>
#include <QMutex>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTextStream>

// MythTV headers
#include "libmythbase/mythobservable.h"
#include "libmythbase/referencecounter.h"

#include "upnpdevice.h"
#include "upnpexp.h"

/// Key == Unique Service Name (USN)
using EntryMap = QMap< QString, DeviceLocation * >;

/////////////////////////////////////////////////////////////////////////////
// QDict Implementation that uses RefCounted pointers
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC SSDPCacheEntries : public ReferenceCounter
{
  protected:
    /// Destructor protected to enforce Release method usage
    ~SSDPCacheEntries() override;

  public:
    SSDPCacheEntries();

    void Clear(void);
    uint Count(void) const
        { QMutexLocker locker(&m_mutex); return m_mapEntries.size(); }
    void Insert(const QString &sUSN, DeviceLocation *pEntry);
    void Remove(const QString &sUSN);
    uint RemoveStale(std::chrono::microseconds ttNow);

    DeviceLocation *Find(const QString &sUSN);

    DeviceLocation *GetFirst(void);

    void GetEntryMap(EntryMap &map);

    QTextStream &OutputXML(QTextStream &os, uint *pnEntryCount = nullptr) const;
    void Dump(uint &nEntryCount) const;

    static QString GetNormalizedUSN(const QString &sUSN);

  public:
    static int      g_nAllocated;       // Debugging only

  protected:
    mutable QMutex  m_mutex;
    EntryMap        m_mapEntries;
};

/// Key == Service Type URI
using SSDPCacheEntriesMap = QMap< QString, SSDPCacheEntries * >;

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
        QStringList             m_badUrlList;
        QStringList             m_goodUrlList;

    protected:

        mutable QMutex          m_mutex;
        SSDPCacheEntriesMap     m_cache;

        void NotifyAdd   ( const QString &sURI,
                           const QString &sUSN,
                           const QString &sLocation );
        void NotifyRemove( const QString &sURI, const QString &sUSN );

    private:

        // ------------------------------------------------------------------
        // Private so the singleton pattern can be enforced.
        // ------------------------------------------------------------------

        SSDPCache();
        Q_DISABLE_COPY(SSDPCache)

    public:

        static SSDPCache* Instance();

        ~SSDPCache() override;

        void Lock       () { m_mutex.lock();   }
        void Unlock     () { m_mutex.unlock(); }

        SSDPCacheEntriesMap::Iterator Begin() { return m_cache.begin(); }
        SSDPCacheEntriesMap::Iterator End  () { return m_cache.end();   }

        int  Count      () { return m_cache.count(); }
        void Clear      ();
        void Add        ( const QString &sURI,
                          const QString &sUSN,
                          const QString &sLocation,
                          std::chrono::seconds sExpiresInSecs );

        void Remove     ( const QString &sURI, const QString &sUSN );
        int  RemoveStale( );

        void Dump       (void);

        QTextStream &OutputXML(QTextStream &os,
                               uint        *pnDevCount   = nullptr,
                               uint        *pnEntryCount = nullptr) const;

        SSDPCacheEntries *Find( const QString &sURI );
        DeviceLocation   *Find( const QString &sURI, const QString &sUSN );
};

#endif // SSDPCLIENT_H
