//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdpcache.cpp
//                                                                            
// Purpose - SSDP Client Implmenetation
//                                                                            
// Created By  : David Blain                    Created On : Jan. 8, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnp.h"
#include "mythevent.h"

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
    // Should be atomic increment
    g_nAllocated++;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDPCacheEntries::~SSDPCacheEntries()
{
    Clear();

    // Should be atomic decrement
    g_nAllocated--;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCacheEntries::Clear()
{
    Lock();

    for (EntryMap::Iterator it  = m_mapEntries.begin();
                            it != m_mapEntries.end();
                          ++it )
    {
        DeviceLocation *pEntry = (DeviceLocation *)it.data();

        if (pEntry != NULL)
            pEntry->Release();
    }

    m_mapEntries.clear();

    Unlock();
}

/////////////////////////////////////////////////////////////////////////////
//  Caller must call AddRef on returned pointer.
/////////////////////////////////////////////////////////////////////////////

DeviceLocation *SSDPCacheEntries::Find( const QString &sUSN )
{
    DeviceLocation *pEntry = NULL;

    Lock();
    EntryMap::Iterator it = m_mapEntries.find( sUSN );

    if ( it != m_mapEntries.end() )
        pEntry = (DeviceLocation *)it.data();

    Unlock();

    return pEntry;
}

/////////////////////////////////////////////////////////////////////////////
//  
/////////////////////////////////////////////////////////////////////////////

void SSDPCacheEntries::Insert( const QString &sUSN, DeviceLocation *pEntry )
{
    Lock();

    pEntry->AddRef();

    // Since insert overrights anything already there
    // we need to see if the key already exists and release
    // it's reference if it does.

    EntryMap::Iterator it = m_mapEntries.find( sUSN );

    if (it != m_mapEntries.end())
    {
        if (it.data() != NULL)
            ((DeviceLocation *)it.data())->Release();
    }

    m_mapEntries.insert( sUSN, pEntry );

    Unlock();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCacheEntries::Remove( const QString &sUSN )
{
    Lock();

    EntryMap::Iterator it = m_mapEntries.find( sUSN );

    if (it != m_mapEntries.end())
    {
        if (it.data() != NULL)
            ((DeviceLocation *)it.data())->Release();

        // -=>TODO: Need to somehow call SSDPCache::NotifyRemove

        m_mapEntries.remove( it );
    }

    Unlock();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int SSDPCacheEntries::RemoveStale( const TaskTime &ttNow )
{
    int          nCount = 0;
    QStringList  lstKeys;

    Lock();

    // ----------------------------------------------------------------------
    // Iterate through all USN's and build list of stale entries keys
    // ----------------------------------------------------------------------

    for (EntryMap::Iterator it  = m_mapEntries.begin();
                            it != m_mapEntries.end();
                          ++it )
    {
        DeviceLocation *pEntry = (DeviceLocation *)it.data();

        if (pEntry != NULL)
        {
            pEntry->AddRef();
        
            if ( pEntry->m_ttExpires < ttNow )
                lstKeys.append( it.key() );

            pEntry->Release();
        }
    }

    Unlock();

    nCount = lstKeys.count();

    // ----------------------------------------------------------------------
    // Iterate through list of keys and remove them.  (This avoids issues when
    // removing from a QMap while iterating it.
    // ----------------------------------------------------------------------

    for ( QStringList::Iterator it = lstKeys.begin(); it != lstKeys.end(); ++it ) 
        Remove( *it );

    return nCount;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPCache Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

SSDPCache::SSDPCache()
{
    VERBOSE( VB_UPNP, "SSDPCache - Constructor" );
}      

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SSDPCache::~SSDPCache()
{
    VERBOSE( VB_UPNP, "SSDPCache - Destructor" );

    Clear();
}      

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCache::Clear()
{
    Lock();

    for (SSDPCacheEntriesMap::Iterator it  = m_cache.begin();
                                       it != m_cache.end();
                                     ++it )
    {
        SSDPCacheEntries *pEntries = (SSDPCacheEntries *)it.data();

        if (pEntries != NULL)
            pEntries->Release();
    }

    m_cache.clear();

    Unlock();
}

/////////////////////////////////////////////////////////////////////////////
//  Caller must call AddRef on returned pointer.
/////////////////////////////////////////////////////////////////////////////

SSDPCacheEntries *SSDPCache::Find( const QString &sURI )
{
    SSDPCacheEntries *pEntries = NULL;

    Lock();
    SSDPCacheEntriesMap::Iterator it = m_cache.find( sURI );

    if ( it != m_cache.end() )
        pEntries = (SSDPCacheEntries *)it.data();

    Unlock();

    return pEntries;
}

/////////////////////////////////////////////////////////////////////////////
//  Caller must call AddRef on returned pointer.
/////////////////////////////////////////////////////////////////////////////

DeviceLocation *SSDPCache::Find( const QString &sURI, const QString &sUSN )
{
    DeviceLocation   *pEntry   = NULL;
    SSDPCacheEntries *pEntries = Find( sURI );

    if (pEntries != NULL)
    {
        pEntries->AddRef();
        pEntry = pEntries->Find( sUSN );
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
    gettimeofday        ( &ttExpires, NULL );
    AddSecondsToTaskTime(  ttExpires, sExpiresInSecs );

    // --------------------------------------------------------------
    // Get a Pointer to a Entries QDict... (Create if not found)
    // --------------------------------------------------------------

    SSDPCacheEntries *pEntries = Find( sURI );

    if (pEntries == NULL)
    {
        pEntries = new SSDPCacheEntries();
        pEntries->AddRef();
        m_cache.insert( sURI, pEntries );
    }

    pEntries->AddRef();

    // --------------------------------------------------------------
    // See if the Entries Collection contains our USN... (Create if not found)
    // --------------------------------------------------------------

    DeviceLocation *pEntry = pEntries->Find( sUSN );

    if (pEntry == NULL)
    {
        pEntry = new DeviceLocation( sURI, sUSN, sLocation, ttExpires );

        Lock();
        pEntries->Insert( sUSN, pEntry );
        Unlock();

        NotifyAdd( sURI, sUSN, sLocation );
    }
    else
    {
        pEntry->AddRef();
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
        SSDPCacheEntries *pEntries = (SSDPCacheEntries *)it.data();

        if (pEntries != NULL)
        {
            pEntries->AddRef();

            pEntries->Remove( sUSN );

            if (pEntries->Count() == 0)
            {
                pEntries->Release();
                m_cache.remove( it );
            }

            pEntries->Release();
        }
    }

    Unlock();

    // -=>TODO: Should this only by notifued if we actually had any entry removed?

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

    gettimeofday( &ttNow, NULL );

    Lock();

    // ----------------------------------------------------------------------
    // Iterate through all Type URI's and build list of stale entries keys
    // ----------------------------------------------------------------------

    for (SSDPCacheEntriesMap::Iterator it  = m_cache.begin();
                                       it != m_cache.end();
                                     ++it )
    {
        SSDPCacheEntries *pEntries = (SSDPCacheEntries *)it.data();

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
    // Iterate through list of keys and remove them.  (This avoids issues when
    // removing from a QMap while iterating it.
    // ----------------------------------------------------------------------

    for ( QStringList::Iterator itKey = lstKeys.begin(); itKey != lstKeys.end(); ++itKey ) 
    {
        SSDPCacheEntriesMap::Iterator it = m_cache.find( *itKey );

        if (it != m_cache.end())
        {
            SSDPCacheEntries *pEntries = (SSDPCacheEntries *)it.data();

            if (pEntries != NULL)
            {
                pEntries->Release();
                m_cache.remove( it );
            }
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCache::Dump()
{
    int nCount = 0;

    if ( print_verbose_messages & VB_UPNP)
    {

        Lock();

        // ----------------------------------------------------------------------
        // Build List of items to be removed
        // ----------------------------------------------------------------------

        VERBOSE( VB_UPNP, "===============================================================================" );
        VERBOSE( VB_UPNP, QString(  " URI (type) - Found: %1 Entries - %2 have been Allocated. " )
                             .arg( m_cache.count() )
                             .arg( SSDPCacheEntries::g_nAllocated ));
        VERBOSE( VB_UPNP, "   \t\tUSN (unique id)\t\t | Expires\t | Location" );
        VERBOSE( VB_UPNP, "-------------------------------------------------------------------------------" );

        for (SSDPCacheEntriesMap::Iterator it  = m_cache.begin();
                                           it != m_cache.end();
                                         ++it )
        {
            SSDPCacheEntries *pEntries = (SSDPCacheEntries *)it.data();

            if (pEntries != NULL)
            {
                VERBOSE( VB_UPNP, it.key() );

                pEntries->Lock();

                EntryMap *pMap = pEntries->GetEntryMap();

                for (EntryMap::Iterator itEntry  = pMap->begin();
                                        itEntry != pMap->end();
                                      ++itEntry )
                {

                    DeviceLocation *pEntry = (DeviceLocation *)itEntry.data();

                    if (pEntry != NULL)
                    {
                        nCount++;

                        pEntry->AddRef();

                        VERBOSE( VB_UPNP, QString( " * \t\t%1\t | %2\t | %3 " ) 
                                             .arg( pEntry->m_sUSN )
                                             .arg( pEntry->ExpiresInSecs() )
                                             .arg( pEntry->m_sLocation ));

                        pEntry->Release();
                    }
                }

                VERBOSE( VB_UPNP, " "); 

                pEntries->Unlock();
            }
        }

        VERBOSE( VB_UPNP, "-------------------------------------------------------------------------------" );
        VERBOSE( VB_UPNP, QString(  " Found: %1 Entries - %2 have been Allocated. " )
                             .arg( nCount )
                             .arg( DeviceLocation::g_nAllocated ));
        VERBOSE( VB_UPNP, "===============================================================================" );

        Unlock();
    }
}
