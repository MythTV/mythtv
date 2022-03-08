#include "filesysteminfo.h"

#include <unistd.h>
#include <cstdlib>

#include "compat.h"

#include <QtGlobal>

#ifdef __linux__
#include <sys/vfs.h>
#include <sys/sysinfo.h>
#endif

#ifdef Q_OS_DARWIN
#include <mach/mach.h>
#include <cstring>
#endif

#ifdef BSD
#include <sys/param.h>
#include <sys/mount.h>  // for struct statfs
#endif

#include <algorithm>

#include <QList>
#include <QString>
#include <QStringList>

#include "mythcorecontext.h"
#include "mythlogging.h"

QStringList FileSystemInfo::ToStringList() const
{
    QStringList list;
    list.reserve(k_lines);
    list << m_hostname;
    list << m_path;
    list << QString::number(m_local);
    list << QString::number(m_fsid);
    list << QString::number(m_grpid);
    list << QString::number(m_blksize);
    list << QString::number(m_total);
    list << QString::number(m_used);

    return list;
}

bool FileSystemInfo::ToStringList(QStringList &list) const
{
    list << ToStringList();

    return true;
}

bool FileSystemInfo::FromStringList(const QStringList &slist)
{
    QStringList::const_iterator it = slist.cbegin();
    return FromStringList(it, slist.cend());
}

/**
@brief Deserialize a FileSystemInfo.
The FileSystemInfo will be in a default state if deserialization fails.
@param [in,out] it iterator to the beginning of the FileSystemInfo to deserialize.
                   It is incremented so this function can be called repeatedly in
                   a while loop.  it will be set to listend when there aren't enough
                   strings to deserialize a FileSystemInfo.
@param [in] listend end of the string list referenced by both iterators
@return Boolean, true if deserialized
*/
bool FileSystemInfo::FromStringList(QStringList::const_iterator &it,
                                    const QStringList::const_iterator& listend)
{
    if (std::distance(it, listend) < k_lines)
    {
        LOG(VB_GENERAL, LOG_ALERT, QStringLiteral("FileSystemInfo::FromStringList, not enough items in list."));
        clear();
        it = listend;
        return false;
    }

    m_hostname  = *it; it++;
    m_path      = *it; it++;
    m_local     = (*it).toLongLong(); it++;
    m_fsid      = (*it).toLongLong(); it++;
    m_grpid     = (*it).toLongLong(); it++;
    m_blksize   = (*it).toLongLong(); it++;
    m_total     = (*it).toLongLong(); it++;
    m_used      = (*it).toLongLong(); it++;

    m_weight = 0;

    return true;
}

bool FileSystemInfo::refresh()
{
    struct statfs statbuf {};

    // there are cases where statfs will return 0 (good), but f_blocks and
    // others are invalid and set to 0 (such as when an automounted directory
    // is not mounted but still visible because --ghost was used),
    // so check to make sure we can have a total size > 0
    if ((statfs(getPath().toLocal8Bit().constData(), &statbuf) == 0) &&
        (statbuf.f_blocks > 0) && (statbuf.f_bsize > 0)
       )
    {
        m_total = statbuf.f_blocks * statbuf.f_bsize;
        //free  = statbuf.f_bavail * statbuf.f_bsize;
        m_used  = m_total - statbuf.f_bavail * statbuf.f_bsize;
        m_blksize = statbuf.f_bsize;

        // TODO keep as B not KiB
        m_total >>= 10;
        m_used  >>= 10;

#ifdef Q_OS_DARWIN
        char *fstypename = statbuf.f_fstypename;
        m_local = !(
                    (strcmp(fstypename, "nfs")   == 0) ||  // NFS|FTP
                    (strcmp(fstypename, "afpfs") == 0) ||  // AppleShare
                    (strcmp(fstypename, "smbfs") == 0)     // SMB
                   );
#elif defined(__linux__)
        long fstype = statbuf.f_type;
        m_local = !(
                    (fstype == 0x6969)  ||   // NFS
                    (fstype == 0x517B)  ||   // SMB
                    (fstype == 0xFF534D42L)  // CIFS
                   );
#else
        m_local = true; // for equivalent behavior
#endif
        return true;
    }
    return false;
}

// default: sock = nullptr
FileSystemInfoList FileSystemInfoManager::GetInfoList(MythSocket *sock)
{
    FileSystemInfoList fsInfos;
    QStringList strlist(QStringLiteral("QUERY_FREE_SPACE_LIST"));

    if ((sock != nullptr)
        ? sock->SendReceiveStringList(strlist)
        : gCoreContext->SendReceiveStringList(strlist)
       )
    {
        fsInfos = FileSystemInfoManager::FromStringList(strlist);
    }

    return fsInfos;
}

// O(n^2)
void FileSystemInfoManager::Consolidate(FileSystemInfoList &disks,
                                        bool merge, int64_t fuzz)
{
    int newid = 0;

    for (auto it1 = disks.begin(); it1 != disks.end(); ++it1)
    {
        if (it1->getFSysID() == -1)
        {
            it1->setFSysID(newid++);
            if (merge)
                it1->setPath(it1->getHostname().section(".", 0, 0)
                                + ":" + it1->getPath());
        }

        for (auto it2 = it1+1; it2 != disks.end(); ++it2)
        {
            if (it2->getFSysID() != -1) // disk has already been matched
                continue;

            // almost certainly guaranteed to be 32 KiB
            int bSize = std::max( {32 << 10, it1->getBlockSize(), it2->getBlockSize()} );
            bSize >>= 10;
            int64_t diffSize = std::abs(it1->getTotalSpace() - it2->getTotalSpace());
            int64_t diffUsed = std::abs(it1->getUsedSpace()  - it2->getUsedSpace());

            if ((diffSize <= bSize) && (diffUsed <= fuzz))
            {
                it2->setFSysID(it1->getFSysID());

                if (merge)
                {
                    if (!it1->getHostname().contains(it2->getHostname()))
                        it1->setHostname(it1->getHostname()
                                        + "," + it2->getHostname());
                    it1->setPath(it1->getPath() + ","
                                 + it2->getHostname().section(".", 0, 0) + ":"
                                 + it2->getPath());
                    disks.erase(it2);
                    it2 = it1;
                }
            }
        }
    }
}

FileSystemInfoList FileSystemInfoManager::FromStringList(const QStringList& list)
{
    FileSystemInfoList fsInfos;
    fsInfos.reserve(list.size() / FileSystemInfo::k_lines + 1);

    QStringList::const_iterator it = list.cbegin();
    while (it < list.cend())
    {
        fsInfos.push_back(FileSystemInfo(it, list.cend()));
    }

    return fsInfos;
}

QStringList FileSystemInfoManager::ToStringList(const FileSystemInfoList& fsInfos)
{
    QStringList strlist;
    for (const auto & fsInfo : fsInfos)
    {
        strlist << fsInfo.ToStringList();
    }
    return strlist;
}
