/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer 
 */
#include <qstringlist.h>
#include <string.h> // For memcmp

#include "dsmcccache.h"
#include "dsmccbiop.h"
#include "dsmccreceiver.h"
#include "dsmcc.h"

#include "mythcontext.h"

/** \class DSMCCCache
 *
 *   The object carousel is transmitted as a directed graph. The leaves
 *   are files and the intermediate nodes are directories.  A directory
 *   can contain references to files or to other directories. Certain
 *   directories, known as gateways, are special and represent roots
 *   of the graph i.e. they are not themselves referred to by other
 *   directories.  One of these gateways is referenced by a
 *   DownloadServerInitiate message and is the root for the particular
 *   carousel.  Crucially, directories and files may be shared between
 *   directories and gateways. For example, the BBC radio channels 
 *   Radio 1, Radio 2, Radio 3 and Radio 4 all share the same object
 *   carousel and differ only in the DownloadServerInitiate message. 
 */

DSMCCCache::DSMCCCache(Dsmcc *dsmcc)
{
    // Delete all this when the cache is deleted.
    m_Dsmcc = dsmcc;
}

DSMCCCache::~DSMCCCache()
{
    QMap<DSMCCCacheReference, DSMCCCacheDir*>::Iterator dir;
    QMap<DSMCCCacheReference, DSMCCCacheFile*>::Iterator fil;

    for (dir = m_Directories.begin(); dir != m_Directories.end(); ++dir)
        delete *dir;

    for (dir = m_Gateways.begin(); dir != m_Gateways.end(); ++dir)
        delete *dir;

    for (fil = m_Files.begin(); fil != m_Files.end(); ++fil)
        delete *fil;
}

// Print out a key for debugging.
QString DSMCCCacheKey::toString() const
{
    QString result;
    for (uint i = 0; i < 4 && i < size(); i++)
    {
        int x = at(i);
        if (x < 16)
            result += QString("0%1").arg(x, 1, 16);
        else
            result += QString("%1").arg(x, 2, 16);
    }

    return result;
}

// Ordering function used in QMap
bool operator < (const DSMCCCacheKey &key1, const DSMCCCacheKey &key2)
{
    const char *data1 = key1.data();
    const char *data2 = key2.data();
    uint size1 = key1.size(), size2 = key2.size();
    uint size;
    if (size1 < size2)
        size = size1;
    else
        size = size2;
    int res = memcmp(data1, data2, size);
    if (res < 0)
        return true;
    else if (res > 0)
        return false;

    return size1 < size2;
}

// Test for equality of the cache references
// which include the carousel and module ids.
bool DSMCCCacheReference::Equal(const DSMCCCacheReference &r) const
{
    return m_nCarouselId == r.m_nCarouselId && m_nModuleId == r.m_nModuleId &&
        m_nStreamTag == r.m_nStreamTag && m_Key == r.m_Key;
}

bool DSMCCCacheReference::Equal(const DSMCCCacheReference *p) const
{
    return p != NULL && Equal(*p);
}


// Print out a cache reference for debugging.
QString DSMCCCacheReference::toString(void) const
{
    return QString("%1-%2-%3-")
        .arg(m_nCarouselId).arg(m_nStreamTag)
        .arg(m_nModuleId) + m_Key.toString();
}

// Operator required for QMap
bool operator < (const DSMCCCacheReference &ref1,
                 const DSMCCCacheReference &ref2)
{
    if (ref1.m_nCarouselId < ref2.m_nCarouselId)
        return true;
    else if (ref1.m_nCarouselId > ref2.m_nCarouselId)
        return false;
    else if (ref1.m_nStreamTag < ref2.m_nStreamTag)
        return true;
    else if (ref1.m_nStreamTag > ref2.m_nStreamTag)
        return false;
    else if (ref1.m_nModuleId < ref2.m_nModuleId)
        return true;
    else if (ref1.m_nModuleId > ref2.m_nModuleId)
        return false;
    else if (ref1.m_Key < ref2.m_Key)
        return true;

    return false;
}

// Create a gateway entry.
DSMCCCacheDir *DSMCCCache::Srg(const DSMCCCacheReference &ref)
{
    // Check to see that it isn't already there.  It shouldn't be.
    QMap<DSMCCCacheReference, DSMCCCacheDir*>::Iterator dir =
        m_Gateways.find(ref);

    if (dir != m_Gateways.end())
    {
        VERBOSE(VB_DSMCC, QString("[DSMCCCache] Already seen gateway %1")
                .arg(ref.toString()));
        return NULL;
    }

    DSMCCCacheDir *pSrg = new DSMCCCacheDir(ref);
    m_Gateways.insert(ref, pSrg);

    return pSrg;
}

// Create a directory entry.
DSMCCCacheDir *DSMCCCache::Directory(const DSMCCCacheReference &ref)
{
    // Check to see that it isn't already there.  It shouldn't be.
    QMap<DSMCCCacheReference, DSMCCCacheDir*>::Iterator dir =
        m_Directories.find(ref);

    if (dir != m_Directories.end())
    {
        VERBOSE(VB_DSMCC, QString("[DSMCCCache] Already seen directory %1")
                .arg(ref.toString()));
        return NULL;
    }

    DSMCCCacheDir *pDir = new DSMCCCacheDir(ref);
    m_Directories.insert(ref, pDir);

    return pDir;
}

// Called when the data for a file module arrives.
void DSMCCCache::CacheFileData(const DSMCCCacheReference &ref,
                               const QByteArray &data)
{
    DSMCCCacheFile *pFile;

    // Do we have the file already?
    VERBOSE(VB_DSMCC,
            QString("[DSMCCCache] Adding file data size %1 for reference %2")
            .arg(data.size()).arg(ref.toString()));

    QMap<DSMCCCacheReference, DSMCCCacheFile*>::Iterator fil =
        m_Files.find(ref);

    if (fil == m_Files.end())
    {
        pFile = new DSMCCCacheFile(ref);
        m_Files.insert(ref, pFile);
    }
    else
    {
        pFile = *fil;
    }

    pFile->m_Contents = data; // Save the data (this is use-counted by Qt).
}

// Add a file to the directory.
void DSMCCCache::AddFileInfo(DSMCCCacheDir *pDir, const BiopBinding *pBB)
{
    QString name;
    name.setAscii(pBB->m_name.m_comps[0].m_id
                  /*, pBB->m_name.m_comps[0].m_id_len*/);

    const DSMCCCacheReference *entry =
        pBB->m_ior.m_profile_body->GetReference();

    pDir->m_Files.insert(name, *entry);

    VERBOSE(VB_DSMCC,
            QString("[DSMCCCache] Adding file with name %1 reference %2")
            .arg(name.ascii()).arg(entry->toString()));
}

// Add a sub-directory to the directory.
void DSMCCCache::AddDirInfo(DSMCCCacheDir *pDir, const BiopBinding *pBB)
{
    // Is it already there?
    QString name;
    name.setAscii(pBB->m_name.m_comps[0].m_id
                  /*, pBB->m_name.m_comps[0].m_id_len*/);
    const DSMCCCacheReference *entry =
        pBB->m_ior.m_profile_body->GetReference();

    pDir->m_SubDirectories.insert(name, *entry);

    VERBOSE(VB_DSMCC,
            QString("[DSMCCCache] Adding directory with name %1 reference %2")
            .arg(name.ascii()).arg(entry->toString()));
}

// Find File, Directory or Gateway by reference.
DSMCCCacheFile *DSMCCCache::FindFileData(DSMCCCacheReference &ref)
{
    // Find a file.
    QMap<DSMCCCacheReference, DSMCCCacheFile*>::Iterator fil =
        m_Files.find(ref);

    if (fil == m_Files.end())
        return NULL;

    return *fil;
}

DSMCCCacheDir *DSMCCCache::FindDir(DSMCCCacheReference &ref)
{
    // Find a directory.
    QMap<DSMCCCacheReference, DSMCCCacheDir*>::Iterator dir =
        m_Directories.find(ref);

    if (dir == m_Directories.end())
        return NULL;

    return *dir;
}

DSMCCCacheDir *DSMCCCache::FindGateway(DSMCCCacheReference &ref)
{
    // Find a gateway.
    QMap<DSMCCCacheReference, DSMCCCacheDir*>::Iterator dir =
        m_Gateways.find(ref);

    if (dir == m_Gateways.end())
        return NULL;

    return *dir;
}


// Return the contents of an object if it exists.
// Returns zero for success, -1 if we know the object does not
// currently exist and +1 if the carousel has not so far loaded
// the object or one of the parent files. 
int DSMCCCache::GetObject(QStringList &objectPath, QByteArray &result)
{
    DSMCCCacheDir *dir = FindGateway(m_GatewayRef);
    if (dir == NULL)
        return 1; // No gateway yet.

    QStringList::Iterator it = objectPath.begin();
    while (it != objectPath.end())
    {
        QString name = *it;
        ++it;
        if (it == objectPath.end())
        { // It's a leaf - look in the file names
            QMap<QString, DSMCCCacheReference>::Iterator ref =
                dir->m_Files.find(name);

            if (ref == dir->m_Files.end())
                return -1; // Not there.

            DSMCCCacheFile *fil = FindFileData(*ref);

            if (fil == NULL) // Exists but not yet set.
                return 1;

            result = fil->m_Contents;
            return 0;
        }
        else
        { // It's a directory
            QMap<QString, DSMCCCacheReference>::Iterator ref =
                dir->m_SubDirectories.find(name);

            if (ref == dir->m_SubDirectories.end())
                return -1; // Not there

            dir = FindDir(*ref);

            if (dir == NULL) // Exists but not yet set.
                return 1;
            // else search in this directory
        }
    }

    return -1;
}

// Set the gateway reference from a DSI message.
void DSMCCCache::SetGateway(const DSMCCCacheReference &ref)
{
    VERBOSE(VB_DSMCC, QString("[DSMCCCache] Setting gateway to reference %1")
            .arg(ref.toString()));

    m_GatewayRef = ref;
}
