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
#endif

#ifdef BSD
#include <sys/param.h>
#include <sys/mount.h>  // for struct statfs
#endif

#include <QList>
#include <QString>
#include <QStringList>
#include <utility>

#include "filesysteminfo.h"
#include "mythcorecontext.h"
#include "mythcoreutil.h"
#include "mythlogging.h"

FileSystemInfo::FileSystemInfo(const FileSystemInfo &other)
{
    FileSystemInfo::clone(other);
}

FileSystemInfo::FileSystemInfo(QString hostname, QString path, bool local,
        int fsid, int groupid, int blksize, int64_t total, int64_t used) :
    m_hostname(std::move(hostname)), m_path(std::move(path)), m_local(local), m_fsid(fsid),
    m_grpid(groupid), m_blksize(blksize), m_total(total), m_used(used)
{
}

FileSystemInfo::FileSystemInfo(QStringList::const_iterator &it,
                   const QStringList::const_iterator& end)
{
    FromStringList(it, end);
}

FileSystemInfo::FileSystemInfo(const QStringList &slist)
{
    FromStringList(slist);
}

void FileSystemInfo::clone(const FileSystemInfo &other)
{
    m_hostname = other.m_hostname;
    m_path = other.m_path;
    m_local = other.m_local;
    m_fsid = other.m_fsid;
    m_grpid = other.m_grpid;
    m_blksize = other.m_blksize;
    m_total = other.m_total;
    m_used = other.m_used;
    m_weight = other.m_weight;
}

FileSystemInfo &FileSystemInfo::operator=(const FileSystemInfo &other)
{
    if (this == &other)
        return *this;

    clone(other);
    return *this;
}

void FileSystemInfo::clear(void)
{
    m_hostname  = "";
    m_path      = "";
    m_local     = false;
    m_fsid      = -1;
    m_grpid     = -1;
    m_blksize   = 4096;
    m_total     = 0;
    m_used      = 0;
    m_weight    = 0;
}

static constexpr int k_lines = 8;

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

QList<FileSystemInfo> FileSystemInfo::RemoteGetInfo(MythSocket *sock)
{
    FileSystemInfo fsInfo;
    QList<FileSystemInfo> fsInfos;
    QStringList strlist(QString("QUERY_FREE_SPACE_LIST"));

    bool sent = false;

    if (sock)
        sent = sock->SendReceiveStringList(strlist);
    else
        sent = gCoreContext->SendReceiveStringList(strlist);

    if (sent)
    {
        int numdisks = strlist.size() / k_lines;

        QStringList::const_iterator it = strlist.cbegin();
        for (int i = 0; i < numdisks; i++)
        {
            fsInfo.FromStringList(it, strlist.cend());
            fsInfos.append(fsInfo);
        }
    }

    return fsInfos;
}

void FileSystemInfo::Consolidate(QList<FileSystemInfo> &disks,
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

            int bSize = std::max(32, std::max(it1->getBlockSize(), it2->getBlockSize())
                                        / 1024);
            int64_t diffSize = it1->getTotalSpace() - it2->getTotalSpace();
            int64_t diffUsed = it1->getUsedSpace() - it2->getUsedSpace();

            if (diffSize < 0)
                diffSize = 0 - diffSize;
            if (diffUsed < 0)
                diffUsed = 0 - diffUsed;

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

void FileSystemInfo::PopulateDiskSpace(void)
{
    int64_t total = -1;
    int64_t used = -1;
    getDiskSpace(getPath().toLatin1().constData(), total, used);
    setTotalSpace(total);
    setUsedSpace(used);
}

void FileSystemInfo::PopulateFSProp(void)
{
    struct statfs statbuf {};

    if (statfs(getPath().toLocal8Bit().constData(), &statbuf) == 0)
    {
#ifdef Q_OS_DARWIN
        char *fstypename = statbuf.f_fstypename;
        if ((!strcmp(fstypename, "nfs")) ||     // NFS|FTP
            (!strcmp(fstypename, "afpfs")) ||   // AppleShare
            (!strcmp(fstypename, "smbfs")))     // SMB
                setLocal(false);
#elif defined(__linux__)
        long fstype = statbuf.f_type;
        if ((fstype == 0x6969)  ||              // NFS
            (fstype == 0x517B)  ||              // SMB
            (fstype == (long)0xFF534D42))       // CIFS
                setLocal(false);
#endif
        setBlockSize(statbuf.f_bsize);
    }
}
