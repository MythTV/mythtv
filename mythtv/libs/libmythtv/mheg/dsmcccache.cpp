/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer
 */
#include <cstring> // For memcmp

#include <QStringList>

#include "libmythbase/mythlogging.h"

#include "dsmcccache.h"
#include "dsmccbiop.h"
#include "dsmccreceiver.h"
#include "dsmcc.h"

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
    // Delete all this when the cache is deleted.
  : m_dsmcc(dsmcc)
{
}

DSMCCCache::~DSMCCCache()
{
    QMap<DSMCCCacheReference, DSMCCCacheDir*>::Iterator dir;
    QMap<DSMCCCacheReference, DSMCCCacheFile*>::Iterator fil;

    for (dir = m_directories.begin(); dir != m_directories.end(); ++dir)
        delete *dir;

    for (dir = m_gateways.begin(); dir != m_gateways.end(); ++dir)
        delete *dir;

    for (fil = m_files.begin(); fil != m_files.end(); ++fil)
        delete *fil;
}

// Print out a key for debugging.
QString DSMCCCacheKey::toString() const
{
    QString result;
    for (int i = 0; i < 4 && i < size(); i++)
        result += QString("%1").arg(at(i), 2, 16, QChar('0'));
    return result;
}

// Ordering function used in QMap
bool operator < (const DSMCCCacheKey &key1, const DSMCCCacheKey &key2)
{
    const char *data1 = key1.data();
    const char *data2 = key2.data();
    uint size1 = key1.size();
    uint size2 = key2.size();
    uint size = 0;
    if (size1 < size2)
        size = size1;
    else
        size = size2;
    int res = memcmp(data1, data2, size);
    if (res < 0)
        return true;
    if (res > 0)
        return false;

    return size1 < size2;
}

// Test for equality of the cache references
// which include the carousel and module ids.
bool DSMCCCacheReference::Equal(const DSMCCCacheReference &r) const
{
    return m_nCarouselId == r.m_nCarouselId && m_nModuleId == r.m_nModuleId &&
        m_nStreamTag == r.m_nStreamTag && m_key == r.m_key;
}

bool DSMCCCacheReference::Equal(const DSMCCCacheReference *p) const
{
    return p != nullptr && Equal(*p);
}


// Print out a cache reference for debugging.
QString DSMCCCacheReference::toString(void) const
{
    return QString("%1-%2-%3-")
        .arg(m_nCarouselId).arg(m_nStreamTag)
        .arg(m_nModuleId) + m_key.toString();
}

// Operator required for QMap
bool operator < (const DSMCCCacheReference &ref1,
                 const DSMCCCacheReference &ref2)
{
    if (ref1.m_nCarouselId < ref2.m_nCarouselId)
        return true;
    if (ref1.m_nCarouselId > ref2.m_nCarouselId)
        return false;
    if (ref1.m_nStreamTag < ref2.m_nStreamTag)
        return true;
    if (ref1.m_nStreamTag > ref2.m_nStreamTag)
        return false;
    if (ref1.m_nModuleId < ref2.m_nModuleId)
        return true;
    if (ref1.m_nModuleId > ref2.m_nModuleId)
        return false;
    if (ref1.m_key < ref2.m_key)
        return true;

    return false;
}

// Create a gateway entry.
DSMCCCacheDir *DSMCCCache::Srg(const DSMCCCacheReference &ref)
{
    // Check to see that it isn't already there.  It shouldn't be.
    QMap<DSMCCCacheReference, DSMCCCacheDir*>::Iterator dir =
        m_gateways.find(ref);

    if (dir != m_gateways.end())
    {
        LOG(VB_DSMCC, LOG_ERR, QString("[DSMCCCache] Already seen gateway %1")
                .arg(ref.toString()));
        return *dir;
    }

    LOG(VB_DSMCC, LOG_INFO, QString("[DSMCCCache] New gateway reference %1")
            .arg(ref.toString()));

    auto *pSrg = new DSMCCCacheDir(ref);
    m_gateways.insert(ref, pSrg);

    return pSrg;
}

// Create a directory entry.
DSMCCCacheDir *DSMCCCache::Directory(const DSMCCCacheReference &ref)
{
    // Check to see that it isn't already there.  It shouldn't be.
    QMap<DSMCCCacheReference, DSMCCCacheDir*>::Iterator dir =
        m_directories.find(ref);

    if (dir != m_directories.end())
    {
        LOG(VB_DSMCC, LOG_ERR, QString("[DSMCCCache] Already seen directory %1")
                .arg(ref.toString()));
        return *dir;
    }

    LOG(VB_DSMCC, LOG_INFO, QString("[DSMCCCache] New directory reference %1")
            .arg(ref.toString()));

    auto *pDir = new DSMCCCacheDir(ref);
    m_directories.insert(ref, pDir);

    return pDir;
}

// Called when the data for a file module arrives.
void DSMCCCache::CacheFileData(const DSMCCCacheReference &ref,
                               const QByteArray &data)
{
    // Do we have the file already?
    LOG(VB_DSMCC, LOG_INFO,
        QString("[DSMCCCache] Adding file data size %1 for reference %2")
            .arg(data.size()).arg(ref.toString()));

    QMap<DSMCCCacheReference, DSMCCCacheFile*>::Iterator fil =
        m_files.find(ref);

    DSMCCCacheFile *pFile = nullptr;
    if (fil == m_files.end())
    {
        pFile = new DSMCCCacheFile(ref);
        m_files.insert(ref, pFile);
    }
    else
    {
        pFile = *fil;
    }

    pFile->m_contents = data; // Save the data (this is use-counted by Qt).
}

// Add a file to the directory.
void DSMCCCache::AddFileInfo(DSMCCCacheDir *pDir, const BiopBinding *pBB)
{
    QString name;
    name = QString::fromLatin1(pBB->m_name.m_comps[0].m_id
                  /*, pBB->m_name.m_comps[0].m_id_len*/);

    const DSMCCCacheReference *entry =
        pBB->m_ior.m_profileBody->GetReference();

    pDir->m_files.insert(name, *entry);

    LOG(VB_DSMCC, LOG_INFO,
        QString("[DSMCCCache] Added file name %1 reference %2 parent %3")
        .arg(name, entry->toString(), pDir->m_reference.toString()));
}

// Add a sub-directory to the directory.
void DSMCCCache::AddDirInfo(DSMCCCacheDir *pDir, const BiopBinding *pBB)
{
    // Is it already there?
    QString name;
    name = QString::fromLatin1(pBB->m_name.m_comps[0].m_id
                  /*, pBB->m_name.m_comps[0].m_id_len*/);
    const DSMCCCacheReference *entry =
        pBB->m_ior.m_profileBody->GetReference();

    pDir->m_subDirectories.insert(name, *entry);

    LOG(VB_DSMCC, LOG_INFO,
        QString("[DSMCCCache] added subdirectory name %1 reference %2 parent %3")
        .arg(name, entry->toString(), pDir->m_reference.toString()));
}

// Find File, Directory or Gateway by reference.
DSMCCCacheFile *DSMCCCache::FindFileData(const DSMCCCacheReference &ref)
{
    // Find a file.
    QMap<DSMCCCacheReference, DSMCCCacheFile*>::Iterator fil =
        m_files.find(ref);

    if (fil == m_files.end())
        return nullptr;

    return *fil;
}

DSMCCCacheDir *DSMCCCache::FindDir(const DSMCCCacheReference &ref)
{
    // Find a directory.
    QMap<DSMCCCacheReference, DSMCCCacheDir*>::Iterator dir =
        m_directories.find(ref);

    if (dir == m_directories.end())
        return nullptr;

    return *dir;
}

DSMCCCacheDir *DSMCCCache::FindGateway(const DSMCCCacheReference &ref)
{
    // Find a gateway.
    QMap<DSMCCCacheReference, DSMCCCacheDir*>::Iterator dir =
        m_gateways.find(ref);

    if (dir == m_gateways.end())
        return nullptr;

    return *dir;
}


// Return the contents of an object if it exists.
// Returns zero for success, -1 if we know the object does not
// currently exist and +1 if the carousel has not so far loaded
// the object or one of the parent files.
int DSMCCCache::GetDSMObject(QStringList &objectPath, QByteArray &result)
{
    DSMCCCacheDir *dir = FindGateway(m_gatewayRef);
    if (dir == nullptr)
        return 1; // No gateway yet.

    QStringList::Iterator it = objectPath.begin();
    while (it != objectPath.end())
    {
        QString name = *it;
        ++it;
        if (it == objectPath.end())
        { // It's a leaf - look in the file names
            QMap<QString, DSMCCCacheReference>::Iterator ref =
                dir->m_files.find(name);

            if (ref == dir->m_files.end())
                return -1; // Not there.

            DSMCCCacheFile *fil = FindFileData(*ref);

            if (fil == nullptr) // Exists but not yet set.
                return 1;

            result = fil->m_contents;
            return 0;
        }

        // It's a directory
        QMap<QString, DSMCCCacheReference>::Iterator ref =
            dir->m_subDirectories.find(name);

        if (ref == dir->m_subDirectories.end())
            return -1; // Not there

        dir = FindDir(*ref);

        if (dir == nullptr) // Exists but not yet set.
            return 1;
        // else search in this directory
    }

    return -1;
}

// Set the gateway reference from a DSI message.
void DSMCCCache::SetGateway(const DSMCCCacheReference &ref)
{
    if (!m_gatewayRef.Equal(ref))
    {
        LOG(VB_DSMCC, LOG_INFO, QString("[DSMCCCache] Setting gateway to reference %1")
            .arg(ref.toString()));
        m_gatewayRef = ref;
    }
}
