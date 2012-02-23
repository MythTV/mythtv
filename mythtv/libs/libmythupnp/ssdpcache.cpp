//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdpcache.cpp
// Created     : Jan. 8, 2007
//
// Purpose     : 
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include "upnp.h"
#include "mythevent.h"
#include "mythlogging.h"
#include "upnptaskcache.h"

SSDPCache* SSDPCache::g_pSSDPCache = NULL;

int SSDPCacheEntries::g_nAllocated = 0;       // Debugging only

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPCacheEntries Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

SSDPCacheEntries::SSDPCacheEntries()
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

    EntryMap::iterator it = m_mapEntries.begin();
    for (; it != m_mapEntries.end(); ++it)
    {
        if ((*it))
            (*it)->Release();
    }

    m_mapEntries.clear();
}

/// Finds the Device in the cache, returns NULL when absent
/// \note Caller must call Release on non-NULL DeviceLocation when done with it.
DeviceLocation *SSDPCacheEntries::Find(const QString &sUSN)
{
    QMutexLocker locker(&m_mutex);

    EntryMap::iterator it = m_mapEntries.find(GetNormalizedUSN(sUSN));
    DeviceLocation *pEntry = (it != m_mapEntries.end()) ? *it : NULL;
    if (pEntry)
        pEntry->AddRef();

    return pEntry;
}

/// Returns random entry in cache, returns NULL when list is empty
/// \note Caller must call Release on non-NULL DeviceLocation when done with it.
DeviceLocation *SSDPCacheEntries::GetFirst(void)
{
    QMutexLocker locker(&m_mutex);
    if (m_mapEntries.empty())
        return NULL;
    DeviceLocation *loc = *m_mapEntries.begin();
    loc->AddRef();
    return loc;
}

/// Returns a copy of the EntryMap
/// \note Caller must call Release() on each entry in the map.
void SSDPCacheEntries::GetEntryMap(EntryMap &map)
{
    QMutexLocker locker(&m_mutex);

    EntryMap::const_iterator it = m_mapEntries.begin();
    for (; it != m_mapEntries.end(); ++it)
    {
        (*it)->AddRef();
        map.insert(it.key(), *it);
    }
}

/// Inserts a device location into the cache
void SSDPCacheEntries::Insert(const QString &sUSN, DeviceLocation *pEntry)
{
    QMutexLocker locker(&m_mutex);

    pEntry->AddRef();

    // Since insert overrights anything already there
    // we need to see if the key already exists and release
    // it's reference if it does.

    QString usn = GetNormalizedUSN(sUSN);

    EntryMap::iterator it = m_mapEntries.find(usn);
    if ((it != m_mapEntries.end()) && (*it != NULL))
        (*it)->Release();

    m_mapEntries[usn] = pEntry;

    LOG(VB_UPNP, LOG_INFO, QString("SSDP Cache adding USN: %1 Location %2")
            .arg(pEntry->m_sUSN).arg(pEntry->m_sLocation));
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
                    .arg((*it)->m_sUSN).arg((*it)->m_sLocation));
            (*it)->Release();
        }

        // -=>TODO: Need to somehow call SSDPCache::NotifyRemove

        m_mapEntries.erase(it);
    }
}

/// Removes expired cache entries, returning the number removed.
uint SSDPCacheEntries::RemoveStale(const TaskTime &ttNow)
{
    QMutexLocker locker(&m_mutex);
    uint nCount = 0;

    EntryMap::iterator it = m_mapEntries.begin();
    while (it != m_mapEntries.end())
    {
        if (*it == NULL)
        {
            it = m_mapEntries.erase(it);
        }
        else if ((*it)->m_ttExpires < ttNow)
        {
            // Note: locking is not required above since we hold
            // one reference to each entry and are holding m_mutex.
            (*it)->Release();

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

    EntryMap::const_iterator it  = m_mapEntries.begin();
    for (; it != m_mapEntries.end(); ++it)
    {
        if (*it == NULL)
            continue;

        // Note: AddRef,Release not required since SSDPCacheEntries
        // holds one reference to each entry and we are holding m_mutex.
        os << "<Service usn='" << (*it)->m_sUSN 
           << "' expiresInSecs='" << (*it)->ExpiresInSecs()
           << "' url='" << (*it)->m_sLocation << "' />" << endl;

        if (pnEntryCount != NULL)
            (*pnEntryCount)++;
    }

    return os;
}

/// Prints this service to the console in human readable form
void SSDPCacheEntries::Dump(uint &nEntryCount) const
{
    QMutexLocker locker(&m_mutex);

    EntryMap::const_iterator it  = m_mapEntries.begin();
    for (; it != m_mapEntries.end(); ++it)
    {
        if (*it == NULL)
            continue;

        // Note: AddRef,Release not required since SSDPCacheEntries
        // holds one reference to each entry and we are holding m_mutex.
        LOG(VB_UPNP, LOG_DEBUG, QString(" * \t\t%1\t | %2\t | %3 ")
                .arg((*it)->m_sUSN) .arg((*it)->ExpiresInSecs())
                .arg((*it)->m_sLocation));

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

    TaskQueue::Instance()->AddTask( new SSDPCacheTask() );

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

    SSDPCacheEntriesMap::iterator it  = m_cache.begin();
    for (; it != m_cache.end(); ++it)
    {
        if (*it)
            (*it)->Release();
    }

    m_cache.clear();
}

/// Finds the SSDPCacheEntries in the cache, returns NULL when absent
/// \note Caller must call Release on non-NULL when done with it.
SSDPCacheEntries *SSDPCache::Find(const QString &sURI)
{
    QMutexLocker locker(&m_mutex);

    SSDPCacheEntriesMap::iterator it = m_cache.find(sURI);
    if (it != m_cache.end() && (*it != NULL))
        (*it)->AddRef();

    return (it != m_cache.end()) ? *it : NULL;
}

/// Finds the Device in the cache, returns NULL when absent
/// \note Caller must call Release on non-NULL when done with it.
DeviceLocation *SSDPCache::Find(const QString &sURI, const QString &sUSN)
{
    DeviceLocation   *pEntry   = NULL;
    SSDPCacheEntries *pEntries = Find(sURI);

    if (pEntries != NULL)
    {
        pEntry = pEntries->Find(sUSN);
        pEntries->Release();
    }

    return pEntry;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCache::Add( const QString &sURI,
                     const QString &sUSN,
                     const QString &sLocation,
                     long           sExpiresInSecs )
{    
    // --------------------------------------------------------------
    // Calculate when this cache entry should expire.
    // --------------------------------------------------------------

    TaskTime ttExpires;
    gettimeofday        ( (&ttExpires), NULL );
    AddSecondsToTaskTime(  ttExpires, sExpiresInSecs );

    // --------------------------------------------------------------
    // Get a Pointer to a Entries QDict... (Create if not found)
    // --------------------------------------------------------------

    SSDPCacheEntries *pEntries = NULL;
    {
        QMutexLocker locker(&m_mutex);
        SSDPCacheEntriesMap::iterator it = m_cache.find(sURI);
        if (it == m_cache.end() || (*it == NULL))
        {
            pEntries = new SSDPCacheEntries();
            pEntries->AddRef();
            it = m_cache.insert(sURI, pEntries);
        }
        pEntries = *it;
        pEntries->AddRef();
    }

    // --------------------------------------------------------------
    // See if the Entries Collection contains our USN... (Create if not found)
    // --------------------------------------------------------------

    DeviceLocation *pEntry = pEntries->Find(sUSN);
    if (pEntry == NULL)
    {
        pEntry = new DeviceLocation(sURI, sUSN, sLocation, ttExpires);
        pEntries->Insert(sUSN, pEntry);
        NotifyAdd(sURI, sUSN, sLocation);
    }
    else
    {
        pEntry->m_sLocation = sLocation;
        pEntry->m_ttExpires = ttExpires;
        pEntry->Release();
    }

    pEntries->Release();
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

        if (pEntries != NULL)
        {
            pEntries->AddRef();

            pEntries->Remove( sUSN );

            if (pEntries->Count() == 0)
            {
                pEntries->Release();
                m_cache.erase(it);
            }

            pEntries->Release();
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
    TaskTime     ttNow;
    QStringList  lstKeys;

    gettimeofday( (&ttNow), NULL );

    Lock();

    // ----------------------------------------------------------------------
    // Iterate through all Type URI's and build list of stale entries keys
    // ----------------------------------------------------------------------

    for (SSDPCacheEntriesMap::Iterator it  = m_cache.begin();
                                       it != m_cache.end();
                                     ++it )
    {
        SSDPCacheEntries *pEntries = *it;

        if (pEntries != NULL)
        {
            pEntries->AddRef();

            nCount += pEntries->RemoveStale( ttNow );
     
            if (pEntries->Count() == 0)
                lstKeys.append( it.key() );

            pEntries->Release();
        }
    }

    Unlock();

    nCount = lstKeys.count();

    // ----------------------------------------------------------------------
    // Iterate through list of keys and remove them.
    // (This avoids issues when removing from a QMap while iterating it)
    // ----------------------------------------------------------------------

    for ( QStringList::Iterator itKey = lstKeys.begin();
                                itKey != lstKeys.end();
                              ++itKey ) 
    {
        SSDPCacheEntriesMap::iterator it = m_cache.find( *itKey );
        if (it == m_cache.end())
            continue;

        if (*it)
        {
            (*it)->Release();
            m_cache.erase(it);
        }
    }

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

    if (pnDevCount != NULL)
        *pnDevCount   = 0;
    if (pnEntryCount != NULL)
        *pnEntryCount = 0;

    SSDPCacheEntriesMap::const_iterator it = m_cache.begin();
    for (; it != m_cache.end(); ++it)
    {
        if (*it != NULL)
        {
            os << "<Device uri='" << it.key() << "'>" << endl;

            uint tmp = 0;

            (*it)->OutputXML(os, &tmp);

            if (pnEntryCount != NULL)
                *pnEntryCount += tmp;

            os << "</Device>" << endl;

            if (pnDevCount != NULL)
                (*pnDevCount)++;
        }
    }
    os << flush;

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
    SSDPCacheEntriesMap::const_iterator it  = m_cache.begin();
    for (; it != m_cache.end(); ++it)
    {
        if (*it != NULL)
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
