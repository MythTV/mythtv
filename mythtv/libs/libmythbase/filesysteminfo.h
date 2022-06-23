#ifndef DISKINFO_H_
#define DISKINFO_H_
static constexpr int8_t NUMDISKINFOLINES { 8 };

#include <cstdint>

#include <QList>
#include <QString>
#include <QStringList>

#include "mythbaseexp.h"
#include "mythsocket.h"
#include "mythcorecontext.h"

class MBASE_PUBLIC FileSystemInfo
{
  public:
    FileSystemInfo() = default;
    FileSystemInfo(const FileSystemInfo &other);
    FileSystemInfo(QString hostname, QString path, bool local, int fsid,
             int groupid, int blksize, int64_t total, int64_t used);
    FileSystemInfo(QStringList::const_iterator &it,
            const QStringList::const_iterator& end);
    explicit FileSystemInfo(const QStringList &slist);

    virtual ~FileSystemInfo(void) = default;

    FileSystemInfo &operator=(const FileSystemInfo &other);
    virtual void clone(const FileSystemInfo &other);
    void clear(void);

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

    // reserved space could potentially result in this being negative
    int64_t     getFreeSpace(void)    const { return m_total-m_used; }

    // information puts
    void setHostname(const QString& hostname) { m_hostname = hostname; }
    void setPath(const QString& path)         { m_path = path; }
    void setLocal(bool local = true)        { m_local = local; }
    void setFSysID(int id)                  { m_fsid = id; }
    void setGroupID(int id)                 { m_grpid = id; }
    void setBlockSize(int size)             { m_blksize = size; }
    void setTotalSpace(int64_t size)        { m_total = size; }
    void setUsedSpace(int64_t size)         { m_used = size; }
    void setWeight(int weight)              { m_weight = weight; }

    bool        ToStringList(QStringList &slist) const;

    static QList<FileSystemInfo> RemoteGetInfo(MythSocket *sock=nullptr);
    static void Consolidate(QList<FileSystemInfo> &disks, bool merge=true,
                            int64_t fuzz=14000);
    void PopulateDiskSpace(void);
    void PopulateFSProp(void);

  private:
    bool        FromStringList(const QStringList &slist);
    bool        FromStringList(QStringList::const_iterator &it,
                               const QStringList::const_iterator& listend);

    QString m_hostname;
    QString m_path;
    bool m_local    {false};
    int m_fsid      {-1};
    int m_grpid     {-1};
    int m_blksize   {4096};
    int64_t m_total {0};
    int64_t m_used  {0};
    int m_weight    {0};
};
#endif
