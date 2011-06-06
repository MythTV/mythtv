#ifndef DISKINFO_H_
#define DISKINFO_H_
#define NUMDISKINFOLINES 8

using namespace std;

#include <QList>
#include <QString>
#include <QStringList>

#include "mythexp.h"
#include "mythsocket.h"
#include "mythcorecontext.h"

class MPUBLIC FileSystemInfo : public QObject
{
    Q_OBJECT
  public:
    FileSystemInfo();
    FileSystemInfo(const FileSystemInfo &other);
    FileSystemInfo(QString hostname, QString path, bool local, int fsid,
             int groupid, int blksize, long long total, long long used);
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
    long long   getTotalSpace(void)   const { return m_total; }
    long long   getUsedSpace(void)    const { return m_used; }
    int         getWeight(void)       const { return m_weight; }

    long long   getFreeSpace(void)    const { return m_total-m_used; }

    // information puts
    void setHostname(QString hostname)      { m_hostname = hostname; }
    void setPath(QString path)              { m_path = path; }
    void setLocal(bool local = true)        { m_local = local; }
    void setFSysID(int id)                  { m_fsid = id; }
    void setGroupID(int id)                 { m_grpid = id; }
    void setBlockSize(int size)             { m_blksize = size; }
    void setTotalSpace(long long size)      { m_total = size; }
    void setUsedSpace(long long size)       { m_used = size; }
    void setWeight(int weight)              { m_weight = weight; }

    bool        ToStringList(QStringList &slist) const;

    static const QList<FileSystemInfo> RemoteGetInfo(MythSocket *sock=NULL);
    static void Consolidate(QList<FileSystemInfo> &disks, bool merge=true,
                            size_t fuzz=14000);
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
    long long m_total;
    long long m_used;
    int m_weight;
};
#endif
