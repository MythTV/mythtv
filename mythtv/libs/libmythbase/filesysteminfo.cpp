#include <unistd.h>
#include <cstdlib>
#include "compat.h"

#ifdef linux
#include <sys/vfs.h>
#include <sys/sysinfo.h>
#endif

#if CONFIG_DARWIN
#include <mach/mach.h>
#endif

#ifdef BSD
#include <sys/param.h>
#include <sys/mount.h>  // for struct statfs
#endif

using namespace std;

#include <QList>
#include <QString>
#include <QStringList>

#include "filesysteminfo.h"
#include "mythcoreutil.h"

// for serialization
#define INT_TO_LIST(x)       do { list << QString::number(x); } while (0)
#define DATETIME_TO_LIST(x)  INT_TO_LIST((x).toTime_t())
#define STR_TO_LIST(x)       do { list << (x); } while (0)

// for deserialization
#define NEXT_STR()        do { if (it == listend)                    \
                               {                                     \
                                   LOG(VB_GENERAL, LOG_ALERT, listerror); \
                                   clear();                          \
                                   return false;                     \
                               }                                     \
                               ts = *it++; } while (0)
#define INT_FROM_LIST(x)     do { NEXT_STR(); (x) = ts.toLongLong(); } while (0)
#define ENUM_FROM_LIST(x, y) do { NEXT_STR(); (x) = ((y)ts.toInt()); } while (0)
#define DATETIME_FROM_LIST(x) \
    do { NEXT_STR(); x = fromTime_t(ts.toUInt()); } while (0)
#define STR_FROM_LIST(x)     do { NEXT_STR(); (x) = ts; } while (0)

#define LOC QString("FileSystemInfo: ")

FileSystemInfo::FileSystemInfo(void) :
    m_hostname(""), m_path(""), m_local(false), m_fsid(-1),
    m_grpid(-1), m_blksize(4096), m_total(0), m_used(0), m_weight(0)
{
}

FileSystemInfo::FileSystemInfo(const FileSystemInfo &other)
{
    clone(other);
}

FileSystemInfo::FileSystemInfo(QString hostname, QString path, bool local,
        int fsid, int groupid, int blksize, int64_t total, int64_t used) :
    m_hostname(hostname), m_path(path), m_local(local), m_fsid(fsid),
    m_grpid(groupid), m_blksize(blksize), m_total(total), m_used(used),
    m_weight(0)
{
}

FileSystemInfo::FileSystemInfo(QStringList::const_iterator &it,
                   QStringList::const_iterator end)
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
    STR_TO_LIST(m_hostname);
    STR_TO_LIST(m_path);
    INT_TO_LIST(m_local);
    INT_TO_LIST(m_fsid);
    INT_TO_LIST(m_grpid);
    INT_TO_LIST(m_blksize);
    INT_TO_LIST(m_total);
    INT_TO_LIST(m_used);

    return true;
}

bool FileSystemInfo::FromStringList(const QStringList &slist)
{
    QStringList::const_iterator it = slist.constBegin();
    return FromStringList(it, slist.constEnd());
}

bool FileSystemInfo::FromStringList(QStringList::const_iterator &it,
                             QStringList::const_iterator listend)
{
    QString listerror = LOC + "FromStringList, not enough items in list.";
    QString ts;

    STR_FROM_LIST(m_hostname);
    STR_FROM_LIST(m_path);
    INT_FROM_LIST(m_local);
    INT_FROM_LIST(m_fsid);
    INT_FROM_LIST(m_grpid);
    INT_FROM_LIST(m_blksize);
    INT_FROM_LIST(m_total);
    INT_FROM_LIST(m_used);

    m_weight = 0;

    return true;
}

const QList<FileSystemInfo> FileSystemInfo::RemoteGetInfo(MythSocket *sock)
{
    FileSystemInfo fsInfo;
    QList<FileSystemInfo> fsInfos;
    QStringList strlist(QString("QUERY_FREE_SPACE_LIST"));

    bool sent;

    if (sock)
        sent = sock->SendReceiveStringList(strlist);
    else
        sent = gCoreContext->SendReceiveStringList(strlist);

    if (sent)
    {
        int numdisks = strlist.size()/NUMDISKINFOLINES;

        QStringList::const_iterator it = strlist.begin();
        for (int i = 0; i < numdisks; i++)
        {
            fsInfo.FromStringList(it, strlist.end());
            fsInfos.append(fsInfo);
        }
    }

    return fsInfos;
}

void FileSystemInfo::Consolidate(QList<FileSystemInfo> &disks,
                                 bool merge, int64_t fuzz)
{
    int newid = 0;

    QList<FileSystemInfo>::iterator it1, it2;
    for (it1 = disks.begin(); it1 != disks.end(); ++it1)
    {
        if (it1->getFSysID() == -1)
        {
            it1->setFSysID(newid++);
            if (merge)
                it1->setPath(it1->getHostname().section(".", 0, 0)
                                + ":" + it1->getPath());
        }

        for (it2 = it1+1; it2 != disks.end(); ++it2)
        {
            if (it2->getFSysID() != -1) // disk has already been matched
                continue;

            int bSize = max(32, max(it1->getBlockSize(), it2->getBlockSize())
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
    int64_t total = -1, used = -1;
    getDiskSpace(getPath().toLatin1().constData(), total, used);
    setTotalSpace(total);
    setUsedSpace(used);
}

void FileSystemInfo::PopulateFSProp(void)
{
    struct statfs statbuf;
    memset(&statbuf, 0, sizeof(statbuf));

    if (!statfs(getPath().toLocal8Bit().constData(), &statbuf))
    {
#if CONFIG_DARWIN
        char *fstypename = statbuf.f_fstypename;
        if ((!strcmp(fstypename, "nfs")) ||     // NFS|FTP
            (!strcmp(fstypename, "afpfs")) ||   // AppleShare
            (!strcmp(fstypename, "smbfs")))     // SMB
                setLocal(false);
#elif __linux__
        long fstype = statbuf.f_type;
        if ((fstype == 0x6969)  ||              // NFS
            (fstype == 0x517B)  ||              // SMB
            (fstype == (long)0xFF534D42))       // CIFS
                setLocal(false);
#endif
        setBlockSize(statbuf.f_bsize);
    }
}
