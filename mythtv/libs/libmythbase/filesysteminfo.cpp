#include "filesysteminfo.h"

#include <QtGlobal>

#include <algorithm>

#include <QByteArray>
#include <QStorageInfo>
#include <QString>
#include <QStringList>

#include "mythcorecontext.h"
#include "mythlogging.h"

QStringList FileSystemInfo::ToStringList() const
{
    QStringList list;
    list.reserve(kLines);
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
    if (std::distance(it, listend) < kLines)
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
    QStorageInfo info {m_path};

    if (info.isValid() && info.isReady())
    {
        m_total   = info.bytesTotal();
        m_used    = info.bytesTotal() - info.bytesAvailable();
        m_blksize = info.blockSize();

        // TODO keep as B not KiB
        m_total >>= 10;
        m_used  >>= 10;

#ifdef Q_OS_WIN
        QByteArray device = info.device();
        m_local = (device.startsWith(R"(\\?\)") && !device.startsWith(R"(\\?\UNC\)")) ||
                  (device.startsWith(R"(\\.\)") && !device.startsWith(R"(\\.\UNC\)"));
#else // Unix-like
        m_local = info.device().startsWith("/dev/");
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
                                        bool merge, int64_t fuzz,
                                        const QString& total_name)
{
    int newid = 0;
    int64_t total_total = 0;
    int64_t total_used  = 0;

    for (
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        auto* it1 = disks.begin();
#else
        auto it1 = disks.begin();
#endif
        it1 != disks.end(); ++it1)
    {
        if (it1->getFSysID() == -1)
        {
            it1->setFSysID(newid++);
            total_total += it1->getTotalSpace();
            total_used  += it1->getUsedSpace();
            if (merge)
                it1->setPath(it1->getHostname().section(".", 0, 0)
                                + ":" + it1->getPath());
        }

        for (
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
           auto* it2 = it1+1;
#else
           auto it2 = it1+1;
#endif
           it2 != disks.end(); ++it2)
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
    if (!total_name.isEmpty())
    {
        disks.append(FileSystemInfo(total_name, "TotalDiskSpace", false, -2, -2,
                                    0, total_total, total_used));
    }
}

FileSystemInfoList FileSystemInfoManager::FromStringList(const QStringList& list)
{
    FileSystemInfoList fsInfos;
    fsInfos.reserve((list.size() / FileSystemInfo::kLines) + 1);

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
