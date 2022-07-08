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
#include "mythcoreutil.h"

// for deserialization
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NEXT_STR()        do { if (it == listend)                    \
                               {                                     \
                                   LOG(VB_GENERAL, LOG_ALERT, listerror); \
                                   clear();                          \
                                   return false;                     \
                               }                                     \
                               ts = *it++; } while (false)

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

bool FileSystemInfo::ToStringList(QStringList &list) const
{
    list << m_hostname;
    list << m_path;
    list << QString::number(m_local);
    list << QString::number(m_fsid);
    list << QString::number(m_grpid);
    list << QString::number(m_blksize);
    list << QString::number(m_total);
    list << QString::number(m_used);

    return true;
}

bool FileSystemInfo::FromStringList(const QStringList &slist)
{
    QStringList::const_iterator it = slist.constBegin();
    return FromStringList(it, slist.constEnd());
}

bool FileSystemInfo::FromStringList(QStringList::const_iterator &it,
                             const QStringList::const_iterator& listend)
{
    QString listerror = "FileSystemInfo: FromStringList, not enough items in list.";
    QString ts;

    NEXT_STR(); m_hostname = ts;
    NEXT_STR(); m_path     = ts;
    NEXT_STR(); m_local    = ts.toLongLong();
    NEXT_STR(); m_fsid     = ts.toLongLong();
    NEXT_STR(); m_grpid    = ts.toLongLong();
    NEXT_STR(); m_blksize  = ts.toLongLong();
    NEXT_STR(); m_total    = ts.toLongLong();
    NEXT_STR(); m_used     = ts.toLongLong();

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
        int numdisks = strlist.size()/NUMDISKINFOLINES;

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
