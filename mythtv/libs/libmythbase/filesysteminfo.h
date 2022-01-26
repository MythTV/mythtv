#ifndef DISKINFO_H_
#define DISKINFO_H_

#include <cstdint>
#include <utility>

#include <QList>
#include <QString>
#include <QStringList>

#include "mythbaseexp.h"
#include "mythsocket.h"

class MBASE_PUBLIC FileSystemInfo
{
  public:
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
    QString     getHostname(void)     const { return m_hostname; }
    QString     getPath(void)         const { return m_path; }
    bool        isLocal(void)         const { return m_local; }
    int         getFSysID(void)       const { return m_fsid; }
    int         getGroupID(void)      const { return m_grpid; }
    int         getBlockSize(void)    const { return m_blksize; }
    int64_t     getTotalSpace(void)   const { return m_total; }
    int64_t     getUsedSpace(void)    const { return m_used; }
    int         getWeight(void)       const { return m_weight; }

    // not cached because of use in mythbackend/autoexpire
    int64_t     getFreeSpace(void)    const { return m_total - m_used; }

    // information puts
    void setHostname(QString hostname)      { m_hostname = std::move(hostname); }
    void setPath(QString path)              { m_path     = std::move(path); }
    void setLocal(bool local = true)        { m_local = local; }
    void setFSysID(int id)                  { m_fsid = id; }
    void setGroupID(int id)                 { m_grpid = id; }
    void setBlockSize(int size)             { m_blksize = size; }
    void setTotalSpace(int64_t size)        { m_total = size; }
    void setUsedSpace(int64_t size)         { m_used = size; }
    void setWeight(int weight)              { m_weight = weight; }

    bool        ToStringList(QStringList &slist) const;
    QStringList ToStringList() const;

    static QList<FileSystemInfo> RemoteGetInfo(MythSocket *sock=nullptr);
    static void Consolidate(QList<FileSystemInfo> &disks, bool merge=true,
                            int64_t fuzz=14000);

    /// @brief update statfs filesystem statistics by reading from the storage device
    /// @returns If successful
    bool refresh();

  private:
    bool        FromStringList(const QStringList &slist);
    bool        FromStringList(QStringList::const_iterator &it,
                               const QStringList::const_iterator& listend);

    QString m_hostname;
    QString m_path;
    bool    m_local {false}; ///< set based on statfs
    int     m_fsid  {-1};    ///< set by Consolidate
    int     m_grpid {-1};    ///< set by setGroupID
    // cached from statfs
    int m_blksize   {4096};
    int64_t m_total {0};
    int64_t m_used  {0};

    // not serialized
    int     m_weight    {0}; ///< set by setWeight
};
#endif
