#ifndef DISKINFO_H_
#define DISKINFO_H_
#define NUMDISKINFOLINES 8

#include <stdint.h>

#include <QList>
#include <QString>
#include <QStringList>

#include "mythbaseexp.h"
#include "mythsocket.h"
#include "mythcorecontext.h"

class MBASE_PUBLIC FileSystemInfo : public QObject
{
    Q_OBJECT
  public:
    FileSystemInfo();
    FileSystemInfo(const FileSystemInfo &other);
    FileSystemInfo(QString hostname, QString path, bool local, int fsid,
             int groupid, int blksize, int64_t total, int64_t used);
    FileSystemInfo(QStringList::const_iterator &it,
            QStringList::const_iterator end);
    FileSystemInfo(const QStringList &slist);

   ~FileSystemInfo(void) {};

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
    void setHostname(QString hostname)      { m_hostname = hostname; }
    void setPath(QString path)              { m_path = path; }
    void setLocal(bool local = true)        { m_local = local; }
    void setFSysID(int id)                  { m_fsid = id; }
    void setGroupID(int id)                 { m_grpid = id; }
    void setBlockSize(int size)             { m_blksize = size; }
    void setTotalSpace(int64_t size)        { m_total = size; }
    void setUsedSpace(int64_t size)         { m_used = size; }
    void setWeight(int weight)              { m_weight = weight; }

    bool        ToStringList(QStringList &slist) const;

    static const QList<FileSystemInfo> RemoteGetInfo(MythSocket *sock=NULL);
    static void Consolidate(QList<FileSystemInfo> &disks, bool merge=true,
                            int64_t fuzz=14000);
    void PopulateDiskSpace(void);
    void PopulateFSProp(void);

  private:
    bool        FromStringList(const QStringList &slist);
    bool        FromStringList(QStringList::const_iterator &it,
                               QStringList::const_iterator listend);

    QString m_hostname;
    QString m_path;
    bool m_local;
    int m_fsid;
    int m_grpid;
    int m_blksize;
    int64_t m_total;
    int64_t m_used;
    int m_weight;
};
#endif
