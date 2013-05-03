#ifndef AUTOEXPIRE_H_
#define AUTOEXPIRE_H_

#include <stdint.h>

#include <vector>
using namespace std;

#include <QWaitCondition>
#include <QDateTime>
#include <QPointer>
#include <QObject>
#include <QString>
#include <QMutex>
#include <QSet>
#include <QMap>

#include "mthread.h"

class ProgramInfo;
class EncoderLink;
class FileSystemInfo;
class MainServer;

typedef vector<ProgramInfo*> pginfolist_t;
typedef vector<EncoderLink*> enclinklist_t;

enum ExpireMethodType {
    emOldestFirst           = 1,
    emLowestPriorityFirst   = 2,
    emWeightedTimePriority  = 3,
    emShortLiveTVPrograms   = 10000,
    emNormalLiveTVPrograms  = 10001,
    emOldDeletedPrograms    = 10002,
    emNormalDeletedPrograms = 10003,
    emQuickDeletedPrograms  = 10004
};

class AutoExpire;

class ExpireThread : public MThread
{
  public:
    ExpireThread(AutoExpire *p) : MThread("Expire"), m_parent(p) {}
    virtual ~ExpireThread() { wait(); }
    virtual void run(void);
  private:
    QPointer<AutoExpire> m_parent;
};

class UpdateThread : public QObject, public MThread
{
    Q_OBJECT
  public:
    UpdateThread(AutoExpire *p) : MThread("Update"), m_parent(p) {}
    virtual ~UpdateThread() { wait(); }
    virtual void run(void);
  private:
    QPointer<AutoExpire> m_parent;
};

class AutoExpire : public QObject
{
    Q_OBJECT

    friend class ExpireThread;
    friend class UpdateThread;
  public:
    AutoExpire(QMap<int, EncoderLink *> *encoderList);
    AutoExpire(void);
   ~AutoExpire();

    void CalcParams(void);
    void PrintExpireList(QString expHost = "ALL");

    uint64_t GetDesiredSpace(int fsID) const;

    void GetAllExpiring(QStringList &strList);
    void GetAllExpiring(pginfolist_t &list);
    void ClearExpireList(pginfolist_t &expireList, bool deleteProg = true);

    static void Update(int encoder, int fsID, bool immediately);
    static void Update(bool immediately) { Update(0, -1, immediately); }

    void SetMainServer(MainServer *ms)
    {
        QMutexLocker locker(&instance_lock);
        main_server = ms;
    }

    QMap<int, EncoderLink *> *encoderList;

  protected:
    void RunExpirer(void);
    void RunUpdate(void);

  private:
    void ExpireLiveTV(int type);
    void ExpireOldDeleted(void);
    void ExpireQuickDeleted(void);
    void ExpireRecordings(void);
    void ExpireEpisodesOverMax(void);

    void FillExpireList(pginfolist_t &expireList);
    void FillDBOrdered(pginfolist_t &expireList, int expMethod);
    void SendDeleteMessages(pginfolist_t &deleteList);
    void Sleep(int sleepTime /*ms*/);

    void UpdateDontExpireSet(void);
    bool IsInDontExpireSet(uint chanid, const QDateTime &recstartts) const;
    static bool IsInExpireList(const pginfolist_t &expireList,
                               uint chanid, const QDateTime &recstartts);

    // main expire info
    QSet<QString> dont_expire_set;
    ExpireThread *expire_thread;     // protected by instance_lock
    uint          desired_freq;      // protected by instance_lock
    bool          expire_thread_run; // protected by instance_lock

    QMap<int, int64_t>  desired_space; // protected by instance_lock
    QMap<int, int>      used_encoders; // protected by instance_lock

    mutable QMutex instance_lock;
    QWaitCondition instance_cond; // protected by instance_lock

    MainServer   *main_server;    // protected by instance_lock

    // update info
    bool          update_pending; // protected by instance_lock
    UpdateThread *update_thread;
};

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
