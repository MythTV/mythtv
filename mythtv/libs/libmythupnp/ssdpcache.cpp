//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdpcache.cpp
// Created     : Jan. 8, 2007
//
// Purpose     : 
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "ssdpcache.h"

#include <chrono>

#include "libmythbase/mythevent.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/portchecker.h"

#include "taskqueue.h"

SSDPCache* SSDPCache::g_pSSDPCache = nullptr;

int SSDPCacheEntries::g_nAllocated = 0;       // Debugging only

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPCacheEntries Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

SSDPCacheEntries::SSDPCacheEntries() : ReferenceCounter("SSDPCacheEntries")
{
    g_nAllocated++;    // Should be atomic increment
}

SSDPCacheEntries::~SSDPCacheEntries()
{
    Clear();
    g_nAllocated--;    // Should be atomic decrement
}

/// Clears the cache of all entries.
void SSDPCacheEntries::Clear(void)
{
    QMutexLocker locker(&m_mutex);

    for (auto *const entry : std::as_const(m_mapEntries))
    {
        if (entry)
            entry->DecrRef();
    }

    m_mapEntries.clear();
}

/// Finds the Device in the cache, returns nullptr when absent
/// \note Caller must call DecrRef on non-nullptr DeviceLocation when done with it.
DeviceLocation *SSDPCacheEntries::Find(const QString &sUSN)
{
    QMutexLocker locker(&m_mutex);

    EntryMap::iterator it = m_mapEntries.find(GetNormalizedUSN(sUSN));
    DeviceLocation *pEntry = (it != m_mapEntries.end()) ? *it : nullptr;
    if (pEntry)
        pEntry->IncrRef();

    return pEntry;
}

/// Returns random entry in cache, returns nullptr when list is empty
/// \note Caller must call DecrRef on non-nullptr DeviceLocation when done with it.
DeviceLocation *SSDPCacheEntries::GetFirst(void)
{
    QMutexLocker locker(&m_mutex);
    if (m_mapEntries.empty())
        return nullptr;
    DeviceLocation *loc = *m_mapEntries.begin();
    loc->IncrRef();
    return loc;
}

/// Returns a copy of the EntryMap
/// \note Caller must call DecrRef() on each entry in the map.
void SSDPCacheEntries::GetEntryMap(EntryMap &map)
{
    QMutexLocker locker(&m_mutex);

    for (auto it = m_mapEntries.cbegin(); it != m_mapEntries.cend(); ++it)
    {
        (*it)->IncrRef();
        map.insert(it.key(), *it);
    }
}

/// Inserts a device location into the cache
void SSDPCacheEntries::Insert(const QString &sUSN, DeviceLocation *pEntry)
{
    QMutexLocker locker(&m_mutex);

    pEntry->IncrRef();

    // Since insert overwrites anything already there
    // we need to see if the key already exists and release
    // it's reference if it does.

    QString usn = GetNormalizedUSN(sUSN);

    EntryMap::iterator it = m_mapEntries.find(usn);
    if ((it != m_mapEntries.end()) && (*it != nullptr))
        (*it)->DecrRef();

    m_mapEntries[usn] = pEntry;

    LOG(VB_UPNP, LOG_INFO, QString("SSDP Cache adding USN: %1 Location %2")
            .arg(pEntry->m_sUSN, pEntry->m_sLocation));
}

/// Removes a specific entry from the cache
void SSDPCacheEntries::Remove( const QString &sUSN )
{
    QMutexLocker locker(&m_mutex);

    QString usn = GetNormalizedUSN(sUSN);
    EntryMap::iterator it = m_mapEntries.find(usn);
    if (it != m_mapEntries.end())
    {
        if (*it)
        {
            LOG(VB_UPNP, LOG_INFO,
                QString("SSDP Cache removing USN: %1 Location %2")
                    .arg((*it)->m_sUSN, (*it)->m_sLocation));
            (*it)->DecrRef();
        }

        // -=>TODO: Need to somehow call SSDPCache::NotifyRemove

        m_mapEntries.erase(it);
    }
}

/// Removes expired cache entries, returning the number removed.
uint SSDPCacheEntries::RemoveStale(const std::chrono::microseconds ttNow)
{
    QMutexLocker locker(&m_mutex);
    uint nCount = 0;

    EntryMap::iterator it = m_mapEntries.begin();
    while (it != m_mapEntries.end())
    {
        if (*it == nullptr)
        {
            it = m_mapEntries.erase(it);
        }
        else if ((*it)->m_ttExpires < ttNow)
        {
            // Note: locking is not required above since we hold
            // one reference to each entry and are holding m_mutex.
            (*it)->DecrRef();

            // -=>TODO: Need to somehow call SSDPCache::NotifyRemove

            it = m_mapEntries.erase(it);
            nCount++;
        }
        else
        {
            ++it;
        }
    }

    return nCount;
}

/// Outputs the XML for this service
QTextStream &SSDPCacheEntries::OutputXML(
    QTextStream &os, uint *pnEntryCount) const
{
    QMutexLocker locker(&m_mutex);

    for (auto *entry : std::as_const(m_mapEntries))
    {
        if (entry == nullptr)
            continue;

        // Note: IncrRef,DecrRef not required since SSDPCacheEntries
        // holds one reference to each entry and we are holding m_mutex.
        os << "<Service usn='" << entry->m_sUSN
           << "' expiresInSecs='" << entry->ExpiresInSecs().count()
           << "' url='" << entry->m_sLocation << "' />" << Qt::endl;

        if (pnEntryCount != nullptr)
            (*pnEntryCount)++;
    }

    return os;
}

/// Prints this service to the console in human readable form
void SSDPCacheEntries::Dump(uint &nEntryCount) const
{
    QMutexLocker locker(&m_mutex);

    for (auto *entry : std::as_const(m_mapEntries))
    {
        if (entry == nullptr)
            continue;

        // Note: IncrRef,DecrRef not required since SSDPCacheEntries
        // holds one reference to each entry and we are holding m_mutex.
        LOG(VB_UPNP, LOG_DEBUG, QString(" * \t\t%1\t | %2\t | %3 ")
                .arg(entry->m_sUSN) .arg(entry->ExpiresInSecs().count())
                .arg(entry->m_sLocation));

        nEntryCount++;
    }
}

/// Returns a normalized USN, so that capitalization 
/// of the uuid is not an issue.
QString SSDPCacheEntries::GetNormalizedUSN(const QString &sUSN)
{
    int uuid_end_loc = sUSN.indexOf(":",5);
    if (uuid_end_loc > 0)
        return sUSN.left(uuid_end_loc).toLower() + sUSN.mid(uuid_end_loc);
    return sUSN;
}

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

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPCache Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

SSDPCache* SSDPCache::Instance()
{
    return g_pSSDPCache ? g_pSSDPCache : (g_pSSDPCache = new SSDPCache());

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDPCache::SSDPCache()
{
    LOG(VB_UPNP, LOG_DEBUG, "SSDPCache - Constructor");

    // ----------------------------------------------------------------------
    // Add Task to keep SSDPCache purged of stale entries.
    // ----------------------------------------------------------------------

    auto *task = new SSDPCacheTask();
    TaskQueue::Instance()->AddTask(task);
    task->DecrRef();
}      

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDPCache::~SSDPCache()
{
    // FIXME: Using this causes crashes
#if 0
    LOG(VB_UPNP, LOG_DEBUG, "SSDPCache - Destructor");
#endif

    Clear();
}      

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCache::Clear(void)
{
    QMutexLocker locker(&m_mutex);

    for (auto *const it : std::as_const(m_cache))
    {
        if (it)
            it->DecrRef();
    }

    m_cache.clear();
}

/// Finds the SSDPCacheEntries in the cache, returns nullptr when absent
/// \note Caller must call DecrRef on non-nullptr when done with it.
SSDPCacheEntries *SSDPCache::Find(const QString &sURI)
{
    QMutexLocker locker(&m_mutex);

    SSDPCacheEntriesMap::iterator it = m_cache.find(sURI);
    if (it != m_cache.end() && (*it != nullptr))
        (*it)->IncrRef();

    return (it != m_cache.end()) ? *it : nullptr;
}

/// Finds the Device in the cache, returns nullptr when absent
/// \note Caller must call DecrRef on non-nullptr when done with it.
DeviceLocation *SSDPCache::Find(const QString &sURI, const QString &sUSN)
{
    DeviceLocation   *pEntry   = nullptr;
    SSDPCacheEntries *pEntries = Find(sURI);

    if (pEntries != nullptr)
    {
        pEntry = pEntries->Find(sUSN);
        pEntries->DecrRef();
    }

    return pEntry;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCache::Add( const QString &sURI,
                     const QString &sUSN,
                     const QString &sLocation,
                     std::chrono::seconds sExpiresInSecs )
{    
    // --------------------------------------------------------------
    // Calculate when this cache entry should expire.
    // --------------------------------------------------------------

    auto ttExpires = nowAsDuration<std::chrono::microseconds>() + sExpiresInSecs;

    // --------------------------------------------------------------
    // Get a Pointer to a Entries QDict... (Create if not found)
    // --------------------------------------------------------------

    SSDPCacheEntries *pEntries = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        SSDPCacheEntriesMap::iterator it = m_cache.find(sURI);
        if (it == m_cache.end() || (*it == nullptr))
        {
            pEntries = new SSDPCacheEntries();
            it = m_cache.insert(sURI, pEntries);
        }
        pEntries = *it;
        pEntries->IncrRef();
    }

    // --------------------------------------------------------------
    // See if the Entries Collection contains our USN... (Create if not found)
    // --------------------------------------------------------------

    DeviceLocation *pEntry = pEntries->Find(sUSN);
    if (pEntry == nullptr)
    {
        QUrl url = sLocation;
        QString host = url.host();
        QString hostport = QString("%1:%2").arg(host).arg(url.port(80));
        // Check if the port can be reached. If not we won't use it.
        // Keep a cache of good and bad URLs found so as not to
        // overwhelm the thread will portchecker requests.
        // Allow up to 3 atempts before a port is finally treated as bad.
        if (m_badUrlList.count(hostport) < 3)
        {
            bool isGoodUrl = false;
            if (m_goodUrlList.contains(hostport))
                isGoodUrl = true;
            else
            {
                PortChecker checker;
                if (checker.checkPort(host, url.port(80), 5s))
                {
                    m_goodUrlList.append(hostport);
                    isGoodUrl=true;
                }
                else
                {
                    m_badUrlList.append(hostport);
                }
            }
            // Only add if the device can be connected
            if (isGoodUrl)
            {
                pEntry = new DeviceLocation(sURI, sUSN, sLocation, ttExpires);
                pEntries->Insert(sUSN, pEntry);
                NotifyAdd(sURI, sUSN, sLocation);
            }
        }
    }
    else
    {
        // Only accept locations that have been tested when added.
        if (pEntry->m_sLocation == sLocation)
            pEntry->m_ttExpires = ttExpires;
    }

    if (pEntry)
        pEntry->DecrRef();
    pEntries->DecrRef();
}
     
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCache::Remove( const QString &sURI, const QString &sUSN )
{    
    Lock();

    // --------------------------------------------------------------
    // Get a Pointer to a Entries QDict... (Create if not found)
    // --------------------------------------------------------------

    SSDPCacheEntriesMap::Iterator it = m_cache.find( sURI );

    if (it != m_cache.end())
    {
        SSDPCacheEntries *pEntries = *it;

        if (pEntries != nullptr)
        {
            pEntries->IncrRef();

            pEntries->Remove( sUSN );

            if (pEntries->Count() == 0)
            {
                pEntries->DecrRef();
                m_cache.erase(it);
            }

            pEntries->DecrRef();
        }
    }

    Unlock();

    // -=>TODO:
    // Should this only by notified if we actually had any entry removed?

    NotifyRemove( sURI, sUSN );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int SSDPCache::RemoveStale()
{
    int          nCount = 0;
    QStringList  lstKeys;

    auto ttNow = nowAsDuration<std::chrono::microseconds>();

    Lock();

    // ----------------------------------------------------------------------
    // Iterate through all Type URI's and build list of stale entries keys
    // ----------------------------------------------------------------------

    for (auto it = m_cache.begin(); it != m_cache.end(); ++it )
    {
        SSDPCacheEntries *pEntries = *it;

        if (pEntries != nullptr)
        {
            pEntries->IncrRef();

            nCount += pEntries->RemoveStale( ttNow );

            if (pEntries->Count() == 0)
                lstKeys.append( it.key() );

            pEntries->DecrRef();
        }
    }

    nCount = lstKeys.count();

    // ----------------------------------------------------------------------
    // Iterate through list of keys and remove them.
    // (This avoids issues when removing from a QMap while iterating it)
    // ----------------------------------------------------------------------

    for (const auto & key : std::as_const(lstKeys))
    {
        SSDPCacheEntriesMap::iterator it = m_cache.find( key );
        if (it == m_cache.end())
            continue;

        if (*it)
        {
            (*it)->DecrRef();
            m_cache.erase(it);
        }
    }

    Unlock();

    return nCount;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCache::NotifyAdd( const QString &sURI,
                           const QString &sUSN,
                           const QString &sLocation )
{
    QStringList values;

    values.append( sURI );
    values.append( sUSN );
    values.append( sLocation );

    MythEvent me( "SSDP_ADD", values );

    dispatch( me );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCache::NotifyRemove( const QString &sURI, const QString &sUSN )
{
    QStringList values;

    values.append( sURI );
    values.append( sUSN );

    MythEvent me( "SSDP_REMOVE", values );

    dispatch( me );
}

/// Outputs the XML for this device
QTextStream &SSDPCache::OutputXML(
    QTextStream &os, uint *pnDevCount, uint *pnEntryCount) const
{
    QMutexLocker locker(&m_mutex);

    if (pnDevCount != nullptr)
        *pnDevCount   = 0;
    if (pnEntryCount != nullptr)
        *pnEntryCount = 0;

    for (auto it = m_cache.cbegin(); it != m_cache.cend(); ++it)
    {
        if (*it != nullptr)
        {
            os << "<Device uri='" << it.key() << "'>" << Qt::endl;

            uint tmp = 0;

            (*it)->OutputXML(os, &tmp);

            if (pnEntryCount != nullptr)
                *pnEntryCount += tmp;

            os << "</Device>" << Qt::endl;

            if (pnDevCount != nullptr)
                (*pnDevCount)++;
        }
    }
    os << Qt::flush;

    return os;
}

/// Prints this device to the console in a human readable form
void SSDPCache::Dump(void)
{
    if (!VERBOSE_LEVEL_CHECK(VB_UPNP, LOG_DEBUG))
        return;

    QMutexLocker locker(&m_mutex);

    LOG(VB_UPNP, LOG_DEBUG, "========================================"
                            "=======================================");
    LOG(VB_UPNP, LOG_DEBUG, QString(" URI (type) - Found: %1 Entries - "
                                    "%2 have been Allocated. ")
            .arg(m_cache.count()).arg(SSDPCacheEntries::g_nAllocated));
    LOG(VB_UPNP, LOG_DEBUG, "   \t\tUSN (unique id)\t\t | Expires"
                            "\t | Location");
    LOG(VB_UPNP, LOG_DEBUG, "----------------------------------------"
                            "---------------------------------------");

    uint nCount = 0;
    for (auto it  = m_cache.cbegin(); it != m_cache.cend(); ++it)
    {
        if (*it != nullptr)
        {
            LOG(VB_UPNP, LOG_DEBUG, it.key());
            (*it)->Dump(nCount);
            LOG(VB_UPNP, LOG_DEBUG, " ");
        }
    }

    LOG(VB_UPNP, LOG_DEBUG, "----------------------------------------"
                            "---------------------------------------");
    LOG(VB_UPNP, LOG_DEBUG,
            QString(" Found: %1 Entries - %2 have been Allocated. ")
            .arg(nCount) .arg(DeviceLocation::g_nAllocated));
    LOG(VB_UPNP, LOG_DEBUG, "========================================"
                            "=======================================" );
}
