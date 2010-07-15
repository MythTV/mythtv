//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdpcache.cpp
// Created     : Jan. 8, 2007
//
// Purpose     : 
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

    EntryMap::iterator it = m_mapEntries.begin();
    for (; it != m_mapEntries.end(); ++it)
    {
        if ((*it))
            (*it)->Release();
    }

    m_mapEntries.clear();

    Unlock();
}

/////////////////////////////////////////////////////////////////////////////
//  Caller must call AddRef on returned pointer.
/////////////////////////////////////////////////////////////////////////////

DeviceLocation *SSDPCacheEntries::Find( const QString &sUSN )
{
    Lock();

    EntryMap::iterator it = m_mapEntries.find(sUSN);
    DeviceLocation *pEntry = (it != m_mapEntries.end()) ? *it : NULL;

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

    if (it != m_mapEntries.end() && *it)
        (*it)->Release();

    m_mapEntries.insert( sUSN, pEntry );

    Unlock();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCacheEntries::Remove( const QString &sUSN )
{
    Lock();

    EntryMap::iterator it = m_mapEntries.find(sUSN);
    if (it != m_mapEntries.end())
    {
        if (*it)
            (*it)->Release();

        // -=>TODO: Need to somehow call SSDPCache::NotifyRemove

        m_mapEntries.erase(it);
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

    EntryMap::iterator it  = m_mapEntries.begin();
    for (; it != m_mapEntries.end(); ++it)
    {
        if (*it)
            continue;

        (*it)->AddRef();
        
        if ((*it)->m_ttExpires < ttNow)
            lstKeys.push_back(it.key());

        (*it)->Release();
    }

    nCount = lstKeys.count();

    // ----------------------------------------------------------------------
    // Iterate through list of keys and remove them.
    // (This avoids issues when removing from a QMap while iterating it)
    // ----------------------------------------------------------------------

    for ( QStringList::Iterator it = lstKeys.begin();
                                it != lstKeys.end();
                              ++it ) 
        Remove( *it );

    Unlock();

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
    // FIXME: Using this causes crashes
    //VERBOSE( VB_UPNP, "SSDPCache - Destructor" );

    Clear();
}      

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCache::Clear()
{
    Lock();

    SSDPCacheEntriesMap::iterator it  = m_cache.begin();
    for (; it != m_cache.end(); ++it)
    {
        if (*it)
            (*it)->Release();
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

    SSDPCacheEntriesMap::iterator it = m_cache.find(sURI);
    if ( it != m_cache.end() )
        pEntries = *it;

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
    gettimeofday        ( (&ttExpires), NULL );
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void SSDPCache::Dump()
{
    int nCount = 0;

    if (VERBOSE_LEVEL_CHECK(VB_UPNP))
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
            SSDPCacheEntries *pEntries = *it;

            if (pEntries != NULL)
            {
                VERBOSE( VB_UPNP, it.key() );

                pEntries->Lock();

                EntryMap *pMap = pEntries->GetEntryMap();

                for (EntryMap::Iterator itEntry  = pMap->begin();
                                        itEntry != pMap->end();
                                      ++itEntry )
                {

                    DeviceLocation *pEntry = *itEntry;

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
