#ifndef DISKINFO_H_
#define DISKINFO_H_

#include <cstdint>
#include <utility>

#include <QVector>
#include <QString>
#include <QStringList>

#include "mythbaseexp.h"
#include "mythsocket.h"

class MBASE_PUBLIC FileSystemInfo
{
  public:
    static constexpr int kLines = 8; ///< number of strings in the serialized stringlist

    FileSystemInfo() = default;
    FileSystemInfo(QString hostname,
                   QString path,
                   int groupid = -1)
                   :
        m_hostname  (std::move(hostname)),
        m_path      (std::move(path)),
        m_grpid     (groupid)
    {
        refresh();
    }
    FileSystemInfo(QString hostname,
                   QString path,
                   bool local,
                   int fsid,
                   int groupid,
                   int blksize,
                   int64_t total,
                   int64_t used)
                   :
        m_hostname  (std::move(hostname)),
        m_path      (std::move(path)),
        m_local     (local),
        m_fsid      (fsid),
        m_grpid     (groupid),
        m_blksize   (blksize),
        m_total     (total),
        m_used      (used)
    {
    }
    FileSystemInfo(QStringList::const_iterator &it, const QStringList::const_iterator& end)
    {
        FromStringList(it, end);
    }
    explicit FileSystemInfo(const QStringList &slist) { FromStringList(slist); }

    void clear() { *this = FileSystemInfo(); }

    // information gets
    QString     getHostname()     const { return m_hostname; }
    QString     getPath()         const { return m_path; }
    bool        isLocal()         const { return m_local; }
    int         getFSysID()       const { return m_fsid; }
    int         getGroupID()      const { return m_grpid; }
    int         getBlockSize()    const { return m_blksize; }
    int64_t     getTotalSpace()   const { return m_total; }
    int64_t     getUsedSpace()    const { return m_used; }
    int         getWeight()       const { return m_weight; }

    // not cached because of use in mythbackend/autoexpire
    int64_t     getFreeSpace()    const { return m_total - m_used; }

    // information puts
    void setHostname(QString hostname)      { m_hostname = std::move(hostname); }
    void setPath(QString path)              { m_path     = std::move(path); }
    void setFSysID(int id)                  { m_fsid = id; } // TODO add regenerate option to Consolidate
    void setUsedSpace(int64_t size)         { m_used = size; } // TODO call refresh in autoexpire after deleting? autoexpire's log message uses the wrong units, should be KiB
    void setWeight(int weight)              { m_weight = weight; } // scheduler, use std::pair<int, FileSystemInfo>?

    bool        ToStringList(QStringList &slist) const;
    QStringList ToStringList() const;

    /// @brief update filesystem statistics by reading from the storage device
    /// @returns Boolean, true if successful
    bool refresh();

  private:
    bool        FromStringList(const QStringList &slist);
    bool        FromStringList(QStringList::const_iterator &it,
                               const QStringList::const_iterator& listend);

    QString m_hostname;
    QString m_path;
    bool    m_local {false}; ///< set based on QStorageInfo::device()
    int     m_fsid  {-1};    ///< set by Consolidate
    int     m_grpid {-1};    ///< set by setGroupID
    int m_blksize   {4096};
    int64_t m_total {0};
    int64_t m_used  {0};

    // not serialized
    int     m_weight    {0}; ///< set by setWeight
};

using FileSystemInfoList = QVector<FileSystemInfo>;
namespace FileSystemInfoManager
{
MBASE_PUBLIC FileSystemInfoList FromStringList(const QStringList& list);

MBASE_PUBLIC QStringList ToStringList(const FileSystemInfoList& fsInfos);

MBASE_PUBLIC FileSystemInfoList GetInfoList(MythSocket *sock = nullptr);

MBASE_PUBLIC
void Consolidate(FileSystemInfoList &disks, bool merge, int64_t fuzz, const QString& total_name = {});
} // namespace FileSystemInfoManager

#endif
